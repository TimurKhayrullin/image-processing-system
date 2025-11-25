#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "image_generator.hpp"
#include <iostream>
#include <string>
#include <zmq.hpp>
#include <chrono>

namespace fs = std::filesystem;

// Reads an arbitrary number of images from a specified location then package and
// send the image data via IPC to the feature extractor. Image data is published 
// continuously until the application is stopped. If all
// images from the input folder have been published, loop over the folder again forever
int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <folder_path>\n";
        return 1;
    }

    print_banner("Image Generator Started");
    
    // inititalizes signals for graceful shutdown
    ShutdownHandler::init();     

    // gets directory of images
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

    // For IPC management, we use ZeroMQ for easy ICP customization, abstraction.
    // Here we implement the publisher/subscriber pattern using Unix domain sockets
    zmq::context_t ctx{1}; // init context with 1 internal thread used for asynchronous sending/receiving.
    zmq::socket_t sender(ctx, zmq::socket_type::pub);

    // Allow at most 250 queued messages in internal zmq queue 
    sender.set(zmq::sockopt::sndhwm, 250);
    sender.connect("ipc:///tmp/camera_pub.sock");

    // keeps track of how many frames have been loaded and sent
    uint64_t frame_count = 0;
    std::optional<uint64_t> frame_limit = std::nullopt; // std::nullopt for no limit

    // keeps track of timestamps and frames/bytes for throughput monitoring
    uint64_t frames_since_last_report = 0;
    uint64_t one_second = 1e9;
    uint64_t local_timestamp_last_report = get_timestamp_ns_utc();
    
    double max_mbps = 600.0;   // configure desired throughput rate
    uint64_t bytes_since_last_report = 0;
    uint64_t throughput_limit = (uint64_t)(max_mbps * 1024.0 * 1024.0); // max bytes per second
    uint64_t total_bytes = 0;

    // keep track of bytes sent every time we send
    uint64_t payload_bytes = 0;

    // averaging calculations
    uint64_t local_timestamp_first_send = 0;
    uint64_t local_timestamp_latest_send = 0;
    uint64_t local_timestamp_last_loadsend_attempt = 0;

    // initialize directory iterator
    fs::directory_iterator dir_it = fs::directory_iterator(path);
    fs::directory_iterator dit_it_end = fs::end(dir_it); // end of directory


    // walks entire directory of images, loads and publishes an image if throughput limit allows. 
    // restarts directory traversal if we've gone through all images
    while (ShutdownHandler::running()) {

        // if we reached end of directory, restart
        if (dir_it == dit_it_end) {
            dir_it = std::filesystem::directory_iterator(path);
        }

        if(frame_limit && frame_count >= frame_limit) break; // break once frame limit is reached if there is one

        if(!ShutdownHandler::running()) break;

        // skip file if it isn't a regular file
        if (!(*dir_it).is_regular_file()){
            dir_it++;
            continue;
        }

        // if throughput limit reached, skip loading and sending file (but keep same file iterator)
        if(bytes_since_last_report > throughput_limit){

            local_timestamp_last_loadsend_attempt = get_timestamp_ns_utc();
        }
        else{

            // get reader for given file
            std::string filepath = (*dir_it).path().string();
                
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
            header.frame_number = frame_count;

            if(!ShutdownHandler::running()) break; // check if we need to shutdown

            // publish the image header and pixels via ZeroMQ, using a multipart message.
            // using a multipart message minimizes buffer allocations and copies. Also allows streaming.
            // ---- Frame 0: header ----
            sender.send(zmq::buffer(&header, sizeof(header)), zmq::send_flags::sndmore | zmq::send_flags::dontwait);
            // ---- Frame 1: pixel bytes ----
            sender.send(zmq::buffer(image_data.data(), header.image_size_bytes),
                        zmq::send_flags::none);

            // timestamps for throughput average calculation
            local_timestamp_last_loadsend_attempt = get_timestamp_ns_utc();
            local_timestamp_latest_send = local_timestamp_last_loadsend_attempt;
            local_timestamp_first_send = local_timestamp_first_send ? local_timestamp_first_send : local_timestamp_last_loadsend_attempt; // sets first send to latest send if first send is 0, otherwise does nothing

            // calculating number of bytes sent
            payload_bytes = sizeof(header) + header.image_size_bytes;
            total_bytes += payload_bytes;
            bytes_since_last_report += payload_bytes;
            
            // increment all counters as well as directory iterator, move on to next file
            frames_since_last_report++;
            frame_count++;
            dir_it++;
        }

        // throughput monitoring
        if ((local_timestamp_last_loadsend_attempt - local_timestamp_last_report) > one_second) {
            std::cout << frame_count << "/" << (frame_limit ? std::to_string(*frame_limit) : "inf") << ", Throughput: " << bytes_since_last_report/(1024*1024) << " MB/sec, " << frames_since_last_report << " FPS\n"; 
            local_timestamp_last_report = local_timestamp_last_loadsend_attempt;
            frames_since_last_report = 0; 
            bytes_since_last_report = 0;
        }

    }

    sender.set(zmq::sockopt::linger, 0); // when done, Drop any unsent messages immediately. Do NOT block on socket close.
    
    // close IPC connections
    sender.close();
    ctx.close();

    print_banner("Image Generator Terminated");

    // Average throughput calculation
    uint64_t elapsed_ns = local_timestamp_latest_send - local_timestamp_first_send;
    double elapsed_s    = elapsed_ns / one_second;
    double mbps = (total_bytes / elapsed_s) / (1024.0 * 1024.0);

    std::cout << "Total frames sent: " << frame_count << "\n";
    std::cout << "Average throughput: " << mbps << " mb per second\n";

    return 0;
}
