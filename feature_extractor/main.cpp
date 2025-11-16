#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include "feature_extractor.hpp"
#include <opencv2/features2d.hpp>
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <algorithm> // For std::transform
#include <cctype>    // For std::toupper
#include <zmq.hpp>

int main() {
    print_banner("Feature Extractor Started");

    // <--- install signal handlers for shutdown
    ShutdownHandler::init();     

    // Create a ZeroMQ context and subscriber socket
    zmq::context_t ctx{1};
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);
    zmq::socket_t publisher(ctx, zmq::socket_type::pub);

    // Connect to the same IPC socket the image generator bound to
    subscriber.connect("ipc:///tmp/camera_pub.sock");

    // Bind to the IPC socket for processed image output
    publisher.bind("ipc:///tmp/features_pub.sock");

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");

    std::cout << "Listening for messages on ipc:///tmp/camera_pub.sock ..." << std::endl;

    try {
        while (ShutdownHandler::running()) {
            
            // recieve image, preserve messages for sending to data logger
            ImageHeader img_header;
            cv::Mat img;
            zmq::message_t header_msg;
            zmq::message_t pixels_msg;
            if (!recv_image_as_mat(subscriber, header_msg, pixels_msg, img_header, img)){
                continue;
            }

            std::cout << "Recieved image #" << img_header.frame_number << " w:" << img.cols << " h:" << img.rows << " c:" << img.channels() << std::endl;

            // process image
            SIFTHeader sift_header;
            cv::Ptr<cv::SIFT> siftPtr = cv::SIFT::create();
            std::vector<cv::KeyPoint> keypoints;
            cv::Mat descriptors;
            siftPtr->detectAndCompute(img, cv::noArray(), keypoints, descriptors);

            std::cout << "Processed Image #" << img_header.frame_number << ", got " << keypoints.size() << " keypoints" << std::endl;
            
            std::vector<KeyPointPortable> keypoints_tosend = serialize_keypoints(keypoints);
            std::vector<uint8_t> desc_mat_data = serialize_descriptors(descriptors);

            // initialize header for sift feature message
            sift_header.params.n_features         = 0;
            sift_header.params.n_octave_layers    = siftPtr->getNOctaveLayers();
            sift_header.params.contrast_threshold = siftPtr->getContrastThreshold();
            sift_header.params.edge_threshold     = siftPtr->getEdgeThreshold();
            sift_header.params.sigma              = siftPtr->getSigma();    
            sift_header.timestamp_ns              = get_timestamp_ns_utc();
            sift_header.frame_number              = img_header.frame_number;
            sift_header.keypoint_count            = keypoints.size();
            sift_header.descriptor_count          = keypoints.size();
            sift_header.descriptor_dim            = siftPtr->descriptorSize();
            sift_header.descriptor_type           = descriptors.type();
            sift_header.descriptors_size_bytes    = desc_mat_data.size();

            // send original image + feature vector
            send_image_plus_features(publisher, header_msg, pixels_msg, sift_header, keypoints_tosend, desc_mat_data);
        }
    }
    catch (const zmq::error_t& e) {
        if (!ShutdownHandler::running()) {
            // Interrupted by shutdown — normal exit
        } else {
            std::cerr << "ZMQ error: " << e.what() << std::endl;
        }
    }

    print_banner("Feature Extractor Terminated");
    return 0;
}

