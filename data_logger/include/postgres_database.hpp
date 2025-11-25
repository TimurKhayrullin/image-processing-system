// header for postgres implementation

#pragma once
#include "database.hpp"
#include "data_logger.hpp"
#include <vector>
#include <pqxx/pqxx>
#include <chrono>

class PostgresDatabase : public Database {
public:
    explicit PostgresDatabase(const std::string& config_path);
    ~PostgresDatabase() override;

    // Main public operation
    bool logData(const Payload& payload, uint64_t timestamp_insert_ns) override;

protected:
    // Internal virtual overrides
    bool connect() override;
    bool setup_schema() override;
    void configureParameters() override;

private:
    std::string connectionInfo;
    std::unique_ptr<pqxx::connection> connection;
    bool split_payload = false;

    // --- size check parameters ---
    long long max_db_size_bytes = 1LL * 1024 * 1024 * 1024;
    int t_size_check_period = 300;       // seconds
    size_t insert_count_size_check = 1000;  // number of inserts
    size_t insert_counter = 0;
    bool db_too_large_cached = false;
    std::chrono::steady_clock::time_point last_size_check_time;
    void prepareStatements();

    // --- internal helpers ---
    bool performInsert(pqxx::work& txn, const Payload& payload, uint64_t timestamp_insert_ns);
    //bool logUnsplitPayload(pqxx::work& txn, const std::string& payload);
    bool shouldRecheckSize();
    bool isDatabaseTooLarge();
};