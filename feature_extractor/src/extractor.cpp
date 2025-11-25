// The ExtractionJob class abstracts away the process of extracting keypoints and descriptor vectors,
// In the event that someone would want to change SIFT for a different algorithm, or use a library 
// other than OpenCV, they could work off of this abstraction.

#include "shared.hpp"
#include "extractor.hpp"
#include <yaml-cpp/yaml.h>

// function for loading SIFT extraction parameters from config file using yaml-cpp
SIFTParams load_sift_params(const std::string& path) {
    SIFTParams params;

    try {
        YAML::Node config = YAML::LoadFile(path);

        params.n_features             = config["n_features"].as<uint32_t>();
        params.n_octave_layers        = config["n_octave_layers"].as<uint32_t>();
        params.contrast_threshold     = config["contrast_threshold"].as<double>();
        params.edge_threshold         = config["edge_threshold"].as<double>();
        params.sigma                  = config["sigma"].as<double>();
        params.descriptor_type        = config["descriptor_type"].as<uint32_t>();
        params.enable_percise_upscale = config["enable_percise_upscale"].as<bool>();

        std::cout << "Loaded SIFT parameters from: " << path << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr 
            << "Failed to load SIFT config from " << path 
            << ": " << e.what() << std::endl;
        throw; // propagate, we want the caller to know it failed
    }

    return params;
}

// constructs an extraction job
SIFTExtractionJob::SIFTExtractionJob(SIFTParams &params, cv::Ptr<cv::SIFT> &sift_ptr){

    this->params = params;
    this->sift_ptr = sift_ptr;
    
    // store size of descriptor vector
    this->descriptor_dim = sift_ptr->descriptorSize();

}

// do extraction work
void SIFTExtractionJob::extract_features(cv::Mat &img){

    this->sift_ptr->detectAndCompute(img, cv::noArray(), this->keypoints, this->descriptors);

    this->timestamp_processed_ns = get_timestamp_ns_utc();
}

// serialize keypoints and descriptors to contiguous byte array for sending
void SIFTExtractionJob::serialize_features(){

    this->serialized_keypoints = serialize_keypoints(keypoints);
    this->serialized_descriptors = serialize_descriptors(descriptors);
}

// set features header with relevant info regarding the algorithm and its outputs
void SIFTExtractionJob::set_header(FeaturesHeader &header){

    header.params = this->params;
    header.timestamp_processed_ns    = this->timestamp_processed_ns;    
    header.descriptor_count          = header.keypoint_count = this->keypoints.size();
    header.descriptor_dim            = this->descriptor_dim;
    header.keypoints_size_bytes      = serialized_keypoints.size() * sizeof(KeyPointPortable);
    header.descriptors_size_bytes    = this->serialized_descriptors.size();

}
