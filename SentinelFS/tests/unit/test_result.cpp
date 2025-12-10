#include "Result.h"
#include <iostream>
#include <cassert>
#include <string>

using namespace sfs;

Result<int> divide(int a, int b) {
    if (b == 0) return Error{ErrorCode::InvalidArgument, "Division by zero"};
    return a / b;
}

void test_success() {
    std::cout << "Running test_success..." << std::endl;
    
    auto res = divide(10, 2);
    // Assuming Result has operator bool() and operator*()
    // Based on the header snippet, it seems to follow that pattern.
    if (res) {
        assert(*res == 5);
    } else {
        assert(false && "Should be success");
    }
    
    std::cout << "test_success passed." << std::endl;
}

void test_failure() {
    std::cout << "Running test_failure..." << std::endl;
    
    auto res = divide(10, 0);
    if (!res) {
        assert(res.error().code == ErrorCode::InvalidArgument);
        assert(res.error().message == "Division by zero");
    } else {
        assert(false && "Should be failure");
    }
    
    std::cout << "test_failure passed." << std::endl;
}

void test_error_string() {
    std::cout << "Running test_error_string..." << std::endl;
    
    assert(std::string(errorCodeToString(ErrorCode::Success)) == "Success");
    assert(std::string(errorCodeToString(ErrorCode::NetworkError)) == "Network error");
    
    std::cout << "test_error_string passed." << std::endl;
}

int main() {
    try {
        test_success();
        test_failure();
        test_error_string();
        std::cout << "All Result tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
