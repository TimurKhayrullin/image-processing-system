// concrete implementations of readers for different file formats,
// and the reader factory class implementation 
#include "image_readers.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

// Returns true if OpenCV can read the file, false if not
bool OpenCVImageReader::can_read(const std::string &filepath) const {
    return cv::haveImageReader(filepath);
}

// Uses OpenCV to Read an image file and load it's properties and pixels into memory for sending
bool OpenCVImageReader::load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c,
              uint32_t &pixel_format) const 
{
    cv::Mat img = cv::imread(filepath, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "[ERROR] Failed to load image: " << filepath << "\n";
        return false;
    }

    w = img.cols;
    h = img.rows;
    c = img.channels();
    pixel_format = img.type();

    outPixels.assign(img.data, img.data + img.total() * c);
    return true;
}

// load image metadata directly into an ImageHeader for sending
bool OpenCVImageReader::load(const std::string &filepath, std::vector<uint8_t> &outPixels, ImageHeader &header) const 
{
    return load(filepath, outPixels, header.width, header.height, header.channels, header.pixel_format);
}

// handler factory implementation
const ImageReader* ImageReaderFactory::get_reader(const std::string &filepath) const {
    for (const auto& h : readers) {
        if (h->can_read(filepath))
            return h.get();
    }
    return nullptr;
}

ImageReaderFactory::ImageReaderFactory() {
        readers.emplace_back(std::make_unique<OpenCVImageReader>());
}