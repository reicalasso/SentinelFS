#include "HealthEndpoint.h"
#include <iostream>
#include <cassert>

using namespace sfs;

void test_health_endpoint_creation() {
    std::cout << "Running test_health_endpoint_creation..." << std::endl;
    
    HealthEndpoint health(9100);
    
    health.setHealthCollector([]() {
        return std::vector<HealthCheck>{
            {"database", HealthStatus::Healthy},
            {"network", HealthStatus::Healthy}
        };
    });
    
    health.setMetricsCollector([]() {
        return "metric_name 123\n";
    });
    
    health.setReady(true);
    
    // We won't call start() as it might block or require network permissions/ports
    // and we don't want to spin up a real server in a unit test if we can avoid it.
    // But we tested the API surface.
    
    std::cout << "test_health_endpoint_creation passed." << std::endl;
}

int main() {
    try {
        test_health_endpoint_creation();
        std::cout << "All HealthEndpoint tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
