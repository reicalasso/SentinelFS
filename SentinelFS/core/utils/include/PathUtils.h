#pragma once

#include <filesystem>
#include <string>

namespace SentinelFS {

class PathUtils {
public:
    static std::filesystem::path getHome();
    static std::filesystem::path getConfigDir();
    static std::filesystem::path getDataDir();
    static std::filesystem::path getRuntimeDir();
    static std::filesystem::path getDatabasePath();
    static std::filesystem::path getSocketPath();
    static void ensureDirectory(const std::filesystem::path& dir);
};

} // namespace SentinelFS
