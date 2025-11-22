#include "shared.hpp"
#include "extractor.hpp"
#include <yaml-cpp/yaml.h>

SIFTExtractor::SIFTExtractor(const std::string& config_path){

    // load params struct with values in config
    load_config(config_path);

    // create SIFT opencv object
    this->sift_ptr = cv::SIFT::create(
        this->params.n_features,
        this->params.n_octave_layers,
        this->params.contrast_threshold,
        this->params.edge_threshold,
        this->params.sigma,
        this->params.descriptor_type,
        this->params.enable_percise_upscale
    );

    // store size of descriptor vector
    this->descriptor_dim = sift_ptr->descriptorSize();

    //TODO: maybe reserve keypoints and descriptors containers' size
}

void SIFTExtractor::load_config(const std::string& path) {

    try {
        YAML::Node config = YAML::LoadFile(path);
        this->params.n_features = config["n_features"].as<uint32_t>();
        this->params.n_octave_layers = config["n_octave_layers"].as<uint32_t>();
        this->params.contrast_threshold = config["contrast_threshold"].as<double>();
        this->params.edge_threshold = config["edge_threshold"].as<double>();
        this->params.sigma = config["sigma"].as<double>();
        this->params.descriptor_type = config["descriptor_type"].as<uint32_t>();
        this->params.enable_percise_upscale = config["enable_percise_upscale"].as<bool>();
        std::cout << "Loaded config for SIFT feature extractor" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load database config from " << path
                  << ": " << e.what() << std::endl;
        throw;
    }
}

// process image
void SIFTExtractor::extract_features(cv::Mat &img){

    this->sift_ptr->detectAndCompute(img, cv::noArray(), this->keypoints, this->descriptors);

    this->timestamp_processed_ns = get_timestamp_ns_utc();
}

// serialize keypoints and descriptors to contiguous byte array for sending
void SIFTExtractor::serialize_features(){

    this->serialized_keypoints = serialize_keypoints(keypoints);
    this->serialized_descriptors = serialize_descriptors(descriptors);
}

void SIFTExtractor::set_header(FeaturesHeader &header){

    // initialize header for sift feature message
    header.params = this->params;
    header.timestamp_processed_ns    = this->timestamp_processed_ns;    
    header.descriptor_count          = header.keypoint_count = this->keypoints.size();
    header.descriptor_dim            = this->descriptor_dim;
    header.keypoints_size_bytes      = serialized_keypoints.size() * sizeof(KeyPointPortable);
    header.descriptors_size_bytes    = this->serialized_descriptors.size();

}
