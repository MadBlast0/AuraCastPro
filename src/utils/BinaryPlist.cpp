#include "BinaryPlist.h"
#include <algorithm>

namespace BinaryPlist {

uint32_t Encoder::toBigEndian(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

uint64_t Encoder::toBigEndian(uint64_t value) {
    return ((value & 0xFF00000000000000ULL) >> 56) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x00000000000000FFULL) << 56);
}

uint16_t Encoder::toBigEndian(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

void Encoder::writeHeader(std::vector<uint8_t>& out) {
    // Magic number: "bpli" (0x62706c69)
    uint32_t magic = toBigEndian(static_cast<uint32_t>(0x62706c69));
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&magic), reinterpret_cast<uint8_t*>(&magic) + 4);
    
    // Version: "st00" (0x73743030)
    uint32_t version = toBigEndian(static_cast<uint32_t>(0x73743030));
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&version), reinterpret_cast<uint8_t*>(&version) + 4);
}

void Encoder::writeTrailer(std::vector<uint8_t>& out, size_t numObjects, size_t offsetTableOffset) {
    // 6 unused bytes
    for (int i = 0; i < 6; i++) out.push_back(0);
    
    // Offset int size (1 byte)
    out.push_back(1);
    
    // Object ref size (1 byte)
    out.push_back(1);
    
    // Number of objects (8 bytes, big endian)
    uint64_t numObj = toBigEndian(static_cast<uint64_t>(numObjects));
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&numObj), reinterpret_cast<uint8_t*>(&numObj) + 8);
    
    // Top object (8 bytes, big endian) - always 0
    uint64_t topObj = 0;
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&topObj), reinterpret_cast<uint8_t*>(&topObj) + 8);
    
    // Offset table offset (8 bytes, big endian)
    uint64_t offset = toBigEndian(static_cast<uint64_t>(offsetTableOffset));
    out.insert(out.end(), reinterpret_cast<uint8_t*>(&offset), reinterpret_cast<uint8_t*>(&offset) + 8);
}

void Encoder::writeInteger(std::vector<uint8_t>& out, int value) {
    if (value >= 0 && value < 128) {
        // 1-byte integer: marker 0x10, then value
        out.push_back(0x10);
        out.push_back(static_cast<uint8_t>(value));
    } else if (value >= -32768 && value <= 32767) {
        // 2-byte integer: marker 0x11, then big-endian int16
        out.push_back(0x11);
        uint16_t val = toBigEndian(static_cast<uint16_t>(value));
        out.insert(out.end(), reinterpret_cast<uint8_t*>(&val), reinterpret_cast<uint8_t*>(&val) + 2);
    } else {
        // 4-byte integer: marker 0x12, then big-endian int32
        out.push_back(0x12);
        uint32_t val = toBigEndian(static_cast<uint32_t>(value));
        out.insert(out.end(), reinterpret_cast<uint8_t*>(&val), reinterpret_cast<uint8_t*>(&val) + 4);
    }
}

void Encoder::writeString(std::vector<uint8_t>& out, const std::string& str) {
    // ASCII string marker: 0x5X where X is length (if < 15)
    if (str.length() < 15) {
        out.push_back(0x50 | static_cast<uint8_t>(str.length()));
    } else {
        // For longer strings: 0x5F followed by integer count
        out.push_back(0x5F);
        out.push_back(0x10);  // 1-byte integer marker
        out.push_back(static_cast<uint8_t>(str.length()));
    }
    out.insert(out.end(), str.begin(), str.end());
}

void Encoder::writeDictMarker(std::vector<uint8_t>& out, size_t count) {
    // Dictionary marker: 0xDX where X is count (if < 15)
    if (count < 15) {
        out.push_back(0xD0 | static_cast<uint8_t>(count));
    } else {
        out.push_back(0xDF);
        out.push_back(0x10);  // 1-byte integer marker
        out.push_back(static_cast<uint8_t>(count));
    }
}

void Encoder::writeArrayMarker(std::vector<uint8_t>& out, size_t count) {
    // Array marker: 0xAX where X is count (if < 15)
    if (count < 15) {
        out.push_back(0xA0 | static_cast<uint8_t>(count));
    } else {
        out.push_back(0xAF);
        out.push_back(0x10);  // 1-byte integer marker
        out.push_back(static_cast<uint8_t>(count));
    }
}

void Encoder::writeObjectRef(std::vector<uint8_t>& out, uint8_t ref) {
    out.push_back(ref);
}

std::vector<uint8_t> Encoder::encodeDictionary(const std::map<std::string, int>& dict) {
    std::vector<uint8_t> result;
    std::vector<size_t> offsets;
    
    // Write header
    writeHeader(result);
    
    // Object table starts at offset 8
    // Object 0: The dictionary (top-level object)
    size_t dictOffset = result.size();
    offsets.push_back(dictOffset);
    
    // Write dictionary marker first (we'll come back to write the actual dict)
    size_t dictMarkerPos = result.size();
    writeDictMarker(result, dict.size());
    
    // Reserve space for key and value references (we'll fill these in later)
    size_t refsStart = result.size();
    for (size_t i = 0; i < dict.size() * 2; i++) {
        result.push_back(0);  // Placeholder for references
    }
    
    // Now write all string keys (objects 1, 2, ...)
    std::vector<uint8_t> keyRefs;
    for (const auto& [key, value] : dict) {
        keyRefs.push_back(static_cast<uint8_t>(offsets.size()));
        offsets.push_back(result.size());
        writeString(result, key);
    }
    
    // Write all integer values (objects N, N+1, ...)
    std::vector<uint8_t> valueRefs;
    for (const auto& [key, value] : dict) {
        valueRefs.push_back(static_cast<uint8_t>(offsets.size()));
        offsets.push_back(result.size());
        writeInteger(result, value);
    }
    
    // Now go back and fill in the references in the dictionary
    size_t refPos = refsStart;
    for (uint8_t keyRef : keyRefs) {
        result[refPos++] = keyRef;
    }
    for (uint8_t valueRef : valueRefs) {
        result[refPos++] = valueRef;
    }
    
    // Write offset table
    size_t offsetTableStart = result.size();
    for (size_t offset : offsets) {
        result.push_back(static_cast<uint8_t>(offset));
    }
    
    // Write trailer
    writeTrailer(result, offsets.size(), offsetTableStart);
    
    return result;
}

std::vector<uint8_t> Encoder::encodeStreamsResponse(int streamType, int dataPort) {
    std::vector<uint8_t> result;
    std::vector<size_t> offsets;
    
    // Write header
    writeHeader(result);
    
    // For video (type=110): only type and dataPort
    // For audio (type=96): type, controlPort, and dataPort
    bool isVideo = (streamType == 110);
    int numKeys = isVideo ? 2 : 3;
    
    // Object 0: top-level dictionary {"streams": array}
    offsets.push_back(result.size());
    writeDictMarker(result, 1);
    result.push_back(isVideo ? 5 : 7);  // key ref: "streams"
    result.push_back(isVideo ? 6 : 8);  // value ref: array
    
    // Object 1: string "type"
    offsets.push_back(result.size());
    writeString(result, "type");
    
    // Object 2: integer streamType
    offsets.push_back(result.size());
    writeInteger(result, streamType);
    
    if (isVideo) {
        // Object 3: string "dataPort"
        offsets.push_back(result.size());
        writeString(result, "dataPort");
        
        // Object 4: integer dataPort
        offsets.push_back(result.size());
        writeInteger(result, dataPort);
        
        // Object 5: string "streams"
        offsets.push_back(result.size());
        writeString(result, "streams");
        
        // Object 6: array [dict]
        offsets.push_back(result.size());
        writeArrayMarker(result, 1);
        writeObjectRef(result, 7);  // reference to dictionary (object 7)
        
        // Object 7: dictionary {"type": streamType, "dataPort": dataPort}
        offsets.push_back(result.size());
        writeDictMarker(result, 2);  // 2 key-value pairs
        writeObjectRef(result, 3);  // key "dataPort"
        writeObjectRef(result, 1);  // key "type"
        writeObjectRef(result, 4);  // value dataPort
        writeObjectRef(result, 2);  // value streamType
    } else {
        // Audio: include controlPort
        // Object 3: string "controlPort"
        offsets.push_back(result.size());
        writeString(result, "controlPort");
        
        // Object 4: integer controlPort
        offsets.push_back(result.size());
        writeInteger(result, dataPort);
        
        // Object 5: string "dataPort"
        offsets.push_back(result.size());
        writeString(result, "dataPort");
        
        // Object 6: integer dataPort
        offsets.push_back(result.size());
        writeInteger(result, dataPort);
        
        // Object 7: string "streams"
        offsets.push_back(result.size());
        writeString(result, "streams");
        
        // Object 8: array [dict]
        offsets.push_back(result.size());
        writeArrayMarker(result, 1);
        writeObjectRef(result, 9);  // reference to dictionary (object 9)
        
        // Object 9: dictionary {"type": streamType, "controlPort": dataPort, "dataPort": dataPort}
        offsets.push_back(result.size());
        writeDictMarker(result, 3);  // 3 key-value pairs
        writeObjectRef(result, 3);  // key "controlPort"
        writeObjectRef(result, 5);  // key "dataPort"
        writeObjectRef(result, 1);  // key "type"
        writeObjectRef(result, 4);  // value controlPort
        writeObjectRef(result, 6);  // value dataPort
        writeObjectRef(result, 2);  // value streamType
    }
    
    // Write offset table
    size_t offsetTableStart = result.size();
    for (size_t offset : offsets) {
        result.push_back(static_cast<uint8_t>(offset));
    }
    
    // Write trailer
    writeTrailer(result, offsets.size(), offsetTableStart);
    
    return result;
}

} // namespace BinaryPlist
