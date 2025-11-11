#include "shared.hpp"
#include "image_generator.hpp"
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

    // get file location as input

    // Read an arbitrary number of images from a specified location then package and
    // send the image data via IPC to 

    // Image data should be published continuously until the application is stopped. If all
    // images from the input folder have been published, loop over the folder again…
    // forever 

    // The app should be able to handle images of varying sizes and resolutions (e.g. few
    // KB to >30MB)

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <folder_path>\n";
        return 1;
    }

    std::signal(SIGINT, signalHandler);  // Handle Ctrl+C

    // ZeroMQ for easy ICP customization, abstraction.
    // Here we implement the publisher/subscriber pattern using Unix domain sockets
    zmq::context_t ctx{1}; // init context with 1 internal thread used for asynchronous sending/receiving.
    zmq::socket_t sender(ctx, zmq::socket_type::pub);
    sender.bind("ipc:///tmp/camera_pub.sock");

    fs::path path(argv[1]);

    if (!fs::exists(path) || !fs::is_directory(path)) {
        std::cerr << "Error: path does not exist or is not a directory.\n";
        return 1;
    }

    std::vector<fs::path> image_files;

    for (const auto& entry : fs::directory_iterator(path)) {
        if (fs::is_regular_file(entry.path()) && has_image_extension(entry.path())) {
            image_files.push_back(entry.path());
        }
    }

    if (image_files.empty()) {
        std::cout << "No image files found in the directory.\n";
        return 0;
    }

    std::cout << "Found " << image_files.size() << " image files:\n";

    // Publishes all the images to the zmq topic, and once all of them have been published loops over them again
    while (keepRunning) {
        for (size_t i = 0; i < image_files.size(); i++) {
            const auto& img = image_files[i];
            std::string job = "Job " + img.string();
            std::cout << "Sending string  " << i+1 << "/" << image_files.size() << ": " << job << '\n';
            sender.send(zmq::buffer(job), zmq::send_flags::none);
        }
    }

    print_banner("Image Generator Terminated");

    return 0;
}
