#include "shared.hpp"
#include "data_logger.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include "postgres_database.hpp"
#include <csignal>
#include <atomic>
#include <yaml-cpp/yaml.h>
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <unordered_set>

// the Data Logger Receives processed data (image data + key points/descriptors) and save the data for future analysis

int main() {
    print_banner("Data Logger Started");

    // <--- install signal handlers for shutdown
    ShutdownHandler::init();

    YAML::Node config = YAML::LoadFile("configs/data_logger/config.yml");

    // initialize database
    PostgresDatabase db("configs/data_logger/PostgreSQL/config.yml");
    db.printStatus();

    // exit if failed to connect to database
    if(!db.isConnected){
        std::cout << "unable to connect to database, terminating\n"; 
        return 1;
    }

    // Create a ZeroMQ context and subscriber socket
    zmq::context_t ctx{1};
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);

    // Allow at most 1000 queued messages in internal zmq queue 
    subscriber.set(zmq::sockopt::rcvhwm, 10000);

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");

    // Subscriber connects; publisher binds.
    subscriber.connect(config["zmq_sub_socket"].as<std::string>());

    // Give ZMQ a moment to establish the initial IPC connection/subscription.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "Listening for messages on" << config["zmq_sub_socket"].as<std::string>() << "ipc:///tmp/features_pub.sock ..." << std::endl;

    // keeps track of timestamps for throughput monitoring
    uint64_t frames_since_last_report = 0;
    uint64_t one_second = 1e9;
    uint64_t local_timestamp_last_report = get_timestamp_ns_utc();

    // variables for throughput averaging calculations
    uint64_t received_count = 0;
    uint64_t inserted_count = 0;
    uint64_t duplicate_count = 0;
    uint64_t insert_fail_count = 0;

    // Attempt to match the image_generator frame_limit (e.g. demo sends 25 frames).
    // If not present, fall back to unlimited logging.
    std::optional<uint64_t> frame_limit = std::nullopt;
    try {
        YAML::Node gen_cfg = YAML::LoadFile("configs/image_generator/config.yml");
        if (gen_cfg["frame_limit"] && !gen_cfg["frame_limit"].IsNull()) {
            frame_limit = gen_cfg["frame_limit"].as<uint64_t>();
        }
    } catch (...) {
        frame_limit = std::nullopt;
    }

    std::unordered_set<uint64_t> inserted_frames;
    inserted_frames.reserve(static_cast<size_t>(frame_limit.value_or(10000)));

    uint64_t local_timestamp_first_insert = 0;
    uint64_t local_timestamp_latest_insert = 0;
    uint64_t local_timestamp_insert_start = 0;
    uint64_t local_timestamp_insert_end = 0;
    uint64_t insert_time = 0;
    std::vector<uint64_t> insert_times;
    insert_times.reserve(frame_limit.value_or(10000));
    uint64_t total_bytes = 0;

    // start receiving images from feature extractor
    try {
        zmq::pollitem_t poll_items[] = {{subscriber.handle(), 0, ZMQ_POLLIN, 0}};
        constexpr auto poll_timeout = std::chrono::milliseconds(200);

        auto handle_payload = [&](const Payload& payload) {
            received_count++;
            const uint64_t frame_num = payload.sift_header.frame_number;

            if (frame_limit && frame_num >= *frame_limit) {
                return;
            }
            if (inserted_frames.find(frame_num) != inserted_frames.end()) {
                duplicate_count++;
                return;
            }

            // get message received time timestamp, it also marks the start of insert time calculation
            uint64_t timestamp_ns = get_timestamp_ns_utc();
            local_timestamp_insert_start = timestamp_ns;

            // run the database subroutine for storing data in database (retry a few times for transient errors)
            bool inserted = false;
            constexpr int max_attempts = 3;
            for (int attempt = 1; attempt <= max_attempts; attempt++) {
                inserted = db.logData(payload, timestamp_ns);
                if (inserted) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // mark timestamp of end of insertion
            local_timestamp_insert_end = get_timestamp_ns_utc();

            if (!inserted) {
                insert_fail_count++;
                return;
            }

            inserted_frames.insert(frame_num);

            // timestamp updates for throughput averaging
            local_timestamp_latest_insert = local_timestamp_insert_end;
            local_timestamp_first_insert = local_timestamp_first_insert ? local_timestamp_first_insert : local_timestamp_latest_insert; // sets first send to latest send if first send is 0, otherwise does nothing
            frames_since_last_report++;
            
            // time taken to insert data into database calculation
            insert_time = local_timestamp_insert_end - local_timestamp_insert_start;

            // sum of number of bytes for each piece of the received payload
            total_bytes += sizeof(payload);
            total_bytes += payload.pixels.size()
                        + payload.desc_mat.size()
                        + payload.keypoints.size() * sizeof(KeyPointPortable);
            
            // store insertion time and increment number of frames received
            insert_times.push_back(insert_time);
            inserted_count++;

            //throughput monitoring
            if ((timestamp_ns - local_timestamp_last_report) > one_second) {
                std::cout << inserted_count << "/" << (frame_limit ? std::to_string(*frame_limit) : "inf") << ", Throughput: " << frames_since_last_report << " FPS\n"; 
                frames_since_last_report = 0; 
                local_timestamp_last_report = timestamp_ns;
            }
        };

        while (ShutdownHandler::running()) {

            // stop exection if frame limit reached, if there is one
            if (frame_limit && inserted_count >= *frame_limit) break;

            // Wait for a full multipart payload to be available.
            zmq::poll(poll_items, 1, poll_timeout);
            if (!(poll_items[0].revents & ZMQ_POLLIN)) {
                continue;
            }

            // declare a payload struct and read received data in from ZMQ IPC socket
            Payload payload;
            bool received = recv_payload(subscriber, payload);

            // if a message was received, store the contents in postgres database
            if (received) {
                std::cout << "receieved:" << payload.sift_header.frame_number << "\n";
                handle_payload(payload);
            }
        }

        // Best-effort drain: after shutdown, keep consuming queued payloads until we've completed
        // the expected frame_limit, or we've been idle for a short grace period.
        auto last_activity = std::chrono::steady_clock::now();
        constexpr auto idle_grace = std::chrono::milliseconds(1500);
        while ((!frame_limit || inserted_count < *frame_limit) &&
               (std::chrono::steady_clock::now() - last_activity) < idle_grace) {
            zmq::poll(poll_items, 1, std::chrono::milliseconds(100));
            if (!(poll_items[0].revents & ZMQ_POLLIN)) {
                continue;
            }

            Payload payload;
            if (!recv_payload(subscriber, payload)) {
                continue;
            }
            std::cout << "receieved:" << payload.sift_header.frame_number << "\n";
            handle_payload(payload);
            last_activity = std::chrono::steady_clock::now();
        }
    }
    catch (const zmq::error_t& e) {
        if (!(ShutdownHandler::running() == false) && e.num() == EINTR) {
            // Interrupted by SIGINT — normal exit
        } else {
            std::cerr << "ZMQ error: " << e.what() << std::endl;
        }
    }

    print_banner("Data Logger Terminated");

    // number of insertions reporting
    std::cout << "Total frames received: " << received_count << "\n";
    std::cout << "Total frames inserted: " << inserted_count << "\n";
    std::cout << "Duplicate frames ignored: " << duplicate_count << "\n";
    std::cout << "Insert failures: " << insert_fail_count << "\n";

    // average insertion time calculation + reporting
    if (!insert_times.empty()) {
        auto const count = static_cast<float>(insert_times.size());
        float avg_insert_time = std::reduce(insert_times.begin(), insert_times.end()) / count;
        std::cout << "Average insertion time: " << avg_insert_time / one_second << " seconds per load\n";
    }

    // average throughput calculation
    if (local_timestamp_first_insert != 0 && local_timestamp_latest_insert > local_timestamp_first_insert) {
        uint64_t elapsed_ns = local_timestamp_latest_insert - local_timestamp_first_insert;
        double elapsed_s    = elapsed_ns / one_second;
        double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);
        std::cout << "Average throughput: " << mbps << " mb per second\n";
    }

    return 0;
}
