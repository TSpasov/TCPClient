#include "db_connector.hpp"
#include <stdexcept>
#include <sqlite3.h>

namespace {
    // ---- Schema & SQL constants ----
    const char* CREATE_TABLE_SQL =
        "CREATE TABLE IF NOT EXISTS transactions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "amount REAL, "
        "resp TEXT, "
        "ts DATETIME DEFAULT CURRENT_TIMESTAMP);";
}

DBConnector::DBConnector(const std::string& path) {
    if (sqlite3_open(path.c_str(), reinterpret_cast<sqlite3**>(&db)) != SQLITE_OK) {
        throw std::runtime_error("failed to open DB: " + path);
    }
    // ensure schema exists
    write(CREATE_TABLE_SQL);
}

DBConnector::~DBConnector() {
    if (db) sqlite3_close(reinterpret_cast<sqlite3*>(db));
}

void DBConnector::write(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(reinterpret_cast<sqlite3*>(db), sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("DB write failed: " + err);
    }
}

std::vector<std::vector<std::string>> DBConnector::read(const std::string& sql) {
    std::vector<std::vector<std::string>> rows;
    char* errMsg = nullptr;
    auto cb = [](void* data, int argc, char** argv, char** colNames) -> int {
        auto* rows = reinterpret_cast<std::vector<std::vector<std::string>>*>(data);
        std::vector<std::string> row;
        for (int i = 0; i < argc; ++i) {
            row.emplace_back(argv[i] ? argv[i] : "");
        }
        rows->push_back(std::move(row));
        return 0;
    };
    if (sqlite3_exec(reinterpret_cast<sqlite3*>(db), sql.c_str(), cb, &rows, &errMsg) != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("DB read failed: " + err);
    }
    return rows;
}
