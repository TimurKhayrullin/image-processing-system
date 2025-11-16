#include "data_logger.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <zmq.hpp>

bool recv_image_plus_features(
    zmq::socket_t& socket,
    ImageHeader& img_header,
    std::vector<uint8_t>& pixels,
    SIFTHeader& sift_header_out,
    std::vector<KeyPointPortable>& keypoints,
    std::vector<uint8_t>& desc_mat
)
{
    // -------------------------------------------------------
    // FRAME 0: Image Header  (zero-copy)
    // -------------------------------------------------------
    zmq::message_t img_header_msg;
    if (!socket.recv(img_header_msg, zmq::recv_flags::none))
        return false;

    if (img_header_msg.size() != sizeof(ImageHeader))
        throw std::runtime_error("Invalid image header size");

    // decode image header
    std::memcpy(&img_header, img_header_msg.data(), sizeof(ImageHeader));


    // -------------------------------------------------------
    // FRAME 1: Image Pixels (zero-copy)
    // -------------------------------------------------------
    zmq::message_t pixels_msg;
    if (!socket.recv(pixels_msg, zmq::recv_flags::none))
        return false;

    // Caller knows pixel_count from ImageHeader.
    pixels.resize(pixels_msg.size());
    std::memcpy(pixels.data(),
                pixels_msg.data(),
                pixels_msg.size());

    // -------------------------------------------------------
    // FRAME 2: SIFTHeader
    // -------------------------------------------------------
    zmq::message_t sift_header_msg;
    if (!socket.recv(sift_header_msg, zmq::recv_flags::none))
        return false;

    if (sift_header_msg.size() != sizeof(SIFTHeader))
        throw std::runtime_error("Invalid SIFTHeader size");

    std::memcpy(&sift_header_out, sift_header_msg.data(), sizeof(SIFTHeader));


    // -------------------------------------------------------
    // FRAME 3: Keypoints array (POD)
    // -------------------------------------------------------
    zmq::message_t keypoints_msg;
    if (!socket.recv(keypoints_msg, zmq::recv_flags::none))
        return false;

    const size_t kpt_count =
        keypoints_msg.size() / sizeof(KeyPointPortable);

    if (kpt_count != sift_header_out.keypoint_count)
        throw std::runtime_error("Keypoint count mismatch");

    keypoints.resize(kpt_count);
    std::memcpy(keypoints.data(),
                keypoints_msg.data(),
                keypoints_msg.size());


    // -------------------------------------------------------
    // FRAME 4: Descriptor matrix (uint8 blob)
    // -------------------------------------------------------
    zmq::message_t desc_msg;
    if (!socket.recv(desc_msg, zmq::recv_flags::none))
        return false;

    desc_mat.resize(desc_msg.size());
    std::memcpy(desc_mat.data(),
                desc_msg.data(),
                desc_msg.size());

    return true;
}
