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

    // Connect to the same IPC socket the Feature extractor is bound to
    subscriber.connect("ipc:///tmp/features_pub.sock");

    // Subscribe to all messages (empty filter = all topics)
    subscriber.set(zmq::sockopt::subscribe, "");

    std::cout << "Listening for messages on ipc:///tmp/features_pub.sock ..." << std::endl;

    try {
        while (ShutdownHandler::running()) {
            zmq::message_t msg;

            ImageHeader img_header;
            std::vector<uint8_t> pixels;
            SIFTHeader sift_header;
            std::vector<KeyPointPortable> keypoints;
            std::vector<uint8_t> desc_mat;

            bool received = recv_image_plus_features(
                                subscriber,
                                img_header,
                                pixels,
                                sift_header,
                                keypoints,
                                desc_mat
                            );

            if (received) {

                uint64_t timestamp_ns = get_timestamp_ns_utc();

                std::cout << "Received: image+features#" << img_header.frame_number << "\n";
                std::cout << "Time to send to extractor: " << (sift_header.timestamp_recieved_ns - img_header.timestamp_ns) / 1'000'000 << "ms\n";
                std::cout << "Time to extract features: " << (sift_header.timestamp_processed_ns - sift_header.timestamp_recieved_ns) / 1'000'000 << "ms\n";
                std::cout << "Time to send to logger: " << (timestamp_ns - sift_header.timestamp_processed_ns) / 1'000'000 << "ms\n";

                // db.logData(data);
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
    return 0;
}
