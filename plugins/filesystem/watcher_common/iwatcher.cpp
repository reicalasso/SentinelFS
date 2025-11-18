#include "iwatcher.h"
#include <chrono>

namespace sfs {
namespace plugins {

uint64_t FsEvent::current_time_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

} // namespace plugins
} // namespace sfs
