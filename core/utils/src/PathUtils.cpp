#include "PathUtils.h"
#include <cstdlib>
#include <stdexcept>

namespace SentinelFS {

std::filesystem::path PathUtils::getHome() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home);
    }
    throw std::runtime_error("HOME environment variable is not set");
}

std::filesystem::path PathUtils::getConfigDir() {
    if (const char* config = std::getenv("XDG_CONFIG_HOME")) {
        return std::filesystem::path(config) / "sentinelfs";
    }
    return getHome() / ".config" / "sentinelfs";
}

std::filesystem::path PathUtils::getDataDir() {
    if (const char* data = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(data) / "sentinelfs";
    }
    return getHome() / ".local" / "share" / "sentinelfs";
}

std::filesystem::path PathUtils::getRuntimeDir() {
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR")) {
        return std::filesystem::path(runtime) / "sentinelfs";
    }
    return std::filesystem::temp_directory_path() / "sentinelfs";
}

std::filesystem::path PathUtils::getSocketPath() {
    return getRuntimeDir() / "sentinel_daemon.sock";
}

std::filesystem::path PathUtils::getDatabasePath() {
    return getDataDir() / "sentinel.db";
}

void PathUtils::ensureDirectory(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir, ec);
    }
    if (ec) {
        throw std::runtime_error("Failed to create directory: " + dir.string() + " (" + ec.message() + ")");
    }
}

} // namespace SentinelFS
