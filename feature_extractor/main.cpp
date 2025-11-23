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
#include <numeric>
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

    // Bind to the same IPC socket the image generator bound to
    subscriber.bind("ipc:///tmp/camera_pub.sock");

    // connect to the IPC socket for processed image output
    publisher.connect("ipc:///tmp/features_pub.sock");

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");

    std::cout << "Listening for messages on ipc:///tmp/camera_pub.sock ..." << std::endl;

    // keeps track of timestamps for throughput monitoring
    uint64_t frames_since_last_report = 0;
    uint64_t one_second = 1e9;
    uint64_t local_timestamp_last_report = get_timestamp_ns_utc();
    uint64_t local_timestamp_payload_sent = 0;
    uint64_t total_bytes = 0;

    // averaging calculations
    uint64_t frame_count = 0;
    uint64_t frame_limit = 100;
    uint64_t local_timestamp_first_send = 0;
    uint64_t local_timestamp_latest_send = 0;
    uint64_t local_timestamp_proc_start = 0;
    uint64_t local_timestamp_proc_end = 0;
    uint64_t proc_time = 0;
    std::vector<uint64_t> proc_times(frame_limit);

    // start receiving images from image generator
    try {
        while (ShutdownHandler::running()) {

            if(frame_count >= frame_limit) break;
            
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
            frame_count++;

            // std::cout << "Recieved image #" << img_header.frame_number << " w:" << img.cols
            //          << " h:" << img.rows << " c:" << img.channels() << " type:" << img.type() << std::endl;

            // correct bit depth of image for processing
            if(img.type() == 18){
                img.convertTo(img, CV_8U, 1.0 / 256.0);
            }

            // process image
            local_timestamp_proc_start = get_timestamp_ns_utc();
            extractor.extract_features(img);
            frames_since_last_report++;
            
            // std::cout << "Processed Image #" << img_header.frame_number << std::endl;
            
            // serialize keypoints and descriptors to contiguous byte array for sending
            extractor.serialize_features();

            // set header values for features message
            extractor.set_header(features_header);
            local_timestamp_proc_end = features_header.timestamp_processed_ns;

            // send original image + feature vector
            send_image_plus_features(
                publisher, 
                header_msg, 
                pixels_msg, 
                features_header, 
                extractor.serialized_keypoints, 
                extractor.serialized_descriptors
            );
            local_timestamp_payload_sent = get_timestamp_ns_utc();
            local_timestamp_latest_send = local_timestamp_payload_sent;
            local_timestamp_first_send = local_timestamp_first_send ? local_timestamp_first_send : local_timestamp_latest_send; // sets first send to latest send if first send is 0, otherwise does nothing


            total_bytes += header_msg.size();
            total_bytes += pixels_msg.size();
            total_bytes += sizeof(features_header);
            total_bytes += extractor.serialized_keypoints.size();
            total_bytes += extractor.serialized_descriptors.size();
            proc_time = local_timestamp_proc_end - local_timestamp_proc_start;
            proc_times[frame_count-1] = proc_time;

            //throughput monitoring
            if ((local_timestamp_payload_sent - local_timestamp_last_report) > one_second) {
                std::cout << frame_count << "/" << frame_limit << ", Throughput: " << frames_since_last_report << " FPS\n"; 
                frames_since_last_report = 0; 
                local_timestamp_last_report = local_timestamp_payload_sent;
            }
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

    std::cout << "Total frames sent: " << frame_count << "\n";

    auto const count = static_cast<float>(proc_times.size());
    float avg_proc_time = std::reduce(proc_times.begin(), proc_times.end()) / count;

    std::cout << "Average processing time: " << avg_proc_time / one_second << " seconds per frame\n";

    // throughput calculation
    uint64_t elapsed_ns = local_timestamp_latest_send - local_timestamp_first_send;
    double elapsed_s    = elapsed_ns / one_second;
    double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);
    std::cout << "Average throughput: " << mbps << " mb per second\n";

    return 0;
}

