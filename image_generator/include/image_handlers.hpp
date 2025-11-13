// This file contains an abtract ImageHandler class, concrete versions for different file formats,
// and a handler factory class declaration
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <iostream>
#include <opencv2/opencv.hpp>

// extension matching function for files
inline bool endsWithIgnoreCase(const std::string &str, const std::string &suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

class ImageHandler {
public:
    virtual ~ImageHandler() = default;

    // Checks if this handler can load this file (usually by extension)
    virtual bool canHandle(const std::string &filepath) const = 0;

    // Load and decode image into raw pixels + shape
    virtual bool load(const std::string &filepath,
                      std::vector<uint8_t> &outPixels,
                      uint32_t &width,
                      uint32_t &height,
                      uint32_t &channels) const = 0;
};

class JpegHandler : public ImageHandler {
public:
    bool canHandle(const std::string &filepath) const override;

    bool load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c) const override;
};

class TiffHandler : public ImageHandler {
public:
    bool canHandle(const std::string &filepath) const override;

    bool load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c) const override;
};

class PngHandler : public ImageHandler {
public:
    bool canHandle(const std::string &filepath) const override;

    bool load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c) const override;
};

class ImageHandlerFactory {
public:
    ImageHandlerFactory();

    const ImageHandler* getHandler(const std::string &filepath) const;

private:
    std::vector<std::unique_ptr<ImageHandler>> handlers;
};