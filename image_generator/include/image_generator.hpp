#pragma once

#include <string>
#include <filesystem>
#include <string>
#include <vector>
#include <csignal>
#include <atomic>
#include <algorithm>

namespace fs = std::filesystem;

inline void make_fourcc(char (&dst)[4], char a, char b, char c, char d);

bool detect_fourcc(const std::vector<std::byte>& buf, char (&fourcc)[4]);

bool load_file_bytes(const std::string& path, std::vector<std::byte> &buffer);

bool load_image(const std::string& path, ImageHeader &header, std::vector<std::byte> &image_data);
