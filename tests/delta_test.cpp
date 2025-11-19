#include "DeltaEngine.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

void testAdler32() {
    std::string data = "Wikipedia";
    // Expected Adler-32 for "Wikipedia" is 0x11E60398
    uint32_t expected = 0x11E60398;
    uint32_t result = SentinelFS::DeltaEngine::calculateAdler32(
        reinterpret_cast<const uint8_t*>(data.c_str()), data.length()
    );
    
    std::cout << "Adler32('Wikipedia'): 0x" << std::hex << result << std::dec << std::endl;
    assert(result == expected);
    std::cout << "Adler32 Test Passed!" << std::endl;
}

void testSignature() {
    std::string testFile = "delta_test_file.txt";
    std::ofstream outfile(testFile);
    // Write enough data to span multiple blocks (Block size is 4096)
    // Write 5000 bytes
    std::string content(5000, 'A'); 
    outfile << content;
    outfile.close();

    auto signatures = SentinelFS::DeltaEngine::calculateSignature(testFile);
    
    std::cout << "Signatures count: " << signatures.size() << std::endl;
    // 5000 bytes / 4096 block size = 2 blocks (4096 + 904)
    assert(signatures.size() == 2);
    
    std::cout << "Block 0 Index: " << signatures[0].index << std::endl;
    assert(signatures[0].index == 0);
    
    std::cout << "Block 1 Index: " << signatures[1].index << std::endl;
    assert(signatures[1].index == 1);

    fs::remove(testFile);
    std::cout << "Signature Test Passed!" << std::endl;
}

void testDeltaCalculation() {
    std::string oldFile = "old_version.txt";
    std::string newFile = "new_version.txt";

    // Create old file: "Block1" + "Block2"
    // Block size is 4096.
    std::string block1(4096, 'A');
    std::string block2(4096, 'B');
    
    std::ofstream oldOut(oldFile);
    oldOut << block1 << block2;
    oldOut.close();

    // Create new file: "Block1" + "INSERTION" + "Block2"
    std::string insertion = "INSERTION";
    std::ofstream newOut(newFile);
    newOut << block1 << insertion << block2;
    newOut.close();

    // 1. Calculate signature of old file
    auto signatures = SentinelFS::DeltaEngine::calculateSignature(oldFile);
    assert(signatures.size() == 2);

    // 2. Calculate delta
    auto deltas = SentinelFS::DeltaEngine::calculateDelta(newFile, signatures);

    // Expected: 
    // 1. Block Reference (Index 0) -> Matches block1
    // 2. Literal ("INSERTION")
    // 3. Block Reference (Index 1) -> Matches block2

    std::cout << "Deltas count: " << deltas.size() << std::endl;
    
    // Note: The implementation might merge literals or split differently depending on the rolling match.
    // But for this simple case:
    // - It should match the first 4096 bytes (Block 0).
    // - Then it sees "INSERTION", which doesn't match any block start.
    // - It consumes "INSERTION" as literals.
    // - Then it sees Block 2.

    int blockRefs = 0;
    int literals = 0;
    size_t literalBytes = 0;

    for (const auto& d : deltas) {
        if (d.isLiteral) {
            literals++;
            literalBytes += d.literalData.size();
            std::string lit(d.literalData.begin(), d.literalData.end());
            std::cout << "Literal: " << lit << std::endl;
        } else {
            blockRefs++;
            std::cout << "Block Ref: " << d.blockIndex << std::endl;
        }
    }

    assert(blockRefs == 2);
    assert(literalBytes == insertion.length());

    fs::remove(oldFile);
    fs::remove(newFile);
    std::cout << "Delta Calculation Test Passed!" << std::endl;
}

int main() {
    testAdler32();
    testSignature();
    testDeltaCalculation();
    return 0;
}
