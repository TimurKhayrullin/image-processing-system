#include "shared.hpp"
#include "feature_extractor.hpp"
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
    print_banner("Feature Extractor Started");

    std::signal(SIGINT, signalHandler);  // Handle Ctrl+C

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
        while (keepRunning) {
            zmq::message_t msg;

            // zmq::recv_result_t === std::optional<size_t>
            zmq::recv_result_t received = subscriber.recv(msg, zmq::recv_flags::none);

            if (received) {
                std::string data(static_cast<char*>(msg.data()), msg.size());

                std::cout << "Received: " << data << std::endl;

                std::string processed = data;

                std::transform(processed.begin(), processed.end(), processed.begin(),
                    [](unsigned char c){ return std::toupper(c); });

                std::cout << "Processed into: " << processed << std::endl;

                std::string combined = data + '\n' + processed;

                publisher.send(zmq::buffer(combined), zmq::send_flags::none);
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

    print_banner("Feature Extractor Terminated");
    return 0;
}

