#pragma once
#include "message_headers.hpp"
#include "extractor.hpp"
#include "feature_serialization.hpp"
#include <opencv2/opencv.hpp>
#include <string>
#include <zmq.hpp>

// function for receiving multipart zmq messages in the form [ImageHeader][pixel array]
bool recv_image(zmq::socket_t& socket,
                ImageHeader& out_header,
                std::vector<std::byte>& out_pixels);

// function for receiving images as [ImageHeader][pixel array] multipart messages
// and immediately constructing a cv::Mat using it. 
bool recv_image_as_mat( zmq::socket_t& socket,
                        zmq::message_t& header_msg_out,
                        zmq::message_t& pixels_msg_out,
                        ImageHeader& out_header,
                        cv::Mat& out_img );

bool send_image_plus_features(zmq::socket_t& socket, zmq::message_t &img_header_msg, zmq::message_t &pixels_msg,
                            FeaturesHeader &sift_header, std::vector<KeyPointPortable> &keypoints_tosend, std::vector<std::byte> &desc_mat_data);

// method for thread to do extraction work
std::tuple<uint64_t, uint64_t, uint64_t> mt_do_extraction(uint64_t frame_number, zmq::context_t &context, zmq::message_t header_msg, zmq::message_t pixels_msg, SIFTParams params, cv::Ptr<cv::SIFT> sift_ptr, FeaturesHeader features_header, cv::Mat img);