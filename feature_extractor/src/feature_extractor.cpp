#include "feature_extractor.hpp"
#include "message_headers.hpp"
#include <opencv2/opencv.hpp>
#include <zmq.hpp>
#include <cstdint>
#include <cstring>
#include <vector>
#include <iostream>

bool recv_image(zmq::socket_t& socket,
                ImageHeader& out_header,
                std::vector<uint8_t>& out_pixels)
{
    zmq::message_t header_msg;
    zmq::message_t pixel_msg;

    // ---- FRAME 0: HEADER ----
    if (!socket.recv(header_msg, zmq::recv_flags::none)) {
        std::cerr << "[ERROR] Failed to receive header frame.\n";
        return false;
    }

    if (header_msg.size() != sizeof(ImageHeader)) {
        std::cerr << "[ERROR] Invalid header size: got "
                  << header_msg.size() << " bytes, expected "
                  << sizeof(ImageHeader) << "\n";
        return false;
    }

    // deserialize directly into struct (safe, correctly packed)
    std::memcpy(&out_header, header_msg.data(), sizeof(ImageHeader));

    // ---- FRAME 1: PIXELS ----
    if (!socket.recv(pixel_msg, zmq::recv_flags::none)) {
        std::cerr << "[ERROR] Failed to receive pixel frame.\n";
        return false;
    }

    if (pixel_msg.size() != out_header.image_size_bytes) {
        std::cerr << "[ERROR] Pixel count mismatch: got "
                  << pixel_msg.size()
                  << ", expected " << out_header.image_size_bytes << "\n";
        return false;
    }

    // Copy pixel data
    out_pixels.resize(out_header.image_size_bytes);
    std::memcpy(out_pixels.data(), pixel_msg.data(), out_header.image_size_bytes);

    return true;
}

bool recv_image_as_mat( zmq::socket_t& socket,
                        zmq::message_t& header_msg_out,
                        zmq::message_t& pixels_msg_out,
                        ImageHeader& out_header,
                        cv::Mat& out_img )
{
    // ---- FRAME 0: HEADER ----
    if (!socket.recv(header_msg_out, zmq::recv_flags::none))
        return false;

    if (header_msg_out.size() != sizeof(ImageHeader))
        return false;

    std::memcpy(&out_header, header_msg_out.data(), sizeof(ImageHeader));

    // ---- FRAME 1: PIXELS ----
    if (!socket.recv(pixels_msg_out, zmq::recv_flags::none))
        return false;

    if (pixels_msg_out.size() != out_header.image_size_bytes)
        return false;

    int cv_type = out_header.pixel_format; // 16 or 18

    int channels = CV_MAT_CN(cv_type);
    int depth    = CV_MAT_DEPTH(cv_type);

    int bytes_per_channel = 0;
    switch (depth) {
        case CV_8U:  bytes_per_channel = 1; break;
        case CV_16U: bytes_per_channel = 2; break;
        // add others if needed
        default:
            std::cerr << "Unsupported depth: " << depth << "\n";
            std::exit(1);
    }

    size_t expected_bytes = static_cast<size_t>(out_header.width) *
                            static_cast<size_t>(out_header.height) *
                            static_cast<size_t>(channels) *
                            static_cast<size_t>(bytes_per_channel);

    if (pixels_msg_out.size() < expected_bytes) {
        std::cerr << "[ERROR] Pixel buffer too small: have "
                << pixels_msg_out.size() << " bytes, expected at least "
                << expected_bytes << " (w=" << out_header.width
                << ", h=" << out_header.height
                << ", channels=" << channels
                << ", bpc=" << bytes_per_channel << ")\n";
        std::exit(1);
    }

    // Wrap *without copying* using the ZMQ pixel buffer

    //std::cout << "received image of size " << out_header.image_size_bytes << " bytes\n";

    cv::Mat wrapped(out_header.height,
                    out_header.width,
                    out_header.pixel_format,
                    pixels_msg_out.data());
    
    //std::cout << "mat created\n";

    // Clone so the cv::Mat owns its own memory
    out_img = wrapped.clone();

    //std::cout << "mat cloned\n";

    return true;
}


bool send_image_plus_features(zmq::socket_t& socket, zmq::message_t &img_header_msg, zmq::message_t &pixels_msg,
                            SIFTHeader &sift_header, std::vector<KeyPointPortable> &keypoints_tosend, std::vector<uint8_t> &desc_mat_data)
{
    // send image header
    socket.send(img_header_msg, zmq::send_flags::sndmore);
    // send image pixels
    socket.send(pixels_msg, zmq::send_flags::sndmore);
    // send SIFT header
    socket.send(zmq::buffer(&sift_header, sizeof(sift_header)), zmq::send_flags::sndmore);
    // send SIFT features
    socket.send(zmq::buffer(keypoints_tosend.data(), sift_header.keypoints_size_bytes), zmq::send_flags::sndmore); // keypoints array
    socket.send(zmq::buffer(desc_mat_data.data(), sift_header.descriptors_size_bytes), zmq::send_flags::none); // descriptors matrix data

    return true;
}
