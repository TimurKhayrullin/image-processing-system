// helper functions for message handling relating to the feature extractor

#include "shared.hpp"
#include "message_handling.hpp"
#include "message_headers.hpp"
#include "extractor.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <zmq.hpp>
#include <cstdint>
#include <cstring>
#include <vector>
#include <iostream>

// this function is used to receive image data from the image generator via ZMQ IPS
bool recv_image(zmq::socket_t& socket,
                ImageHeader& out_header,
                std::vector<uint8_t>& image_data)
{
    // declare messages we want to receive
    zmq::message_t header_msg_out;
    zmq::message_t image_msg_out;

    // ---- FRAME 0: HEADER ----
    if (!socket.recv(header_msg_out, zmq::recv_flags::none)) {
        // std::cerr << "[ERROR] Failed to receive header frame.\n";
        return false;
    }

    if (header_msg_out.size() != sizeof(ImageHeader)) {
        std::cerr << "[ERROR] Invalid header size: got "
                  << header_msg_out.size() << " bytes, expected "
                  << sizeof(ImageHeader) << "\n";
        return false;
    }

    // deserialize directly into struct
    std::memcpy(&out_header, header_msg_out.data(), sizeof(ImageHeader));

    // ---- FRAME 1: PIXELS ----
    if (!socket.recv(image_msg_out, zmq::recv_flags::none)) {
        std::cerr << "[ERROR] Failed to receive pixel frame.\n";
        return false;
    }

    if (image_msg_out.size() != out_header.image_size_bytes) {
        std::cerr << "[ERROR] Pixel count mismatch: got "
                  << image_msg_out.size()
                  << ", expected " << out_header.image_size_bytes << "\n";
        return false;
    }

    // Copy image data
    image_data.resize(out_header.image_size_bytes);
    std::memcpy(image_data.data(), image_msg_out.data(), out_header.image_size_bytes);

    return true;
}

// sends image data and keypoints/features to data logger
bool send_image_plus_features(zmq::socket_t& socket, ImageHeader &image_header, std::vector<uint8_t> & image_data, 
                            FeaturesHeader &features_header, std::vector<KeyPointPortable> &keypoints_tosend, std::vector<std::byte> &desc_mat_data)
{
    // send image header
    socket.send(zmq::buffer(&image_header, sizeof(image_header)), zmq::send_flags::sndmore | zmq::send_flags::dontwait);
    // send image pixels
    //socket.send(orig_img_msg, zmq::send_flags::sndmore);
    socket.send(zmq::buffer(image_data.data(), image_header.image_size_bytes), zmq::send_flags::sndmore);
    // send SIFT header
    socket.send(zmq::buffer(&features_header, sizeof(features_header)), zmq::send_flags::sndmore);
    // send SIFT features
    socket.send(zmq::buffer(keypoints_tosend.data(), features_header.keypoints_size_bytes), zmq::send_flags::sndmore); // keypoints array
    socket.send(zmq::buffer(desc_mat_data.data(), features_header.descriptors_size_bytes), zmq::send_flags::none); // descriptors matrix data

    return true;
}

// method for async thread to do extraction work
std::tuple<uint64_t, uint64_t, uint64_t> mt_do_extraction(uint64_t frame_number, 
                                                        zmq::context_t &context,
                                                        const std::string pub_socket_addr,
                                                        ImageHeader image_header,
                                                        std::vector<uint8_t> image_data,
                                                        SIFTParams params, 
                                                        cv::Ptr<cv::SIFT> sift_ptr, 
                                                        FeaturesHeader features_header)
{

    // timestamp init
    uint64_t timestamp_proc_start = 0;
    uint64_t timestamp_proc_end = 0;
    uint64_t num_bytes_sent = 0;

    try {

        // because we are a separate thread from main(), and socket's aren't thread safe, 
        // we need to connect to the publisher once for every extraction job
        zmq::socket_t publisher(context, zmq::socket_type::pub);

        // connect to the IPC socket for processed image output
        publisher.connect(pub_socket_addr);

        // decode image_data into 8-bit cv matrix
        cv::Mat img = cv::imdecode(image_data, cv::IMREAD_COLOR);

        // fill rest of image header with image data now that we can get it from OpenCV
        image_header.width = img.cols;
        image_header.height = img.rows;
        image_header.channels = img.channels();

        // initialize an extraction job
        SIFTExtractionJob extractor(params, sift_ptr);

        // start processing time timestamp collection, and actually extract the features
        timestamp_proc_start = get_timestamp_ns_utc();
        extractor.extract_features(img);

        // serialize keypoints and descriptors to contiguous byte array for sending
        extractor.serialize_features();

        // set header values for features message
        extractor.set_header(features_header);

        // get end of processing time timestamp from features header
        timestamp_proc_end = features_header.timestamp_processed_ns;

        // send original image + feature vector
        send_image_plus_features(
            publisher, 
            image_header,
            image_data, 
            features_header, 
            extractor.serialized_keypoints, 
            extractor.serialized_descriptors
        );
        
        // sum number of bytes from each piece of information we sent
        num_bytes_sent += sizeof(image_header);
        num_bytes_sent += image_header.image_size_bytes;
        num_bytes_sent += sizeof(features_header);
        num_bytes_sent += extractor.serialized_keypoints.size();
        num_bytes_sent += extractor.serialized_descriptors.size();

    }
    catch (const zmq::error_t& e) {
        std::cerr << "[THREAD] ZMQ exception: " << e.what() << std::endl;
    }
    catch (const cv::Exception& e) {
        std::cerr << "[THREAD] OpenCV exception:\n" << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[THREAD] std::exception: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "[THREAD] unknown exception\n";
    }

    // return frame number, time taken for processing and number of bytes sent back to feature extractor's main()
    return std::make_tuple(
        frame_number,
        timestamp_proc_end - timestamp_proc_start,
        num_bytes_sent
    );
}
