#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include <cstring>
#include <arpa/inet.h>

namespace SentinelFS {

std::vector<uint8_t> DeltaSerialization::serializeSignature(const std::vector<BlockSignature>& sigs) {
    std::vector<uint8_t> buffer;
    uint32_t count = htonl(sigs.size());
    buffer.insert(buffer.end(), (uint8_t*)&count, (uint8_t*)&count + sizeof(count));

    for (const auto& sig : sigs) {
        uint32_t index = htonl(sig.index);
        uint32_t adler = htonl(sig.adler32);
        uint32_t shaLen = htonl(sig.sha256.length());
        
        buffer.insert(buffer.end(), (uint8_t*)&index, (uint8_t*)&index + sizeof(index));
        buffer.insert(buffer.end(), (uint8_t*)&adler, (uint8_t*)&adler + sizeof(adler));
        buffer.insert(buffer.end(), (uint8_t*)&shaLen, (uint8_t*)&shaLen + sizeof(shaLen));
        buffer.insert(buffer.end(), sig.sha256.begin(), sig.sha256.end());
    }
    return buffer;
}

std::vector<BlockSignature> DeltaSerialization::deserializeSignature(const std::vector<uint8_t>& data) {
    std::vector<BlockSignature> sigs;
    size_t offset = 0;
    
    if (data.size() < 4) return sigs;
    
    uint32_t count;
    memcpy(&count, data.data() + offset, 4);
    count = ntohl(count);
    offset += 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (offset + 12 > data.size()) break;
        
        BlockSignature sig;
        uint32_t val;
        
        memcpy(&val, data.data() + offset, 4);
        sig.index = ntohl(val);
        offset += 4;
        
        memcpy(&val, data.data() + offset, 4);
        sig.adler32 = ntohl(val);
        offset += 4;
        
        memcpy(&val, data.data() + offset, 4);
        uint32_t shaLen = ntohl(val);
        offset += 4;
        
        if (offset + shaLen > data.size()) break;
        sig.sha256 = std::string((char*)data.data() + offset, shaLen);
        offset += shaLen;
        
        sigs.push_back(sig);
    }
    return sigs;
}

std::vector<uint8_t> DeltaSerialization::serializeDelta(const std::vector<DeltaInstruction>& deltas) {
    std::vector<uint8_t> buffer;
    uint32_t count = htonl(deltas.size());
    buffer.insert(buffer.end(), (uint8_t*)&count, (uint8_t*)&count + sizeof(count));

    for (const auto& delta : deltas) {
        uint8_t isLiteral = delta.isLiteral ? 1 : 0;
        buffer.push_back(isLiteral);
        
        if (delta.isLiteral) {
            uint32_t len = htonl(delta.literalData.size());
            buffer.insert(buffer.end(), (uint8_t*)&len, (uint8_t*)&len + sizeof(len));
            buffer.insert(buffer.end(), delta.literalData.begin(), delta.literalData.end());
        } else {
            uint32_t index = htonl(delta.blockIndex);
            buffer.insert(buffer.end(), (uint8_t*)&index, (uint8_t*)&index + sizeof(index));
        }
    }
    return buffer;
}

std::vector<DeltaInstruction> DeltaSerialization::deserializeDelta(const std::vector<uint8_t>& data) {
    std::vector<DeltaInstruction> deltas;
    size_t offset = 0;
    
    if (data.size() < 4) return deltas;
    
    uint32_t count;
    memcpy(&count, data.data() + offset, 4);
    count = ntohl(count);
    offset += 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (offset + 1 > data.size()) break;
        
        DeltaInstruction delta;
        delta.isLiteral = (data[offset++] == 1);
        
        if (delta.isLiteral) {
            if (offset + 4 > data.size()) break;
            uint32_t len;
            memcpy(&len, data.data() + offset, 4);
            len = ntohl(len);
            offset += 4;
            
            if (offset + len > data.size()) break;
            delta.literalData.assign(data.begin() + offset, data.begin() + offset + len);
            offset += len;
        } else {
            if (offset + 4 > data.size()) break;
            uint32_t index;
            memcpy(&index, data.data() + offset, 4);
            delta.blockIndex = ntohl(index);
            offset += 4;
        }
        deltas.push_back(delta);
    }
    return deltas;
}

} // namespace SentinelFS
