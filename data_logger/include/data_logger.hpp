// headers for helper logic pertaining to data logger

#pragma once
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <vector>
#include <zmq.hpp>


// payload is a struct to hold all the pieces of a message received from the feature extractor
struct Payload{
    ImageHeader image_header;
    std::vector<std::byte> pixels;
    FeaturesHeader sift_header;
    std::vector<KeyPointPortable> keypoints;
    std::vector<std::byte> desc_mat;
};

// receives message from ZMQ and decodes the bytes into a payload struct
bool recv_payload(
    zmq::socket_t& socket,
    Payload &payload
);