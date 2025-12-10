#include "EventHandlers.h"
#include "MockPlugins.h"
#include "EventBus.h"
#include <iostream>
#include <cassert>

using namespace SentinelFS;

void test_event_handlers_creation() {
    std::cout << "Running test_event_handlers_creation..." << std::endl;
    
    EventBus eventBus;
    MockNetwork network;
    MockStorage storage;
    MockFile file;
    std::string watchDir = "/tmp/test_watch";
    
    EventHandlers handlers(eventBus, &network, &storage, &file, watchDir);
    
    // Test if handlers are registered (conceptually)
    // We can't easily check internal state of EventBus without exposing it,
    // but we can verify no crash on creation.
    
    std::cout << "test_event_handlers_creation passed." << std::endl;
}

int main() {
    try {
        test_event_handlers_creation();
        std::cout << "All EventHandlers tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
