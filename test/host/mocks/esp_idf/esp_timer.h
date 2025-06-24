// Mock esp_timer.h for host testing
#pragma once

#include <stdint.h>
#include <chrono>

// Mock timer functions using std::chrono
inline int64_t esp_timer_get_time() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}