// =============================================================================
// CastV2Host.cpp -- Google Cast V2 receiver with TLS 1.3 + Protobuf routing
//
// Cast protocol:
//   TCP TLS 1.3 on port 8009
//   Messages: 4-byte big-endian length + Protobuf CastMessage
//   Channels: tp.connection | tp.heartbeat | cast.receiver | cast.media
//
// TLS approach:
//   Self-signed RSA-2048 cert + key generated at startup (in-memory)
//   OpenSSL SSL_CTX with TLSv1_2_server_method(), upgraded to TLS1.3 when available
//   Any Android or Chrome device accepts this self-signed cert for mDNS-discovered receivers
//
// Protobuf CastMessage structure (binary, simplified parser):
//   Field 1  (varint): protocol_version = 0 (CASTV2_1_0)
//   Field 2  (bytes) : source_id
//   Field 3  (bytes) : destination_id
//   Field 4  (bytes) : namespace
//   Field 5  (varint): payload_type (0=STRING, 1=BINARY)
//   Field 6  (bytes) : payload_utf8  (if STRING)
//   Field 7  (bytes) : payload_binary (if BINARY)
// =============================================================================

#include "../pch.h"  // PCH
#include "CastV2Host.h"
#include "../utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

// OpenSSL TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>
#include <format>

namespace aura {

// Returns false if SSL_write fails (connection dropped)
static bool sslWrite(SSL* ssl, const std::vector<uint8_t>& data) {
    if (data.empty()) return true;
    int written = SSL_write(ssl, data.data(), static_cast<int>(data.size()));
    return (written > 0);
}


constexpr uint16_t CAST_PORT = 8009;

// =============================================================================
// Generate a self-signed RSA-2048 cert + key in memory
// =============================================================================
static bool generateSelfSignedCert(X509** outCert, EVP_PKEY** outKey) {
    // Generate RSA-2048 key
    EVP_PKEY* pkey = EVP_PKEY_new();
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
    EVP_PKEY_keygen(ctx, &pkey);
    EVP_PKEY_CTX_free(ctx);

    // Create X.509 cert
    X509* x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 365 * 24 * 60 * 60LL); // 1 year

    X509_set_pubkey(x509, pkey);

    // Subject / issuer (minimal)
    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("AuraCastPro"), -1, -1, 0);
    X509_set_issuer_name(x509, name);

    // Self-sign
    X509_sign(x509, pkey, EVP_sha256());

    *outCert = x509;
    *outKey  = pkey;
    return true;
}

// =============================================================================
// Minimal protobuf varint + field parser
// =============================================================================
static uint64_t readVarint(const uint8_t* data, size_t len, size_t& offset) {
    uint64_t result = 0;
    int shift = 0;
    while (offset < len) {
        uint8_t b = data[offset++];
        result |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return result;
}

static std::string readLengthDelimited(const uint8_t* data, size_t len, size_t& offset) {
    uint64_t sz = readVarint(data, len, offset);
    if (offset + sz > len) return {};
    std::string s(reinterpret_cast<const char*>(data + offset), sz);
    offset += sz;
    return s;
}

struct CastMessage {
    uint32_t    protocolVersion{0};
    std::string sourceId;
    std::string destinationId;
    std::string ns;         // namespace
    uint32_t    payloadType{0}; // 0=STRING, 1=BINARY
    std::string payloadUtf8;
    std::string payloadBinary;

    bool parse(const uint8_t* data, size_t len) {
        size_t offset = 0;
        while (offset < len) {
            uint64_t tag = readVarint(data, len, offset);
            uint32_t fieldNum  = static_cast<uint32_t>(tag >> 3);
            uint32_t wireType  = static_cast<uint32_t>(tag & 0x7);

            if (wireType == 0) { // varint
                uint64_t v = readVarint(data, len, offset);
                if (fieldNum == 1) protocolVersion = static_cast<uint32_t>(v);
                if (fieldNum == 5) payloadType     = static_cast<uint32_t>(v);
            } else if (wireType == 2) { // length-delimited
                std::string s = readLengthDelimited(data, len, offset);
                switch (fieldNum) {
                    case 2: sourceId      = s; break;
                    case 3: destinationId = s; break;
                    case 4: ns            = s; break;
                    case 6: payloadUtf8   = s; break;
                    case 7: payloadBinary = s; break;
                }
            } else break; // unknown wire type
        }
        return !ns.empty();
    }
};

// Encode a protobuf string field
static void writeStringField(std::vector<uint8_t>& out, uint32_t fieldNum, const std::string& s) {
    if (s.empty()) return;
    // Tag: (fieldNum << 3) | 2
    uint32_t tag = (fieldNum << 3) | 2;
    // Write tag varint
    while (tag > 0x7F) { out.push_back((tag & 0x7F) | 0x80); tag >>= 7; }
    out.push_back(static_cast<uint8_t>(tag));
    // Write length varint
    uint32_t sz = static_cast<uint32_t>(s.size());
    while (sz > 0x7F) { out.push_back((sz & 0x7F) | 0x80); sz >>= 7; }
    out.push_back(static_cast<uint8_t>(sz));
    out.insert(out.end(), s.begin(), s.end());
}

static void writeVarintField(std::vector<uint8_t>& out, uint32_t fieldNum, uint64_t v) {
    uint32_t tag = (fieldNum << 3) | 0;
    while (tag > 0x7F) { out.push_back((tag & 0x7F) | 0x80); tag >>= 7; }
    out.push_back(static_cast<uint8_t>(tag));
    while (v > 0x7F) { out.push_back((v & 0x7F) | 0x80); v >>= 7; }
    out.push_back(static_cast<uint8_t>(v));
}

static std::vector<uint8_t> encodeCastMessage(
    const std::string& sourceId, const std::string& destId,
    const std::string& ns, const std::string& payload)
{
    std::vector<uint8_t> body;
    writeVarintField(body, 1, 0);          // protocol_version = CASTV2_1_0
    writeStringField(body, 2, sourceId);
    writeStringField(body, 3, destId);
    writeStringField(body, 4, ns);
    writeVarintField(body, 5, 0);          // payload_type = STRING
    writeStringField(body, 6, payload);

    // Prefix with 4-byte big-endian length
    uint32_t len = static_cast<uint32_t>(body.size());
    std::vector<uint8_t> frame;
    frame.push_back((len >> 24) & 0xFF);
    frame.push_back((len >> 16) & 0xFF);
    frame.push_back((len >>  8) & 0xFF);
    frame.push_back( len        & 0xFF);
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

// =============================================================================
// Impl
// =============================================================================
struct CastV2Host::Impl {
    SOCKET      listenSocket{INVALID_SOCKET};
    SSL_CTX*    sslCtx{nullptr};
    X509*       cert{nullptr};
    EVP_PKEY*   privKey{nullptr};
};

CastV2Host::CastV2Host() : m_impl(std::make_unique<Impl>()) {}
CastV2Host::~CastV2Host() { shutdown(); }

// =============================================================================
// init -- create SSL context + self-signed cert
// =============================================================================
void CastV2Host::init() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    m_impl->sslCtx = SSL_CTX_new(TLS_server_method());
    if (!m_impl->sslCtx) {
        AURA_LOG_ERROR("CastV2Host", "SSL_CTX_new failed.");
        return;
    }

    // Require TLS 1.2+ (Cast devices may not support 1.3 universally)
    SSL_CTX_set_min_proto_version(m_impl->sslCtx, TLS1_2_VERSION);

    // Generate ephemeral self-signed cert
    if (!generateSelfSignedCert(&m_impl->cert, &m_impl->privKey)) {
        AURA_LOG_ERROR("CastV2Host", "Failed to generate self-signed cert.");
        return;
    }

    SSL_CTX_use_certificate(m_impl->sslCtx, m_impl->cert);
    SSL_CTX_use_PrivateKey(m_impl->sslCtx, m_impl->privKey);

    if (!SSL_CTX_check_private_key(m_impl->sslCtx)) {
        AURA_LOG_ERROR("CastV2Host", "SSL private key check failed.");
        return;
    }

    // Accept any client cert (Cast clients use self-signed certs too)
    SSL_CTX_set_verify(m_impl->sslCtx, SSL_VERIFY_NONE, nullptr);

    AURA_LOG_INFO("CastV2Host",
        "Initialised. TLS 1.2+ on port {}. Self-signed RSA-2048 cert ready.",
        CAST_PORT);
}

// =============================================================================
// start
// =============================================================================
void CastV2Host::start() {
    if (m_running.exchange(true)) return;

    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    m_impl->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int yes = 1;
    setsockopt(m_impl->listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(CAST_PORT);

    if (bind(m_impl->listenSocket,
             reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(m_impl->listenSocket, 4) == SOCKET_ERROR) {
        AURA_LOG_ERROR("CastV2Host",
            "bind/listen failed on port {}: {}", CAST_PORT, WSAGetLastError());
        m_running = false;
        return;
    }

    AURA_LOG_INFO("CastV2Host", "Listening on TCP 0.0.0.0:{} (TLS)", CAST_PORT);
    m_acceptThread = std::thread([this]() { acceptLoop(); });
}

// =============================================================================
// acceptLoop
// =============================================================================
void CastV2Host::acceptLoop() {
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
                AURA_LOG_DEBUG("CastV2Host", "Connection refused -- mirroring inactive.");
                closesocket(clientSock);
                continue;
            }

            char ip[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
            AURA_LOG_INFO("CastV2Host", "Cast connection from {}", ip);

            std::thread([this, clientSock, ipStr = std::string(ip)]() {
                m_activeSessions.fetch_add(1, std::memory_order_relaxed);
                try {
                    handleSession(static_cast<int>(clientSock), ipStr);
                } catch (const std::exception& e) {
                    AURA_LOG_ERROR("CastV2Host",
                        "Session thread exception ({}): {}", ipStr, e.what());
                } catch (...) {
                    AURA_LOG_ERROR("CastV2Host",
                        "Session thread unknown exception ({})", ipStr);
                }
                m_activeSessions.fetch_sub(1, std::memory_order_relaxed);
            }).detach();
        } catch (const std::exception& e) {
            AURA_LOG_ERROR("CastV2Host",
                "acceptLoop exception: {} -- continuing.", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (...) {
            AURA_LOG_ERROR("CastV2Host",
                "acceptLoop unknown exception -- continuing.");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// =============================================================================
// handleSession -- TLS handshake + Protobuf message loop
// =============================================================================
void CastV2Host::handleSession(int clientSocketRaw, const std::string& clientIp) {
    SOCKET sock = static_cast<SOCKET>(clientSocketRaw);

    // ── TLS upgrade ───────────────────────────────────────────────────────
    SSL* ssl = SSL_new(m_impl->sslCtx);
    SSL_set_fd(ssl, static_cast<int>(sock));

    if (SSL_accept(ssl) <= 0) {
        char errBuf[256]{};
        ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
        AURA_LOG_WARN("CastV2Host",
            "TLS handshake failed from {}: {}", clientIp, errBuf);
        SSL_free(ssl);
        closesocket(sock);
        return;
    }

    AURA_LOG_INFO("CastV2Host",
        "TLS {} established with {}",
        SSL_get_version(ssl), clientIp);

    CastSessionInfo session;
    session.ipAddress = clientIp;
    bool sessionStarted = false;

    // ── Protobuf message loop ─────────────────────────────────────────────
    std::array<uint8_t, 65536> msgBuf{};

    auto sslRead = [&](uint8_t* dst, int n) -> bool {
        int total = 0;
        while (total < n) {
            int r = SSL_read(ssl, dst + total, n - total);
            if (r <= 0) return false;
            total += r;
        }
        return true;
    };

    while (m_running.load()) {
        // Read 4-byte message length prefix
        uint8_t lenBuf[4]{};
        if (!sslRead(lenBuf, 4)) break;

        const uint32_t msgLen =
            (static_cast<uint32_t>(lenBuf[0]) << 24) |
            (static_cast<uint32_t>(lenBuf[1]) << 16) |
            (static_cast<uint32_t>(lenBuf[2]) <<  8) |
             static_cast<uint32_t>(lenBuf[3]);

        if (msgLen == 0 || msgLen > msgBuf.size()) {
            AURA_LOG_WARN("CastV2Host",
                "Invalid Cast message length: {}", msgLen);
            break;
        }

        if (!sslRead(msgBuf.data(), static_cast<int>(msgLen))) break;

        // Parse CastMessage protobuf
        CastMessage msg;
        if (!msg.parse(msgBuf.data(), msgLen)) {
            AURA_LOG_DEBUG("CastV2Host", "Unparseable Cast message ({} bytes)", msgLen);
            continue;
        }

        AURA_LOG_DEBUG("CastV2Host",
            "Cast[{}->{}] ns={} payload={:.60s}",
            msg.sourceId, msg.destinationId, msg.ns,
            msg.payloadUtf8.empty() ? "(binary)" : msg.payloadUtf8);

        // ── Route by namespace ────────────────────────────────────────────

        if (msg.ns == "urn:x-cast:com.google.cast.tp.connection") {
            // CONNECT / CLOSE
            if (msg.payloadUtf8.find("CONNECT") != std::string::npos) {
                AURA_LOG_INFO("CastV2Host",
                    "Cast channel CONNECTED from {}", clientIp);
            }
        }
        else if (msg.ns == "urn:x-cast:com.google.cast.tp.heartbeat") {
            // PING -> PONG
            auto pong = encodeCastMessage(
                msg.destinationId, msg.sourceId,
                "urn:x-cast:com.google.cast.tp.heartbeat",
                R"({"type":"PONG"})");
            sslWrite(ssl, pong);
        }
        else if (msg.ns == "urn:x-cast:com.google.cast.receiver") {
            // GET_STATUS -> send receiver status
            if (msg.payloadUtf8.find("GET_STATUS") != std::string::npos) {
                const std::string status =
                    R"({"type":"RECEIVER_STATUS","status":{"applications":[)"
                    R"({"appId":"AuraCastPro","displayName":"AuraCastPro","namespaces":[)"
                    R"({"name":"urn:x-cast:com.google.cast.media"},)"
                    R"({"name":"urn:x-cast:com.google.cast.webrtc"}],)"
                    R"("sessionId":"session-1","statusText":"Ready","transportId":"transport-1"}],)"
                    R"("isActiveInput":true,"isStandby":false,"volume":{"level":1.0,"muted":false}}})";
                auto resp = encodeCastMessage(
                    msg.destinationId, msg.sourceId,
                    msg.ns, status);
                sslWrite(ssl, resp);
            }
            else if (msg.payloadUtf8.find("LAUNCH") != std::string::npos) {
                AURA_LOG_INFO("CastV2Host", "Cast LAUNCH from {}", clientIp);
                if (!sessionStarted) {
                    sessionStarted = true;
                    session.sessionId = "session-1";
                    if (m_onStarted) m_onStarted(session);
                }
                // Respond with LAUNCH_ERROR or receiver status
                auto resp = encodeCastMessage(
                    msg.destinationId, msg.sourceId,
                    msg.ns,
                    R"({"type":"RECEIVER_STATUS","status":{"applications":[{"appId":"AuraCastPro","sessionId":"session-1"}]}})");
                sslWrite(ssl, resp);
            }
        }
        else if (msg.ns == "urn:x-cast:com.google.cast.media") {
            // Media control: LOAD, PLAY, PAUSE, STOP, GET_STATUS
            if (msg.payloadUtf8.find("LOAD") != std::string::npos) {
                AURA_LOG_INFO("CastV2Host", "Cast LOAD (media start) from {}", clientIp);
                auto resp = encodeCastMessage(
                    "receiver-0", msg.sourceId, msg.ns,
                    R"({"type":"MEDIA_STATUS","status":[{"mediaSessionId":1,"playbackRate":1,"playerState":"PLAYING","currentTime":0}]})");
                sslWrite(ssl, resp);
            }
        }
        else if (msg.ns == "urn:x-cast:com.google.cast.webrtc") {
            // Screen mirroring WebRTC signalling (offer/answer/ice)
            if (msg.payloadUtf8.find("offer") != std::string::npos ||
                msg.payloadUtf8.find("OFFER") != std::string::npos) {
                // Acknowledge the WebRTC offer -- actual WebRTC negotiation
                // happens in a future phase. For now we accept and signal the session.
                AURA_LOG_INFO("CastV2Host", "Cast WebRTC OFFER received from {}", clientIp);
                auto resp = encodeCastMessage(
                    msg.destinationId, msg.sourceId, msg.ns,
                    R"({"type":"ANSWER","seqNum":1,"answer":{"type":"answer","sdp":""}})");
                sslWrite(ssl, resp);
                if (!sessionStarted) {
                    sessionStarted = true;
                    session.sessionId = "session-1";
                    if (m_onStarted) m_onStarted(session);
                }
            } else {
                AURA_LOG_DEBUG("CastV2Host", "WebRTC message: {:.80s}", msg.payloadUtf8);
            }
        }
        else {
            AURA_LOG_DEBUG("CastV2Host", "Unknown Cast namespace: {}", msg.ns);
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    closesocket(sock);

    AURA_LOG_INFO("CastV2Host", "Cast session ended: {}", clientIp);
    if (sessionStarted && m_onEnded) m_onEnded(clientIp);
}

// =============================================================================
// stop / shutdown
// =============================================================================
void CastV2Host::stop() {
    if (!m_running.exchange(false)) return;
    if (m_impl->listenSocket != INVALID_SOCKET) {
        closesocket(m_impl->listenSocket);
        m_impl->listenSocket = INVALID_SOCKET;
    }
    if (m_acceptThread.joinable()) m_acceptThread.join();

    // Wait for active session threads to finish (max 3 s)
    constexpr int kMaxWaitMs = 3000;
    constexpr int kStepMs    = 50;
    for (int waited = 0; m_activeSessions.load() > 0 && waited < kMaxWaitMs; waited += kStepMs)
        std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));

    if (m_impl->sslCtx) {
        SSL_CTX_free(m_impl->sslCtx);
        m_impl->sslCtx = nullptr;
    }
    if (m_impl->cert) { X509_free(m_impl->cert); m_impl->cert = nullptr; }
    if (m_impl->privKey) { EVP_PKEY_free(m_impl->privKey); m_impl->privKey = nullptr; }
    WSACleanup();
    AURA_LOG_INFO("CastV2Host", "Stopped.");
}

void CastV2Host::shutdown() { stop(); }

void CastV2Host::setSessionStartedCallback(SessionStartedCallback cb) { m_onStarted = std::move(cb); }
void CastV2Host::setSessionEndedCallback(SessionEndedCallback cb)     { m_onEnded   = std::move(cb); }

} // namespace aura
