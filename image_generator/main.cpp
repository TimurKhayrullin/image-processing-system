#include "shared.hpp"
#include "shutdown_handler.hpp"
#include "image_generator.hpp"
#include "image_handlers.hpp"
#include <iostream>
#include <string>
#include <zmq.hpp>

namespace fs = std::filesystem;

static std::atomic<bool> keepRunning(true);

void signalHandler(int) {
    keepRunning = false;
}

int main(int argc, char* argv[]) {

    print_banner("Image Generator Started");

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <folder_path>\n";
        return 1;
    }
    
    // <--- install signal handlers for shutdown
    ShutdownHandler::init();     

    // get file location as input

    // Read an arbitrary number of images from a specified location then package and
    // send the image data via IPC to 

    // Image data should be published continuously until the application is stopped. If all
    // images from the input folder have been published, loop over the folder again…
    // forever 

    // The app should be able to handle images of varying sizes and resolutions (e.g. few
    // KB to >30MB)

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
    ImageHandlerFactory factory;

    // ZeroMQ for easy ICP customization, abstraction.
    // Here we implement the publisher/subscriber pattern using Unix domain sockets
    zmq::context_t ctx{1}; // init context with 1 internal thread used for asynchronous sending/receiving.
    zmq::socket_t sender(ctx, zmq::socket_type::pub);
    sender.bind("ipc:///tmp/camera_pub.sock");

    // Publishes all the images to the zmq topic, and once all of them have been published loops over them again
    while (ShutdownHandler::running()) {

        // iterate over entire directory, creating a new iterator with each new loop.
        for (const auto& entry : std::filesystem::directory_iterator(path)) {

            if(!ShutdownHandler::running()) break;

            if (!entry.is_regular_file())
                continue;

            std::string filepath = entry.path().string();
            const ImageHandler* handler = factory.getHandler(filepath);

            if (!handler) {
                std::cerr << "[WARN] No handler for " << filepath << "\n";
                continue;
            }

            std::vector<uint8_t> pixels;
            uint32_t w, h, c;

            if (!handler->load(filepath, pixels, w, h, c)) {
                std::cerr << "[WARN] Failed to load: " << filepath << "\n";
                continue;
            }

            if(!ShutdownHandler::running()) break;

            std::cout << "Loaded " << filepath << " ("
                    << w << "x" << h << ", " << c << " channels)\n";

            std::string message = "image: " + std::to_string(w) + "x" + std::to_string(h) + ", " + std::to_string(pixels.size()) + "pixels.";

            // publish the frame via ZeroMQ
            sender.send(zmq::buffer(message), zmq::send_flags::none);
        }

    }

    print_banner("Image Generator Terminated");

    return 0;
}
