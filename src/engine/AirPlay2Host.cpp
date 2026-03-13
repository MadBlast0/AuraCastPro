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

#include "../pch.h"  // PCH
#include "AirPlay2Host.h"
#include "../utils/Logger.h"
#include "../utils/NetworkTools.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

// OpenSSL for SRP-6a + AES-128-CTR
#include <openssl/srp.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bn.h>
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

#include <array>
#include <chrono>
#include <format>
#include <random>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <map>
#include <thread>
#include <fstream>
#include <filesystem>
#include <optional>

namespace aura {

constexpr uint16_t AIRPLAY_CONTROL_PORT  = 7236;
constexpr uint16_t AIRPLAY_AUDIO_RTP_PORT = 7011;   // Audio RTP (used in SETUP response)
constexpr uint16_t AIRPLAY_TIMING_SYNC_PORT = 7237;  // RTCP timing (control+1, informational)
constexpr uint16_t AIRPLAY_TIMING_PORT   = 7238;   // RTCP sync (control+2)
constexpr int      BACKLOG              = 4;
constexpr int      RECV_TIMEOUT_MS      = 8000;
constexpr uint32_t AIRPLAY_FEATURES     = 0x5A7FFFF7;
constexpr uint32_t AIRPLAY_FLAGS        = 0x0;
constexpr uint8_t  kPairingMethodSetup  = 0x00;
constexpr uint8_t  kPairingMethodSetupAuth = 0x01;
constexpr uint8_t  kPairingTransientFlag = 0x10;

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
        if (!value) return std::vector<uint8_t>{0};
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
        BN_free(N); BN_free(g); BN_free(k); BN_free(v);
        BN_free(b); BN_free(B); BN_free(A); BN_free(u); BN_free(S);
        BN_CTX_free(ctx);
        if (aesCTX) EVP_CIPHER_CTX_free(aesCTX);
    }

    void computeVerifier(const std::string& pin) {
        static constexpr char kUser[] = "Pair-Setup";
        const std::string userPass = std::string(kUser) + ":" + pin;
        const auto h1 = sha512OfParts({
            {reinterpret_cast<const uint8_t*>(userPass.data()), userPass.size()}
        });
        const auto xBuf = sha512OfParts({
            {salt.data(), salt.size()},
            {h1.data(), h1.size()}
        });

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
        if (!A || BN_is_zero(A)) return false;

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
        const auto hUser = sha512OfParts({
            {reinterpret_cast<const uint8_t*>(kUser), strlen(kUser)}
        });
        std::array<uint8_t, kHashBytes> hnXorHg{};
        for (size_t i = 0; i < hnXorHg.size(); ++i) {
            hnXorHg[i] = hN[i] ^ hg[i];
        }

        auto expectedClientProof = sha512OfParts({
            {hnXorHg.data(), hnXorHg.size()},
            {hUser.data(), hUser.size()},
            {salt.data(), salt.size()},
            {aBin.data(), aBin.size()},
            {bBin.data(), bBin.size()},
            {sessionKey.data(), sessionKey.size()}
        });

        if (clientProof.size() != expectedClientProof.size() ||
            CRYPTO_memcmp(clientProof.data(), expectedClientProof.data(), expectedClientProof.size()) != 0) {
            return false;
        }

        const auto serverProof = sha512OfParts({
            {aBin.data(), aBin.size()},
            {expectedClientProof.data(), expectedClientProof.size()},
            {sessionKey.data(), sessionKey.size()}
        });
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
        EVP_EncryptInit_ex(aesCTX, EVP_aes_128_ctr(), nullptr,
                           aesKey.data(), aesIV.data());
        aesPrepared = true;
    }

    // Decrypt an in-place video packet
    void decryptPacket(uint8_t* data, int len) {
        if (!aesPrepared || !aesCTX) return;
        int outLen = 0;
        EVP_EncryptUpdate(aesCTX, data, &outLen, data, len);
    }
};

// =============================================================================
// RTSP / HTTP session state
// =============================================================================
struct AirPlaySessionState {
    SOCKET       sock{INVALID_SOCKET};
    std::string  clientIp;
    std::string  videoCodec{"H265"};
    uint16_t     videoPort{0};
    uint16_t     audioPort{0};
    bool         paired{false};
    std::string  deviceId;
    std::string  deviceName;

    // SRP pairing state (created on first pair-setup)
    std::unique_ptr<SRPSession> srp;
    std::string                 srpPin;
    int                         srpStep{0}; // 0=none,1=M2 sent,2=M4 sent
    uint32_t                    pairingFlags{0};
    int                         pairVerifyStep{0};
    std::array<uint8_t, 32>     peerCurveKey{};
    std::array<uint8_t, 32>     peerEdKey{};
    std::array<uint8_t, 32>     ourCurveKey{};
    std::array<uint8_t, 32>     sharedSecret{};

    AirPlaySessionInfo toInfo() const {
        AirPlaySessionInfo info;
        info.deviceId   = deviceId;
        info.deviceName = deviceName;
        info.ipAddress  = clientIp;
        info.videoCodec = videoCodec;
        info.videoPort  = videoPort;
        info.audioPort  = audioPort;
        info.isPaired   = paired;
        return info;
    }
};

// =============================================================================
// HTTP-style header parser
// =============================================================================
static std::string extractHeader(const std::string& req, const std::string& name) {
    auto pos = req.find(name + ":");
    if (pos == std::string::npos) return {};
    pos += name.size() + 1;
    while (pos < req.size() && (req[pos] == ' ' || req[pos] == '\t')) ++pos;
    auto end = req.find('\r', pos);
    if (end == std::string::npos) end = req.find('\n', pos);
    return req.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

static std::string extractCSeq(const std::string& req) {
    return extractHeader(req, "CSeq");
}

static std::vector<uint8_t> extractBody(const std::string& req) {
    auto pos = req.find("\r\n\r\n");
    if (pos == std::string::npos) pos = req.find("\n\n");
    if (pos == std::string::npos) return {};
    pos += (req[pos+2] == '\n' ? 2 : 4);
    return std::vector<uint8_t>(req.begin() + pos, req.end());
}

static std::string firstRequestLine(const std::string& req) {
    const auto end = req.find("\r\n");
    if (end != std::string::npos) return req.substr(0, end);
    const auto lf = req.find('\n');
    if (lf != std::string::npos) return req.substr(0, lf);
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
        if (i) oss << ' ';
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
        if (offset + len > body.size()) break;
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
        if (!first) oss << ", ";
        first = false;
        oss << static_cast<int>(tag) << "(" << value.size() << "B)";
    }
    return oss.str();
}

static void appendTlvValue(std::vector<uint8_t>& out, uint8_t tag, const std::vector<uint8_t>& value) {
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
        out.insert(out.end(),
                   value.begin() + static_cast<std::ptrdiff_t>(offset),
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

static std::string makeRTSP(int code, const std::string& reason,
                              const std::string& cseq,
                              const std::string& extraHeaders = {},
                              const std::vector<uint8_t>& body = {}) {
    std::string resp = std::format("RTSP/1.0 {} {}\r\nCSeq: {}\r\n",
                                   code, reason, cseq);
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

static std::string makePairingRTSP(int code, const std::string& reason,
                                   const std::string& cseq,
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

    if (pkHex.size() != 64) pkHex.assign(64, '0');
    if (piUUID.size() != 36) piUUID = "00000000-0000-0000-0000-000000000000";
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
        if (hex.size() != 64) return std::nullopt;
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
        if (!pkey) return std::nullopt;
        std::ofstream sk(skFile);
        sk << toHex(priv);
    }

    AirPlaySigningIdentity id;
    id.privateKey = priv;
    id.pairingId = pairingId;
    size_t pubLen = id.publicKey.size();
    if (!EVP_PKEY_get_raw_public_key(pkey, id.publicKey.data(), &pubLen) || pubLen != id.publicKey.size()) {
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
    if (!ctx) return std::nullopt;

    EVP_PKEY* key = nullptr;
    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &key) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        if (key) EVP_PKEY_free(key);
        return std::nullopt;
    }
    EVP_PKEY_CTX_free(ctx);

    std::array<uint8_t, 32> priv{};
    std::array<uint8_t, 32> pub{};
    size_t privLen = priv.size();
    size_t pubLen = pub.size();
    const bool ok =
        EVP_PKEY_get_raw_private_key(key, priv.data(), &privLen) &&
        EVP_PKEY_get_raw_public_key(key, pub.data(), &pubLen) &&
        privLen == priv.size() &&
        pubLen == pub.size();
    EVP_PKEY_free(key);
    if (!ok) return std::nullopt;
    return std::make_pair(priv, pub);
}

static std::optional<std::array<uint8_t, 32>> deriveX25519SharedSecret(
    const std::array<uint8_t, 32>& ourPrivateKey,
    const std::array<uint8_t, 32>& peerPublicKey) {
    EVP_PKEY* ours = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr,
        ourPrivateKey.data(), ourPrivateKey.size());
    EVP_PKEY* theirs = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
        peerPublicKey.data(), peerPublicKey.size());
    if (!ours || !theirs) {
        if (ours) EVP_PKEY_free(ours);
        if (theirs) EVP_PKEY_free(theirs);
        return std::nullopt;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(ours, nullptr);
    std::array<uint8_t, 32> secret{};
    size_t secretLen = secret.size();
    const bool ok =
        ctx &&
        EVP_PKEY_derive_init(ctx) > 0 &&
        EVP_PKEY_derive_set_peer(ctx, theirs) > 0 &&
        EVP_PKEY_derive(ctx, secret.data(), &secretLen) > 0 &&
        secretLen == secret.size();

    if (ctx) EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(ours);
    EVP_PKEY_free(theirs);
    if (!ok) return std::nullopt;
    return secret;
}

static bool ed25519Sign(const std::array<uint8_t, 32>& privateKey,
    const uint8_t* message, size_t messageLen,
    std::array<uint8_t, 64>& signature) {
    EVP_PKEY* key = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
        privateKey.data(), privateKey.size());
    if (!key) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    size_t sigLen = signature.size();
    const bool ok = ctx &&
        EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) > 0 &&
        EVP_DigestSign(ctx, signature.data(), &sigLen, message, messageLen) > 0 &&
        sigLen == signature.size();
    if (ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ok;
}

static bool ed25519Verify(const std::array<uint8_t, 32>& publicKey,
    const uint8_t* message, size_t messageLen,
    const uint8_t* signature, size_t signatureLen) {
    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
        publicKey.data(), publicKey.size());
    if (!key) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const bool ok = ctx &&
        EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) > 0 &&
        EVP_DigestVerify(ctx, signature, signatureLen, message, messageLen) > 0;
    if (ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ok;
}

static void derivePairVerifyCtrKeyMaterial(const std::array<uint8_t, 32>& sharedSecret,
    std::array<uint8_t, 16>& key,
    std::array<uint8_t, 16>& iv) {
    static constexpr char kSaltKey[] = "Pair-Verify-AES-Key";
    static constexpr char kSaltIv[] = "Pair-Verify-AES-IV";
    const auto keyHash = sha512Bytes({
        {reinterpret_cast<const uint8_t*>(kSaltKey), sizeof(kSaltKey) - 1},
        {sharedSecret.data(), sharedSecret.size()}
    });
    const auto ivHash = sha512Bytes({
        {reinterpret_cast<const uint8_t*>(kSaltIv), sizeof(kSaltIv) - 1},
        {sharedSecret.data(), sharedSecret.size()}
    });
    std::copy_n(keyHash.begin(), key.size(), key.begin());
    std::copy_n(ivHash.begin(), iv.size(), iv.begin());
}

static bool aes128CtrTransform(const std::array<uint8_t, 16>& key,
    const std::array<uint8_t, 16>& iv,
    const uint8_t* input, size_t inputLen,
    uint8_t* output, bool warmupBlock = false) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    int outLen = 0;
    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), iv.data()) > 0;
    if (ok && warmupBlock) {
        std::array<uint8_t, 64> zeros{};
        std::array<uint8_t, 64> scratch{};
        ok = EVP_EncryptUpdate(ctx, scratch.data(), &outLen, zeros.data(), static_cast<int>(zeros.size())) > 0;
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
        if (iface.ipv4.empty() || iface.ipv4.starts_with("169.254.")) continue;
        if (iface.macAddress.empty()) continue;

        std::string mac;
        mac.reserve(iface.macAddress.size());
        for (char c : iface.macAddress) {
            if (c != ':') {
                mac.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }
        if (mac.size() == 12) return mac;
    }

    std::string fallback = NetworkTools::bestLocalIPv4();
    fallback.erase(std::remove(fallback.begin(), fallback.end(), '.'), fallback.end());
    return fallback;
}

static std::vector<uint8_t> makeAirPlayInfoPlist(const std::string& displayName) {
    const auto [pkHex, piUUID] = loadAirPlayIdentity();
    const std::string deviceId = bestAirPlayDeviceId();

    const std::string xml = std::format(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
        "<plist version=\"1.0\">"
        "<dict>"
        "<key>audioLatencies</key><array/>"
        "<key>deviceID</key><string>{}</string>"
        "<key>features</key><integer>{}</integer>"
        "<key>flags</key><integer>{}</integer>"
        "<key>manufacturer</key><string>AuraCastPro</string>"
        "<key>model</key><string>AppleTV3,2</string>"
        "<key>name</key><string>{}</string>"
        "<key>pk</key><data>{}</data>"
        "<key>pi</key><string>{}</string>"
        "<key>protovers</key><string>1.1</string>"
        "<key>sourceVersion</key><string>220.68</string>"
        "<key>statusFlags</key><integer>0</integer>"
        "</dict>"
        "</plist>",
        xmlEscape(deviceId),
        AIRPLAY_FEATURES,
        AIRPLAY_FLAGS,
        xmlEscape(displayName),
        pkHex,
        piUUID);

    return std::vector<uint8_t>(xml.begin(), xml.end());
}

// =============================================================================
// Impl struct
// =============================================================================
struct AirPlay2Host::Impl {
    SOCKET listenSocket{INVALID_SOCKET};
};

// =============================================================================
// Construction / destruction
// =============================================================================
AirPlay2Host::AirPlay2Host()
    : m_impl(std::make_unique<Impl>()) {}

AirPlay2Host::~AirPlay2Host() { shutdown(); }

// =============================================================================
// init / start / acceptLoop
// =============================================================================
void AirPlay2Host::init() {
    AURA_LOG_INFO("AirPlay2Host",
        "Initialised. TCP:{} SRP-6a pairing + AES-128-CTR decryption ready.",
        AIRPLAY_CONTROL_PORT);
}

void AirPlay2Host::start() {
    if (m_running.exchange(true)) return;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        AURA_LOG_ERROR("AirPlay2Host", "WSAStartup failed: {}", WSAGetLastError());
        m_running = false;
        return;
    }

    m_impl->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_impl->listenSocket == INVALID_SOCKET) {
        AURA_LOG_ERROR("AirPlay2Host", "socket() failed: {}", WSAGetLastError());
        m_running = false;
        return;
    }

    int yes = 1;
    setsockopt(m_impl->listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(AIRPLAY_CONTROL_PORT);

    if (bind(m_impl->listenSocket,
             reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(m_impl->listenSocket, BACKLOG) == SOCKET_ERROR) {
        AURA_LOG_ERROR("AirPlay2Host",
            "bind/listen failed on port {}: {}", AIRPLAY_CONTROL_PORT, WSAGetLastError());
        closesocket(m_impl->listenSocket);
        m_running = false;
        return;
    }

    AURA_LOG_INFO("AirPlay2Host",
        "Listening on TCP 0.0.0.0:{}", AIRPLAY_CONTROL_PORT);

    m_acceptThread = std::thread([this]() { acceptLoop(); });
}

void AirPlay2Host::acceptLoop() {
    while (m_running.load()) {
        try {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(m_impl->listenSocket, &readSet);
            timeval tv{0, 200000};

            if (select(0, &readSet, nullptr, nullptr, &tv) <= 0) continue;

            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock = accept(m_impl->listenSocket,
                reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
            if (clientSock == INVALID_SOCKET) continue;

            if (!m_mirroringActive.load(std::memory_order_relaxed)) {
                AURA_LOG_DEBUG("AirPlay2Host", "Connection refused -- mirroring inactive.");
                closesocket(clientSock);
                continue;
            }

            char ipBuf[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));

            AURA_LOG_INFO("AirPlay2Host", "Connection from {}", ipBuf);

            std::thread([this, clientSock, ip = std::string(ipBuf)]() {
                m_activeSessions.fetch_add(1, std::memory_order_relaxed);
                try {
                    handleSession(clientSock, ip);
                } catch (const std::exception& e) {
                    AURA_LOG_ERROR("AirPlay2Host",
                        "Session thread exception ({}): {}", ip, e.what());
                } catch (...) {
                    AURA_LOG_ERROR("AirPlay2Host",
                        "Session thread unknown exception ({})", ip);
                }
                m_activeSessions.fetch_sub(1, std::memory_order_relaxed);
            }).detach();
        } catch (const std::exception& e) {
            AURA_LOG_ERROR("AirPlay2Host",
                "acceptLoop exception: {} -- continuing.", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (...) {
            AURA_LOG_ERROR("AirPlay2Host",
                "acceptLoop unknown exception -- continuing.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// =============================================================================
// Session handler -- full RTSP + SRP-6a pairing loop
// =============================================================================
void AirPlay2Host::handleSession(int clientSocketRaw, std::string clientIp) {
    SOCKET sock = static_cast<SOCKET>(clientSocketRaw);

    DWORD timeout = RECV_TIMEOUT_MS;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<char*>(&timeout), sizeof(timeout));

    AirPlaySessionState session;
    session.sock     = sock;
    session.clientIp = clientIp;
    session.deviceId = clientIp; // overwritten by ANNOUNCE if device-id header present

    // Generate a 4-digit PIN for this session
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, 9999);
    session.srpPin = std::format("{:04d}", dist(rng));

    AURA_LOG_INFO("AirPlay2Host",
        "Session PIN: {} (used for SRP pairing)", session.srpPin);

    if (m_onPinRequest) m_onPinRequest(session.srpPin);

    // Pre-compute SRP verifier so we're ready for pair-setup
    session.srp = std::make_unique<SRPSession>();
    session.srp->computeVerifier(session.srpPin);

    // ── RTSP receive loop ─────────────────────────────────────────────────
    std::array<char, 32768> buf{};

    while (m_running.load()) {
        int n = recv(sock, buf.data(), static_cast<int>(buf.size()) - 1, 0);
        if (n <= 0) break;

        buf[n] = '\0';
        std::string req(buf.data(), n);

        // Route by first line method / URL path
        const std::string method = req.substr(0, req.find(' '));
        const std::string cseq   = extractCSeq(req);

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
            AURA_LOG_INFO("AirPlay2Host",
                "<<< binary request ({} bytes) ascii='{}' hex={}",
                req.size(), asciiPreview(req), hexPreview(reinterpret_cast<const uint8_t*>(req.data()), req.size()));
        }

        std::string response;

        // ── OPTIONS ──────────────────────────────────────────────────────
        if (method == "OPTIONS") {
            response = makeRTSP(200, "OK", cseq,
                "Public: OPTIONS, ANNOUNCE, SETUP, RECORD, PAUSE, "
                "FLUSH, TEARDOWN, GET_PARAMETER, SET_PARAMETER, POST, GET\r\n");
        }

        // ── /info: receiver capabilities queried by iOS before pairing ───
        else if (method == "GET" && urlPath == "/info") {
            const auto body = makeAirPlayInfoPlist("AuraCastPro");
            response = makeRTSP(200, "OK", cseq,
                "Content-Type: application/x-apple-binary-plist\r\n"
                "Server: AirTunes/220.68\r\n",
                body);
            AURA_LOG_INFO("AirPlay2Host", "Returned /info plist to {}", clientIp);
        }

        // ── TLV8 pair-setup (transient SRP) ───────────────────────────────
        else if (req.find("POST /pair-setup") != std::string::npos ||
                 req.find("POST /auth-setup") != std::string::npos) {
            const auto tlv = decodeTlv8(extractBody(req));
            const auto stateIt = tlv.find(static_cast<uint8_t>(TlvTag::State));
            const auto methodIt = tlv.find(static_cast<uint8_t>(TlvTag::Method));
            const auto flagsIt = tlv.find(static_cast<uint8_t>(TlvTag::Flags));
            const uint8_t state = (stateIt != tlv.end() && !stateIt->second.empty()) ? stateIt->second.front() : 0;
            const uint8_t methodValue = (methodIt != tlv.end() && !methodIt->second.empty()) ? methodIt->second.front() : 0xFF;

            AURA_LOG_INFO("AirPlay2Host",
                "pair-setup request from {}: state=M{} method={} flags=0x{:X} tags=[{}]",
                clientIp, static_cast<int>(state), static_cast<int>(methodValue), session.pairingFlags,
                describeTlvTags(tlv));

            if (flagsIt != tlv.end() && !flagsIt->second.empty()) {
                session.pairingFlags = 0;
                for (uint8_t byte : flagsIt->second) {
                    session.pairingFlags = (session.pairingFlags << 8) | byte;
                }
            }

            if ((state == static_cast<uint8_t>(PairState::M1)) &&
                (methodValue == kPairingMethodSetup || methodValue == kPairingMethodSetupAuth)) {
                auto bBytes = session.srp->computeB();
                std::vector<uint8_t> payload;
                appendTlvByte(payload, TlvTag::State, static_cast<uint8_t>(PairState::M2));
                appendTlvValue(payload, static_cast<uint8_t>(TlvTag::Salt),
                    std::vector<uint8_t>(session.srp->salt.begin(), session.srp->salt.end()));
                appendTlvValue(payload, static_cast<uint8_t>(TlvTag::PublicKey), bBytes);
                appendTlvByte(payload, TlvTag::Flags, kPairingTransientFlag);

                session.pairingFlags = kPairingTransientFlag;
                session.srpStep = 1;
                response = makePairingRTSP(200, "OK", cseq, payload);
                AURA_LOG_INFO("AirPlay2Host",
                    "pair-setup M1->M2: sent SRP salt/public key to {}", clientIp);
            } else if (state == static_cast<uint8_t>(PairState::M3) && session.srpStep == 1) {
                const auto aIt = tlv.find(static_cast<uint8_t>(TlvTag::PublicKey));
                const auto proofIt = tlv.find(static_cast<uint8_t>(TlvTag::Proof));
                if (aIt == tlv.end() || proofIt == tlv.end()) {
                    std::vector<uint8_t> payload;
                    appendTlvByte(payload, TlvTag::State, static_cast<uint8_t>(PairState::M4));
                    appendTlvByte(payload, TlvTag::Error, static_cast<uint8_t>(PairError::Authentication));
                    response = makePairingRTSP(470, "Connection Authorization Required", cseq, payload);
                } else {
                    std::vector<uint8_t> serverProof;
                    AURA_LOG_INFO("AirPlay2Host",
                        "pair-setup M3 payload sizes from {}: A={} proof={}",
                        clientIp, aIt->second.size(), proofIt->second.size());
                    if (session.srp->computeSessionKey(aIt->second, proofIt->second, serverProof)) {
                        session.srp->deriveAESKey();
                        session.paired = true;
                        session.srpStep = 2;
                        if (m_onPairingResult) m_onPairingResult(true);

                        std::vector<uint8_t> payload;
                        appendTlvByte(payload, TlvTag::State, static_cast<uint8_t>(PairState::M4));
                        appendTlvValue(payload, static_cast<uint8_t>(TlvTag::Proof), serverProof);
                        response = makePairingRTSP(200, "OK", cseq, payload);

                        AURA_LOG_INFO("AirPlay2Host",
                            "pair-setup M3->M4 complete for {} (transient={})",
                            clientIp, (session.pairingFlags & kPairingTransientFlag) != 0);
                    } else {
                        if (m_onPairingResult) m_onPairingResult(false);
                        std::vector<uint8_t> payload;
                        appendTlvByte(payload, TlvTag::State, static_cast<uint8_t>(PairState::M4));
                        appendTlvByte(payload, TlvTag::Error, static_cast<uint8_t>(PairError::Authentication));
                        response = makePairingRTSP(470, "Connection Authorization Required", cseq, payload);
                        AURA_LOG_WARN("AirPlay2Host", "pair-setup proof rejected from {}", clientIp);
                    }
                }
            } else {
                std::vector<uint8_t> payload;
                appendTlvByte(payload, TlvTag::State,
                    state == static_cast<uint8_t>(PairState::M5)
                        ? static_cast<uint8_t>(PairState::M6)
                        : static_cast<uint8_t>(PairState::M2));
                appendTlvByte(payload, TlvTag::Error, static_cast<uint8_t>(PairError::Unavailable));
                response = makePairingRTSP(470, "Connection Authorization Required", cseq, payload);
            }
        }

        // ── pair-verify: confirm pairing with session key hash ────────────
        else if (req.find("POST /pair-verify") != std::string::npos) {
            const auto body = extractBody(req);
            const std::string contentType = extractHeader(req, "Content-Type");

            if (!body.empty() && body.size() >= 4 &&
                (contentType == "application/octet-stream" || contentType.empty())) {
                const uint8_t state = body[0];
                AURA_LOG_INFO("AirPlay2Host",
                    "pair-verify raw request from {}: state={} body={} bytes hex={}",
                    clientIp, static_cast<int>(state), body.size(),
                    hexPreview(body.data(), body.size(), 48));

                if (state == 1 && body.size() == 68) {
                    std::copy_n(body.data() + 4, session.peerCurveKey.size(), session.peerCurveKey.begin());
                    std::copy_n(body.data() + 4 + session.peerCurveKey.size(),
                        session.peerEdKey.size(), session.peerEdKey.begin());

                    const auto identity = loadOrCreateAirPlaySigningIdentity();
                    const auto x25519 = generateX25519KeyPair();
                    if (!identity || !x25519) {
                        response = makeRTSP(500, "Internal Server Error", cseq);
                    } else {
                        const auto& [ourPrivateKey, ourPublicKey] = *x25519;
                        session.ourCurveKey = ourPublicKey;
                        const auto shared = deriveX25519SharedSecret(ourPrivateKey, session.peerCurveKey);
                        if (!shared) {
                            response = makeRTSP(500, "Internal Server Error", cseq);
                        } else {
                            session.sharedSecret = *shared;
                            session.pairVerifyStep = 1;

                            std::array<uint8_t, 64> signatureInput{};
                            std::copy(session.ourCurveKey.begin(), session.ourCurveKey.end(), signatureInput.begin());
                            std::copy(session.peerCurveKey.begin(), session.peerCurveKey.end(),
                                signatureInput.begin() + session.ourCurveKey.size());

                            std::array<uint8_t, 64> signature{};
                            std::array<uint8_t, 16> key{};
                            std::array<uint8_t, 16> iv{};
                            derivePairVerifyCtrKeyMaterial(session.sharedSecret, key, iv);

                            if (!ed25519Sign(identity->privateKey, signatureInput.data(), signatureInput.size(), signature) ||
                                !aes128CtrTransform(key, iv, signature.data(), signature.size(), signature.data())) {
                                response = makeRTSP(500, "Internal Server Error", cseq);
                            } else {
                                std::vector<uint8_t> payload;
                                payload.insert(payload.end(), session.ourCurveKey.begin(), session.ourCurveKey.end());
                                payload.insert(payload.end(), signature.begin(), signature.end());
                                response = makeRTSP(200, "OK", cseq,
                                    "Content-Type: application/octet-stream\r\n"
                                    "Server: AirTunes/220.68\r\n",
                                    payload);
                                AURA_LOG_INFO("AirPlay2Host",
                                    "pair-verify raw M1->M2 complete for {}", clientIp);
                            }
                        }
                    }
                } else if (state == 0 && body.size() == 68 && session.pairVerifyStep == 1) {
                    std::array<uint8_t, 64> decryptedSig{};
                    std::array<uint8_t, 16> key{};
                    std::array<uint8_t, 16> iv{};
                    derivePairVerifyCtrKeyMaterial(session.sharedSecret, key, iv);
                    if (!aes128CtrTransform(key, iv, body.data() + 4, decryptedSig.size(),
                            decryptedSig.data(), true)) {
                        response = makeRTSP(500, "Internal Server Error", cseq);
                    } else {
                        std::array<uint8_t, 64> verifyInput{};
                        std::copy(session.peerCurveKey.begin(), session.peerCurveKey.end(), verifyInput.begin());
                        std::copy(session.ourCurveKey.begin(), session.ourCurveKey.end(),
                            verifyInput.begin() + session.peerCurveKey.size());

                        if (ed25519Verify(session.peerEdKey, verifyInput.data(), verifyInput.size(),
                                decryptedSig.data(), decryptedSig.size())) {
                            session.pairVerifyStep = 2;
                            response = makeRTSP(200, "OK", cseq,
                                "Content-Type: application/octet-stream\r\n"
                                "Server: AirTunes/220.68\r\n");
                            AURA_LOG_INFO("AirPlay2Host",
                                "pair-verify raw M3->M4 complete for {}", clientIp);
                        } else {
                            response = makeRTSP(470, "Connection Authorization Required", cseq);
                            AURA_LOG_WARN("AirPlay2Host",
                                "pair-verify raw signature rejected from {}", clientIp);
                        }
                    }
                } else {
                    response = makeRTSP(470, "Connection Authorization Required", cseq);
                    AURA_LOG_WARN("AirPlay2Host",
                        "pair-verify raw state/body not recognised from {}: state={} len={}",
                        clientIp, static_cast<int>(state), body.size());
                }
            } else {
                const auto tlv = decodeTlv8(body);
                const auto stateIt = tlv.find(static_cast<uint8_t>(TlvTag::State));
                const uint8_t state = (stateIt != tlv.end() && !stateIt->second.empty()) ? stateIt->second.front() : 0;
                std::vector<uint8_t> payload;
                appendTlvByte(payload, TlvTag::State, static_cast<uint8_t>(PairState::M2));
                appendTlvByte(payload, TlvTag::Error, static_cast<uint8_t>(PairError::Unavailable));
                response = makePairingRTSP(470, "Connection Authorization Required", cseq, payload);
                AURA_LOG_WARN("AirPlay2Host",
                    "pair-verify TLV8 still unsupported ({}): state=M{} tags=[{}]",
                    clientIp, static_cast<int>(state), describeTlvTags(tlv));
            }
        }

        // ── ANNOUNCE: codec negotiation ───────────────────────────────────
        else if (method == "ANNOUNCE") {
            // Extract video codec from SDP body
            std::string sdp = req.substr(req.find("\r\n\r\n") + 4);
            if (sdp.find("H265") != std::string::npos ||
                sdp.find("hevc") != std::string::npos)
                session.videoCodec = "H265";
            else
                session.videoCodec = "H264";

            // Extract device-id if present
            const std::string devId = extractHeader(req, "X-Apple-Device-ID");
            if (!devId.empty()) session.deviceId = devId;
            const std::string devName = extractHeader(req, "X-Apple-Device-Name");
            if (!devName.empty()) session.deviceName = devName;

            AURA_LOG_INFO("AirPlay2Host",
                "ANNOUNCE: codec={} device={}", session.videoCodec, session.deviceId);
            response = makeRTSP(200, "OK", cseq);
        }

        // ── SETUP: negotiate UDP ports ────────────────────────────────────
        else if (method == "SETUP") {
            // Parse the client's requested RTP port from Transport header.
            // Example: "Transport: RTP/AVP/UDP;unicast;client_port=49152-49153"
            uint16_t clientRtpPort = 0;
            const std::string transport = extractHeader(req, "Transport");
            const auto cpPos = transport.find("client_port=");
            if (cpPos != std::string::npos) {
                const std::string cpStr = transport.substr(cpPos + 12);
                try { clientRtpPort = static_cast<uint16_t>(std::stoi(cpStr)); }
                catch (...) { clientRtpPort = 0; }
            }

            // Server-side RTP ports: 7010 for video, 7011 for audio.
            // AirPlay2 sends separate SETUP requests per stream (video, audio, timing).
            // We route by URL path (e.g. /stream/video vs /stream/audio).
            session.videoPort = 7010;
            session.audioPort = AIRPLAY_AUDIO_RTP_PORT;

            const uint16_t serverRtpPort = isAudioSetup ? session.audioPort : session.videoPort;
            const std::string streamType = isAudioSetup ? "audio" : "video";

            response = makeRTSP(200, "OK", cseq,
                std::format("Session: 1\r\n"
                    "Transport: RTP/AVP/UDP;unicast;"
                    "client_port={};server_port={};timing_port={}\r\n",
                    clientRtpPort > 0 ? clientRtpPort : 49152U,
                    serverRtpPort,         // 7010 for video, 7011 for audio
                    AIRPLAY_TIMING_PORT)); // NTP timing on 7238
            AURA_LOG_INFO("AirPlay2Host",
                "SETUP ({}): client_port={}, server_port={}",
                streamType, clientRtpPort, serverRtpPort);
        }

        // ── RECORD: streaming starts ──────────────────────────────────────
        else if (method == "RECORD") {
            response = makeRTSP(200, "OK", cseq, "Session: 1\r\n");
            AURA_LOG_INFO("AirPlay2Host",
                "RECORD -- stream flowing from {}. Codec: {}", clientIp, session.videoCodec);
            if (m_onSessionStarted) m_onSessionStarted(session.toInfo());
        }

        // ── TEARDOWN: clean end ───────────────────────────────────────────
        else if (method == "TEARDOWN") {
            response = makeRTSP(200, "OK", cseq);
            sendResponse(sock, response);
            break;
        }

        // ── SET_PARAMETER / GET_PARAMETER: volume, progress, etc. ─────────
        else if (method == "SET_PARAMETER" || method == "GET_PARAMETER") {
            response = makeRTSP(200, "OK", cseq,
                "Content-Length: 0\r\n");
        }

        // ── FLUSH: client wants to seek / buffer flush ────────────────────
        else if (method == "FLUSH") {
            response = makeRTSP(200, "OK", cseq, "Session: 1\r\n");
        }

        // ── PAUSE: phone locked / backgrounded ───────────────────────────
        else if (method == "PAUSE") {
            response = makeRTSP(200, "OK", cseq, "Session: 1\r\n");
            AURA_LOG_INFO("AirPlay2Host",
                "PAUSE from {} -- stream paused (screen locked?)", clientIp);
            if (m_onSessionPaused) m_onSessionPaused(session.deviceId);
        }

        // ── Anything else -> 501 ───────────────────────────────────────────
        else {
            AURA_LOG_WARN("AirPlay2Host",
                "Unhandled method: {:.30s}", req);
            response = makeRTSP(501, "Not Implemented", cseq);
        }

        if (!response.empty()) {
            sendResponse(sock, response);
            AURA_LOG_INFO("AirPlay2Host", ">>> responded to {} with {} bytes", clientIp, response.size());
        }
    }

    closesocket(sock);
    AURA_LOG_INFO("AirPlay2Host", "Session ended: {}", clientIp);
    if (m_onSessionEnded) m_onSessionEnded(session.deviceId);
}

// =============================================================================
// Decryption helper -- can be called by the receiver pipeline to decrypt
// AES-128-CTR video packets before passing to NALParser
// =============================================================================
void AirPlay2Host::decryptVideoPacket(uint8_t* data, int len,
                                       const std::array<uint8_t, 16>& key,
                                       const std::array<uint8_t, 16>& iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return;
    int outLen = 0;
    EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), nullptr, key.data(), iv.data());
    EVP_DecryptUpdate(ctx, data, &outLen, data, len);
    EVP_CIPHER_CTX_free(ctx);
}

// =============================================================================
// stop / shutdown
// =============================================================================
void AirPlay2Host::stop() {
    if (!m_running.exchange(false)) return;
    if (m_impl->listenSocket != INVALID_SOCKET) {
        closesocket(m_impl->listenSocket);
        m_impl->listenSocket = INVALID_SOCKET;
    }
    if (m_acceptThread.joinable()) m_acceptThread.join();

    // Wait for all active session threads to finish (max 3 s).
    // Closing the listen socket causes handleSession to return on next recv error.
    constexpr int kMaxWaitMs = 3000;
    constexpr int kStepMs    = 50;
    for (int waited = 0; m_activeSessions.load() > 0 && waited < kMaxWaitMs; waited += kStepMs)
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));

    WSACleanup();
    AURA_LOG_INFO("AirPlay2Host", "Stopped.");
}

void AirPlay2Host::shutdown() { stop(); }

// =============================================================================
// Callback setters
// =============================================================================
void AirPlay2Host::setSessionStartedCallback(SessionStartedCallback cb) { m_onSessionStarted = std::move(cb); }
void AirPlay2Host::setSessionPausedCallback(SessionPausedCallback cb) {
    m_onSessionPaused = std::move(cb);
}

void AirPlay2Host::setSessionEndedCallback(SessionEndedCallback cb)     { m_onSessionEnded    = std::move(cb); }
void AirPlay2Host::setPinRequestCallback(PinRequestCallback cb)         { m_onPinRequest      = std::move(cb); }
void AirPlay2Host::setPairingResultCallback(PairingResultCallback cb)   { m_onPairingResult   = std::move(cb); }

std::string AirPlay2Host::generatePin() const {
    std::random_device rd;
    std::mt19937 rng(rd());
    return std::format("{:04d}", std::uniform_int_distribution<int>(0, 9999)(rng));
}

int AirPlay2Host::activeSessionCount() const { return m_activeSessions.load(std::memory_order_relaxed); }

} // namespace aura
