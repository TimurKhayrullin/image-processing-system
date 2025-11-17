#include "data_logger.hpp"
#include "postgres_database.hpp"

PostgresDatabase::PostgresDatabase(const std::string& config_path)
    : Database(config_path) {

    loadConfig(config_path);
    configureParameters();

    // data_handling section
    if (config["data_handling"]) {
        auto dh = config["data_handling"];
        if (dh["split_payload"]) split_payload = dh["split_payload"].as<bool>();
        if (dh["max_db_size_mb"])
            max_db_size_bytes = static_cast<long long>(dh["max_db_size_mb"].as<int>()) * 1024 * 1024;
        if (dh["t_size_check_period"])
            t_size_check_period = dh["t_size_check_period"].as<int>();
        if (dh["insert_count_size_check"])
            insert_count_size_check = dh["insert_count_size_check"].as<int>();
    }

    std::cout << "Split payload: " << (split_payload ? "ENABLED" : "DISABLED")
              << " | Max DB size: " << (max_db_size_bytes / (1024 * 1024))
              << " MB | Check period: " << t_size_check_period
              << " s | Check every " << insert_count_size_check
              << " inserts\n";

    if (connect()) setupSchema();

    prepareStatements();

    last_size_check_time = std::chrono::steady_clock::now();
}

PostgresDatabase::~PostgresDatabase() {
    if (connection && connection->is_open()) {
        std::cout << "Closing PostgreSQL connection to "
                  << dbName << std::endl;
        // connection.reset();
    }
}

bool PostgresDatabase::connect() {
    try {
        connection = std::make_unique<pqxx::connection>(connectionInfo);
        if (connection->is_open()) {
            std::cout << "Connected to PostgreSQL: " << dbName << std::endl;
            isConnected = true;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "PostgreSQL connection failed: " << e.what() << std::endl;
    }
    return false;
}

void PostgresDatabase::configureParameters() {
    const auto& db = config["database"];
    std::ostringstream conninfo;
    conninfo << "dbname=" << db["name"].as<std::string>()
             << " user=" << db["user"].as<std::string>()
             << " password=" << db["password"].as<std::string>()
             << " host=" << db["host"].as<std::string>()
             << " port=" << db["port"].as<int>();
    connectionInfo = conninfo.str();
}

bool PostgresDatabase::setupSchema() {
    if (!isConnected || !connection || !connection->is_open()) return false;

    try {
        pqxx::work txn(*connection);
        const auto& tables = config["tables"];

        for (auto it = tables.begin(); it != tables.end(); ++it) {
            if (!it->second["enabled"].as<bool>()) continue;

            std::ostringstream query;
            query << "CREATE TABLE IF NOT EXISTS "
                  << it->second["name"].as<std::string>() << " (";

            const auto& cols = it->second["columns"];
            bool first = true;
            for (auto c = cols.begin(); c != cols.end(); ++c) {
                if (!first) query << ", ";
                first = false;
                query << c->first.as<std::string>() << " "
                      << c->second.as<std::string>();
            }
            query << ");";

            std::cout << query.str() << std::endl;

            txn.exec(query.str());
        }

        txn.commit();
        std::cout << "Schema verified for database: " << dbName << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Schema setup failed: " << e.what() << std::endl;
        return false;
    }
}

// prepare a sql statement for inserting data into db
void PostgresDatabase::prepareStatements() {
    connection->prepare(
        "insert_payload",
        "INSERT INTO payloads ("
            "timestamp_insert_ns, width, height, channels, pixel_format, frame_number,"
            "timestamp_captured_ns, image_size_bytes, image_data,"
            "sift_param_n_features, sift_param_n_octave_layers,"
            "sift_param_contrast_threshold, sift_param_edge_threshold, sift_param_sigma,"
            "timestamp_extractor_received_ns, timestamp_extractor_processed_ns,"
            "sift_keypoint_count, sift_descriptor_count, sift_descriptor_dim, sift_descriptor_type,"
            "sift_keypoints_size_bytes, sift_descriptors_size_bytes,"
            "sift_keypoints_data, sift_descriptors_data"
        ") VALUES ("
            "$1,$2,$3,$4,$5,"
            "$6,$7,$8,"
            "$9,$10,$11,$12,$13,"
            "$14,$15,"
            "$16,$17,$18,$19,"
            "$20,$21,"
            "$22,$23,$24"
        ")"
    );
}


// -----------------------------------------------------------
//  Database size management
// -----------------------------------------------------------

bool PostgresDatabase::shouldRecheckSize() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - last_size_check_time).count();

    if (elapsed >= t_size_check_period || insert_counter >= insert_count_size_check) {
        insert_counter = 0;
        last_size_check_time = now;
        return true;
    }
    return false;
}

bool PostgresDatabase::isDatabaseTooLarge() {
    try {
        pqxx::work txn(*connection);
        pqxx::result r = txn.exec("SELECT pg_database_size(current_database());");
        long long size = r[0][0].as<long long>();
        db_too_large_cached = size > max_db_size_bytes;

        std::cout << "[Postgres] DB size = " << (size / (1024 * 1024)) << " MB → "
                  << (db_too_large_cached ? "TOO LARGE" : "OK") << std::endl;

        return db_too_large_cached;
    } catch (const std::exception& e) {
        std::cerr << "[Postgres] Failed to check size: " << e.what() << std::endl;
        return db_too_large_cached; // retain previous status
    }
}

// -----------------------------------------------------------
//  Logging entry point
// -----------------------------------------------------------

bool PostgresDatabase::logData(const Payload& payload, uint64_t timestamp_insert_ns) {
    if (!isConnected || !connection || !connection->is_open()) {
        std::cerr << "Database not connected, cannot log data." << std::endl;
        return false;
    }

    if (shouldRecheckSize()) {
        isDatabaseTooLarge();
    }

    if (db_too_large_cached) {
        std::cerr << "[Postgres] Skipping log (database exceeds limit)\n";
        return false;
    }

    try {

        pqxx::work txn(*connection);

        bool success = performInsert(txn, payload, timestamp_insert_ns);

        if (success) {
            txn.commit();
            insert_counter++;
        }
        
        return success;

    } catch (const std::exception& e) {
        std::cerr << "Data logging failed: " << e.what() << std::endl;
        return false;
    }
}



// -----------------------------------------------------------
//  Helper: actually do the insert operation
// -----------------------------------------------------------
bool PostgresDatabase::performInsert(pqxx::work& txn, const Payload& payload, uint64_t timestamp_insert_ns)
{

    std::basic_string<std::byte> img_bytes(
        reinterpret_cast<const std::byte*>(payload.pixels.data()),
        payload.pixels.size());

    std::basic_string<std::byte> kp_bytes(
        reinterpret_cast<const std::byte*>(payload.keypoints.data()),
        payload.keypoints.size() * sizeof(KeyPointPortable));

    std::basic_string<std::byte> desc_bytes(
        reinterpret_cast<const std::byte*>(payload.desc_mat.data()),
        payload.desc_mat.size());
    
    // this is marked depracted, but this version of libpqxx doesn't have the newer API for prepared statements
    // and newer version is c++20 only.
    txn.exec_prepared(
        "insert_payload",
        timestamp_insert_ns,
        payload.image_header.width,
        payload.image_header.height,
        payload.image_header.channels,
        payload.image_header.pixel_format,
        payload.image_header.frame_number,
        payload.image_header.timestamp_ns,
        payload.image_header.image_size_bytes,
        img_bytes,
        payload.sift_header.params.n_features,
        payload.sift_header.params.n_octave_layers,
        payload.sift_header.params.contrast_threshold,
        payload.sift_header.params.edge_threshold,
        payload.sift_header.params.sigma,
        payload.sift_header.timestamp_received_ns,
        payload.sift_header.timestamp_processed_ns,
        payload.sift_header.keypoint_count,
        payload.sift_header.descriptor_count,
        payload.sift_header.descriptor_dim,
        payload.sift_header.descriptor_type,
        payload.sift_header.keypoints_size_bytes,
        payload.sift_header.descriptors_size_bytes,
        kp_bytes,
        desc_bytes
    );

    return true;
}
