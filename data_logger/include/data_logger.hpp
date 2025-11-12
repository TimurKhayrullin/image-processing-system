#pragma once
#include <string>
#include <vector>
#include <pqxx/pqxx>
#include <yaml-cpp/yaml.h>
#include <chrono>

// Abstract base class for databases
class Database {
public:
    // Virtual destructor is required for proper cleanup of derived classes
    virtual ~Database() = default;

    // Public interface
    Database(const std::string& name);

    // Data logging API
    virtual bool logData(const std::string& payload) = 0;

    // Utility method for diagnostics or testing
    virtual void printStatus() const;

    std::string dbName;
    bool isConnected = false;
protected:
    // Protected (not private) so derived classes can use these internally
    virtual bool connect() = 0;         // Connection setup
    virtual bool setupSchema() = 0;     // Schema creation or validation
    virtual void configureParameters() = 0; // Internal configuration

    // Loads YAML configuration (used by derived classes)
    virtual void loadConfig(const std::string& path);

    YAML::Node config;
};

class PostgresDatabase : public Database {
public:
    explicit PostgresDatabase(const std::string& config_path);
    ~PostgresDatabase() override;

    // Main public operation
    bool logData(const std::string& payload) override;

protected:
    // Internal virtual overrides
    bool connect() override;
    bool setupSchema() override;
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

    // --- internal helpers ---
    bool logSplitPayload(pqxx::work& txn, const std::string& payload);
    bool logUnsplitPayload(pqxx::work& txn, const std::string& payload);
    bool shouldRecheckSize();
    bool isDatabaseTooLarge();
};

