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
    };

}
