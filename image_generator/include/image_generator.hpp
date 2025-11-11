#pragma once

#include <string>
#include <filesystem>
#include <string>
#include <vector>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;

std::string generateImage(const std::string& input);
bool has_image_extension(const fs::path& file_path);
