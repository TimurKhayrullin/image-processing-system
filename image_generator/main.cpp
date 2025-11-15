#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "message_headers.hpp"
#include "image_generator.hpp"
#include "image_readers.hpp"
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

    if (!fs::exists(path) || !fs::is_directory(path)) {
        std::cerr << "Error: path does not exist or is not a directory.\n";
        return 1;
    }

    std::filesystem::directory_iterator dir_iterator(path);

    if (dir_iterator == fs::end(dir_iterator)) {
        std::cout << "directory is empty.\n";
        return 0;
    }



    // Setup image handler 
    ImageReaderFactory factory;

    // ZeroMQ for easy ICP customization, abstraction.
    // Here we implement the publisher/subscriber pattern using Unix domain sockets
    zmq::context_t ctx{1}; // init context with 1 internal thread used for asynchronous sending/receiving.
    zmq::socket_t sender(ctx, zmq::socket_type::pub);
    sender.bind("ipc:///tmp/camera_pub.sock");

    // keeps track of how many frames have been read
    uint64_t frame_count = 0;

    // Publishes all the images to the zmq topic, and once all of them have been published loops over them again
    while (ShutdownHandler::running()) {

        // iterate over entire directory, creating a new iterator with each new loop.
        for (const auto& entry : std::filesystem::directory_iterator(path)) {

            if(!ShutdownHandler::running()) break;

            if (!entry.is_regular_file())
                continue;

            std::string filepath = entry.path().string();
            const ImageReader* reader = factory.get_reader(filepath);

            if (!reader) {
                std::cerr << "[WARN] No reader for " << filepath << "\n";
                continue;
            }

            ImageHeader header;
            std::vector<uint8_t> pixels;

            // loads image info and pixels into header and pixel vector
            if (!reader->load(filepath, pixels, header)) {
                std::cerr << "[WARN] Failed to load: " << filepath << "\n";
                continue;
            }

            header.timestamp_ns = get_timestamp_ns_utc();

            header.frame_number = frame_count++;

            if(!ShutdownHandler::running()) break;

            std::cout << "Loaded image #" << header.frame_number << " ("
                    << header.width << "x" << header.height << ", of type " << header.pixel_format << " at time " << header.timestamp_ns << ")\n";
            
            // publish the image header and pixels via ZeroMQ, using a multipart message.
            // using a multipart message minimizes buffer allocations and copies. Also allows streaming.
            // ---- Frame 0: header ----
            sender.send(zmq::buffer(&header, sizeof(header)), zmq::send_flags::sndmore);
            // ---- Frame 1: pixel bytes ----
            sender.send(zmq::buffer(pixels.data(), header.pixel_count),
                        zmq::send_flags::none);
        }

    }

    print_banner("Image Generator Terminated");

    return 0;
}
