//declares structs to send image and keypoint/feature messages in a standard format
#pragma once
#include <vector>
#include <cstdint>

// we use pragma pack(push, 1) to line up the struct members contiguously in memory, without padding.
// This allows us to send the struct using a binary protocol (fast)
#pragma pack(push, 1)
// struct for packaging image messages before sending them over IPC. 
struct ImageHeader {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    char format_fourcc[4];      // four character code for image format (eg "PNG\0")
    uint64_t frame_number;
    uint64_t timestamp_ns;
    uint64_t image_size_bytes;
};

// struct for specifying which parameters were used when running SIFT keypoint detection + feature extraction
struct SIFTParams {
    uint32_t n_features;
    uint32_t n_octave_layers;
    double   contrast_threshold;
    double   edge_threshold;
    double   sigma;
    uint32_t descriptor_type;
    bool     enable_percise_upscale;
}; 

// struct for packaging keypoint + feature messages to send them over IPC
struct FeaturesHeader {
    
    SIFTParams params;

    uint64_t frame_number;
    uint64_t timestamp_received_ns;
    uint64_t timestamp_processed_ns;

    uint32_t keypoint_count;
    uint32_t descriptor_count;
    uint32_t descriptor_dim;

    uint64_t keypoints_size_bytes;
    uint64_t descriptors_size_bytes;

};

#pragma pack(pop)