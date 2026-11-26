#include "time.h"
#include <chrono>

namespace rb {

uint64_t nowMicros() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace rb
