#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include "message_handling.hpp"
#include "extractor.hpp"
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
    
    // Create Processor object
    SIFTExtractor extractor = SIFTExtractor("configs/feature_extractor/SIFT_params.yml");

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

    // start receiving images from image generator
    try {
        while (ShutdownHandler::running()) {
            
            // prepare image + features payload
            FeaturesHeader features_header;
            ImageHeader img_header;
            cv::Mat img;
            zmq::message_t header_msg;
            zmq::message_t pixels_msg;

            // recieve image, preserve messages for sending to data logger
            if (!recv_image_as_mat(subscriber, header_msg, pixels_msg, img_header, img)){
                continue;
            }

            // mark processing payload with timestamp, and set frame number 
            features_header.timestamp_received_ns = get_timestamp_ns_utc();
            features_header.frame_number = img_header.frame_number;

            std::cout << "Recieved image #" << img_header.frame_number << " w:" << img.cols
                     << " h:" << img.rows << " c:" << img.channels() << " type:" << img.type() << std::endl;

            // correct bit depth of image for processing
            if(img.type() == 18){
                img.convertTo(img, CV_8U, 1.0 / 256.0);
            }

            // process image
            extractor.extract_features(img);
            
            std::cout << "Processed Image #" << img_header.frame_number << std::endl;
            
            // serialize keypoints and descriptors to contiguous byte array for sending
            extractor.serialize_features();

            // set header values for features message
            extractor.set_header(features_header);

            // send original image + feature vector
            send_image_plus_features(
                publisher, 
                header_msg, 
                pixels_msg, 
                features_header, 
                extractor.serialized_keypoints, 
                extractor.serialized_descriptors
            );
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

