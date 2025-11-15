//declares an ImageMessage class to send image messages in a standard format
#pragma once
#include <vector>

// ### Struct for packaging image messages before sending them over IPC. 
// we use pragma pack(push, 1) to line up the struct members contiguously in memory, without padding.
// This allows us to send the struct using a binary protocol (fast)
#pragma pack(push, 1)
struct ImageHeader {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint32_t pixel_format;      // e.g. RGB8, GRAY8
    uint64_t frame_number;
    uint64_t timestamp_ns;
    uint64_t pixel_count;       // number of bytes following the header
};
#pragma pack(pop)