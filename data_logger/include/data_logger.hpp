#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <vector>
#include <zmq.hpp>

bool recv_image_plus_features(
    zmq::socket_t& socket,
    ImageHeader& img_header,
    std::vector<uint8_t>& pixels,
    SIFTHeader& sift_header_out,
    std::vector<KeyPointPortable>& keypoints,
    std::vector<uint8_t>& desc_mat
);