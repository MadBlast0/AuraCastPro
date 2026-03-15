#pragma once

#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <cstring>

namespace BinaryPlist {

// Simple binary plist encoder for AirPlay SETUP responses
// Supports only: dictionaries with string keys and integer values, arrays of dictionaries
class Encoder {
public:
    // Encode a simple dictionary: {"key1": int_value1, "key2": int_value2}
    static std::vector<uint8_t> encodeDictionary(const std::map<std::string, int>& dict);
    
    // Encode a dictionary with a "streams" array: {"streams": [{"type": 110, "dataPort": 7000}]}
    static std::vector<uint8_t> encodeStreamsResponse(int streamType, int dataPort);

private:
    static void writeHeader(std::vector<uint8_t>& out);
    static void writeTrailer(std::vector<uint8_t>& out, size_t numObjects, size_t offsetTableOffset);
    static void writeInteger(std::vector<uint8_t>& out, int value);
    static void writeString(std::vector<uint8_t>& out, const std::string& str);
    static void writeDictMarker(std::vector<uint8_t>& out, size_t count);
    static void writeArrayMarker(std::vector<uint8_t>& out, size_t count);
    static void writeObjectRef(std::vector<uint8_t>& out, uint8_t ref);
    
    static uint32_t toBigEndian(uint32_t value);
    static uint64_t toBigEndian(uint64_t value);
    static uint16_t toBigEndian(uint16_t value);
};

} // namespace BinaryPlist
