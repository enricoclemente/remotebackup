#pragma once

#include <iostream>
#include <sqlite3.h>

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

    void init();
    void open();
    void close();
    void query(std::string);

private:
    Database();

    sqlite3 *db;
};