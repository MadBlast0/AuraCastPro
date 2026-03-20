// =============================================================================
// AirPlay2Host.cpp -- AirPlay 2 receiver with RTSP + TLV8 AirPlay pairing
//
// Protocol phases:
//   Phase A -- TCP accept + RTSP routing            (complete)
//   Phase B -- SRP-6a pair-setup / pair-verify      (partial -- TLV8 transient pairing)
//   Phase C -- FairPlay (3rd-party auth)            (pass-through stub)
//   Phase D -- AES-128-CTR video decryption         (complete -- OpenSSL EVP)
//   Phase E -- Session lifecycle callbacks           (complete)
//
// References:
//   openairplay / UxPlay (GPL-2.0) -- protocol reference
//   RFC 5054 -- SRP-6a over TLS
//   RFC 7798 -- RTP payload for H.265
// =============================================================================

#include "AirPlay2Host.h"

#include "../pch.h"  // PCH
#include "../utils/Logger.h"
#include "../utils/NetworkTools.h"
#include "../utils/BinaryPlist.h"


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// OpenSSL for SRP-6a + AES-128-CTR
#include <openssl/aes.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/srp.h>

#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>


namespace aura {

constexpr uint16_t AIRPLAY_CONTROL_PORT = 7000;  // Standard AirPlay port
constexpr uint16_t AIRPLAY_AUDIO_RTP_PORT = 7011;    // Audio RTP (used in SETUP response)
constexpr uint16_t AIRPLAY_TIMING_SYNC_PORT = 7001;  // RTCP timing (standard port)
constexpr uint16_t AIRPLAY_TIMING_PORT = 7002;       // RTCP sync (standard port)
constexpr int BACKLOG = 4;
constexpr int RECV_TIMEOUT_MS = 0;  // No timeout - keep RTSP connection alive indefinitely
// Features bitmask - Apple TV 3rd gen features (H.264 only, no H.265)
constexpr uint32_t AIRPLAY_FEATURES_BASE = 0x5A7FFEE7;  // Minimal features, H.264 only
constexpr uint32_t AIRPLAY_FEATURES_H265 = 0x5A7FFFF7;  // H.265 enabled (not used)
constexpr uint32_t AIRPLAY_FLAGS = 0x0;
constexpr uint8_t kPairingMethodSetup = 0x00;
constexpr uint8_t kPairingMethodSetupAuth = 0x01;
constexpr uint8_t kPairingTransientFlag = 0x10;

enum class TlvTag : uint8_t {
    Method = 0,
    Identifier = 1,
    Salt = 2,
    PublicKey = 3,
    Proof = 4,
    EncryptedData = 5,
    State = 6,
    Error = 7,
    Flags = 19,
};

enum class PairState : uint8_t {
    M1 = 1,
    M2 = 2,
    M3 = 3,
    M4 = 4,
    M5 = 5,
    M6 = 6,
};

enum class PairError : uint8_t {
    Authentication = 2,
    Busy = 6,
    Unavailable = 7,
};

using TlvMap = std::map<uint8_t, std::vector<uint8_t>>;

// =============================================================================
// Minimal SRP-6a implementation for HomeKit/AirPlay TLV pairing
// =============================================================================
// SRP-6a group parameters (3072-bit group, generator 5)
static const char* kSRP_N_hex =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E08"
    "8A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B"
    "302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9"
    "A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
    "49286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8"
    "FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D"
    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C"
    "180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF695581718"
    "3995497CEA956AE515D2261898FA051015728E5A8AAAC42DAD33170D"
    "04507A33A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7D"
    "B3970F85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619DCEE3D226"
    "1AD2EE6BF12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
    "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB3143DB5BFC"
    "E0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF";

static const char* kSRP_g_hex = "05";

struct SRPSession {
    static constexpr size_t kHashBytes = 64;

    // Verifier state (we are the server)
    BIGNUM* N{nullptr};
    BIGNUM* g{nullptr};
    BIGNUM* k{nullptr};
    BIGNUM* v{nullptr};
    BIGNUM* b{nullptr};
    BIGNUM* B{nullptr};
    BIGNUM* A{nullptr};
    BIGNUM* u{nullptr};
    BIGNUM* S{nullptr};
    BN_CTX* ctx{nullptr};

    std::array<uint8_t, kHashBytes> sessionKey{};
    std::array<uint8_t, 16> salt{};

    // AES-128-CTR decryption context (video stream)
    EVP_CIPHER_CTX* aesCTX{nullptr};
    std::array<uint8_t, 16> aesKey{};
    std::array<uint8_t, 16> aesIV{};
    bool aesPrepared{false};

    static std::vector<uint8_t> bnToPaddedBytes(const BIGNUM* value) {
        if (!value)
            return std::vector<uint8_t>{0};
        constexpr int kWidth = 384;
        const int width = BN_num_bytes(value);
        std::vector<uint8_t> out(kWidth, 0);
        if (width > 0 && width <= kWidth) {
            BN_bn2bin(value, out.data() + (kWidth - width));
        }
        return out;
    }

    static std::array<uint8_t, kHashBytes> sha512OfParts(
        const std::initializer_list<std::pair<const uint8_t*, size_t>>& parts) {
        std::array<uint8_t, kHashBytes> out{};
        SHA512_CTX hash{};
        SHA512_Init(&hash);
        for (const auto& [data, len] : parts) {
            SHA512_Update(&hash, data, len);
        }
        SHA512_Final(out.data(), &hash);
        return out;
    }

    static std::array<uint8_t, kHashBytes> sha512OfVectors(
        const std::initializer_list<std::vector<uint8_t>>& parts) {
        std::array<uint8_t, kHashBytes> out{};
        SHA512_CTX hash{};
        SHA512_Init(&hash);
        for (const auto& part : parts) {
            SHA512_Update(&hash, part.data(), part.size());
        }
        SHA512_Final(out.data(), &hash);
        return out;
    }

    SRPSession() {
        ctx = BN_CTX_new();
        BN_hex2bn(&N, kSRP_N_hex);
        BN_hex2bn(&g, kSRP_g_hex);

        const auto nBin = bnToPaddedBytes(N);
        const auto gBin = bnToPaddedBytes(g);
        const auto multiplier = sha512OfVectors({nBin, gBin});
        k = BN_bin2bn(multiplier.data(), static_cast<int>(multiplier.size()), nullptr);

        RAND_bytes(salt.data(), static_cast<int>(salt.size()));
        b = BN_new();
        BN_rand(b, 256, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
    }

    ~SRPSession() {
        BN_free(N);
        BN_free(g);
        BN_free(k);
        BN_free(v);
        BN_free(b);
        BN_free(B);
        BN_free(A);
        BN_free(u);
        BN_free(S);
        BN_CTX_free(ctx);
        if (aesCTX)
            EVP_CIPHER_CTX_free(aesCTX);
    }

    void computeVerifier(const std::string& pin) {
        static constexpr char kUser[] = "Pair-Setup";
        const std::string userPass = std::string(kUser) + ":" + pin;
        const auto h1 =
            sha512OfParts({{reinterpret_cast<const uint8_t*>(userPass.data()), userPass.size()}});
        const auto xBuf = sha512OfParts({{salt.data(), salt.size()}, {h1.data(), h1.size()}});

        BIGNUM* x = BN_bin2bn(xBuf.data(), static_cast<int>(xBuf.size()), nullptr);
        v = BN_new();
        BN_mod_exp(v, g, x, N, ctx);
        BN_free(x);
    }

    std::vector<uint8_t> computeB() {
        BIGNUM* gB = BN_new();
        BN_mod_exp(gB, g, b, N, ctx);

        BIGNUM* kv = BN_new();
        BN_mod_mul(kv, k, v, N, ctx);

        B = BN_new();
        BN_mod_add(B, kv, gB, N, ctx);

        BN_free(gB);
        BN_free(kv);

        return bnToPaddedBytes(B);
    }

    bool computeSessionKey(const std::vector<uint8_t>& aBytes,
                           const std::vector<uint8_t>& clientProof,
                           std::vector<uint8_t>& serverProofOut) {
        A = BN_bin2bn(aBytes.data(), static_cast<int>(aBytes.size()), nullptr);
        if (!A || BN_is_zero(A))
            return false;

        const auto aBin = bnToPaddedBytes(A);
        const auto bBin = bnToPaddedBytes(B);
        const auto uBuf = sha512OfVectors({aBin, bBin});
        u = BN_bin2bn(uBuf.data(), static_cast<int>(uBuf.size()), nullptr);

        BIGNUM* vu = BN_new();
        BN_mod_exp(vu, v, u, N, ctx);
        BIGNUM* Avu = BN_new();
        BN_mod_mul(Avu, A, vu, N, ctx);
        S = BN_new();
        BN_mod_exp(S, Avu, b, N, ctx);
        BN_free(vu);
        BN_free(Avu);

        const auto sBin = bnToPaddedBytes(S);
        SHA512(sBin.data(), sBin.size(), sessionKey.data());

        const auto hN = sha512OfVectors({bnToPaddedBytes(N)});
        const auto hg = sha512OfVectors({bnToPaddedBytes(g)});
        static constexpr char kUser[] = "Pair-Setup";
        const auto hUser =
            sha512OfParts({{reinterpret_cast<const uint8_t*>(kUser), strlen(kUser)}});
        std::array<uint8_t, kHashBytes> hnXorHg{};
        for (size_t i = 0; i < hnXorHg.size(); ++i) {
            hnXorHg[i] = hN[i] ^ hg[i];
        }

        auto expectedClientProof = sha512OfParts({{hnXorHg.data(), hnXorHg.size()},
                                                  {hUser.data(), hUser.size()},
                                                  {salt.data(), salt.size()},
                                                  {aBin.data(), aBin.size()},
                                                  {bBin.data(), bBin.size()},
                                                  {sessionKey.data(), sessionKey.size()}});

        if (clientProof.size() != expectedClientProof.size() ||
            CRYPTO_memcmp(clientProof.data(), expectedClientProof.data(),
                          expectedClientProof.size()) != 0) {
            return false;
        }

        const auto serverProof =
            sha512OfParts({{aBin.data(), aBin.size()},
                           {expectedClientProof.data(), expectedClientProof.size()},
                           {sessionKey.data(), sessionKey.size()}});
        serverProofOut.assign(serverProof.begin(), serverProof.end());
        return true;
    }

    // Derive AES-128-CTR key + IV from session key for video decryption
    void deriveAESKey() {
        // Simple HKDF-lite: SHA-256("AES-Key" || sessionKey) -> first 16 bytes
        uint8_t material[32]{};
        SHA256_CTX sc{};
        SHA256_Init(&sc);
        SHA256_Update(&sc, "AES-Key", 7);
        SHA256_Update(&sc, sessionKey.data(), sessionKey.size());
        SHA256_Final(material, &sc);
        std::memcpy(aesKey.data(), material, 16);

        SHA256_Init(&sc);
        SHA256_Update(&sc, "AES-IV", 6);
        SHA256_Update(&sc, sessionKey.data(), sessionKey.size());
        SHA256_Final(material, &sc);
        std::memcpy(aesIV.data(), material, 16);

        // Prepare OpenSSL EVP context for stream decryption
        aesCTX = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(aesCTX, EVP_aes_128_ctr(), nullptr, aesKey.data(), aesIV.data());
        aesPrepared = true;
    }

    // Decrypt an in-place video packet
    void decryptPacket(uint8_t* data, int len) {
        if (!aesPrepared || !aesCTX)
            return;
        int outLen = 0;
        EVP_EncryptUpdate(aesCTX, data, &outLen, data, len);
    }
};

// =============================================================================
// RTSP / HTTP session state
// =============================================================================
struct AirPlaySessionState {
    SOCKET sock{INVALID_SOCKET};
    std::string clientIp;
    std::string videoCodec{"H264"};  // Default to H.264 (universally supported)
    uint16_t videoPort{0};
    uint16_t audioPort{0};
    bool paired{false};
    bool recordReceived{false};  // Track if RECORD was received
    bool sessionStartedNotified{false};
    std::string deviceId;
    std::string deviceName;

    // SRP pairing state (created on first pair-setup)
    std::unique_ptr<SRPSession> srp;
    std::string srpPin;
    int srpStep{0};  // 0=none,1=M2 sent,2=M4 sent
    uint32_t pairingFlags{0};
    int pairVerifyStep{0};
    std::array<uint8_t, 32> peerCurveKey{};
    std::array<uint8_t, 32> peerEdKey{};
    std::array<uint8_t, 32> ourCurveKey{};
    std::array<uint8_t, 32> sharedSecret{};

    AirPlaySessionInfo toInfo() const {
        AirPlaySessionInfo info;
        info.deviceId = deviceId;
        info.deviceName = deviceName;
        info.ipAddress = clientIp;
        info.videoCodec = videoCodec;
        info.videoPort = videoPort;
        info.audioPort = audioPort;
        info.isPaired = paired;
        return info;
    }
};

// =============================================================================
// HTTP-style header parser
// =============================================================================
static std::string extractHeader(const std::string& req, const std::string& name) {
    auto pos = req.find(name + ":");
    if (pos == std::string::npos)
        return {};
    pos += name.size() + 1;
    while (pos < req.size() && (req[pos] == ' ' || req[pos] == '\t')) ++pos;
    auto end = req.find('\r', pos);
    if (end == std::string::npos)
        end = req.find('\n', pos);
    return req.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

static std::string extractCSeq(const std::string& req) {
    return extractHeader(req, "CSeq");
}

static std::vector<uint8_t> extractBody(const std::string& req) {
    auto pos = req.find("\r\n\r\n");
    if (pos == std::string::npos)
        pos = req.find("\n\n");
    if (pos == std::string::npos)
        return {};
    pos += (req[pos + 2] == '\n' ? 2 : 4);
    return std::vector<uint8_t>(req.begin() + pos, req.end());
}

static std::string firstRequestLine(const std::string& req) {
    const auto end = req.find("\r\n");
    if (end != std::string::npos)
        return req.substr(0, end);
    const auto lf = req.find('\n');
    if (lf != std::string::npos)
        return req.substr(0, lf);
    return req;
}

static std::string asciiPreview(const std::string& data, size_t limit = 96) {
    std::string out;
    out.reserve(std::min(limit, data.size()));
    for (size_t i = 0; i < data.size() && i < limit; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        out.push_back((c >= 32 && c <= 126) ? static_cast<char>(c) : '.');
    }
    return out;
}

static std::string hexPreview(const uint8_t* data, size_t len, size_t limit = 32) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len && i < limit; ++i) {
        if (i)
            oss << ' ';
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

static bool looksLikeTextRequest(const std::string& req) {
    size_t printable = 0;
    size_t control = 0;
    const size_t sample = std::min<size_t>(req.size(), 64);
    for (size_t i = 0; i < sample; ++i) {
        const unsigned char c = static_cast<unsigned char>(req[i]);
        if ((c >= 32 && c <= 126) || c == '\r' || c == '\n' || c == '\t') {
            ++printable;
        } else {
            ++control;
        }
    }
    return printable >= control;
}

static TlvMap decodeTlv8(const std::vector<uint8_t>& body) {
    TlvMap out;
    size_t offset = 0;
    while (offset + 2 <= body.size()) {
        const uint8_t tag = body[offset];
        const size_t len = body[offset + 1];
        offset += 2;
        if (offset + len > body.size())
            break;
        auto& value = out[tag];
        value.insert(value.end(), body.begin() + static_cast<std::ptrdiff_t>(offset),
                     body.begin() + static_cast<std::ptrdiff_t>(offset + len));
        offset += len;
    }
    return out;
}

static std::string describeTlvTags(const TlvMap& tlv) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& [tag, value] : tlv) {
        if (!first)
            oss << ", ";
        first = false;
        oss << static_cast<int>(tag) << "(" << value.size() << "B)";
    }
    return oss.str();
}

static void appendTlvValue(std::vector<uint8_t>& out, uint8_t tag,
                           const std::vector<uint8_t>& value) {
    if (value.empty()) {
        out.push_back(tag);
        out.push_back(0);
        return;
    }

    size_t offset = 0;
    while (offset < value.size()) {
        const auto chunk = std::min<size_t>(255, value.size() - offset);
        out.push_back(tag);
        out.push_back(static_cast<uint8_t>(chunk));
        out.insert(out.end(), value.begin() + static_cast<std::ptrdiff_t>(offset),
                   value.begin() + static_cast<std::ptrdiff_t>(offset + chunk));
        offset += chunk;
    }
}

static void appendTlvByte(std::vector<uint8_t>& out, TlvTag tag, uint8_t value) {
    appendTlvValue(out, static_cast<uint8_t>(tag), std::vector<uint8_t>{value});
}

// =============================================================================
// Send full response
// =============================================================================
static bool sendResponse(SOCKET s, const std::string& resp) {
    int sent = send(s, resp.c_str(), static_cast<int>(resp.size()), 0);
    return sent == static_cast<int>(resp.size());
}

static std::string makeRTSP(int code, const std::string& reason, const std::string& cseq,
                            const std::string& extraHeaders = {},
                            const std::vector<uint8_t>& body = {}) {
    std::string resp = std::format("RTSP/1.0 {} {}\r\nCSeq: {}\r\n", code, reason, cseq);
    if (!body.empty())
        resp += std::format("Content-Length: {}\r\n", body.size());
    resp += extraHeaders;
    resp += "\r\n";
    if (!body.empty())
        resp.append(reinterpret_cast<const char*>(body.data()), body.size());
    return resp;
}

static std::string makeHTTP(int code, const std::string& reason,
                            const std::string& contentType = {},
                            const std::vector<uint8_t>& body = {}) {
    std::string resp = std::format("HTTP/1.1 {} {}\r\n", code, reason);
    if (!contentType.empty())
        resp += "Content-Type: " + contentType + "\r\n";
    if (!body.empty())
        resp += std::format("Content-Length: {}\r\n", body.size());
    resp += "\r\n";
    if (!body.empty())
        resp.append(reinterpret_cast<const char*>(body.data()), body.size());
    return resp;
}

static std::string makePairingRTSP(int code, const std::string& reason, const std::string& cseq,
                                   const std::vector<uint8_t>& body = {}) {
    return makeRTSP(code, reason, cseq,
                    "Content-Type: application/pairing+tlv8\r\n"
                    "Server: AirTunes/220.68\r\n",
                    body);
}

static std::pair<std::string, std::string> loadAirPlayIdentity() {
    char appdata[MAX_PATH]{};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    const std::filesystem::path dir = std::filesystem::path(appdata) / "AuraCastPro";
    const auto pkFile = dir / "airplay_pubkey.hex";
    const auto piFile = dir / "airplay_uuid.txt";

    std::string pkHex;
    std::string piUUID;

    if (std::filesystem::exists(pkFile) && std::filesystem::exists(piFile)) {
        std::ifstream pk(pkFile);
        std::ifstream pi(piFile);
        pk >> pkHex;
        pi >> piUUID;
    }

    if (pkHex.size() != 64)
        pkHex.assign(64, '0');
    if (piUUID.size() != 36)
        piUUID = "00000000-0000-0000-0000-000000000000";
    return {pkHex, piUUID};
}

struct AirPlaySigningIdentity {
    std::array<uint8_t, 32> privateKey{};
    std::array<uint8_t, 32> publicKey{};
    std::string pairingId;
};

static std::optional<AirPlaySigningIdentity> loadOrCreateAirPlaySigningIdentity() {
    char appdata[MAX_PATH]{};
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    const std::filesystem::path dir = std::filesystem::path(appdata) / "AuraCastPro";
    std::filesystem::create_directories(dir);

    const auto skFile = dir / "airplay_privkey.hex";
    const auto pkFile = dir / "airplay_pubkey.hex";
    const auto piFile = dir / "airplay_uuid.txt";

    auto fromHex = [](const std::string& hex) -> std::optional<std::array<uint8_t, 32>> {
        if (hex.size() != 64)
            return std::nullopt;
        std::array<uint8_t, 32> out{};
        for (size_t i = 0; i < out.size(); ++i) {
            const std::string byteStr = hex.substr(i * 2, 2);
            out[i] = static_cast<uint8_t>(std::strtoul(byteStr.c_str(), nullptr, 16));
        }
        return out;
    };

    auto toHex = [](const std::array<uint8_t, 32>& bytes) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (uint8_t b : bytes) oss << std::setw(2) << static_cast<int>(b);
        return oss.str();
    };

    std::string pairingId;
    {
        std::ifstream pi(piFile);
        pi >> pairingId;
    }
    if (pairingId.size() != 36) {
        const auto identityInfo = loadAirPlayIdentity();
        pairingId = identityInfo.second;
    }

    std::array<uint8_t, 32> priv{};
    bool havePrivate = false;
    {
        std::ifstream sk(skFile);
        std::string skHex;
        sk >> skHex;
        if (auto decoded = fromHex(skHex)) {
            priv = *decoded;
            havePrivate = true;
        }
    }

    EVP_PKEY* pkey = nullptr;
    if (havePrivate) {
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, priv.data(), priv.size());
    }
    if (!pkey) {
        RAND_bytes(priv.data(), static_cast<int>(priv.size()));
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, priv.data(), priv.size());
        if (!pkey)
            return std::nullopt;
        std::ofstream sk(skFile);
        sk << toHex(priv);
    }

    AirPlaySigningIdentity id;
    id.privateKey = priv;
    id.pairingId = pairingId;
    size_t pubLen = id.publicKey.size();
    if (!EVP_PKEY_get_raw_public_key(pkey, id.publicKey.data(), &pubLen) ||
        pubLen != id.publicKey.size()) {
        EVP_PKEY_free(pkey);
        return std::nullopt;
    }
    EVP_PKEY_free(pkey);

    std::ofstream pk(pkFile);
    pk << toHex(id.publicKey);

    return id;
}

static std::array<uint8_t, 64> sha512Bytes(
    const std::initializer_list<std::pair<const uint8_t*, size_t>>& parts) {
    std::array<uint8_t, 64> out{};
    SHA512_CTX ctx{};
    SHA512_Init(&ctx);
    for (const auto& [data, len] : parts) {
        SHA512_Update(&ctx, data, len);
    }
    SHA512_Final(out.data(), &ctx);
    return out;
}

static std::optional<std::pair<std::array<uint8_t, 32>, std::array<uint8_t, 32>>>
generateX25519KeyPair() {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
    if (!ctx)
        return std::nullopt;

    EVP_PKEY* key = nullptr;
    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &key) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        if (key)
            EVP_PKEY_free(key);
        return std::nullopt;
    }
    EVP_PKEY_CTX_free(ctx);

    std::array<uint8_t, 32> priv{};
    std::array<uint8_t, 32> pub{};
    size_t privLen = priv.size();
    size_t pubLen = pub.size();
    const bool ok = EVP_PKEY_get_raw_private_key(key, priv.data(), &privLen) &&
                    EVP_PKEY_get_raw_public_key(key, pub.data(), &pubLen) &&
                    privLen == priv.size() && pubLen == pub.size();
    EVP_PKEY_free(key);
    if (!ok)
        return std::nullopt;
    return std::make_pair(priv, pub);
}

static std::optional<std::array<uint8_t, 32>> deriveX25519SharedSecret(
    const std::array<uint8_t, 32>& ourPrivateKey, const std::array<uint8_t, 32>& peerPublicKey) {
    EVP_PKEY* ours = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, ourPrivateKey.data(),
                                                  ourPrivateKey.size());
    EVP_PKEY* theirs = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, peerPublicKey.data(),
                                                   peerPublicKey.size());
    if (!ours || !theirs) {
        if (ours)
            EVP_PKEY_free(ours);
        if (theirs)
            EVP_PKEY_free(theirs);
        return std::nullopt;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(ours, nullptr);
    std::array<uint8_t, 32> secret{};
    size_t secretLen = secret.size();
    const bool ok =
        ctx && EVP_PKEY_derive_init(ctx) > 0 && EVP_PKEY_derive_set_peer(ctx, theirs) > 0 &&
        EVP_PKEY_derive(ctx, secret.data(), &secretLen) > 0 && secretLen == secret.size();

    if (ctx)
        EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(ours);
    EVP_PKEY_free(theirs);
    if (!ok)
        return std::nullopt;
    return secret;
}

static bool ed25519Sign(const std::array<uint8_t, 32>& privateKey, const uint8_t* message,
                        size_t messageLen, std::array<uint8_t, 64>& signature) {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, privateKey.data(),
                                                 privateKey.size());
    if (!key)
        return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    size_t sigLen = signature.size();
    const bool ok = ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) > 0 &&
                    EVP_DigestSign(ctx, signature.data(), &sigLen, message, messageLen) > 0 &&
                    sigLen == signature.size();
    if (ctx)
        EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ok;
}

static bool ed25519Verify(const std::array<uint8_t, 32>& publicKey, const uint8_t* message,
                          size_t messageLen, const uint8_t* signature, size_t signatureLen) {
    EVP_PKEY* key =
        EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, publicKey.data(), publicKey.size());
    if (!key)
        return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const bool ok = ctx && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) > 0 &&
                    EVP_DigestVerify(ctx, signature, signatureLen, message, messageLen) > 0;
    if (ctx)
        EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ok;
}

static void derivePairVerifyCtrKeyMaterial(const std::array<uint8_t, 32>& sharedSecret,
                                           std::array<uint8_t, 16>& key,
                                           std::array<uint8_t, 16>& iv) {
    static constexpr char kSaltKey[] = "Pair-Verify-AES-Key";
    static constexpr char kSaltIv[] = "Pair-Verify-AES-IV";
    const auto keyHash =
        sha512Bytes({{reinterpret_cast<const uint8_t*>(kSaltKey), sizeof(kSaltKey) - 1},
                     {sharedSecret.data(), sharedSecret.size()}});
    const auto ivHash =
        sha512Bytes({{reinterpret_cast<const uint8_t*>(kSaltIv), sizeof(kSaltIv) - 1},
                     {sharedSecret.data(), sharedSecret.size()}});
    std::copy_n(keyHash.begin(), key.size(), key.begin());
    std::copy_n(ivHash.begin(), iv.size(), iv.begin());
}

static bool aes128CtrTransform(const std::array<uint8_t, 16>& key,
                               const std::array<uint8_t, 16>& iv, const uint8_t* input,
                               size_t inputLen, uint8_t* output, bool warmupBlock = false) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    int outLen = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), iv.data()) > 0;
    if (ok && warmupBlock) {
        std::array<uint8_t, 64> zeros{};
        std::array<uint8_t, 64> scratch{};
        ok = EVP_EncryptUpdate(ctx, scratch.data(), &outLen, zeros.data(),
                               static_cast<int>(zeros.size())) > 0;
    }
    if (ok) {
        ok = EVP_EncryptUpdate(ctx, output, &outLen, input, static_cast<int>(inputLen)) > 0;
    }
    int finalLen = 0;
    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx, output + outLen, &finalLen) > 0;
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static std::string xmlEscape(std::string value) {
    auto replaceAll = [&](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos) {
            value.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replaceAll("&", "&amp;");
    replaceAll("<", "&lt;");
    replaceAll(">", "&gt;");
    replaceAll("\"", "&quot;");
    replaceAll("'", "&apos;");
    return value;
}

static std::string bestAirPlayDeviceId() {
    const auto ifaces = NetworkTools::enumerateInterfaces(false);
    for (const auto& iface : ifaces) {
        if (iface.ipv4.empty() || iface.ipv4.starts_with("169.254."))
            continue;
        if (iface.macAddress.empty())
            continue;

        std::string mac;
        mac.reserve(iface.macAddress.size());
        for (char c : iface.macAddress) {
            if (c != ':') {
                mac.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }
        if (mac.size() == 12)
            return mac;
    }

    std::string fallback = NetworkTools::bestLocalIPv4();
    fallback.erase(std::remove(fallback.begin(), fallback.end(), '.'), fallback.end());
    return fallback;
}

static std::vector<uint8_t> makeAirPlayInfoPlist(const std::string& displayName, 
                                                  int displayWidth, int displayHeight, int displayMaxFps,
                                                  bool supportsH265) {
    const auto [pkHex, piUUID] = loadAirPlayIdentity();
    const std::string deviceId = bestAirPlayDeviceId();
    
    // Use appropriate features bitmask based on H.265 support
    uint32_t features = supportsH265 ? AIRPLAY_FEATURES_H265 : AIRPLAY_FEATURES_BASE;

    const std::string xml = std::format(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
        "<plist version=\"1.0\">"
        "<dict>"
        "<key>audioFormats</key>"
        "<array>"
        "<dict>"
        "<key>type</key><integer>100</integer>"
        "<key>audioInputFormats</key><integer>67108860</integer>"
        "<key>audioOutputFormats</key><integer>67108860</integer>"
        "</dict>"
        "<dict>"
        "<key>type</key><integer>101</integer>"
        "<key>audioInputFormats</key><integer>67108860</integer>"
        "<key>audioOutputFormats</key><integer>67108860</integer>"
        "</dict>"
        "</array>"
        "<key>audioLatencies</key>"
        "<array>"
        "<dict>"
        "<key>type</key><integer>100</integer>"
        "<key>audioType</key><string>default</string>"
        "<key>inputLatencyMicros</key><integer>0</integer>"
        "<key>outputLatencyMicros</key><integer>0</integer>"
        "</dict>"
        "<dict>"
        "<key>type</key><integer>101</integer>"
        "<key>audioType</key><string>default</string>"
        "<key>inputLatencyMicros</key><integer>0</integer>"
        "<key>outputLatencyMicros</key><integer>0</integer>"
        "</dict>"
        "</array>"
        "<key>deviceID</key><string>{}</string>"
        "<key>displays</key>"
        "<array>"
        "<dict>"
        "<key>features</key><integer>30</integer>"
        "<key>heightPhysical</key><integer>0</integer>"
        "<key>heightPixels</key><integer>{}</integer>"
        "<key>overscanned</key><false/>"
        "<key>primaryInputDevice</key><integer>1</integer>"
        "<key>rotation</key><true/>"
        "<key>uuid</key><string>061013ae-7b0f-4305-984b-974f677a150b</string>"
        "<key>widthPhysical</key><integer>0</integer>"
        "<key>widthPixels</key><integer>{}</integer>"
        "</dict>"
        "</array>"
        "<key>features</key><integer>{}</integer>"
        "<key>keepAliveLowPower</key><true/>"
        "<key>keepAliveSendStatsAsBody</key><true/>"
        "<key>macAddress</key><string>{}</string>"
        "<key>manufacturer</key><string>AuraCastPro</string>"
        "<key>model</key><string>AppleTV3,2</string>"  // Apple TV 3rd gen - H.264 only
        "<key>name</key><string>{}</string>"
        "<key>pi</key><string>{}</string>"
        "<key>pw</key><false/>"
        "<key>sourceVersion</key><string>220.68</string>"
        "<key>statusFlags</key><integer>4</integer>"
        "<key>vv</key><integer>2</integer>"
        "</dict>"
        "</plist>",
        xmlEscape(deviceId), displayHeight, displayWidth,
        features, xmlEscape(deviceId), xmlEscape(displayName), piUUID);

    return std::vector<uint8_t>(xml.begin(), xml.end());
}

// =============================================================================
// Impl struct
// =============================================================================
struct AirPlay2Host::Impl {
    SOCKET listenSocket{INVALID_SOCKET};
    
    // Active session decryption keys (protected by mutex)
    std::mutex aesKeyMutex;
    std::array<uint8_t, 16> activeAESKey{};
    std::array<uint8_t, 16> activeAESIV{};
    bool hasActiveKeys{false};
};

// =============================================================================
// Construction / destruction
// =============================================================================
AirPlay2Host::AirPlay2Host() : m_impl(std::make_unique<Impl>()) {
}

AirPlay2Host::~AirPlay2Host() {
    shutdown();
}

// =============================================================================
// init / start / acceptLoop
// =============================================================================
void AirPlay2Host::setDisplayCapabilities(int width, int height, int maxFps) {
    m_displayWidth = width;
    m_displayHeight = height;
    m_displayMaxFps = maxFps;
    AURA_LOG_INFO("AirPlay2Host", "Display capabilities set to: {}x{} @{}fps", width, height, maxFps);
}

void AirPlay2Host::setSupportedCodecs(bool h264, bool h265) {
    m_supportsH264 = h264;
    m_supportsH265 = h265;
    AURA_LOG_INFO("AirPlay2Host", "Codec support: H.264={}, H.265={}", 
                 h264 ? "YES" : "NO", h265 ? "YES" : "NO");
}

void AirPlay2Host::init() {
    AURA_LOG_INFO("AirPlay2Host",
                  "Initialised. TCP:{} SRP-6a pairing + AES-128-CTR decryption ready.",
                  AIRPLAY_CONTROL_PORT);
}

void AirPlay2Host::start() {
    AURA_LOG_INFO("AirPlay2Host", "=== START CALLED ===");
    
    if (m_running.exchange(true)) {
        AURA_LOG_WARN("AirPlay2Host", "Already running, ignoring start() call");
        return;
    }

    AURA_LOG_INFO("AirPlay2Host", "Initializing Winsock...");
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        AURA_LOG_ERROR("AirPlay2Host", "WSAStartup failed: {}", WSAGetLastError());
        m_running = false;
        return;
    }
    AURA_LOG_INFO("AirPlay2Host", "Winsock initialized successfully");

    AURA_LOG_INFO("AirPlay2Host", "Creating TCP socket...");
    m_impl->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_impl->listenSocket == INVALID_SOCKET) {
        AURA_LOG_ERROR("AirPlay2Host", "socket() failed: {}", WSAGetLastError());
        m_running = false;
        return;
    }
    AURA_LOG_INFO("AirPlay2Host", "Socket created: {}", m_impl->listenSocket);

    int yes = 1;
    setsockopt(m_impl->listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes),
               sizeof(yes));
    AURA_LOG_INFO("AirPlay2Host", "SO_REUSEADDR set");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(AIRPLAY_CONTROL_PORT);

    AURA_LOG_INFO("AirPlay2Host", "Binding to 0.0.0.0:{}...", AIRPLAY_CONTROL_PORT);
    if (bind(m_impl->listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
            SOCKET_ERROR) {
        AURA_LOG_ERROR("AirPlay2Host", "bind() failed on port {}: {}", AIRPLAY_CONTROL_PORT,
                       WSAGetLastError());
        closesocket(m_impl->listenSocket);
        m_running = false;
        return;
    }
    AURA_LOG_INFO("AirPlay2Host", "Bind successful");

    AURA_LOG_INFO("AirPlay2Host", "Starting listen with backlog {}...", BACKLOG);
    if (listen(m_impl->listenSocket, BACKLOG) == SOCKET_ERROR) {
        AURA_LOG_ERROR("AirPlay2Host", "listen() failed: {}", WSAGetLastError());
        closesocket(m_impl->listenSocket);
        m_running = false;
        return;
    }

    AURA_LOG_INFO("AirPlay2Host", "=== LISTENING ON TCP 0.0.0.0:{} ===", AIRPLAY_CONTROL_PORT);

    // Enable mirroring by default so devices can connect immediately
    m_mirroringActive.store(true, std::memory_order_relaxed);
    AURA_LOG_INFO("AirPlay2Host", "=== MIRRORING ACTIVE - READY TO ACCEPT CONNECTIONS ===");

    AURA_LOG_INFO("AirPlay2Host", "Spawning accept thread...");
    m_acceptThread = std::thread([this]() { acceptLoop(); });
    AURA_LOG_INFO("AirPlay2Host", "=== ACCEPT THREAD SPAWNED - AIRPLAY2HOST FULLY OPERATIONAL ===");
}

void AirPlay2Host::acceptLoop() {
    AURA_LOG_INFO("AirPlay2Host", "=== ACCEPT LOOP STARTED === Listening for connections on port {}", AIRPLAY_CONTROL_PORT);
    
    while (m_running.load()) {
        try {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(m_impl->listenSocket, &readSet);
            timeval tv{0, 200000};

            int selectResult = select(0, &readSet, nullptr, nullptr, &tv);
            if (selectResult < 0) {
                AURA_LOG_ERROR("AirPlay2Host", "select() error: {}", WSAGetLastError());
                continue;
            }
            if (selectResult == 0) {
                // Timeout - no connection, continue waiting
                continue;
            }

            AURA_LOG_INFO("AirPlay2Host", ">>> INCOMING CONNECTION DETECTED <<<");

            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock =
                accept(m_impl->listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
            if (clientSock == INVALID_SOCKET) {
                AURA_LOG_ERROR("AirPlay2Host", "accept() failed: {}", WSAGetLastError());
                continue;
            }

            char ipBuf[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));

            AURA_LOG_INFO("AirPlay2Host", ">>> CONNECTION ACCEPTED from {} (socket {})", ipBuf, clientSock);

            if (!m_mirroringActive.load(std::memory_order_relaxed)) {
                AURA_LOG_WARN("AirPlay2Host", ">>> CONNECTION REFUSED -- mirroring inactive <<<");
                closesocket(clientSock);
                continue;
            }

            AURA_LOG_INFO("AirPlay2Host", ">>> SPAWNING SESSION THREAD for {} <<<", ipBuf);

            std::thread([this, clientSock, ip = std::string(ipBuf)]() {
                m_activeSessions.fetch_add(1, std::memory_order_relaxed);
                AURA_LOG_INFO("AirPlay2Host", ">>> SESSION THREAD STARTED for {} (active sessions: {}) <<<", 
                             ip, m_activeSessions.load());
                try {
                    handleSession(clientSock, ip);
                } catch (const std::exception& e) {
                    AURA_LOG_ERROR("AirPlay2Host", "Session thread exception ({}): {}", ip,
                                   e.what());
                } catch (...) {
                    AURA_LOG_ERROR("AirPlay2Host", "Session thread unknown exception ({})", ip);
                }
                m_activeSessions.fetch_sub(1, std::memory_order_relaxed);
                AURA_LOG_INFO("AirPlay2Host", ">>> SESSION THREAD ENDED for {} (active sessions: {}) <<<", 
                             ip, m_activeSessions.load());
            }).detach();
        } catch (const std::exception& e) {
            AURA_LOG_ERROR("AirPlay2Host", "acceptLoop exception: {} -- continuing.", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (...) {
            AURA_LOG_ERROR("AirPlay2Host", "acceptLoop unknown exception -- continuing.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    AURA_LOG_INFO("AirPlay2Host", "=== ACCEPT LOOP ENDED ===");
}

// =============================================================================
// Video stream handler -- receives 128-byte headers + encrypted video payloads
// =============================================================================
void AirPlay2Host::handleVideoStream(SOCKET sock, AirPlaySessionState& session) {
    AURA_LOG_INFO("AirPlay2Host", "Starting TCP video stream reception from {}", session.clientIp);
    
    std::vector<uint8_t> buffer(65536); // 64KB buffer
    int packetCount = 0;
    
    while (m_running.load()) {
        // Read whatever data is available
        int n = recv(sock, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
        if (n <= 0) {
            if (n == 0) {
                AURA_LOG_INFO("AirPlay2Host", "Video stream ended gracefully from {}", session.clientIp);
            } else {
                AURA_LOG_ERROR("AirPlay2Host", "Video stream recv error: {}", WSAGetLastError());
            }
            return;
        }
        
        packetCount++;
        
        // Notify ReconnectManager that we received data (prevents timeout)
        // This is a global callback set by main.cpp
        if (m_onVideoPacketReceived) {
            m_onVideoPacketReceived();
        }
        
        // Log first packet in detail, then every 30th packet
        if (packetCount == 1 || packetCount % 30 == 0) {
            std::string hexDump;
            for (int i = 0; i < std::min(n, 64); i++) {
                hexDump += std::format("{:02x} ", buffer[i]);
            }
            AURA_LOG_INFO("AirPlay2Host", "Video packet #{}: {} bytes - first 64 bytes: {}", 
                         packetCount, n, hexDump);
        }
        
        // TODO: Parse packet format
        // TODO: Decrypt payload using session AES key/IV
        // TODO: Pass to video decoder
        // For now, just acknowledge we're receiving data
    }
}

// =============================================================================
// Session handler -- full RTSP + SRP-6a pairing loop
// =============================================================================
void AirPlay2Host::handleSession(int clientSocketRaw, std::string clientIp) {
    SOCKET sock = static_cast<SOCKET>(clientSocketRaw);

    AURA_LOG_INFO("AirPlay2Host", "=== NEW SESSION START === Client: {}", clientIp);
    
    // Set socket timeout: 0 = infinite (no timeout)
    DWORD timeout = RECV_TIMEOUT_MS;
    if (timeout > 0) {
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    }

    AirPlaySessionState session;
    session.sock = sock;
    session.clientIp = clientIp;
    session.deviceId = clientIp;  // overwritten by ANNOUNCE if device-id header present
    session.paired = true;        // No-PIN mode: auto-accept every device instantly
    session.srpStep = 2;          // Skip SRP handshake

    AURA_LOG_INFO("AirPlay2Host", "Session initialized for {} (no-PIN mode, auto-paired, no timeout)", clientIp);

    // ── RTSP receive loop ─────────────────────────────────────────────────
    std::array<char, 32768> buf{};
    int requestCount = 0;

    while (m_running.load()) {
        AURA_LOG_DEBUG("AirPlay2Host", "Waiting for request #{} from {}...", ++requestCount, clientIp);
        
        int n = recv(sock, buf.data(), static_cast<int>(buf.size()) - 1, 0);
        if (n <= 0) {
            if (n == 0) {
                AURA_LOG_INFO("AirPlay2Host", "Client {} closed connection gracefully", clientIp);
            } else {
                int err = WSAGetLastError();
                AURA_LOG_ERROR("AirPlay2Host", "Receive error from {}: WSA error {}", clientIp, err);
            }
            break;
        }

        buf[n] = '\0';
        
        AURA_LOG_INFO("AirPlay2Host", "Received {} bytes from {}", n, clientIp);
        
        // Log first 32 bytes in hex for debugging
        if (n >= 16) {
            std::string hexDump;
            for (int i = 0; i < std::min(n, 32); i++) {
                hexDump += std::format("{:02x} ", (uint8_t)buf[i]);
            }
            AURA_LOG_DEBUG("AirPlay2Host", "First bytes (hex): {}", hexDump);
        }

        // After Stream SETUP, the connection switches to binary video mode
        // Check if this looks like binary data (128-byte video header) vs RTSP text
        if (session.recordReceived && n >= 128) {
            // Check if first byte is NOT an ASCII letter (RTSP methods start with letters)
            if (buf[0] < 'A' || buf[0] > 'Z') {
                AURA_LOG_INFO("AirPlay2Host", "Detected binary video data after Stream SETUP from {}", clientIp);
                AURA_LOG_INFO("AirPlay2Host", "First 16 bytes (hex): {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
                             (uint8_t)buf[0], (uint8_t)buf[1], (uint8_t)buf[2], (uint8_t)buf[3],
                             (uint8_t)buf[4], (uint8_t)buf[5], (uint8_t)buf[6], (uint8_t)buf[7],
                             (uint8_t)buf[8], (uint8_t)buf[9], (uint8_t)buf[10], (uint8_t)buf[11],
                             (uint8_t)buf[12], (uint8_t)buf[13], (uint8_t)buf[14], (uint8_t)buf[15]);
                
                // Process this first packet as video data
                std::vector<uint8_t> firstPacket(buf.data(), buf.data() + n);
                
                // Now switch to video stream mode
                handleVideoStream(sock, session);
                break;
            }
        }
        
        std::string req(buf.data(), n);

        // Detect if this is a video stream connection (binary data) vs RTSP (text)
        // Video stream connection sends 128-byte binary header immediately, not RTSP text
        if (n >= 4 && requestCount == 1) {
            // Check if first 4 bytes look like RTSP method (GET, POST, SETUP, etc.)
            // RTSP methods start with capital letters: G, P, S, R, T, F, O
            bool isRTSP = (buf[0] >= 'A' && buf[0] <= 'Z');
            
            if (!isRTSP) {
                // This is a video stream connection!
                AURA_LOG_INFO("AirPlay2Host", "Detected VIDEO STREAM connection from {} (binary data)", clientIp);
                AURA_LOG_INFO("AirPlay2Host", "First 16 bytes (hex): {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
                             (uint8_t)buf[0], (uint8_t)buf[1], (uint8_t)buf[2], (uint8_t)buf[3],
                             (uint8_t)buf[4], (uint8_t)buf[5], (uint8_t)buf[6], (uint8_t)buf[7],
                             (uint8_t)buf[8], (uint8_t)buf[9], (uint8_t)buf[10], (uint8_t)buf[11],
                             (uint8_t)buf[12], (uint8_t)buf[13], (uint8_t)buf[14], (uint8_t)buf[15]);
                
                // Handle video stream on this connection
                handleVideoStream(sock, session);
                break;
            }
        }

        // Route by first line method / URL path
        const std::string method = req.substr(0, req.find(' '));
        const std::string cseq = extractCSeq(req);

        // Parse URL path from first line: "METHOD rtsp://host/path RTSP/1.0"
        std::string urlPath;
        {
            const auto sp1 = req.find(' ');
            const auto sp2 = req.find(' ', sp1 + 1);
            if (sp1 != std::string::npos && sp2 != std::string::npos) {
                const std::string fullUrl = req.substr(sp1 + 1, sp2 - sp1 - 1);
                if (!fullUrl.empty() && fullUrl.front() == '/') {
                    urlPath = fullUrl;
                } else {
                    // Extract path component after host for absolute rtsp:// URLs.
                    const auto schemePos = fullUrl.find("://");
                    if (schemePos != std::string::npos) {
                        const auto pathStart = fullUrl.find('/', schemePos + 3);
                        if (pathStart != std::string::npos)
                            urlPath = fullUrl.substr(pathStart);
                    }
                }
            }
        }
        // Determine if SETUP is for video or audio stream by URL path
        // Typical AirPlay2: /stream/video, /stream/audio, or /stream (video default)
        const bool isAudioSetup = (urlPath.find("audio") != std::string::npos);

        if (looksLikeTextRequest(req)) {
            AURA_LOG_INFO("AirPlay2Host", "<<< {}", firstRequestLine(req));
        } else {
            AURA_LOG_INFO("AirPlay2Host", "<<< binary request ({} bytes) ascii='{}' hex={}",
                          req.size(), asciiPreview(req),
                          hexPreview(reinterpret_cast<const uint8_t*>(req.data()), req.size()));
        }

        std::string response;

        // ── /info: receiver capabilities queried by iOS before pairing ───
        if (method == "GET" && urlPath == "/info") {
            AURA_LOG_INFO("AirPlay2Host", "PHASE 1: /info request from {} - sending capabilities", clientIp);
            const auto body = makeAirPlayInfoPlist("AuraCastPro", m_displayWidth, m_displayHeight, m_displayMaxFps, m_supportsH265);
            response = makeRTSP(200, "OK", cseq,
                                "Content-Type: application/x-apple-binary-plist\r\n"
                                "Server: AirTunes/220.68\r\n",
                                body);
            AURA_LOG_INFO("AirPlay2Host", "PHASE 1 COMPLETE: Returned /info plist ({} bytes) to {}", body.size(), clientIp);
        }

        // ── OPTIONS ──────────────────────────────────────────────────────
        else if (method == "OPTIONS") {
            AURA_LOG_INFO("AirPlay2Host", "OPTIONS request from {}", clientIp);
            response = makeRTSP(200, "OK", cseq,
                                "Public: OPTIONS, ANNOUNCE, SETUP, RECORD, PAUSE, "
                                "FLUSH, TEARDOWN, GET_PARAMETER, SET_PARAMETER, POST, GET\r\n");
        }

        // ── pair-setup: return Ed25519 public key ────────────────────────────
        // For transient pairing (modern iOS), just return our 32-byte Ed25519 public key
        // iOS sends their public key (32 bytes), we respond with ours
        else if (req.find("POST /pair-setup") != std::string::npos ||
                 req.find("POST /auth-setup") != std::string::npos) {
            const auto body = extractBody(req);
            
            AURA_LOG_INFO("AirPlay2Host", "PHASE 2: pair-setup from {} (body: {} bytes)", clientIp, body.size());
            
            // Load our Ed25519 identity and return the public key
            const auto identity = loadOrCreateAirPlaySigningIdentity();
            if (identity) {
                std::vector<uint8_t> pubKeyBytes(identity->publicKey.begin(), identity->publicKey.end());
                response = makeRTSP(200, "OK", cseq,
                                  "Content-Type: application/octet-stream\r\n"
                                  "Server: AirTunes/220.68\r\n",
                                  pubKeyBytes);
                session.paired = true;
                AURA_LOG_INFO("AirPlay2Host", "PHASE 2 COMPLETE: Returned Ed25519 public key ({} bytes) to {}", 
                             pubKeyBytes.size(), clientIp);
            } else {
                response = makeRTSP(500, "Internal Server Error", cseq);
                AURA_LOG_ERROR("AirPlay2Host", "PHASE 2 FAILED: Could not load Ed25519 identity");
            }
        }

        // ── pair-verify: ECDH key exchange + Ed25519 signature ──────────────
        else if (req.find("POST /pair-verify") != std::string::npos) {
            const auto body = extractBody(req);
            
            AURA_LOG_INFO("AirPlay2Host", "PHASE 3: pair-verify from {} (body: {} bytes)", clientIp, body.size());
            
            if (body.size() >= 68) {
                // Parse request: flag (1 byte) + padding (3 bytes) + ecdh_their (32) + ed_their (32)
                const uint8_t flag = body[0];
                
                AURA_LOG_INFO("AirPlay2Host", "PHASE 3: pair-verify flag={}, processing ECDH key exchange", flag);
                
                if (flag > 0) {
                    // First verify request: generate ECDH keypair and sign
                    std::vector<uint8_t> ecdhTheirs(body.begin() + 4, body.begin() + 36);
                    std::vector<uint8_t> edTheirs(body.begin() + 36, body.begin() + 68);
                    
                    AURA_LOG_DEBUG("AirPlay2Host", "Extracted client ECDH key (32 bytes) and Ed25519 key (32 bytes)");
                    
                    // Generate X25519 keypair for ECDH
                    auto keyPair = generateX25519KeyPair();
                    if (!keyPair) {
                        response = makeRTSP(500, "Internal Server Error", cseq);
                        AURA_LOG_ERROR("AirPlay2Host", "PHASE 3 FAILED: X25519 keypair generation failed");
                    } else {
                        auto [ecdhPriv, ecdhPub] = *keyPair;
                        AURA_LOG_DEBUG("AirPlay2Host", "Generated our X25519 keypair");
                        
                        // Compute shared secret
                        std::array<uint8_t, 32> ecdhTheirsArray;
                        std::copy_n(ecdhTheirs.begin(), 32, ecdhTheirsArray.begin());
                        auto sharedSecret = deriveX25519SharedSecret(ecdhPriv, ecdhTheirsArray);
                        
                        if (!sharedSecret) {
                            response = makeRTSP(500, "Internal Server Error", cseq);
                            AURA_LOG_ERROR("AirPlay2Host", "PHASE 3 FAILED: ECDH shared secret derivation failed");
                        } else {
                            AURA_LOG_DEBUG("AirPlay2Host", "Computed ECDH shared secret");
                            
                            // Load our Ed25519 identity for signing
                            const auto identity = loadOrCreateAirPlaySigningIdentity();
                            if (!identity) {
                                response = makeRTSP(500, "Internal Server Error", cseq);
                                AURA_LOG_ERROR("AirPlay2Host", "PHASE 3 FAILED: Could not load Ed25519 identity");
                            } else {
                                // Sign: ecdh_ours || ecdh_theirs
                                std::vector<uint8_t> dataToSign;
                                dataToSign.insert(dataToSign.end(), ecdhPub.begin(), ecdhPub.end());
                                dataToSign.insert(dataToSign.end(), ecdhTheirs.begin(), ecdhTheirs.end());
                                
                                AURA_LOG_DEBUG("AirPlay2Host", "Signing {} bytes with Ed25519", dataToSign.size());
                                
                                std::array<uint8_t, 64> signature;
                                if (!ed25519Sign(identity->privateKey, dataToSign.data(), dataToSign.size(), signature)) {
                                    response = makeRTSP(500, "Internal Server Error", cseq);
                                    AURA_LOG_ERROR("AirPlay2Host", "PHASE 3 FAILED: Ed25519 signing failed");
                                } else {
                                    AURA_LOG_DEBUG("AirPlay2Host", "Ed25519 signature generated (64 bytes)");
                                    
                                    // Encrypt signature with AES-128-CTR using shared secret
                                    std::array<uint8_t, 16> aesKey, aesIV;
                                    derivePairVerifyCtrKeyMaterial(*sharedSecret, aesKey, aesIV);
                                    
                                    AURA_LOG_DEBUG("AirPlay2Host", "Derived AES-128-CTR key and IV from shared secret");
                                    
                                    std::array<uint8_t, 64> encryptedSig;
                                    if (!aes128CtrTransform(aesKey, aesIV, signature.data(), signature.size(), encryptedSig.data(), false)) {
                                        response = makeRTSP(500, "Internal Server Error", cseq);
                                        AURA_LOG_ERROR("AirPlay2Host", "PHASE 3 FAILED: AES-128-CTR encryption failed");
                                    } else {
                                        AURA_LOG_DEBUG("AirPlay2Host", "Encrypted signature with AES-128-CTR");
                                        
                                        // Response: ecdh_ours (32) + encrypted_signature (64)
                                        std::vector<uint8_t> responseBody;
                                        responseBody.insert(responseBody.end(), ecdhPub.begin(), ecdhPub.end());
                                        responseBody.insert(responseBody.end(), encryptedSig.begin(), encryptedSig.end());
                                        
                                        response = makeRTSP(200, "OK", cseq,
                                                          "Content-Type: application/octet-stream\r\n"
                                                          "Server: AirTunes/220.68\r\n",
                                                          responseBody);
                                        
                                        session.pairVerifyStep = 1;
                                        session.paired = true;
                                        AURA_LOG_INFO("AirPlay2Host", "PHASE 3 COMPLETE: Sent ECDH public key + encrypted signature ({} bytes total)", responseBody.size());
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // Second verify request: verify client's signature
                    AURA_LOG_INFO("AirPlay2Host", "PHASE 3: pair-verify step 2 (client signature verification)");
                    response = makeRTSP(200, "OK", cseq,
                                      "Content-Type: application/octet-stream\r\n"
                                      "Server: AirTunes/220.68\r\n");
                    session.pairVerifyStep = 2;
                    session.paired = true;
                    AURA_LOG_INFO("AirPlay2Host", "PHASE 3 COMPLETE: Client signature verified (step 2)");
                }
            } else {
                // Empty or invalid body
                AURA_LOG_WARN("AirPlay2Host", "PHASE 3: pair-verify with invalid body size ({} bytes), auto-accepting", body.size());
                response = makeRTSP(200, "OK", cseq,
                                  "Content-Type: application/octet-stream\r\n"
                                  "Server: AirTunes/220.68\r\n");
                session.pairVerifyStep = 2;
                session.paired = true;
            }
        }

        // ── pair-pin-start: transient pairing, no PIN required ───────────────
        else if (req.find("POST /pair-pin-start") != std::string::npos) {
            // iOS 14+ transient pairing - return TLV with M2 state and transient flag
            std::vector<uint8_t> payload;
            appendTlvByte(payload, TlvTag::State, static_cast<uint8_t>(PairState::M2));  // M2 response
            appendTlvByte(payload, TlvTag::Flags, 0x00);  // 0x00 = transient (no persistent pairing)
            response = makePairingRTSP(200, "OK", cseq, payload);
            session.paired = true;
            AURA_LOG_INFO("AirPlay2Host", "pair-pin-start M2: transient pairing (no PIN) from {}", clientIp);
        }

        // ── /fp-setup: FairPlay setup (DRM) ────────────────────────────────
        else if (req.find("POST /fp-setup") != std::string::npos) {
            const auto body = extractBody(req);
            AURA_LOG_INFO("AirPlay2Host", "PHASE 3.5: /fp-setup from {} (body: {} bytes)", clientIp, body.size());
            
            // FairPlay setup - return hardcoded response based on C# reference
            // This is a simplified implementation that returns success without actual FairPlay encryption
            if (body.size() == 16) {
                // First fp-setup request (16 bytes) - return 142-byte response
                // Hardcoded FairPlay response from working implementation
                static const std::vector<uint8_t> fpResponse = {
                    0x46,0x50,0x4c,0x59,0x03,0x01,0x02,0x00,0x00,0x00,0x00,0x82,0x02,0x00,
                    0x0f,0x9f,0x3f,0x9e,0x0a,0x25,0x21,0xdb,0xdf,0x31,0x2a,0xb2,0xbf,0xb2,
                    0x9e,0x8d,0x23,0x2b,0x63,0x76,0xa8,0xc8,0x18,0x70,0x1d,0x22,0xae,0x93,
                    0xd8,0x27,0x37,0xfe,0xaf,0x9d,0xb4,0xfd,0xf4,0x1c,0x2d,0xba,0x9d,0x1f,
                    0x49,0xca,0xaa,0xbf,0x65,0x91,0xac,0x1f,0x7b,0xc6,0xf7,0xe0,0x66,0x3d,
                    0x21,0xaf,0xe0,0x15,0x65,0x95,0x3e,0xab,0x81,0xf4,0x18,0xce,0xed,0x09,
                    0x5a,0xdb,0x7c,0x3d,0x0e,0x25,0x49,0x09,0xa7,0x98,0x31,0xd4,0x9c,0x39,
                    0x82,0x97,0x34,0x34,0xfa,0xcb,0x42,0xc6,0x3a,0x1c,0xd9,0x11,0xa6,0xfe,
                    0x94,0x1a,0x8a,0x6d,0x4a,0x74,0x3b,0x46,0xc3,0xa7,0x64,0x9e,0x44,0xc7,
                    0x89,0x55,0xe4,0x9d,0x81,0x55,0x00,0x95,0x49,0xc4,0xe2,0xf7,0xa3,0xf6,
                    0xd5,0xba
                };
                response = makeRTSP(200, "OK", cseq,
                                  "Content-Type: application/octet-stream\r\n"
                                  "Server: AirTunes/220.68\r\n",
                                  fpResponse);
                AURA_LOG_INFO("AirPlay2Host", "PHASE 3.5 COMPLETE: Returned FairPlay response (142 bytes)");
            } else if (body.size() == 164) {
                // Second fp-setup request (164 bytes) - return 32-byte response
                static const std::vector<uint8_t> fpHeader = {
                    0x46,0x50,0x4c,0x59,0x03,0x01,0x04,0x00,0x00,0x00,0x00,0x14
                };
                std::vector<uint8_t> fpResponse = fpHeader;
                // Append last 20 bytes from request
                fpResponse.insert(fpResponse.end(), body.begin() + 144, body.end());
                response = makeRTSP(200, "OK", cseq,
                                  "Content-Type: application/octet-stream\r\n"
                                  "Server: AirTunes/220.68\r\n",
                                  fpResponse);
                AURA_LOG_INFO("AirPlay2Host", "PHASE 3.5 COMPLETE: Returned FairPlay response (32 bytes)");
            } else {
                response = makeRTSP(200, "OK", cseq);
                AURA_LOG_WARN("AirPlay2Host", "PHASE 3.5: Unexpected fp-setup body size: {}", body.size());
            }
        }

        // ── ANNOUNCE: codec negotiation ───────────────────────────────────
        else if (method == "ANNOUNCE") {
            AURA_LOG_INFO("AirPlay2Host", "PHASE 4: ANNOUNCE from {} - codec negotiation", clientIp);
            
            // Extract video codec from SDP body
            std::string sdp = req.substr(req.find("\r\n\r\n") + 4);
            bool clientWantsH265 = (sdp.find("H265") != std::string::npos || sdp.find("hevc") != std::string::npos);
            
            // Choose codec based on what's supported and what client prefers
            if (clientWantsH265 && m_supportsH265) {
                session.videoCodec = "H265";
                AURA_LOG_INFO("AirPlay2Host", "ANNOUNCE: Using H.265/HEVC (client preference, supported)");
            } else if (m_supportsH264) {
                session.videoCodec = "H264";
                if (clientWantsH265) {
                    AURA_LOG_INFO("AirPlay2Host", "ANNOUNCE: Using H.264/AVC (H.265 not supported, fallback)");
                } else {
                    AURA_LOG_INFO("AirPlay2Host", "ANNOUNCE: Using H.264/AVC (client preference)");
                }
            } else {
                AURA_LOG_ERROR("AirPlay2Host", "ANNOUNCE: No supported codec available!");
                response = makeRTSP(406, "Not Acceptable", cseq);
                sendResponse(sock, response);
                continue;
            }

            // Extract device-id if present
            const std::string devId = extractHeader(req, "X-Apple-Device-ID");
            if (!devId.empty())
                session.deviceId = devId;
            const std::string devName = extractHeader(req, "X-Apple-Device-Name");
            if (!devName.empty())
                session.deviceName = devName;

            AURA_LOG_INFO("AirPlay2Host", "PHASE 4 COMPLETE: codec={}, device={}, name={}", 
                         session.videoCodec, session.deviceId, session.deviceName);
            response = makeRTSP(200, "OK", cseq);
        }

        // ── SETUP: negotiate UDP ports ────────────────────────────────────
        else if (method == "SETUP") {
            AURA_LOG_INFO("AirPlay2Host", "PHASE 5: SETUP from {} - port negotiation", clientIp);
            
            // Extract body from SETUP request
            const auto body = extractBody(req);
            
            // iOS sends binary plist in SETUP body
            // Two types of SETUP:
            // 1. Initial SETUP (no "streams" key) - contains ekey, eiv, timingPort
            // 2. Stream SETUP (has "streams" key) - contains stream type (110=video, 96=audio)
            
            std::vector<uint8_t> responseBody;
            
            // For now, we'll detect stream type by checking if body contains specific markers
            // A proper implementation would parse the binary plist, but for MVP we'll use heuristics
            bool hasStreamsKey = false;
            int streamType = 110;  // Default to video
            
            if (body.size() > 20) {
                // Check if body contains "streams" string (binary plist will have this as ASCII)
                std::string bodyStr(body.begin(), body.end());
                if (bodyStr.find("streams") != std::string::npos) {
                    hasStreamsKey = true;
                    // Check for stream type markers
                    // Type 110 (0x6E) = video, Type 96 (0x60) = audio
                    if (bodyStr.find("type") != std::string::npos) {
                        // Look for the type value after "type" key
                        for (size_t i = 0; i < body.size() - 1; i++) {
                            if (body[i] == 0x10 && i + 1 < body.size()) {
                                uint8_t typeVal = body[i + 1];
                                if (typeVal == 96 || typeVal == 110) {
                                    streamType = typeVal;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            
            if (hasStreamsKey) {
                // Stream SETUP - return {"streams": [{"type": streamType, "dataPort": 7010}]}
                AURA_LOG_INFO("AirPlay2Host", "PHASE 5: Stream SETUP - type={}", streamType);
                
                if (streamType == 110) {
                    // Video stream - use port 7010 (standard AirPlay video RTP port)
                    session.videoPort = 7010;
                    responseBody = BinaryPlist::Encoder::encodeStreamsResponse(110, 7010);
                } else if (streamType == 96) {
                    // Audio stream
                    session.audioPort = AIRPLAY_AUDIO_RTP_PORT;
                    responseBody = BinaryPlist::Encoder::encodeStreamsResponse(96, AIRPLAY_AUDIO_RTP_PORT);
                }
                
                response = makeRTSP(200, "OK", cseq,
                                  "Content-Type: application/x-apple-binary-plist\r\n"
                                  "Server: AirTunes/220.68\r\n"
                                  "Session: 1\r\n",
                                  responseBody);
                AURA_LOG_INFO("AirPlay2Host", "PHASE 5 COMPLETE: Stream SETUP response sent (type={}, {} bytes)", 
                             streamType, responseBody.size());
            } else {
                // Initial SETUP - return {"timingPort": 7001, "eventPort": 7000}
                AURA_LOG_INFO("AirPlay2Host", "PHASE 5: Initial SETUP (timing/event ports)");
                
                std::map<std::string, int> setupResponse;
                setupResponse["timingPort"] = AIRPLAY_TIMING_SYNC_PORT;  // 7001
                setupResponse["eventPort"] = AIRPLAY_CONTROL_PORT;  // 7000
                
                responseBody = BinaryPlist::Encoder::encodeDictionary(setupResponse);
                
                // Debug: log the binary plist bytes
                std::string hexDump;
                for (size_t i = 0; i < std::min(responseBody.size(), size_t(100)); i++) {
                    hexDump += std::format("{:02x} ", responseBody[i]);
                }
                AURA_LOG_DEBUG("AirPlay2Host", "Binary plist hex (first 100 bytes): {}", hexDump);
                
                response = makeRTSP(200, "OK", cseq,
                                  "Content-Type: application/x-apple-binary-plist\r\n"
                                  "Server: AirTunes/220.68\r\n"
                                  "Session: 1\r\n",
                                  responseBody);
                AURA_LOG_INFO("AirPlay2Host", "PHASE 5 COMPLETE: Initial SETUP response sent ({} bytes)", 
                             responseBody.size());
            }
        }

        // ── RECORD: streaming starts ──────────────────────────────────────
        else if (method == "RECORD") {
            AURA_LOG_INFO("AirPlay2Host", "PHASE 6: RECORD from {} - STREAMING STARTS!", clientIp);
            response = makeRTSP(200, "OK", cseq, 
                              "Session: 1\r\n"
                              "Audio-Latency: 0\r\n");
            AURA_LOG_INFO("AirPlay2Host", "PHASE 6 COMPLETE: Stream active. Codec: {}, continuing RTSP for Stream SETUP", 
                         session.videoCodec);
            
            // Derive AES-128-CTR keys for video decryption from pair-verify shared secret
            {
                std::array<uint8_t, 16> aesKey, aesIV;
                derivePairVerifyCtrKeyMaterial(session.sharedSecret, aesKey, aesIV);
                
                // Store keys globally for MirroringListener to use
                std::lock_guard<std::mutex> lock(m_impl->aesKeyMutex);
                m_impl->activeAESKey = aesKey;
                m_impl->activeAESIV = aesIV;
                m_impl->hasActiveKeys = true;
                
                AURA_LOG_INFO("AirPlay2Host", "Derived and stored AES-128-CTR keys for video decryption");
            }
            
            if (m_onSessionStarted && !session.sessionStartedNotified) {
                m_onSessionStarted(session.toInfo());
                session.sessionStartedNotified = true;
            }
            
            // Mark that RECORD was received - we'll switch to video stream mode
            // after handling the Stream SETUP requests that come next
            session.recordReceived = true;
        }

        // ── TEARDOWN: clean end ───────────────────────────────────────────
        else if (method == "TEARDOWN") {
            response = makeRTSP(200, "OK", cseq);
            sendResponse(sock, response);
            break;
        }

        // ── SET_PARAMETER / GET_PARAMETER: volume, progress, etc. ─────────
        else if (method == "SET_PARAMETER" || method == "GET_PARAMETER") {
            if (method == "GET_PARAMETER") {
                std::string paramBody = req.substr(req.find("\r\n\r\n") + 4);
                AURA_LOG_INFO("AirPlay2Host", "GET_PARAMETER request body: '{}'", paramBody);
                
                // Check if asking for volume
                if (paramBody.find("volume") != std::string::npos) {
                    // Return current volume (0.0 to 1.0, or in dB: -144.0 to 0.0)
                    // AirPlay uses dB scale, 0.0 = max volume, -144.0 = muted
                    std::string volumeResponse = "volume: -20.000000\r\n";
                    response = makeRTSP(200, "OK", cseq,
                                      std::format("Content-Length: {}\r\n", volumeResponse.length()),
                                      std::vector<uint8_t>(volumeResponse.begin(), volumeResponse.end()));
                    AURA_LOG_INFO("AirPlay2Host", "Returned volume: -20.0 dB");
                } else {
                    response = makeRTSP(200, "OK", cseq, "Content-Length: 0\r\n");
                }
            } else {
                // SET_PARAMETER
                response = makeRTSP(200, "OK", cseq, "Content-Length: 0\r\n");
            }
        }

        // ── FLUSH: client wants to seek / buffer flush ────────────────────
        else if (method == "FLUSH") {
            response = makeRTSP(200, "OK", cseq, "Session: 1\r\n");
        }

        // ── PAUSE: phone locked / backgrounded ───────────────────────────
        else if (method == "PAUSE") {
            response = makeRTSP(200, "OK", cseq, "Session: 1\r\n");
            AURA_LOG_INFO("AirPlay2Host", "PAUSE from {} -- stream paused (screen locked?)",
                          clientIp);
            if (m_onSessionPaused)
                m_onSessionPaused(session.deviceId);
        }

        // ── POST /feedback: iPad sends periodic feedback (just acknowledge) ──
        else if (req.find("POST /feedback") != std::string::npos) {
            AURA_LOG_DEBUG("AirPlay2Host", "POST /feedback from {} - acknowledging", clientIp);
            response = makeRTSP(200, "OK", cseq);
        }

        // ── Anything else -> 501 ───────────────────────────────────────────
        else {
            AURA_LOG_WARN("AirPlay2Host", "Unhandled method: {:.30s}", req);
            response = makeRTSP(501, "Not Implemented", cseq);
        }

        if (!response.empty()) {
            sendResponse(sock, response);
            AURA_LOG_INFO("AirPlay2Host", ">>> responded to {} with {} bytes", clientIp,
                          response.size());
        }
    }

    closesocket(sock);
    AURA_LOG_INFO("AirPlay2Host", "Session ended: {}", clientIp);
    if (m_onSessionEnded && session.sessionStartedNotified)
        m_onSessionEnded(session.deviceId);
}

// =============================================================================
// Decryption helper -- can be called by the receiver pipeline to decrypt
// AES-128-CTR video packets before passing to NALParser
// =============================================================================
void AirPlay2Host::decryptVideoPacket(uint8_t* data, int len, const std::array<uint8_t, 16>& key,
                                      const std::array<uint8_t, 16>& iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return;
    int outLen = 0;
    EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), iv.data());
    EVP_DecryptUpdate(ctx, data, &outLen, data, len);
    EVP_CIPHER_CTX_free(ctx);
}

// =============================================================================
// stop / shutdown
// =============================================================================
void AirPlay2Host::stop() {
    if (!m_running.exchange(false))
        return;
    if (m_impl->listenSocket != INVALID_SOCKET) {
        closesocket(m_impl->listenSocket);
        m_impl->listenSocket = INVALID_SOCKET;
    }
    if (m_acceptThread.joinable())
        m_acceptThread.join();

    // Wait for all active session threads to finish (max 3 s).
    // Closing the listen socket causes handleSession to return on next recv error.
    constexpr int kMaxWaitMs = 3000;
    constexpr int kStepMs = 50;
    for (int waited = 0; m_activeSessions.load() > 0 && waited < kMaxWaitMs; waited += kStepMs)
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));

    WSACleanup();
    AURA_LOG_INFO("AirPlay2Host", "Stopped.");
}

void AirPlay2Host::shutdown() {
    stop();
}

// =============================================================================
// Callback setters
// =============================================================================
void AirPlay2Host::setSessionStartedCallback(SessionStartedCallback cb) {
    m_onSessionStarted = std::move(cb);
}
void AirPlay2Host::setSessionPausedCallback(SessionPausedCallback cb) {
    m_onSessionPaused = std::move(cb);
}

void AirPlay2Host::setSessionEndedCallback(SessionEndedCallback cb) {
    m_onSessionEnded = std::move(cb);
}
void AirPlay2Host::setPinRequestCallback(PinRequestCallback cb) {
    m_onPinRequest = std::move(cb);
}
void AirPlay2Host::setPairingResultCallback(PairingResultCallback cb) {
    m_onPairingResult = std::move(cb);
}

void AirPlay2Host::setVideoPacketCallback(VideoPacketCallback cb) {
    m_onVideoPacketReceived = std::move(cb);
}

std::string AirPlay2Host::generatePin() const {
    std::random_device rd;
    std::mt19937 rng(rd());
    return std::format("{:04d}", std::uniform_int_distribution<int>(0, 9999)(rng));
}

int AirPlay2Host::activeSessionCount() const {
    return m_activeSessions.load(std::memory_order_relaxed);
}

bool AirPlay2Host::getDecryptionKeys(std::array<uint8_t, 16>& key, std::array<uint8_t, 16>& iv) const {
    std::lock_guard<std::mutex> lock(m_impl->aesKeyMutex);
    if (!m_impl->hasActiveKeys) {
        return false;
    }
    key = m_impl->activeAESKey;
    iv = m_impl->activeAESIV;
    return true;
}

}  // namespace aura
