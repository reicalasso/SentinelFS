#pragma once

#include <variant>
#include <string>
#include <stdexcept>
#include <functional>

namespace SentinelFS {

/**
 * @brief Error type for Result pattern
 */
struct Error {
    std::string message;
    int code{0};
    std::string component;
    
    Error() = default;
    Error(std::string msg, int c = 0, std::string comp = "")
        : message(std::move(msg)), code(c), component(std::move(comp)) {}
    
    std::string toString() const {
        std::string result = message;
        if (!component.empty()) {
            result = "[" + component + "] " + result;
        }
        if (code != 0) {
            result += " (code: " + std::to_string(code) + ")";
        }
        return result;
    }
};

/**
 * @brief Result type for explicit error handling
 * 
 * Similar to Rust's Result<T, E> or Haskell's Either.
 * Forces callers to handle errors explicitly.
 * 
 * Usage:
 *   Result<int, Error> divide(int a, int b) {
 *       if (b == 0) return Error{"Division by zero"};
 *       return a / b;
 *   }
 * 
 *   auto result = divide(10, 2);
 *   if (result.isOk()) {
 *       std::cout << result.value() << std::endl;
 *   } else {
 *       std::cerr << result.error().toString() << std::endl;
 *   }
 */
template<typename T, typename E = Error>
class Result {
public:
    // Constructors
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const E& error) : data_(error) {}
    Result(E&& error) : data_(std::move(error)) {}
    
    // Check status
    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool isError() const { return std::holds_alternative<E>(data_); }
    
    // Explicit operators
    explicit operator bool() const { return isOk(); }
    
    // Access value (throws if error)
    T& value() {
        if (isError()) {
            throw std::runtime_error("Called value() on Error result: " + 
                std::get<E>(data_).toString());
        }
        return std::get<T>(data_);
    }
    
    const T& value() const {
        if (isError()) {
            throw std::runtime_error("Called value() on Error result: " + 
                std::get<E>(data_).toString());
        }
        return std::get<T>(data_);
    }
    
    // Access error (throws if ok)
    E& error() {
        if (isOk()) {
            throw std::runtime_error("Called error() on Ok result");
        }
        return std::get<E>(data_);
    }
    
    const E& error() const {
        if (isOk()) {
            throw std::runtime_error("Called error() on Ok result");
        }
        return std::get<E>(data_);
    }
    
    // Safe accessors with defaults
    T valueOr(const T& defaultValue) const {
        return isOk() ? std::get<T>(data_) : defaultValue;
    }
    
    // Map operations (functional style)
    template<typename F>
    auto map(F&& func) -> Result<decltype(func(std::declval<T>())), E> {
        if (isOk()) {
            return Result<decltype(func(std::declval<T>())), E>(func(value()));
        }
        return Result<decltype(func(std::declval<T>())), E>(error());
    }
    
    // Flat map (for chaining Results)
    template<typename F>
    auto andThen(F&& func) -> decltype(func(std::declval<T>())) {
        if (isOk()) {
            return func(value());
        }
        using ReturnType = decltype(func(std::declval<T>()));
        return ReturnType(error());
    }
    
    // Execute callback if error
    Result& onError(std::function<void(const E&)> callback) {
        if (isError()) {
            callback(error());
        }
        return *this;
    }
    
    // Execute callback if ok
    Result& onOk(std::function<void(const T&)> callback) {
        if (isOk()) {
            callback(value());
        }
        return *this;
    }

private:
    std::variant<T, E> data_;
};

// Specialization for void (no value, only success/error)
template<typename E>
class Result<void, E> {
public:
    Result() : data_(OkType{}) {}
    Result(const E& error) : data_(error) {}
    Result(E&& error) : data_(std::move(error)) {}
    
    bool isOk() const { return std::holds_alternative<OkType>(data_); }
    bool isError() const { return std::holds_alternative<E>(data_); }
    
    explicit operator bool() const { return isOk(); }
    
    E& error() {
        if (isOk()) {
            throw std::runtime_error("Called error() on Ok result");
        }
        return std::get<E>(data_);
    }
    
    const E& error() const {
        if (isOk()) {
            throw std::runtime_error("Called error() on Ok result");
        }
        return std::get<E>(data_);
    }
    
    Result& onError(std::function<void(const E&)> callback) {
        if (isError()) {
            callback(error());
        }
        return *this;
    }

private:
    struct OkType {};
    std::variant<OkType, E> data_;
};

// Helper type aliases
template<typename T>
using FileResult = Result<T, Error>;

using VoidResult = Result<void, Error>;

// Helper functions
template<typename T>
Result<T, Error> Ok(T value) {
    return Result<T, Error>(std::move(value));
}

inline VoidResult Ok() {
    return VoidResult();
}

template<typename E = Error>
auto Err(std::string message, int code = 0, std::string component = "") {
    return E(std::move(message), code, std::move(component));
}

} // namespace SentinelFS
