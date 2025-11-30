#pragma once

/**
 * @file Result.h
 * @brief Consistent error handling types for SentinelFS
 * 
 * Provides a Result<T, E> type similar to Rust's Result or C++23's std::expected.
 * Use this for functions that can fail instead of mixing exceptions and return codes.
 */

#include <variant>
#include <string>
#include <optional>
#include <utility>

namespace sfs {

/**
 * @brief Error codes for SentinelFS operations
 */
enum class ErrorCode {
    Success = 0,
    
    // Network errors (100-199)
    NetworkError = 100,
    ConnectionFailed = 101,
    ConnectionTimeout = 102,
    HandshakeFailed = 103,
    PeerNotFound = 104,
    SendFailed = 105,
    ReceiveFailed = 106,
    
    // File system errors (200-299)
    FileNotFound = 200,
    FileAccessDenied = 201,
    FileReadError = 202,
    FileWriteError = 203,
    DirectoryNotFound = 204,
    DirectoryCreateFailed = 205,
    
    // Storage/Database errors (300-399)
    DatabaseError = 300,
    DatabaseOpenFailed = 301,
    QueryFailed = 302,
    TransactionFailed = 303,
    
    // Plugin errors (400-499)
    PluginLoadFailed = 400,
    PluginNotFound = 401,
    PluginVersionMismatch = 402,
    PluginDependencyMissing = 403,
    
    // Sync errors (500-599)
    SyncError = 500,
    DeltaCalculationFailed = 501,
    DeltaApplyFailed = 502,
    ConflictDetected = 503,
    
    // Configuration errors (600-699)
    ConfigError = 600,
    InvalidConfig = 601,
    MissingConfig = 602,
    
    // General errors (900-999)
    InvalidArgument = 900,
    OutOfMemory = 901,
    InternalError = 999
};

/**
 * @brief Convert error code to human-readable string
 */
inline const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::NetworkError: return "Network error";
        case ErrorCode::ConnectionFailed: return "Connection failed";
        case ErrorCode::ConnectionTimeout: return "Connection timeout";
        case ErrorCode::HandshakeFailed: return "Handshake failed";
        case ErrorCode::PeerNotFound: return "Peer not found";
        case ErrorCode::SendFailed: return "Send failed";
        case ErrorCode::ReceiveFailed: return "Receive failed";
        case ErrorCode::FileNotFound: return "File not found";
        case ErrorCode::FileAccessDenied: return "File access denied";
        case ErrorCode::FileReadError: return "File read error";
        case ErrorCode::FileWriteError: return "File write error";
        case ErrorCode::DirectoryNotFound: return "Directory not found";
        case ErrorCode::DirectoryCreateFailed: return "Directory creation failed";
        case ErrorCode::DatabaseError: return "Database error";
        case ErrorCode::DatabaseOpenFailed: return "Database open failed";
        case ErrorCode::QueryFailed: return "Query failed";
        case ErrorCode::TransactionFailed: return "Transaction failed";
        case ErrorCode::PluginLoadFailed: return "Plugin load failed";
        case ErrorCode::PluginNotFound: return "Plugin not found";
        case ErrorCode::PluginVersionMismatch: return "Plugin version mismatch";
        case ErrorCode::PluginDependencyMissing: return "Plugin dependency missing";
        case ErrorCode::SyncError: return "Sync error";
        case ErrorCode::DeltaCalculationFailed: return "Delta calculation failed";
        case ErrorCode::DeltaApplyFailed: return "Delta apply failed";
        case ErrorCode::ConflictDetected: return "Conflict detected";
        case ErrorCode::ConfigError: return "Configuration error";
        case ErrorCode::InvalidConfig: return "Invalid configuration";
        case ErrorCode::MissingConfig: return "Missing configuration";
        case ErrorCode::InvalidArgument: return "Invalid argument";
        case ErrorCode::OutOfMemory: return "Out of memory";
        case ErrorCode::InternalError: return "Internal error";
        default: return "Unknown error";
    }
}

/**
 * @brief Error type with code and optional message
 */
struct Error {
    ErrorCode code;
    std::string message;
    
    Error(ErrorCode c) : code(c), message(errorCodeToString(c)) {}
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
    
    bool operator==(const Error& other) const { return code == other.code; }
    bool operator!=(const Error& other) const { return code != other.code; }
};

/**
 * @brief Result type for operations that can fail
 * 
 * @tparam T Success value type
 * @tparam E Error type (defaults to Error)
 * 
 * Usage:
 * @code
 * Result<int> divide(int a, int b) {
 *     if (b == 0) return Error{ErrorCode::InvalidArgument, "Division by zero"};
 *     return a / b;
 * }
 * 
 * auto result = divide(10, 2);
 * if (result) {
 *     std::cout << "Result: " << *result << std::endl;
 * } else {
 *     std::cout << "Error: " << result.error().message << std::endl;
 * }
 * @endcode
 */
template<typename T, typename E = Error>
class Result {
public:
    /// Construct success result
    Result(T value) : data_(std::move(value)) {}
    
    /// Construct error result
    Result(E error) : data_(std::move(error)) {}
    
    /// Check if result is success
    bool ok() const { return std::holds_alternative<T>(data_); }
    
    /// Check if result is success (bool conversion)
    explicit operator bool() const { return ok(); }
    
    /// Check if result is error
    bool isError() const { return std::holds_alternative<E>(data_); }
    
    /// Get success value (throws if error)
    T& value() & { return std::get<T>(data_); }
    const T& value() const& { return std::get<T>(data_); }
    T&& value() && { return std::get<T>(std::move(data_)); }
    
    /// Get success value with default
    T valueOr(T defaultValue) const {
        if (ok()) return std::get<T>(data_);
        return defaultValue;
    }
    
    /// Get error (throws if success)
    E& error() & { return std::get<E>(data_); }
    const E& error() const& { return std::get<E>(data_); }
    
    /// Dereference operator (get value)
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(value()); }
    
    /// Arrow operator (access value members)
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

private:
    std::variant<T, E> data_;
};

/**
 * @brief Specialization for void success type
 */
template<typename E>
class Result<void, E> {
public:
    /// Construct success result
    Result() : error_(std::nullopt) {}
    
    /// Construct error result
    Result(E error) : error_(std::move(error)) {}
    
    /// Check if result is success
    bool ok() const { return !error_.has_value(); }
    
    /// Check if result is success (bool conversion)
    explicit operator bool() const { return ok(); }
    
    /// Check if result is error
    bool isError() const { return error_.has_value(); }
    
    /// Get error (throws if success)
    E& error() { return *error_; }
    const E& error() const { return *error_; }

private:
    std::optional<E> error_;
};

/// Convenience alias for Result with default Error type
template<typename T>
using Res = Result<T, Error>;

/// Create a success result
template<typename T>
Result<T> Ok(T value) {
    return Result<T>(std::move(value));
}

/// Create a void success result
inline Result<void> Ok() {
    return Result<void>();
}

/// Create an error result
template<typename T = void>
Result<T> Err(ErrorCode code) {
    return Result<T>(Error{code});
}

/// Create an error result with message
template<typename T = void>
Result<T> Err(ErrorCode code, std::string message) {
    return Result<T>(Error{code, std::move(message)});
}

} // namespace sfs
