#pragma once
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <vector>
#include <zmq.hpp>

struct Payload{
    ImageHeader image_header;
    std::vector<uint8_t> pixels;
    FeaturesHeader sift_header;
    std::vector<KeyPointPortable> keypoints;
    std::vector<uint8_t> desc_mat;
};

bool recv_payload(
    zmq::socket_t& socket,
    Payload &payload
);