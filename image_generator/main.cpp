#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "image_generator.hpp"
#include <iostream>
#include <string>
#include <zmq.hpp>
#include <chrono>

namespace fs = std::filesystem;


// get file location as input

// Read an arbitrary number of images from a specified location then package and
// send the image data via IPC to 

// Image data should be published continuously until the application is stopped. If all
// images from the input folder have been published, loop over the folder again…
// forever 

// The app should be able to handle images of varying sizes and resolutions (e.g. few
// KB to >30MB)
int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <folder_path>\n";
        return 1;
    }

    print_banner("Image Generator Started");
    
    // <--- install signal handlers for shutdown
    ShutdownHandler::init();     

    fs::path path(argv[1]);

    // check that directory exists
    if (!fs::exists(path) || !fs::is_directory(path)) {
        std::cerr << "Error: path does not exist or is not a directory.\n";
        return 1;
    }

    // check that directory isn't empty
    std::filesystem::directory_iterator dir_iterator(path);
    if (dir_iterator == fs::end(dir_iterator)) {
        std::cout << "directory is empty.\n";
        return 0;
    }

    // ZeroMQ for easy ICP customization, abstraction.
    // Here we implement the publisher/subscriber pattern using Unix domain sockets
    zmq::context_t ctx{1}; // init context with 1 internal thread used for asynchronous sending/receiving.
    zmq::socket_t sender(ctx, zmq::socket_type::pub);
    // Allow at most N queued messages in internal zmq queue 
    sender.set(zmq::sockopt::sndhwm, 250);
    sender.connect("ipc:///tmp/camera_pub.sock");

    // keeps track of how many frames have been read
    uint64_t frame_count = 0;

    // keeps track of timestamps for throughput monitoring
    uint64_t frames_since_last_report = 0;
    uint64_t one_second = 1e9;
    uint64_t local_timestamp_last_report = get_timestamp_ns_utc();
    uint64_t total_bytes = 0;

    // averaging calculations
    std::optional<uint64_t> frame_limit = 200; //std::nullopt; // std::nullopt for no limit
    uint64_t local_timestamp_first_send = 0;
    uint64_t local_timestamp_latest_send = 0;

    // Publishes all the images to the zmq topic, and once all of them have been published loops over them again
    while (ShutdownHandler::running()) {

        // iterate over entire directory, creating a new iterator with each new loop.
        for (const auto& entry : std::filesystem::directory_iterator(path)) {

            if(frame_limit && frame_count >= frame_limit) break; // break once frame limit is reached if there is one
            if(!ShutdownHandler::running()) break;

            if (!entry.is_regular_file())
                continue;

            // get reader for given file
            std::string filepath = entry.path().string();
            
            // setup message payload
            ImageHeader header;
            std::vector<std::byte> image_data;

            // loads image info and image data into header and data vector
            if (!load_image(filepath, header, image_data)) {
                std::cerr << "[WARN] Failed to load: " << filepath << "\n";
                continue;
            }

            // mark payload with timestamp and frame number
            header.timestamp_ns = get_timestamp_ns_utc();
            header.frame_number = frame_count++;
            frames_since_last_report++;

            if(!ShutdownHandler::running()) break;

            // publish the image header and pixels via ZeroMQ, using a multipart message.
            // using a multipart message minimizes buffer allocations and copies. Also allows streaming.
            // ---- Frame 0: header ----
            sender.send(zmq::buffer(&header, sizeof(header)), zmq::send_flags::sndmore | zmq::send_flags::dontwait);
            // ---- Frame 1: pixel bytes ----
            sender.send(zmq::buffer(image_data.data(), header.image_size_bytes),
                        zmq::send_flags::none);

            local_timestamp_latest_send = get_timestamp_ns_utc();
            local_timestamp_first_send = local_timestamp_first_send ? local_timestamp_first_send : local_timestamp_latest_send; // sets first send to latest send if first send is 0, otherwise does nothing

            total_bytes += sizeof(header);
            total_bytes += header.image_size_bytes;
            // dir_it++;
            // std::cout << "Sent image #" << header.frame_number << " ("
            //         << header.width << "x" << header.height << ", of type " << header.pixel_format << ", with size " << header.image_size_bytes << " bytes)\n";
            
            //throughput monitoring
            if ((local_timestamp_latest_send - local_timestamp_last_report) > one_second) {
                std::cout << frame_count << "/" << (frame_limit ? std::to_string(*frame_limit) : "inf") << ", Throughput: " << frames_since_last_report << " FPS\n"; 
                frames_since_last_report = 0; 
                local_timestamp_last_report = local_timestamp_latest_send;
            }

        }

        if(frame_limit && frame_count >= frame_limit) break; // break once frame limit is reached if there is one

    }

    sender.set(zmq::sockopt::linger, 0); // Drop any unsent messages immediately. Do NOT block on socket close.
    sender.close();
    ctx.close();

    print_banner("Image Generator Terminated");

    // throughput calculation
    uint64_t elapsed_ns = local_timestamp_latest_send - local_timestamp_first_send;
    double elapsed_s    = elapsed_ns / one_second;
    double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);

    std::cout << "Total frames sent: " << frame_count << "\n";
    std::cout << "Average throughput: " << mbps << " mb per second\n";

    return 0;
}
