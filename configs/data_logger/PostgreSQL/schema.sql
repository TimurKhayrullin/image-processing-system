-- creates a simple, single-table, flat schema that stores every 
-- image+features message received by the data logger as a separate record.  
CREATE TABLE IF NOT EXISTS payloads (
    -- data_logger
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    timestamp_insert_ns BIGINT,
    -- image
    width INT,
    height INT,
    channels INT,
    image_format_fourcc CHAR(4),
    frame_number BIGINT,
    timestamp_captured_ns BIGINT,
    image_size_bytes BIGINT,
    image_data BYTEA,
    -- feature extraction
    sift_param_n_features INT,
    sift_param_n_octave_layers INT,
    sift_param_contrast_threshold DOUBLE PRECISION,
    sift_param_edge_threshold DOUBLE PRECISION,
    sift_param_sigma DOUBLE PRECISION,
    sift_param_descriptor_type INT,
    sift_param_enable_percise_upscale BOOLEAN,
    timestamp_extractor_received_ns BIGINT,
    timestamp_extractor_processed_ns BIGINT,
    sift_keypoint_count INT,
    sift_descriptor_count INT,
    sift_descriptor_dim INT,
    sift_keypoints_size_bytes BIGINT,
    sift_descriptors_size_bytes BIGINT,
    sift_keypoints_data BYTEA,
    sift_descriptors_data BYTEA
);  