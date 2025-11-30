#pragma once

/**
 * @file HealthEndpoint.h
 * @brief HTTP health check endpoint for SentinelFS daemon
 * 
 * Provides /health and /metrics endpoints for monitoring.
 */

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <vector>

namespace sfs {

/**
 * @brief Health status of a component
 */
enum class HealthStatus {
    Healthy,
    Degraded,
    Unhealthy
};

/**
 * @brief Health check result for a component
 */
struct HealthCheck {
    std::string name;
    HealthStatus status;
    std::string message;
    
    HealthCheck(std::string n, HealthStatus s, std::string msg = "")
        : name(std::move(n)), status(s), message(std::move(msg)) {}
};

/**
 * @brief Callback to collect health checks
 */
using HealthCollector = std::function<std::vector<HealthCheck>()>;

/**
 * @brief Callback to collect metrics in Prometheus format
 */
using MetricsCollector = std::function<std::string()>;

/**
 * @brief Simple HTTP health endpoint server
 * 
 * Endpoints:
 * - GET /health - Returns JSON health status
 * - GET /metrics - Returns Prometheus-format metrics
 * - GET /ready - Returns 200 if ready, 503 if not
 * - GET /live - Returns 200 if alive
 * 
 * Usage:
 * @code
 * HealthEndpoint health(9100);
 * health.setHealthCollector([]() {
 *     return std::vector<HealthCheck>{
 *         {"database", HealthStatus::Healthy},
 *         {"network", HealthStatus::Healthy}
 *     };
 * });
 * health.start();
 * @endcode
 */
class HealthEndpoint {
public:
    explicit HealthEndpoint(int port = 9100);
    ~HealthEndpoint();
    
    // Non-copyable
    HealthEndpoint(const HealthEndpoint&) = delete;
    HealthEndpoint& operator=(const HealthEndpoint&) = delete;
    
    /**
     * @brief Set the health check collector
     */
    void setHealthCollector(HealthCollector collector);
    
    /**
     * @brief Set the metrics collector
     */
    void setMetricsCollector(MetricsCollector collector);
    
    /**
     * @brief Set ready state
     */
    void setReady(bool ready);
    
    /**
     * @brief Start the HTTP server
     */
    bool start();
    
    /**
     * @brief Stop the HTTP server
     */
    void stop();
    
    /**
     * @brief Check if server is running
     */
    bool isRunning() const;
    
    /**
     * @brief Get the port number
     */
    int port() const { return port_; }

private:
    void serverLoop();
    void handleClient(int clientSocket);
    std::string buildHealthResponse();
    std::string buildMetricsResponse();
    
    int port_;
    int serverSocket_{-1};
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::thread serverThread_;
    
    HealthCollector healthCollector_;
    MetricsCollector metricsCollector_;
    mutable std::mutex mutex_;
};

/**
 * @brief Convert HealthStatus to string
 */
inline const char* healthStatusToString(HealthStatus status) {
    switch (status) {
        case HealthStatus::Healthy: return "healthy";
        case HealthStatus::Degraded: return "degraded";
        case HealthStatus::Unhealthy: return "unhealthy";
        default: return "unknown";
    }
}

} // namespace sfs
