// ProtocolHandshake.cpp
#include "../pch.h"  // PCH
#include "ProtocolHandshake.h"
#include "../utils/Logger.h"
#include "../utils/EncryptHelper.h"

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

#include <random>
#include <format>
#include <sstream>

namespace aura {

void ProtocolHandshake::init() {
    AURA_LOG_INFO("ProtocolHandshake",
        "Initialised. Provides: AirPlay PIN generation, "
        "SRP session key derivation, RTSP response helpers, HMAC-SHA1 verification.");
}

void ProtocolHandshake::start()    {}
void ProtocolHandshake::stop()     {}
void ProtocolHandshake::shutdown() {}

std::string ProtocolHandshake::generateAirPlayPIN() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0, 9999);
    return std::format("{:04d}", dist(rng));
}

std::vector<uint8_t> ProtocolHandshake::deriveAirPlaySessionKey(
    const std::vector<uint8_t>& srpSecret,
    const std::string& context) {
    // AirPlay uses SHA-512 HKDF to derive session keys
    // Extract + Expand pattern with the SRP shared secret as IKM
    std::vector<uint8_t> derived(16, 0); // AES-128 key

    const uint8_t* ikm    = srpSecret.data();
    const size_t   ikmLen = srpSecret.size();

    // Proper HKDF-SHA512 (RFC 5869) -- used by AirPlay2 pairing protocol
    // Step 1: Extract -- HMAC-SHA512(salt=context, IKM=srpSecret)
    // For AirPlay2: salt is the UTF-8 context string
    unsigned char prk[SHA512_DIGEST_LENGTH]; // pseudo-random key
    unsigned int prkLen = 0;
    HMAC(EVP_sha512(),
         context.data(), static_cast<int>(context.size()), // salt
         ikm, static_cast<int>(ikmLen),                    // IKM
         prk, &prkLen);

    // Step 2: Expand -- HMAC-SHA512(PRK, "" || 0x01) truncated to 16 bytes
    // For AirPlay2 context the info string is empty and counter starts at 1
    const uint8_t counter = 0x01;
    unsigned char okm[SHA512_DIGEST_LENGTH];
    unsigned int  okmLen = 0;
    HMAC_CTX* hctx = HMAC_CTX_new();
    HMAC_Init_ex(hctx, prk, static_cast<int>(prkLen), EVP_sha512(), nullptr);
    HMAC_Update(hctx, &counter, 1);
    HMAC_Final(hctx, okm, &okmLen);
    HMAC_CTX_free(hctx);

    std::copy(okm, okm + 16, derived.begin());
    return derived;
}

std::string ProtocolHandshake::buildRTSPOK(int cseq, const std::string& extra) {
    return std::format("RTSP/1.0 200 OK\r\nCSeq: {}\r\n{}\r\n", cseq, extra);
}

bool ProtocolHandshake::verifyHMAC(const std::vector<uint8_t>& data,
                                    const std::vector<uint8_t>& key,
                                    const std::vector<uint8_t>& expected) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int  resultLen = 0;
    HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()),
         data.data(), data.size(), result, &resultLen);

    if (resultLen != expected.size()) return false;
    return CRYPTO_memcmp(result, expected.data(), resultLen) == 0;
}

} // namespace aura
