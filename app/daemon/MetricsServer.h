#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace SentinelFS {

class MetricsServer {
public:
    using MetricsHandler = std::function<std::string()>;
    using HealthHandler = std::function<bool()>;

    explicit MetricsServer(int port);
    ~MetricsServer();

    MetricsServer(const MetricsServer&) = delete;
    MetricsServer& operator=(const MetricsServer&) = delete;

    void setMetricsHandler(MetricsHandler handler);
    void setLivenessHandler(HealthHandler handler);
    void setReadinessHandler(HealthHandler handler);

    bool start();
    void stop();

private:
    void serverLoop();
    void handleClient(int clientSocket);
    void sendResponse(int clientSocket,
                      int statusCode,
                      const std::string& statusText,
                      const std::string& body,
                      const std::string& contentType = "text/plain; charset=utf-8");

    int port_;
    int serverSocket_{-1};
    std::atomic<bool> running_{false};
    std::thread serverThread_;

    MetricsHandler metricsHandler_;
    HealthHandler livenessHandler_;
    HealthHandler readinessHandler_;
};

} // namespace SentinelFS
