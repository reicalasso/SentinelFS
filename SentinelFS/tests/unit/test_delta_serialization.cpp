#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cstring>

using namespace SentinelFS;

void test_signature_serialization() {
    std::cout << "Running test_signature_serialization..." << std::endl;

    std::vector<BlockSignature> originalSigs;
    
    BlockSignature sig1;
    sig1.index = 0;
    sig1.adler32 = 0x12345678;
    sig1.sha256 = "sha256_hash_1";
    originalSigs.push_back(sig1);

    BlockSignature sig2;
    sig2.index = 1;
    sig2.adler32 = 0x87654321;
    sig2.sha256 = "sha256_hash_2_longer_string";
    originalSigs.push_back(sig2);

    std::vector<uint8_t> serialized = DeltaSerialization::serializeSignature(originalSigs);
    std::vector<BlockSignature> deserialized = DeltaSerialization::deserializeSignature(serialized);

    assert(deserialized.size() == originalSigs.size());
    
    assert(deserialized[0].index == originalSigs[0].index);
    assert(deserialized[0].adler32 == originalSigs[0].adler32);
    assert(deserialized[0].sha256 == originalSigs[0].sha256);

    assert(deserialized[1].index == originalSigs[1].index);
    assert(deserialized[1].adler32 == originalSigs[1].adler32);
    assert(deserialized[1].sha256 == originalSigs[1].sha256);

    std::cout << "test_signature_serialization passed." << std::endl;
}

void test_delta_serialization() {
    std::cout << "Running test_delta_serialization..." << std::endl;

    std::vector<DeltaInstruction> originalDeltas;

    // Literal instruction
    DeltaInstruction d1;
    d1.isLiteral = true;
    std::string literalStr = "some literal data";
    d1.literalData.assign(literalStr.begin(), literalStr.end());
    d1.blockIndex = 0; // Should be ignored/default
    originalDeltas.push_back(d1);

    // Reference instruction
    DeltaInstruction d2;
    d2.isLiteral = false;
    d2.blockIndex = 42;
    originalDeltas.push_back(d2);

    std::vector<uint8_t> serialized = DeltaSerialization::serializeDelta(originalDeltas, 4096);
    size_t blockSize;
    std::vector<DeltaInstruction> deserialized = DeltaSerialization::deserializeDelta(serialized, blockSize);

    assert(deserialized.size() == originalDeltas.size());

    // Check d1
    assert(deserialized[0].isLiteral == true);
    assert(deserialized[0].literalData == originalDeltas[0].literalData);

    // Check d2
    assert(deserialized[1].isLiteral == false);
    assert(deserialized[1].blockIndex == originalDeltas[1].blockIndex);

    std::cout << "test_delta_serialization passed." << std::endl;
}

void test_empty_serialization() {
    std::cout << "Running test_empty_serialization..." << std::endl;

    std::vector<BlockSignature> emptySigs;
    auto serSigs = DeltaSerialization::serializeSignature(emptySigs);
    auto deserSigs = DeltaSerialization::deserializeSignature(serSigs);
    assert(deserSigs.empty());

    std::vector<DeltaInstruction> emptyDeltas;
    size_t blockSize;
    auto serDeltas = DeltaSerialization::serializeDelta(emptyDeltas, 4096);
    auto deserDeltas = DeltaSerialization::deserializeDelta(serDeltas, blockSize);
    assert(deserDeltas.empty());

    std::cout << "test_empty_serialization passed." << std::endl;
}

int main() {
    try {
        test_signature_serialization();
        test_delta_serialization();
        test_empty_serialization();
        std::cout << "All DeltaSerialization tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
