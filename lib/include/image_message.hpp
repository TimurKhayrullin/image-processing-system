//declares an ImageMessage class to send image messages in a standard format
#include <vector>

struct ImageHeader {
    uint32_t width;
    uint32_t height;
    // uint32_t channels; // not needed since we can infer number of channels from pixel format
    uint32_t pixel_format;     // e.g. RGB8 or GRAY8
    uint64_t frame_number;
    uint64_t timestamp_ns;
};

class ImageMessage {
public:
    ImageHeader header;
    std::vector<uint8_t> pixels;
};