// header file for extraction job
#pragma once
#include "message_headers.hpp"
#include "feature_serialization.hpp"
#include <opencv2/opencv.hpp>
#include <vector>

SIFTParams load_sift_params(const std::string& path);

// could turn this into an abstract class + concrete class implementation
class SIFTExtractionJob {
public:
    // called before starting to send/receive messages, 
    // initializes all memory to be used for processing
    // initializes algorithm parameters if needed
    SIFTExtractionJob(SIFTParams &params, cv::Ptr<cv::SIFT> &sift_ptr); 

    // load parameters in from config file
    void load_config(const std::string& config_path);

    // called when you want to actually perform the operation
    void extract_features(cv::Mat &img);

    // called to package result of processing into IPC message
    void serialize_features();

    void set_header(FeaturesHeader &header);

    std::vector<KeyPointPortable> serialized_keypoints;
    std::vector<std::byte> serialized_descriptors;

private:

    SIFTParams params;
    cv::Ptr<cv::SIFT> sift_ptr;
    std::vector<cv::KeyPoint> keypoints;
    cv::Mat descriptors;
    uint32_t descriptor_dim;

    uint64_t timestamp_processed_ns;
};