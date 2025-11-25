// implementations for helper functions the image generator uses to partially decode image files, as well as load them into memory

#include "message_headers.hpp"
#include "image_generator.hpp"
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <cstddef>   // for std::byte


inline void make_fourcc(char (&dst)[4], char a, char b, char c, char d)
{
    dst[0] = a;
    dst[1] = b;
    dst[2] = c;
    dst[3] = d;
}

bool detect_fourcc(const std::vector<std::byte>& buf, char (&fourcc)[4])
{
    // tiny helper lambda to convert from size_t to uint8
    auto b = [&](size_t i) -> uint8_t {
        return std::to_integer<uint8_t>(buf[i]);
    };

    if (buf.size() < 12){
        throw std::runtime_error("Failed to get image format: Buffer too small to detect image format.");
        return false;
    }

    // ---- PNG ----
    if (buf.size() >= 8 &&
        b(0)==0x89 && b(1)==0x50 && b(2)==0x4E && b(3)==0x47 &&
        b(4)==0x0D && b(5)==0x0A && b(6)==0x1A && b(7)==0x0A)
    {
        make_fourcc(fourcc, 'P','N','G','\0');
        return true;
    }

    // ---- JPEG ----
    if (b(0)==0xFF && b(1)==0xD8 && b(2)==0xFF)
    {
        make_fourcc(fourcc, 'J','P','G','\0');
        return true;
    }

    // ---- TIFF ----
    if ((b(0)==0x49 && b(1)==0x49 && b(2)==0x2A && b(3)==0x00) ||
        (b(0)==0x4D && b(1)==0x4D && b(2)==0x00 && b(3)==0x2A))
    {
        make_fourcc(fourcc, 'T','I','F','F');
        return true;
    }

    // ---- BMP ----
    if (b(0)==0x42 && b(1)==0x4D)
    {
        make_fourcc(fourcc, 'B','M','P','\0');
        return true;
    }

    // ---- WEBP ----
    if (buf.size() >= 12 &&
        b(0)=='R' && b(1)=='I' && b(2)=='F' && b(3)=='F' &&
        b(8)=='W' && b(9)=='E' && b(10)=='B' && b(11)=='P')
    {
        make_fourcc(fourcc, 'W','E','B','P');
        return true;
    }

    // ---- UNKNOWN ----
    // throw std::runtime_error("Failed to get image format: unsupported or unknown image format.");
    // return false;

    // if unable to decode fourcc, put placeholder
    make_fourcc(fourcc, 'X', 'X', 'X', 'X');

    return true;
}

bool load_file_bytes(const std::string& path, std::vector<std::byte> &buffer)
{
    // open file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path);
        return false;
    }

    // Get file size
    const std::streamsize size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to get file size: " + path);
        return false;
    }

    // Allocate buffer
    buffer.resize(static_cast<size_t>(size));

    // Go back to beginning of file
    file.seekg(0, std::ios::beg);

    // Read all bytes
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + path);
        return false;
    }

    return true;
}

// loads image data buffer and fills image header info
bool load_image(const std::string& path, ImageHeader &header, std::vector<std::byte> &image_data){

    // load image_data vector with image data, then fill header with size of image in bytes
    if(!load_file_bytes(path, image_data)) return false;
    header.image_size_bytes = image_data.size();
    
    // detect format fourcc of image via signature sniffing and fill the header with the result
    if(!detect_fourcc(image_data, header.format_fourcc)) return false;

    return true;
}