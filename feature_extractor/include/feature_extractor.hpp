#pragma once
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <opencv2/opencv.hpp>
#include <string>
#include <zmq.hpp>

// function for receiving multipart zmq messages in the form [ImageHeader][pixel array]
bool recv_image(zmq::socket_t& socket,
                ImageHeader& out_header,
                std::vector<uint8_t>& out_pixels);

// function for receiving images as [ImageHeader][pixel array] multipart messages
// and immediately constructing a cv::Mat using it. 
bool recv_image_as_mat( zmq::socket_t& socket,
                        zmq::message_t& header_msg_out,
                        zmq::message_t& pixels_msg_out,
                        ImageHeader& out_header,
                        cv::Mat& out_img );

bool send_image_plus_features(zmq::socket_t& socket, zmq::message_t &img_header_msg, zmq::message_t &pixels_msg,
                            SIFTHeader &sift_header, std::vector<KeyPointPortable> &keypoints_tosend, std::vector<uint8_t> &desc_mat_data);
