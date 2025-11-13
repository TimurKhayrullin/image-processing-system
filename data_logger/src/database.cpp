#include "database.hpp"

Database::Database(const std::string& name)
    : dbName(name), isConnected(false) {}

void Database::loadConfig(const std::string& path) {
    try {
        config = YAML::LoadFile(path);
        dbName = config["database"]["name"].as<std::string>();
        std::cout << "Loaded config for database: " << dbName << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load database config from " << path
                  << ": " << e.what() << std::endl;
        throw;
    }
}

void Database::printStatus() const {
    std::cout << "Database [" << dbName << "] status: "
              << (isConnected ? "Connected" : "Disconnected") << std::endl;
}