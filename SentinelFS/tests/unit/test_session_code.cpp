#include "SessionCode.h"
#include <iostream>
#include <cassert>
#include <string>
#include <set>

using namespace SentinelFS;

void test_generate() {
    std::cout << "Running test_generate..." << std::endl;
    
    std::string code = SessionCode::generate();
    assert(code.length() == 6);
    assert(SessionCode::isValid(code));
    
    // Test uniqueness (probabilistic)
    std::set<std::string> codes;
    for (int i = 0; i < 100; ++i) {
        codes.insert(SessionCode::generate());
    }
    // It's highly unlikely to generate duplicates in 100 tries with 32^6 possibilities
    assert(codes.size() > 90); 
    
    std::cout << "test_generate passed." << std::endl;
}

void test_is_valid() {
    std::cout << "Running test_is_valid..." << std::endl;
    
    assert(SessionCode::isValid("ABCDEF"));
    assert(SessionCode::isValid("123456"));
    assert(SessionCode::isValid("A1B2C3"));
    
    assert(!SessionCode::isValid("ABCDE")); // Too short
    assert(!SessionCode::isValid("ABCDEFG")); // Too long
    assert(!SessionCode::isValid("ABC-DE")); // Invalid char
    assert(!SessionCode::isValid("ABC DEF")); // Invalid char
    
    std::cout << "test_is_valid passed." << std::endl;
}

void test_format() {
    std::cout << "Running test_format..." << std::endl;
    
    assert(SessionCode::format("ABCDEF") == "ABC-DEF");
    assert(SessionCode::format("123456") == "123-456");
    assert(SessionCode::format("ABC") == "ABC"); // Should return original if not 6 chars
    
    std::cout << "test_format passed." << std::endl;
}

void test_normalize() {
    std::cout << "Running test_normalize..." << std::endl;
    
    assert(SessionCode::normalize("ABC-DEF") == "ABCDEF");
    assert(SessionCode::normalize("abc-def") == "ABCDEF"); // Should handle case
    assert(SessionCode::normalize("ABC DEF") == "ABCDEF");
    assert(SessionCode::normalize("A-B-C-D-E-F") == "ABCDEF");
    
    std::cout << "test_normalize passed." << std::endl;
}

int main() {
    try {
        test_generate();
        test_is_valid();
        test_format();
        test_normalize();
        std::cout << "All SessionCode tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
