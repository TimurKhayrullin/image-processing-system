// concrete implementations of handlers for different file formats,
// and the handler factory class implementation 
#include "image_handlers.hpp"

// jpeg handler
bool JpegHandler::canHandle(const std::string &filepath) const {
    return endsWithIgnoreCase(filepath, ".jpg") ||
            endsWithIgnoreCase(filepath, ".jpeg");
}

bool JpegHandler::load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c) const 
{
    cv::Mat img = cv::imread(filepath, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "[ERROR] Failed to load JPEG: " << filepath << "\n";
        return false;
    }

    w = img.cols;
    h = img.rows;
    c = img.channels();

    outPixels.assign(img.data, img.data + img.total() * c);
    return true;
}

// Tiff handler
bool TiffHandler::canHandle(const std::string &filepath) const {
    return endsWithIgnoreCase(filepath, ".tif") ||
            endsWithIgnoreCase(filepath, ".tiff");
}

bool TiffHandler::load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c) const
{
    cv::Mat img = cv::imread(filepath, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "[ERROR] Failed to load TIFF: " << filepath << "\n";
        return false;
    }

    w = img.cols;
    h = img.rows;
    c = img.channels();

    outPixels.assign(img.data, img.data + img.total() * c);
    return true;
}

// png handler
bool PngHandler::canHandle(const std::string &filepath) const {
    return endsWithIgnoreCase(filepath, ".png");
}

bool PngHandler::load(const std::string &filepath,
        std::vector<uint8_t> &outPixels,
        uint32_t &w, uint32_t &h, uint32_t &c) const
{
    cv::Mat img = cv::imread(filepath, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        std::cerr << "[ERROR] Failed to load PNG: " << filepath << "\n";
        return false;
    }

    w = img.cols;
    h = img.rows;
    c = img.channels();

    outPixels.assign(img.data, img.data + img.total() * c);
    return true;
}

// handler factory implementation
const ImageHandler* ImageHandlerFactory::getHandler(const std::string &filepath) const {
    for (const auto& h : handlers) {
        if (h->canHandle(filepath))
            return h.get();
    }
    return nullptr;
}

ImageHandlerFactory::ImageHandlerFactory() {
        handlers.emplace_back(std::make_unique<PngHandler>());
        handlers.emplace_back(std::make_unique<JpegHandler>());
        handlers.emplace_back(std::make_unique<TiffHandler>());
}