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

bool PostgresDatabase::logData(const std::string& payload) {
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

        bool success = split_payload
            ? logSplitPayload(txn, payload)
            : logUnsplitPayload(txn, payload);

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
//  Helper: split payload mode
// -----------------------------------------------------------
bool PostgresDatabase::logSplitPayload(pqxx::work& txn,
                                       const std::string& payload) {
    // Expect payload formatted as:  "<image>|<features>|<optional_model>"
    std::istringstream ss(payload);
    std::string image, features, model;
    std::getline(ss, image, '|');
    std::getline(ss, features, '|');
    std::getline(ss, model, '|'); // may be empty

    if (image.empty() || features.empty()) {
        std::cerr << "Invalid split payload: must contain at least image and features." << std::endl;
        return false;
    }

    std::ostringstream imgQuery;
    imgQuery << "INSERT INTO images (image_data, metadata) VALUES ("
             << txn.quote(image) << ", NULL) RETURNING id;";
    pqxx::result imgRes = txn.exec(imgQuery.str());
    int image_id = imgRes[0][0].as<int>();

    std::ostringstream featQuery;
    featQuery << "INSERT INTO features (image_id, feature_vector, model_version) VALUES ("
              << image_id << ", "
              << txn.quote(features) << ", "
              << (model.empty() ? "NULL" : txn.quote(model))
              << ");";
    txn.exec(featQuery.str());

    std::cout << "Logged split payload [image_id=" << image_id
              << "] image='" << image << "', features='" << features << "'"
              << std::endl;
    return true;
}

// -----------------------------------------------------------
//  Helper: unsplit payload mode
// -----------------------------------------------------------
bool PostgresDatabase::logUnsplitPayload(pqxx::work& txn,
                                         const std::string& payload) {
    const std::string payloadTable =
        config["tables"]["payloads"]["name"].as<std::string>();

    std::ostringstream query;
    query << "INSERT INTO " << txn.esc(payloadTable)
          << " (payload_data) VALUES (" << txn.quote(payload) << ");";

    txn.exec(query.str());
    std::cout << "Logged unsplit payload to '" << payloadTable
              << "' (" << payload.size() << " chars)" << std::endl;
    return true;
}