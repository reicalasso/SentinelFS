#pragma once

#include "IPlugin.h"
#include <string>
#include <vector>
#include <cstdint>

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
    };
}


