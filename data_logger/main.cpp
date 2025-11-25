#include "shared.hpp"
#include "data_logger.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include "postgres_database.hpp"
#include <csignal>
#include <atomic>
#include <zmq.hpp>

int main() {
    print_banner("Data Logger Started");

    // <--- install signal handlers for shutdown
    ShutdownHandler::init();

    // initialize database
    PostgresDatabase db("configs/data_logger/PostgreSQL/config.yml");
    db.printStatus();

    // exit if failed to connect to database
    if(!db.isConnected){
        std::cout << "unable to connect to database, terminating\n"; 
    }

    // Create a ZeroMQ context and subscriber socket
    zmq::context_t ctx{1};
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);
    // Allow at most N queued messages in internal zmq queue 
    subscriber.set(zmq::sockopt::sndhwm, 1000);

    // Connect to the same IPC socket the Feature extractor is bound to
    subscriber.bind("ipc:///tmp/features_pub.sock");

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");
    subscriber.set(zmq::sockopt::rcvtimeo, 500);   // 0.5s timeout

    std::cout << "Listening for messages on ipc:///tmp/features_pub.sock ..." << std::endl;

    // keeps track of timestamps for throughput monitoring
    uint64_t frames_since_last_report = 0;
    uint64_t one_second = 1e9;
    uint64_t local_timestamp_last_report = get_timestamp_ns_utc();
    
    uint64_t total_bytes = 0;

    // averaging calculations
    uint64_t frame_count = 0;
    std::optional<uint64_t> frame_limit = std::nullopt; //std::nullopt for no limit
    uint64_t local_timestamp_first_insert = 0;
    uint64_t local_timestamp_latest_insert = 0;
    uint64_t local_timestamp_insert_start = 0;
    uint64_t local_timestamp_insert_end = 0;
    uint64_t insert_time = 0;
    std::vector<uint64_t> insert_times;
    insert_times.reserve(frame_limit.value_or(10000));

    try {
        while (ShutdownHandler::running()) {

            if(frame_limit && frame_count>=frame_limit) break;

            Payload payload;

            bool received = recv_payload(subscriber, payload);

            if (received) {

                uint64_t timestamp_ns = get_timestamp_ns_utc();

                // std::cout << "Received: image+features#" << payload.image_header.frame_number << "\n";
                // std::cout << "Time to send to extractor: " << (payload.sift_header.timestamp_received_ns - payload.image_header.timestamp_ns) / 1'000'000 << "ms\n";
                // std::cout << "Time to extract features: " << (payload.sift_header.timestamp_processed_ns - payload.sift_header.timestamp_received_ns) / 1'000'000 << "ms\n";
                // std::cout << "Time to send to logger: " << (timestamp_ns - payload.sift_header.timestamp_processed_ns) / 1'000'000 << "ms\n";
                
                local_timestamp_insert_start = timestamp_ns;
                db.logData(payload, timestamp_ns);
                local_timestamp_insert_end = get_timestamp_ns_utc();
                local_timestamp_latest_insert = local_timestamp_insert_end;
                local_timestamp_first_insert = local_timestamp_first_insert ? local_timestamp_first_insert : local_timestamp_latest_insert; // sets first send to latest send if first send is 0, otherwise does nothing
                frames_since_last_report++;
                insert_time = local_timestamp_insert_end - local_timestamp_insert_start;
                total_bytes += sizeof(payload);
                total_bytes += payload.pixels.size()
                            + payload.desc_mat.size()
                            + payload.keypoints.size() * sizeof(KeyPointPortable);
                insert_times.push_back(insert_time);
                frame_count++;

                //throughput monitoring
                if ((timestamp_ns - local_timestamp_last_report) > one_second) {
                    std::cout << frame_count << "/" << (frame_limit ? std::to_string(*frame_limit) : "inf") << ", Throughput: " << frames_since_last_report << " FPS\n"; 
                    frames_since_last_report = 0; 
                    local_timestamp_last_report = timestamp_ns;
                }
            }
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

    std::cout << "Total frames inserted: " << frame_count << "\n";

    auto const count = static_cast<float>(insert_times.size());
    float avg_insert_time = std::reduce(insert_times.begin(), insert_times.end()) / count;

    std::cout << "Average insertion time: " << avg_insert_time / one_second << " seconds per load\n";

    // throughput calculation
    uint64_t elapsed_ns = local_timestamp_latest_insert - local_timestamp_first_insert;
    double elapsed_s    = elapsed_ns / one_second;
    double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);
    std::cout << "Average throughput: " << mbps << " mb per second\n";


    return 0;
}
