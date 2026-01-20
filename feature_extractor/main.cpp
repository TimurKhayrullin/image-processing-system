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
#include <chrono>
#include <thread>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <zmq.hpp>


// the feature extractor receives image data and processes it to find key features then
// publish the original image and the extracted features for other applications to consume.
int main() {
    print_banner("Feature Extractor Started");

    // inititalizes signals for graceful shutdown
    ShutdownHandler::init(); 

    // read in extractor config file
    YAML::Node config = YAML::LoadFile("configs/feature_extractor/config.yml");
    
    // read in SIFT extraction parameters from param config file defined in general extractor config 
    SIFTParams params = load_sift_params(config["alg_param_config"].as<std::string>());

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

    // Limit inbound queue to max number of messages, as per config
    subscriber.set(zmq::sockopt::rcvhwm, config["zmq_sub_hwm"].as<int>());

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");

    // Subscriber connects; publisher binds.
    subscriber.connect(config["zmq_sub_socket"].as<std::string>());

    const std::string pub_socket_addr = config["zmq_pub_socket"].as<std::string>(); // sets publisher socket address as per config

    subscriber.set(zmq::sockopt::rcvtimeo, config["zmq_recv_timeout_ms"].as<int>()); // receive command timeout in milliseconds as per config

    std::cout << "Listening for messages on" << config["zmq_sub_socket"].as<std::string>() << "..." << std::endl;

    // Publisher socket for extracted features
    zmq::socket_t publisher(ctx, zmq::socket_type::xpub);
    publisher.set(zmq::sockopt::xpub_verbose, 1);
    publisher.set(zmq::sockopt::sndhwm, config["zmq_sub_hwm"].as<int>());
    if (pub_socket_addr.rfind("ipc://", 0) == 0) {
        std::error_code ec;
        std::filesystem::remove(pub_socket_addr.substr(std::string("ipc://").size()), ec);
    }
    publisher.bind(pub_socket_addr);

    // Avoid "slow joiner": wait for at least one subscriber subscription before sending.
    {
        zmq::pollitem_t items[] = {{publisher.handle(), 0, ZMQ_POLLIN, 0}};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        bool got_sub = false;
        while (std::chrono::steady_clock::now() < deadline) {
            zmq::poll(items, 1, std::chrono::milliseconds(100));
            if (!(items[0].revents & ZMQ_POLLIN)) continue;

            zmq::message_t sub_msg;
            if (!publisher.recv(sub_msg, zmq::recv_flags::none)) continue;
            if (sub_msg.size() < 1) continue;
            const auto* bytes = static_cast<const uint8_t*>(sub_msg.data());
            if (bytes[0] == 1) { // subscribe
                got_sub = true;
                break;
            }
        }
        if (!got_sub) {
            std::cerr << "[WARN] No data_logger subscriber detected yet; first frames may be dropped.\n";
        }
    }

    // keep track of frames sent, and optionally sets a frame limit
    uint64_t frame_count = 0;

    // sets frame_limit to std::nullopt for no limit (config value is null) otherwise frame_limit int from config
    std::optional<uint64_t> frame_limit;
    if(config["frame_limit"].IsNull()) frame_limit = std::nullopt;
    else frame_limit = config["frame_limit"].as<uint64_t>();

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
    std::vector<std::future<ExtractedPayload>> futures;
    const size_t MAX_TASKS = config["num_threads"].as<size_t>(); // max number of asycn extraction threads allowed


    // start receiving images from image generator
    try {
        while (ShutdownHandler::running()) {

            auto send_extracted = [&](ExtractedPayload& extracted) {
                if (extracted.image_header.image_size_bytes == 0) return;
                const bool sent = send_image_plus_features(
                    publisher,
                    extracted.image_header,
                    extracted.image_data,
                    extracted.features_header,
                    extracted.keypoints,
                    extracted.descriptors
                );
                if (!sent) {
                    std::cerr << "[WARN] Failed to send extracted payload for frame "
                              << extracted.image_header.frame_number << "\n";
                    return;
                }
                proc_times.push_back(extracted.proc_time_ns);
                total_bytes += extracted.num_bytes;
                local_timestamp_payload_sent = get_timestamp_ns_utc();
                local_timestamp_latest_send = local_timestamp_payload_sent;
                local_timestamp_first_send = local_timestamp_first_send ? local_timestamp_first_send : local_timestamp_latest_send;
            };

            // Remove completed async tasks
            auto it = futures.begin();
            while (it != futures.end()) {
                if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready){

                    //Get the return value from thread
                    auto extracted = it->get();
                    send_extracted(extracted);

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
            if(MAX_TASKS <= 1){
                auto extracted = mt_do_extraction(
                    img_header,
                    std::move(image_data),
                    params,
                    sift_ptr,
                    std::move(features_header)
                );
                send_extracted(extracted);
            }
            else{
                futures.push_back(
                    std::async(
                        std::launch::async,
                        mt_do_extraction,
                        img_header,
                        std::move(image_data),
                        params,
                        sift_ptr,
                        std::move(features_header))
                );
            }

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
                auto extracted = it->get();
                auto send_extracted_end = [&](ExtractedPayload& extracted_end) {
                    if (extracted_end.image_header.image_size_bytes == 0) return;
                    const bool sent = send_image_plus_features(
                        publisher,
                        extracted_end.image_header,
                        extracted_end.image_data,
                        extracted_end.features_header,
                        extracted_end.keypoints,
                        extracted_end.descriptors
                    );
                    if (!sent) return;
                    proc_times.push_back(extracted_end.proc_time_ns);
                    total_bytes += extracted_end.num_bytes;
                    local_timestamp_payload_sent = get_timestamp_ns_utc();
                    local_timestamp_latest_send = local_timestamp_payload_sent;
                    local_timestamp_first_send = local_timestamp_first_send ? local_timestamp_first_send : local_timestamp_latest_send;
                };
                send_extracted_end(extracted);

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
    futures.clear();
    subscriber.set(zmq::sockopt::linger, 0); // Drop any unsent messages immediately. Do NOT block on socket close.

    // close IPC connections
    subscriber.close();
    publisher.close();
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
