#include <sqlite3.h>

#include "Database.h"

int print_results(void* unused, int num_cols, char** row_fields, char** col_names)
{
    unused = 0;

    for (int i = 0; i < num_cols; i++) {
        printf("%s: %s, ", col_names[i], row_fields[i] ? row_fields[i] : "NULL");
    }

    printf("\n");

    return 0;
}

Database::Database()
{
    RBLog("Database()");
}

void Database::init()
{
    // query("SELECT name FROM sqlite_master WHERE type='table' AND name='<table_name>';") // Another way to check for the existance of tables
    exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, password TEXT NOT NULL, token TEXT);");
    exec("CREATE TABLE IF NOT EXISTS fs (id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, path TEXT NOT NULL, filename TEXT NOT NULL, hash TEXT NOT NULL, size TEXT NOT NULL, tmp_chunks TEXT);");
}

void Database::open()
{
    RBLog(std::string("SQLite version: ") + sqlite3_libversion());

    int res = sqlite3_open("test.db", &db);
    if (res != SQLITE_OK) {
        RBLog(std::string("Cannot open database: ") + sqlite3_errmsg(db));
        return close();
    }

    RBLog("DB opened successfully");

    init();
}

void Database::close()
{
    int res = sqlite3_close(db);
    if (res != SQLITE_OK) {
        RBLog(std::string("Cannot close database: ") + sqlite3_errmsg(db));
    }
}

void Database::exec(std::string sql) {
    char *errmsg = 0;

    int res = sqlite3_exec(db, sql.c_str(), print_results, nullptr, &errmsg); // Synchronous callback 'print_results'
    if (res != SQLITE_OK) {
        RBLog(std::string("Cannot execute statement: ") + errmsg);
        sqlite3_free(errmsg);
        return close();
    }

    RBLog(std::string("Executed: ") + sql);
}
