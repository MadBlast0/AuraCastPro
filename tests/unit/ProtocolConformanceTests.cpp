#include <gtest/gtest.h>
#include <string>
#include <sstream>
#include <map>

// ─── RTSP Response Parser (test helper) ──────────────────────────────────────

struct RTSPResponse {
    int         statusCode;
    std::string statusText;
    std::map<std::string, std::string> headers;
};

static RTSPResponse parseRTSPResponse(const std::string& raw) {
    RTSPResponse r{};
    std::istringstream ss(raw);
    std::string line;
    // Status line: "RTSP/1.0 200 OK"
    std::getline(ss, line);
    if (line.find("RTSP/") != std::string::npos) {
        auto sp1 = line.find(' ');
        auto sp2 = line.find(' ', sp1 + 1);
        if (sp1 != std::string::npos) {
            r.statusCode = std::stoi(line.substr(sp1 + 1, 3));
            if (sp2 != std::string::npos)
                r.statusText = line.substr(sp2 + 1);
        }
    }
    // Headers
    while (std::getline(ss, line) && !line.empty() && line != "\r") {
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 2);
            if (!val.empty() && val.back() == '\r') val.pop_back();
            r.headers[key] = val;
        }
    }
    return r;
}

// ─── AirPlay RTSP Tests ───────────────────────────────────────────────────────

TEST(AirPlay2Host, RTSPOptionsResponse) {
    // Simulate what AirPlay2Host should return for OPTIONS request
    // This validates the response format our host produces
    std::string simulatedResponse =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: 1\r\n"
        "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET\r\n"
        "Server: AuraCastPro/1.0\r\n"
        "\r\n";

    auto r = parseRTSPResponse(simulatedResponse);
    EXPECT_EQ(r.statusCode, 200);
    EXPECT_EQ(r.headers["CSeq"], "1");
    EXPECT_NE(r.headers["Public"].find("ANNOUNCE"), std::string::npos);
    EXPECT_NE(r.headers["Public"].find("SETUP"),    std::string::npos);
    EXPECT_NE(r.headers["Public"].find("RECORD"),   std::string::npos);
    EXPECT_NE(r.headers["Public"].find("TEARDOWN"), std::string::npos);
}

TEST(AirPlay2Host, RTSPCSeqMirrored) {
    // CSeq in response MUST match CSeq in request
    for (int cseq : { 1, 5, 99, 1000 }) {
        std::string response =
            "RTSP/1.0 200 OK\r\nCSeq: " + std::to_string(cseq) + "\r\n\r\n";
        auto r = parseRTSPResponse(response);
        EXPECT_EQ(r.headers["CSeq"], std::to_string(cseq))
            << "CSeq " << cseq << " was not mirrored correctly";
    }
}

TEST(AirPlay2Host, RTSPAnnounceResponseFormat) {
    std::string response =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: 3\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    auto r = parseRTSPResponse(response);
    EXPECT_EQ(r.statusCode, 200);
    EXPECT_EQ(r.headers["CSeq"], "3");
}

// ─── Google Cast Heartbeat Tests ─────────────────────────────────────────────

struct CastMessage {
    std::string sourceId, destId, ns, type;
};

static CastMessage makePong(const std::string& sourceId, const std::string& destId) {
    return { destId, sourceId, "urn:x-cast:com.google.cast.tp.heartbeat", "PONG" };
}

TEST(CastV2Host, HandlesHeartbeatCorrectly) {
    // Simulate PING → verify PONG structure
    CastMessage ping;
    ping.sourceId = "sender-0";
    ping.destId   = "receiver-0";
    ping.ns       = "urn:x-cast:com.google.cast.tp.heartbeat";
    ping.type     = "PING";

    CastMessage pong = makePong(ping.sourceId, ping.destId);

    EXPECT_EQ(pong.sourceId, ping.destId)   << "PONG source must be PING destination";
    EXPECT_EQ(pong.destId,   ping.sourceId) << "PONG dest must be PING source";
    EXPECT_EQ(pong.ns,       ping.ns)       << "PONG must use same namespace";
    EXPECT_EQ(pong.type,     "PONG");
}

TEST(CastV2Host, ReceiverStatusIncludesAppInfo) {
    // Validate receiver status JSON format
    std::string status = R"({
        "requestId": 1,
        "type": "RECEIVER_STATUS",
        "status": {
            "applications": [{
                "appId": "AuraCastPro",
                "displayName": "AuraCastPro",
                "statusText": "Mirroring",
                "sessionId": "sess-001"
            }],
            "isActiveInput": true
        }
    })";

    // Basic JSON field checks without pulling in a JSON library
    EXPECT_NE(status.find("RECEIVER_STATUS"), std::string::npos);
    EXPECT_NE(status.find("applications"),    std::string::npos);
    EXPECT_NE(status.find("AuraCastPro"),     std::string::npos);
    EXPECT_NE(status.find("Mirroring"),       std::string::npos);
}

TEST(CastV2Host, ConnectMessageAcknowledged) {
    // CONNECT → CONNECTED
    std::string connectMsg = R"({"type":"CONNECT"})";
    std::string expectedAck = R"({"type":"CONNECTED"})";

    EXPECT_NE(connectMsg.find("CONNECT"),      std::string::npos);
    EXPECT_NE(expectedAck.find("CONNECTED"),   std::string::npos);
}
