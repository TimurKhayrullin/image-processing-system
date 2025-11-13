#pragma once

#include <string>
#include <filesystem>
#include <string>
#include <vector>
#include <csignal>
#include <atomic>
#include <algorithm>

namespace fs = std::filesystem;

bool has_image_extension(const fs::path& file_path);
