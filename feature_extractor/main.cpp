#include "shared.hpp"
#include "feature_extractor.hpp"

int main() {
    print_banner("Feature Extractor");
    auto result = extractFeatures("image_data");
    std::cout << result << std::endl;
    return 0;
}
