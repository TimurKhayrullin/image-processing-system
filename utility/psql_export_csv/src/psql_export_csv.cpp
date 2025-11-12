#include <pqxx/pqxx>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

// -------------------------------------------------------------
// Utility: write a pqxx::result to a CSV file
// -------------------------------------------------------------
void writeToCSV(const pqxx::result &rows, const std::string &filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open())
        throw std::runtime_error("Failed to open CSV file for writing");

    // Write headers
    for (pqxx::row::size_type i = 0; i < rows.columns(); ++i) {
        ofs << rows.column_name(i);
        if (i < rows.columns() - 1)
            ofs << ",";
    }
    ofs << "\n";

    // Write rows
    for (const auto &row : rows) {
        for (pqxx::row::size_type i = 0; i < row.size(); ++i) {
            ofs << '"' << row[i].c_str() << '"';
            if (i < row.size() - 1)
                ofs << ",";
        }
        ofs << "\n";
    }

    std::cout << "Exported " << rows.size() << " rows to " << filename << "\n";
}

// -------------------------------------------------------------
// Estimate average row size (bytes) for a table
// -------------------------------------------------------------
double estimateRowSize(pqxx::work &txn, const std::string &table) {
    std::ostringstream q;
    q << R"(
        SELECT
            CASE WHEN reltuples = 0 THEN 0
                 ELSE pg_total_relation_size(oid) / reltuples
            END AS avg_row_bytes
        FROM pg_class
        WHERE relname = )"
      << txn.quote(table) << ";";

    pqxx::result r = txn.exec(q.str());
    if (r.empty() || r[0]["avg_row_bytes"].is_null()) {
        std::cerr << "Warning: Could not estimate row size for table '"
                  << table << "', defaulting to 64 bytes.\n";
        return 64.0;
    }
    return r[0]["avg_row_bytes"].as<double>();
}

// -------------------------------------------------------------
// Main
// -------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <megabyte_limit>\n";
        return 1;
    }

    long mbLimit = std::stol(argv[1]);
    long byteLimit = mbLimit * 1024 * 1024;

    const std::string configPath = "configs/data_logger/PostgreSQL/config.yml";

    try {
        YAML::Node config = YAML::LoadFile(configPath);

        // --- Read database connection info ---
        const auto &db = config["database"];
        std::ostringstream conninfo;
        conninfo << "dbname=" << db["name"].as<std::string>()
                 << " user=" << db["user"].as<std::string>()
                 << " password=" << db["password"].as<std::string>()
                 << " hostaddr=" << db["host"].as<std::string>()
                 << " port=" << db["port"].as<int>();

        pqxx::connection conn(conninfo.str());
        if (!conn.is_open()) {
            std::cerr << "Failed to open PostgreSQL connection.\n";
            return 1;
        }

        pqxx::work txn(conn);

        // --- Determine which table(s) to export ---
        bool split_payload = false;
        if (config["data_handling"] && config["data_handling"]["split_payload"])
            split_payload = config["data_handling"]["split_payload"].as<bool>();

        std::string image_table;
        std::string feature_table;
        std::string payload_table;
        bool exporting_join = false;

        if (split_payload &&
            config["tables"]["images"]["enabled"].as<bool>() &&
            config["tables"]["features"]["enabled"].as<bool>()) {
            image_table = config["tables"]["images"]["name"].as<std::string>();
            feature_table = config["tables"]["features"]["name"].as<std::string>();
            exporting_join = true;
        } else {
            payload_table = config["tables"]["payloads"]["name"].as<std::string>();
        }

        // -------------------------------------------------------------
        // Estimate average row size
        // -------------------------------------------------------------
        double avgRowSize = 64.0;
        if (exporting_join) {
            double imgSize = estimateRowSize(txn, image_table);
            double featSize = estimateRowSize(txn, feature_table);
            // joined rows roughly equal to sum of both
            avgRowSize = imgSize + featSize;
        } else {
            avgRowSize = estimateRowSize(txn, payload_table);
        }

        if (avgRowSize <= 0)
            avgRowSize = 64.0;

        long rowLimit = std::max(1L, static_cast<long>(byteLimit / avgRowSize));

        // -------------------------------------------------------------
        // Build the export query
        // -------------------------------------------------------------
        std::ostringstream query;
        std::string outputFile;

        if (exporting_join) {
            query << "SELECT "
                << "i.id AS image_id, "
                << "i.timestamp AS image_timestamp, "
                << "i.image_data AS image_data, "
                << "f.id AS feature_id, "
                << "f.feature_vector AS feature_vector, "
                << "f.model_version AS model_version "
                << "FROM " << txn.esc(image_table) << " AS i "
                << "JOIN " << txn.esc(feature_table) << " AS f "
                << "ON i.id = f.image_id "
                << "ORDER BY i.id "
                << "LIMIT " << rowLimit << ";";
            outputFile = "images_features_export.csv";
        }
        else {
            query << "SELECT * FROM " << txn.esc(payload_table)
                  << " ORDER BY id LIMIT " << rowLimit << ";";
            outputFile = "payloads_export.csv";
        }

        // -------------------------------------------------------------
        // Execute and export
        // -------------------------------------------------------------
        pqxx::result rows = txn.exec(query.str());
        txn.commit();

        writeToCSV(rows, outputFile);

        std::cout << std::fixed << std::setprecision(2)
                  << "Estimated row size: " << avgRowSize << " bytes\n"
                  << " Approx. "
                  << (rows.size() * avgRowSize / (1024.0 * 1024.0))
                  << " MB exported to " << outputFile << "\n";

    } catch (const YAML::Exception &e) {
        std::cerr << "YAML parse error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
