#include "delta_types.h"
#include <sstream>
#include <iomanip>

namespace sfs {
namespace core {

std::string StrongHash::to_hex() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

} // namespace core
} // namespace sfs
