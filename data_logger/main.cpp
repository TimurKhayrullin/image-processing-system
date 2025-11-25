#include "shared.hpp"
#include "data_logger.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include "postgres_database.hpp"
#include <csignal>
#include <atomic>
#include <zmq.hpp>

// the Data Logger Receives processed data (image data + key points/descriptors) and save the data for future analysis

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

    // Allow at most 1000 queued messages in internal zmq queue 
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

    // variables for throughput averaging calculations
    uint64_t frame_count = 0;
    std::optional<uint64_t> frame_limit = std::nullopt; //std::nullopt for no limit
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
        while (ShutdownHandler::running()) {

            // stop exection if frame limit reached, if there is one
            if(frame_limit && frame_count>=frame_limit) break;

            // declare a payload struct and read received data in from ZMQ IPC socket
            Payload payload;
            bool received = recv_payload(subscriber, payload);

            // if a message was received, store the contents in postgres database
            if (received) {

                // get message received time timestamp, it also marks the start of insert time calculation
                uint64_t timestamp_ns = get_timestamp_ns_utc();
                local_timestamp_insert_start = timestamp_ns;

                // run the database subroutine for storing data in database
                db.logData(payload, timestamp_ns);

                // mark timestamp of end of insertion
                local_timestamp_insert_end = get_timestamp_ns_utc();

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

    // number of insertions reporting
    std::cout << "Total frames inserted: " << frame_count << "\n";

    // average insertion time calculation + reporting
    auto const count = static_cast<float>(insert_times.size());
    float avg_insert_time = std::reduce(insert_times.begin(), insert_times.end()) / count;
    std::cout << "Average insertion time: " << avg_insert_time / one_second << " seconds per load\n";

    // average throughput calculation
    uint64_t elapsed_ns = local_timestamp_latest_insert - local_timestamp_first_insert;
    double elapsed_s    = elapsed_ns / one_second;
    double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);
    std::cout << "Average throughput: " << mbps << " mb per second\n";

    return 0;
}
