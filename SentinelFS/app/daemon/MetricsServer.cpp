#include "MetricsServer.h"
#include "Logger.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace SentinelFS {

MetricsServer::MetricsServer(int port)
    : port_(port) {
}

MetricsServer::~MetricsServer() {
    stop();
}

void MetricsServer::setMetricsHandler(MetricsHandler handler) {
    metricsHandler_ = std::move(handler);
}

void MetricsServer::setLivenessHandler(HealthHandler handler) {
    livenessHandler_ = std::move(handler);
}

void MetricsServer::setReadinessHandler(HealthHandler handler) {
    readinessHandler_ = std::move(handler);
}

bool MetricsServer::start() {
    if (running_) return true;
    if (port_ <= 0) return false;

    serverSocket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        Logger::instance().error("MetricsServer: failed to create socket: " + std::string(std::strerror(errno)), "MetricsServer");
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Logger::instance().error("MetricsServer: failed to set socket options: " + std::string(std::strerror(errno)), "MetricsServer");
        ::close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::instance().error("MetricsServer: failed to bind socket: " + std::string(std::strerror(errno)), "MetricsServer");
        ::close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    if (listen(serverSocket_, 16) < 0) {
        Logger::instance().error("MetricsServer: failed to listen: " + std::string(std::strerror(errno)), "MetricsServer");
        ::close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }

    running_ = true;
    serverThread_ = std::thread(&MetricsServer::serverLoop, this);
    Logger::instance().info("MetricsServer listening on port " + std::to_string(port_), "MetricsServer");
    return true;
}

void MetricsServer::stop() {
    if (!running_) return;
    running_ = false;

    if (serverSocket_ >= 0) {
        ::shutdown(serverSocket_, SHUT_RDWR);
        ::close(serverSocket_);
        serverSocket_ = -1;
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void MetricsServer::serverLoop() {
    while (running_) {
        int client = accept(serverSocket_, nullptr, nullptr);
        if (client < 0) {
            if (running_) {
                Logger::instance().error("MetricsServer: accept failed: " + std::string(std::strerror(errno)), "MetricsServer");
            }
            continue;
        }

        handleClient(client);
        ::close(client);
    }
}

void MetricsServer::handleClient(int clientSocket) {
    char buffer[4096];
    ssize_t received = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return;
    }

    buffer[received] = '\0';
    std::istringstream req(buffer);
    std::string method;
    std::string path;
    req >> method >> path;

    if (method != "GET") {
        sendResponse(clientSocket, 405, "Method Not Allowed", "Method Not Allowed\n");
        return;
    }

    if (path == "/metrics") {
        if (!metricsHandler_) {
            sendResponse(clientSocket, 404, "Not Found", "Metrics handler not configured\n");
            return;
        }
        std::string body = metricsHandler_();
        sendResponse(clientSocket, 200, "OK", body, "text/plain; version=0.0.4");
    } else if (path == "/healthz") {
        bool ok = livenessHandler_ ? livenessHandler_() : true;
        sendResponse(clientSocket, ok ? 200 : 503, ok ? "OK" : "Service Unavailable",
                     ok ? "ok\n" : "unhealthy\n");
    } else if (path == "/readyz") {
        bool ok = readinessHandler_ ? readinessHandler_() : true;
        sendResponse(clientSocket, ok ? 200 : 503, ok ? "OK" : "Service Unavailable",
                     ok ? "ready\n" : "not ready\n");
    } else {
        sendResponse(clientSocket, 404, "Not Found", "Unknown endpoint\n");
    }
}

void MetricsServer::sendResponse(int clientSocket,
                                 int statusCode,
                                 const std::string& statusText,
                                 const std::string& body,
                                 const std::string& contentType) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << statusCode << ' ' << statusText << "\r\n"
       << "Content-Type: " << contentType << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << body;

    auto response = ss.str();
    send(clientSocket, response.data(), response.size(), 0);
}

} // namespace SentinelFS
