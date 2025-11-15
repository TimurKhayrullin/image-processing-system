// This file contains an abtract ImageReader class, concrete versions for different file formats,
// and a handler factory class declaration
#pragma once
#include "message_headers.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <iostream>

class ImageReader {
public:
    virtual ~ImageReader() = default;

    // Checks if this handler can load this file
    virtual bool can_read(const std::string &filepath) const = 0;

    // Load and decode image into raw pixels + shape
    virtual bool load(const std::string &filepath,
                      std::vector<uint8_t> &outPixels,
                      uint32_t &width,
                      uint32_t &height,
                      uint32_t &channels,
                      uint32_t &pixel_format) const = 0;

    // load image directly into an ImageMessage
    virtual bool load(const std::string &filepath, std::vector<uint8_t> &outPixels, ImageHeader &header) const = 0;
};


class OpenCVImageReader: public ImageReader {
public:
    bool can_read(const std::string &filepath) const override;

    bool load(const std::string &filepath,
              std::vector<uint8_t> &outPixels,
              uint32_t &w, uint32_t &h, uint32_t &c,
              uint32_t &pixel_format) const override;

    // load image metadata directly into an ImageHeader for sending
    bool load(const std::string &filepath, std::vector<uint8_t> &outPixels, ImageHeader &header) const override;
};

class ImageReaderFactory {
public:
    ImageReaderFactory();

    const ImageReader* get_reader(const std::string &filepath) const;

private:
    std::vector<std::unique_ptr<ImageReader>> readers;
};