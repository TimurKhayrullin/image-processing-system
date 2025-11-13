#pragma once
#include <string>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>

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