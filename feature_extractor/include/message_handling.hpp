#pragma once
#include "message_headers.hpp"
#include "extractor.hpp"
#include "feature_serialization.hpp"
#include <opencv2/opencv.hpp>
#include <string>
#include <zmq.hpp>
#include <cstdint>
#include <vector>

// function for receiving multipart zmq messages in the form [ImageHeader][pixel array]
bool recv_image(zmq::socket_t& socket,
                ImageHeader& out_header,
                std::vector<uint8_t>& image_data);

bool send_image_plus_features(zmq::socket_t& socket, ImageHeader &image_header, std::vector<uint8_t> & image_data, 
                            FeaturesHeader &features_header, std::vector<KeyPointPortable> &keypoints_tosend, std::vector<std::byte> &desc_mat_data);

struct ExtractedPayload {
    ImageHeader image_header;
    std::vector<uint8_t> image_data;
    FeaturesHeader features_header;
    std::vector<KeyPointPortable> keypoints;
    std::vector<std::byte> descriptors;
    uint64_t proc_time_ns = 0;
    uint64_t num_bytes = 0;
};

// method for thread to do extraction work
ExtractedPayload mt_do_extraction(ImageHeader image_header,
                                 std::vector<uint8_t> image_data,
                                 SIFTParams params,
                                 cv::Ptr<cv::SIFT> sift_ptr,
                                 FeaturesHeader features_header);
