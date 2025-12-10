#include "ConflictResolver.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace SentinelFS;

void test_vector_clock() {
    std::cout << "Running test_vector_clock..." << std::endl;
    
    VectorClock vc1;
    vc1.increment("peer1"); // peer1: 1
    
    VectorClock vc2;
    vc2.increment("peer1"); // peer1: 1
    vc2.increment("peer1"); // peer1: 2
    
    // vc1 < vc2
    assert(vc1.happensBefore(vc2));
    assert(!vc2.happensBefore(vc1));
    
    VectorClock vc3;
    vc3.increment("peer2"); // peer2: 1
    
    // vc1 and vc3 are concurrent (neither knows about the other)
    assert(vc1.isConcurrentWith(vc3));
    assert(!vc1.happensBefore(vc3));
    assert(!vc3.happensBefore(vc1));
    
    // Merge
    vc1.merge(vc3); // peer1: 1, peer2: 1
    assert(vc1.get("peer1") == 1);
    assert(vc1.get("peer2") == 1);
    
    // Now vc3 happens before vc1 (since vc1 has everything vc3 has + more)
    // Wait, vc3 is peer2:1. vc1 is peer1:1, peer2:1.
    // vc3 <= vc1.
    // happensBefore usually means strictly less.
    // Let's check implementation logic:
    // happensBefore: for all i, v1[i] <= v2[i] AND exists j, v1[j] < v2[j].
    // vc3: p2=1, p1=0. vc1: p2=1, p1=1.
    // p2: 1 <= 1. p1: 0 < 1.
    // So vc3 happensBefore vc1.
    assert(vc3.happensBefore(vc1));
    
    std::cout << "test_vector_clock passed." << std::endl;
}

void test_conflict_struct() {
    std::cout << "Running test_conflict_struct..." << std::endl;
    
    FileConflict conflict;
    conflict.path = "test.txt";
    conflict.strategy = ResolutionStrategy::NEWEST_WINS;
    
    assert(!conflict.resolved);
    assert(conflict.path == "test.txt");
    
    std::cout << "test_conflict_struct passed." << std::endl;
}

int main() {
    try {
        test_vector_clock();
        test_conflict_struct();
        std::cout << "All ConflictResolver tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
