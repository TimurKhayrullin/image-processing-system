#include "image_generator.hpp"

bool has_image_extension(const fs::path& file_path) {
    static const std::vector<std::string> exts = {".png", ".jpg", ".jpeg", ".bmp", ".tiff"};
    std::string ext = file_path.extension().string();
    for (auto& e : exts) {
        if (ext == e || ext == std::string(e.begin(), e.end())) return true;
    }
    return false;
}
