#pragma once

#include "IPlugin.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace SentinelFS {

    class INetworkAPI : public IPlugin {
    public:
        virtual ~INetworkAPI() = default;

        /**
         * @brief Connect to a peer.
         * @param address The IP address or hostname of the peer.
         * @param port The port number.
         * @return true if connection was successful, false otherwise.
         */
        virtual bool connectToPeer(const std::string& address, int port) = 0;

        /**
         * @brief Send data to a specific peer.
         * @param peerId The ID of the peer.
         * @param data The data to send.
         * @return true if sent successfully, false otherwise.
         */
        virtual bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) = 0;

        /**
         * @brief Start listening for incoming connections.
         * @param port The port to listen on.
         */
        virtual void startListening(int port) = 0;

        /**
         * @brief Start peer discovery (UDP Broadcast).
         * @param port The UDP port to use for discovery.
         */
        virtual void startDiscovery(int port) = 0;

        /**
         * @brief Broadcast presence to the network.
         * @param discoveryPort The UDP port to broadcast to.
         * @param tcpPort The TCP port this peer is listening on.
         */
        virtual void broadcastPresence(int discoveryPort, int tcpPort) = 0;

        /**
         * @brief Measure round-trip time (RTT) to a peer.
         * @param peerId The ID of the peer to ping.
         * @return RTT in milliseconds, or -1 if failed.
         */
        virtual int measureRTT(const std::string& peerId) = 0;

        /**
         * @brief Get the current RTT to a peer.
         * @param peerId The ID of the peer.
         * @return Last measured RTT in milliseconds, or -1 if not available.
         */
        virtual int getPeerRTT(const std::string& peerId) = 0;

        /**
         * @brief Disconnect from a specific peer.
         * @param peerId The ID of the peer to disconnect.
         */
        virtual void disconnectPeer(const std::string& peerId) = 0;

        /**
         * @brief Check if a peer is currently connected.
         * @param peerId The ID of the peer.
         * @return true if connected, false otherwise.
         */
        virtual bool isPeerConnected(const std::string& peerId) = 0;

        /**
         * @brief Set the session code for this peer group.
         * @param code The 6-character session code.
         */
        virtual void setSessionCode(const std::string& code) = 0;

        /**
         * @brief Get the current session code.
         * @return The session code, or empty string if not set.
         */
        virtual std::string getSessionCode() const = 0;

        /**
         * @brief Enable or disable encryption for data transfer.
         * @param enable true to enable encryption, false to disable.
         */
        virtual void setEncryptionEnabled(bool enable) = 0;

        /**
         * @brief Check if encryption is enabled.
         * @return true if encryption is enabled, false otherwise.
         */
        virtual bool isEncryptionEnabled() const = 0;
        
        /**
         * @brief Set global upload bandwidth limit.
         * @param bytesPerSecond Maximum upload rate in bytes per second (0 = unlimited)
         */
        virtual void setGlobalUploadLimit(std::size_t bytesPerSecond) = 0;

        /**
         * @brief Set global download bandwidth limit.
         * @param bytesPerSecond Maximum download rate in bytes per second (0 = unlimited)
         */
        virtual void setGlobalDownloadLimit(std::size_t bytesPerSecond) = 0;

        /**
         * @brief Get human-readable bandwidth limiter statistics.
         */
        virtual std::string getBandwidthStats() const = 0;
        
        /**
         * @brief Enable or disable TCP relay for NAT traversal.
         * @param enabled true to enable relay, false to disable.
         */
        virtual void setRelayEnabled(bool enabled) = 0;
        
        /**
         * @brief Check if TCP relay is enabled.
         * @return true if relay is enabled, false otherwise.
         */
        virtual bool isRelayEnabled() const = 0;
        
        /**
         * @brief Check if connected to relay server.
         * @return true if connected to relay, false otherwise.
         */
        virtual bool isRelayConnected() const = 0;
        
        /**
         * @brief Get the local peer ID.
         * @return The local peer ID string.
         */
        virtual std::string getLocalPeerId() const = 0;
        
        /**
         * @brief Get the TCP port this peer is listening on.
         * @return The TCP listening port.
         */
        virtual int getLocalPort() const = 0;
        
        /**
         * @brief Connect to a relay server for NAT traversal.
         * @param host The relay server hostname.
         * @param port The relay server port.
         * @param sessionCode The session code for peer discovery.
         * @return true if connected successfully, false otherwise.
         */
        virtual bool connectToRelay(const std::string& host, int port, const std::string& sessionCode) = 0;
        
        /**
         * @brief Disconnect from the relay server.
         */
        virtual void disconnectFromRelay() = 0;
        
        /**
         * @brief Relay peer info structure
         */
        struct RelayPeerInfo {
            std::string id;
            std::string ip;
            int port;
            std::string natType;
            std::string connectedAt;
        };
        
        /**
         * @brief Get list of peers available via relay server.
         * @return Vector of relay peer information.
         */
        virtual std::vector<RelayPeerInfo> getRelayPeers() const = 0;
    };
}




