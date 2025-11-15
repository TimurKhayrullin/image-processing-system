#pragma once
#include <string>
#include <iostream>
#include <chrono>
#include <cstdint>

inline void print_banner(const std::string& name) {
    std::cout << "=== " << name << " ===" << std::endl;
}

inline uint64_t get_timestamp_ns_utc()
{
    const uint64_t timestamp_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    return timestamp_ns;
}

