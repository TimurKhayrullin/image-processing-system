// Utility functions for serialization/deserialization of keypoints vector from OpenCV
#pragma once
#include <cstdint>
#include <vector>
#include <opencv2/features2d.hpp>

#pragma pack(push, 1)
struct KeyPointPortable {
    float x;
    float y;
    float size;
    float angle;
    float response;
    int32_t octave;
    int32_t class_id;
};
#pragma pack(pop)

inline std::vector<KeyPointPortable>
serialize_keypoints(const std::vector<cv::KeyPoint>& keypoints)
{
    std::vector<KeyPointPortable> out;
    out.reserve(keypoints.size());

    for (const auto& kp : keypoints) {
        out.push_back({
            kp.pt.x,
            kp.pt.y,
            kp.size,
            kp.angle,
            kp.response,
            kp.octave,
            kp.class_id
        });
    }

    return out;
}

inline std::vector<cv::KeyPoint>
deserialize_keypoints(const KeyPointPortable* raw, size_t count)
{
    std::vector<cv::KeyPoint> keypoints;
    keypoints.reserve(count);

    for (size_t i = 0; i < count; i++) {
        const auto& k = raw[i];
        keypoints.emplace_back(
            cv::Point2f{k.x, k.y},
            k.size,
            k.angle,
            k.response,
            k.octave,
            k.class_id
        );
    }

    return keypoints;
}

inline std::vector<uint8_t>
serialize_descriptors(const cv::Mat& descriptors)
{
    const size_t bytes = descriptors.total() * descriptors.elemSize();
    std::vector<uint8_t> out(bytes);

    std::memcpy(out.data(), descriptors.data, bytes);
    return out;
}

inline cv::Mat deserialize_descriptors(
    const uint8_t* raw,
    uint32_t descriptor_count,
    uint32_t descriptor_dim,
    int cv_type
) {
    cv::Mat desc(descriptor_count, descriptor_dim, cv_type);
    const size_t bytes = desc.total() * desc.elemSize();

    std::memcpy(desc.data, raw, bytes);
    return desc;
}

