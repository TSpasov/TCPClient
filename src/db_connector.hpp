#pragma once
#include <string>
#include <vector>

class DBConnector {
    void* db{nullptr}; // sqlite3*

public:
    explicit DBConnector(const std::string& path);
    ~DBConnector();

    void write(const std::string& sql);
    std::vector<std::vector<std::string>> read(const std::string& sql);
};

