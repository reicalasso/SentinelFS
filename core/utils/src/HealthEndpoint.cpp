#include "HealthEndpoint.h"
#include "Logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

namespace sfs {

HealthEndpoint::HealthEndpoint(int port) : port_(port) {}

HealthEndpoint::~HealthEndpoint() {
    stop();
}

void HealthEndpoint::setHealthCollector(HealthCollector collector) {
    std::lock_guard<std::mutex> lock(mutex_);
    healthCollector_ = std::move(collector);
}

void HealthEndpoint::setMetricsCollector(MetricsCollector collector) {
    std::lock_guard<std::mutex> lock(mutex_);
    metricsCollector_ = std::move(collector);
}

void HealthEndpoint::setReady(bool ready) {
    ready_ = ready;
}

bool HealthEndpoint::start() {
    auto& logger = SentinelFS::Logger::instance();
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        logger.error("Failed to create health endpoint socket", "HealthEndpoint");
        return false;
    }
    
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.error("Failed to bind health endpoint to port " + std::to_string(port_), "HealthEndpoint");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    if (listen(serverSocket_, 5) < 0) {
        logger.error("Failed to listen on health endpoint", "HealthEndpoint");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    running_ = true;
    serverThread_ = std::thread(&HealthEndpoint::serverLoop, this);
    
    logger.info("Health endpoint started on port " + std::to_string(port_), "HealthEndpoint");
    return true;
}

void HealthEndpoint::stop() {
    running_ = false;
    
    if (serverSocket_ >= 0) {
        shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    SentinelFS::Logger::instance().info("Health endpoint stopped", "HealthEndpoint");
}

bool HealthEndpoint::isRunning() const {
    return running_;
}

void HealthEndpoint::serverLoop() {
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(serverSocket_ + 1, &readfds, nullptr, nullptr, &tv);
        if (activity <= 0) continue;
        
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &len);
        
        if (clientSocket >= 0) {
            handleClient(clientSocket);
            close(clientSocket);
        }
    }
}

void HealthEndpoint::handleClient(int clientSocket) {
    char buffer[1024];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) return;
    buffer[bytesRead] = '\0';
    
    std::string request(buffer);
    std::string response;
    std::string body;
    std::string contentType = "application/json";
    int statusCode = 200;
    
    if (request.find("GET /health") != std::string::npos) {
        body = buildHealthResponse();
    } else if (request.find("GET /metrics") != std::string::npos) {
        body = buildMetricsResponse();
        contentType = "text/plain; charset=utf-8";
    } else if (request.find("GET /ready") != std::string::npos) {
        if (ready_) {
            body = R"({"ready": true})";
        } else {
            statusCode = 503;
            body = R"({"ready": false})";
        }
    } else if (request.find("GET /live") != std::string::npos) {
        body = R"({"alive": true})";
    } else {
        statusCode = 404;
        body = R"({"error": "Not found"})";
    }
    
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " ";
    switch (statusCode) {
        case 200: oss << "OK"; break;
        case 404: oss << "Not Found"; break;
        case 503: oss << "Service Unavailable"; break;
        default: oss << "Unknown"; break;
    }
    oss << "\r\n";
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    
    response = oss.str();
    send(clientSocket, response.c_str(), response.size(), 0);
}

std::string HealthEndpoint::buildHealthResponse() {
    std::vector<HealthCheck> checks;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (healthCollector_) {
            checks = healthCollector_();
        }
    }
    
    bool allHealthy = true;
    bool anyUnhealthy = false;
    
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"status\": ";
    
    for (const auto& check : checks) {
        if (check.status != HealthStatus::Healthy) allHealthy = false;
        if (check.status == HealthStatus::Unhealthy) anyUnhealthy = true;
    }
    
    if (anyUnhealthy) {
        oss << "\"unhealthy\"";
    } else if (!allHealthy) {
        oss << "\"degraded\"";
    } else {
        oss << "\"healthy\"";
    }
    
    oss << ",\n  \"checks\": [\n";
    
    for (size_t i = 0; i < checks.size(); ++i) {
        const auto& check = checks[i];
        oss << "    {\n";
        oss << "      \"name\": \"" << check.name << "\",\n";
        oss << "      \"status\": \"" << healthStatusToString(check.status) << "\"";
        if (!check.message.empty()) {
            oss << ",\n      \"message\": \"" << check.message << "\"";
        }
        oss << "\n    }";
        if (i < checks.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ]\n";
    oss << "}";
    
    return oss.str();
}

std::string HealthEndpoint::buildMetricsResponse() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (metricsCollector_) {
        return metricsCollector_();
    }
    
    // Default metrics
    std::ostringstream oss;
    oss << "# HELP sentinel_up Whether SentinelFS daemon is up\n";
    oss << "# TYPE sentinel_up gauge\n";
    oss << "sentinel_up 1\n";
    oss << "# HELP sentinel_ready Whether SentinelFS daemon is ready\n";
    oss << "# TYPE sentinel_ready gauge\n";
    oss << "sentinel_ready " << (ready_ ? 1 : 0) << "\n";
    
    return oss.str();
}

} // namespace sfs
