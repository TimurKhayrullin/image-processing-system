#include "shared.hpp"
#include "data_logger.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <algorithm> // For std::transform
#include <cctype>    // For std::toupper
#include <zmq.hpp>

static std::atomic<bool> keepRunning(true);

void signalHandler(int) {
    keepRunning = false;
}

int main() {
    print_banner("Data Logger Started");

    std::signal(SIGINT, signalHandler);  // Handle Ctrl+C

    PostgresDatabase db("configs/data_logger/PostgreSQL/config.yml");
    db.printStatus();

    if(!db.isConnected){
        std::cout << "unable to connect to database, terminating\n"; 
        keepRunning = false;
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
        while (keepRunning) {
            zmq::message_t msg;

            // zmq::recv_result_t === std::optional<size_t>
            zmq::recv_result_t received = subscriber.recv(msg, zmq::recv_flags::none);

            if (received) {

                std::string data(static_cast<char*>(msg.data()), msg.size());

                // std::cout << "Received: " << data << std::endl;

                db.logData(data);
            }
        }
    }
    catch (const zmq::error_t& e) {
        if (keepRunning == false && e.num() == EINTR) {
            // Interrupted by SIGINT — normal exit
        } else {
            std::cerr << "ZMQ error: " << e.what() << std::endl;
        }
    }

    print_banner("Data Logger Terminated");
    return 0;
}
