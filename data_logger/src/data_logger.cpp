#include "data_logger.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <zmq.hpp>
#include <iostream>

bool recv_payload(
    zmq::socket_t& socket,
    Payload &payload
)
{
    // -------------------------------------------------------
    // FRAME 0: Image Header  (zero-copy)
    // -------------------------------------------------------
    zmq::message_t img_header_msg;
    if (!socket.recv(img_header_msg, zmq::recv_flags::none)){

        std::cout << "nothing\n";
        return false;

    }

    if (img_header_msg.size() != sizeof(ImageHeader))
        throw std::runtime_error("Invalid image header size");

    // decode image header
    std::memcpy(&payload.image_header, img_header_msg.data(), sizeof(ImageHeader));


    // -------------------------------------------------------
    // FRAME 1: Image Pixels (zero-copy)
    // -------------------------------------------------------
    zmq::message_t pixels_msg;
    if (!socket.recv(pixels_msg, zmq::recv_flags::none))
        return false;

    // Caller knows image_size_bytes from ImageHeader.
    payload.pixels.resize(pixels_msg.size());
    std::memcpy(payload.pixels.data(),
                pixels_msg.data(),
                pixels_msg.size());

    // -------------------------------------------------------
    // FRAME 2: SIFTHeader
    // -------------------------------------------------------
    zmq::message_t sift_header_msg;
    if (!socket.recv(sift_header_msg, zmq::recv_flags::none))
        return false;

    if (sift_header_msg.size() != sizeof(FeaturesHeader))
        throw std::runtime_error("Invalid SIFTHeader size");

    std::memcpy(&payload.sift_header, sift_header_msg.data(), sizeof(FeaturesHeader));


    // -------------------------------------------------------
    // FRAME 3: Keypoints array (POD)
    // -------------------------------------------------------
    zmq::message_t keypoints_msg;
    if (!socket.recv(keypoints_msg, zmq::recv_flags::none))
        return false;

    const size_t kpt_count =
        keypoints_msg.size() / sizeof(KeyPointPortable);

    if (kpt_count != payload.sift_header.keypoint_count)
        throw std::runtime_error("Keypoint count mismatch");

    payload.keypoints.resize(kpt_count);
    std::memcpy(payload.keypoints.data(),
                keypoints_msg.data(),
                keypoints_msg.size());


    // -------------------------------------------------------
    // FRAME 4: Descriptor matrix (uint8 blob)
    // -------------------------------------------------------
    zmq::message_t desc_msg;
    if (!socket.recv(desc_msg, zmq::recv_flags::none))
        return false;

    payload.desc_mat.resize(desc_msg.size());
    std::memcpy(payload.desc_mat.data(),
                desc_msg.data(),
                desc_msg.size());

    return true;
}
