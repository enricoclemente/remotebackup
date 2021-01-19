#pragma once

#include <iostream>
#include <sqlite3.h>
#include <vector>

#include "RBHelpers.h"

// Singleton implementation
class Database
{
public:
    static Database &get_instance()
    {
        static Database instance;
        return instance;
    }

    Database(Database const &) = delete;
    void operator=(Database const &) = delete;

    void open();
    void close();
    void exec(std::string); // For statements without parameters, no returned results
    std::unordered_map<int, std::vector<std::string>> query(const std::string &, const std::initializer_list<std::string> &); // For statements with parameters, with returned results

private:
    Database();

    void init(); // Prepare database

    sqlite3 *db;
};
