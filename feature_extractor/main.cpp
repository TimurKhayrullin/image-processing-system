#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include "message_handling.hpp"
#include "extractor.hpp"
#include <opencv2/features2d.hpp>
#include <iostream>
#include <string>
#include <numeric>
#include <future>
#include <zmq.hpp>


// the feature extractor receives image data and processes it to find key features then
// publish the original image and the extracted features for other applications to consume.
int main() {
    print_banner("Feature Extractor Started");

    // inititalizes signals for graceful shutdown
    ShutdownHandler::init(); 
    
    // read in SIFT extraction parameters from config
    SIFTParams params = load_sift_params("configs/feature_extractor/SIFT_params.yml");

    // create SIFT extractor object
    cv::Ptr<cv::SIFT> sift_ptr = cv::SIFT::create(
        params.n_features,
        params.n_octave_layers,
        params.contrast_threshold,
        params.edge_threshold,
        params.sigma,
        params.descriptor_type,
        params.enable_percise_upscale
    );

    // Create a ZeroMQ context and subscriber socket
    zmq::context_t ctx{1};
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);

    // Limit inbound queue to 250 messages
    subscriber.set(zmq::sockopt::rcvhwm, 250);

    // bind to the same IPC socket the image generator connects to
    subscriber.bind("ipc:///tmp/camera_pub.sock");

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");
    subscriber.set(zmq::sockopt::rcvtimeo, 500);   // 0.5s timeout

    std::cout << "Listening for messages on ipc:///tmp/camera_pub.sock ..." << std::endl;

    // keep track of frames sent, and optionally sets a frame limit
    uint64_t frame_count = 0;
    std::optional<uint64_t> frame_limit = std::nullopt; //std::nullopt for no frame limit

    // keeps track of timestamps for throughput monitoring
    uint64_t frames_since_last_report = 0;
    uint64_t one_second = 1e9;
    uint64_t local_timestamp_last_report = get_timestamp_ns_utc();
    uint64_t local_timestamp_payload_sent = 0;
    uint64_t total_bytes = 0;

    // timestamps for averaging calculations
    uint64_t local_timestamp_first_send = 0;
    uint64_t local_timestamp_latest_send = 0;
    std::vector<uint64_t> proc_times;
    proc_times.reserve(frame_limit.value_or(10000));

    // initializing vector of thread futures used to optionally parallelize SIFT extraction 
    std::vector<std::future<std::tuple<uint64_t, uint64_t, uint64_t>>> futures;
    const size_t MAX_TASKS = 1; // TODO: move to config


    // start receiving images from image generator
    try {
        while (ShutdownHandler::running()) {

            // Remove completed async tasks
            auto it = futures.begin();
            while (it != futures.end()) {
                if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready){

                    //Get the return value from thread
                    auto [frame_num, proc_time, num_bytes_sent] = it->get();

                    // store processing time and number of bytes sent
                    proc_times.push_back(proc_time);
                    total_bytes += num_bytes_sent;

                    // erase task
                    it = futures.erase(it);
                }
                else{
                    it++;
                }
                    
            }

            // exit if frame limit reached in case there is one
            if(frame_limit && frame_count >= frame_limit) break;

            // Limit concurrency
            if (futures.size() >= MAX_TASKS) continue; 
            
            // prepare image + features payload
            FeaturesHeader features_header;
            ImageHeader img_header;
            std::vector<uint8_t> image_data; // openCV doesn't have a mat std::byte constructor for Mat so we use uint8_t 

            // recieve image, preserve messages for sending to data logger
            if (!recv_image(subscriber, img_header, image_data)){
                continue;
            }

            // mark processing payload with timestamp, and set frame number 
            features_header.timestamp_received_ns = get_timestamp_ns_utc();
            features_header.frame_number = img_header.frame_number;
            frame_count++;
            frames_since_last_report++;

            // start new async extraction job
            futures.push_back(
                std::async(
                    std::launch::async,
                    mt_do_extraction,
                    frame_count,
                    std::ref(ctx),
                    img_header,
                    std::move(image_data),
                    params,
                    sift_ptr,
                    std::move(features_header))
            );

            // timestamps for throughput average calculation
            local_timestamp_payload_sent = get_timestamp_ns_utc();
            local_timestamp_latest_send = local_timestamp_payload_sent;
            local_timestamp_first_send = local_timestamp_first_send ? local_timestamp_first_send : local_timestamp_latest_send; // sets first send to latest send if first send is 0, otherwise does nothing

            // throughput monitoring
            if ((local_timestamp_payload_sent - local_timestamp_last_report) > one_second) {
                std::cout << frame_count << "/" << (frame_limit ? std::to_string(*frame_limit) : "inf") << ", Throughput: " << frames_since_last_report << " FPS\n"; 
                frames_since_last_report = 0; 
                local_timestamp_last_report = local_timestamp_payload_sent;
            }
        }

        // Remove completed async tasks after loop ends
        auto it = futures.begin();
        while (it != futures.end()) {
            if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready){

                //Get the return value from thread
                auto [frame_num, proc_time, num_bytes_sent] = it->get();

                // store processing time and number of bytes sent
                proc_times.push_back(proc_time);
                total_bytes += num_bytes_sent;

                // erase task
                it = futures.erase(it);
            }
            else{
                it++;
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

    subscriber.set(zmq::sockopt::linger, 0); // Drop any unsent messages immediately. Do NOT block on socket close.

    // close IPC connections
    subscriber.close();
    ctx.close();

    print_banner("Feature Extractor Terminated");

    // Average throughput and processing time calculation + reporting
    std::cout << "Total frames sent: " << frame_count << "\n";
    auto const count = static_cast<float>(proc_times.size());
    float avg_proc_time = std::reduce(proc_times.begin(), proc_times.end()) / count;
    std::cout << "Average processing time: " << avg_proc_time / one_second << " seconds per frame\n";

    uint64_t elapsed_ns = local_timestamp_latest_send - local_timestamp_first_send;
    double elapsed_s    = elapsed_ns / one_second;
    double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);
    std::cout << "Average throughput: " << mbps << " mb per second\n";

    return 0;
}

