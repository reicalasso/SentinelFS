#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace SentinelFS {

class INetworkAPI;
class IStorageAPI;
class IFileAPI;

/**
 * @brief Handles delta sync protocol messages
 */
class DeltaSyncProtocolHandler {
public:
    DeltaSyncProtocolHandler(INetworkAPI* network, IStorageAPI* storage, 
                            IFileAPI* filesystem, const std::string& watchDir);

    /**
     * @brief Handle UPDATE_AVAILABLE message
     */
    void handleUpdateAvailable(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle REQUEST_DELTA message
     */
    void handleDeltaRequest(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Handle DELTA_DATA message
     */
    void handleDeltaData(const std::string& peerId, const std::vector<uint8_t>& data);

    /**
     * @brief Set callback for marking files as patched
     */
    void setMarkAsPatchedCallback(std::function<void(const std::string&)> callback) {
        markAsPatchedCallback_ = callback;
    }

private:
    INetworkAPI* network_;
    IStorageAPI* storage_;
    IFileAPI* filesystem_;
    std::string watchDirectory_;
    std::function<void(const std::string&)> markAsPatchedCallback_;
};

} // namespace SentinelFS
