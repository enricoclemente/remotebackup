#include <sqlite3.h>

#include "Database.h"

// TODO: make queries throw exception in case of errors

int print_results(void* unused, int num_cols, char** row_fields, char** col_names)
{
    unused = 0;

    std::ostringstream oss;
    oss << "[DB] ";
    for (int i = 0; i < num_cols; i++)
        oss << col_names[i] << ": " << (row_fields[i] ? row_fields[i] : "NULL") << ", ";
    oss << std::endl;

    RBLog(oss.str());

    return 0;
}

Database::Database()
{
    RBLog("[DB] Database()");
}

void Database::init()
{
    // query("SELECT name FROM sqlite_master WHERE type='table' AND name='<table_name>';") // Another way to check for the existance of tables
    exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, password TEXT NOT NULL, token TEXT);");
    // TODO: make (username, path) unique
    exec("CREATE TABLE IF NOT EXISTS fs (id INTEGER PRIMARY KEY, username TEXT NOT NULL, path TEXT NOT NULL, hash TEXT NOT NULL DEFAULT '', last_write_time TEXT NOT NULL DEFAULT '', size TEXT NOT NULL DEFAULT '', tmp_chunks TEXT NOT NULL DEFAULT '');");
}

void Database::open()
{
    RBLog(std::string("[DB] SQLite version: ") + sqlite3_libversion());

    int res = sqlite3_open("test.db", &db);
    if (res != SQLITE_OK) {
        RBLog(std::string("[DB] Cannot open database: ") + sqlite3_errmsg(db));
        return close();
    }

    RBLog("[DB] DB opened successfully");

    init();
}

void Database::close()
{
    int res = sqlite3_close(db);
    if (res != SQLITE_OK) {
        RBLog(std::string("[DB] Cannot close database: ") + sqlite3_errmsg(db));
    }
}

void Database::exec(std::string sql) {
    char *errmsg = 0;

    int res = sqlite3_exec(db, sql.c_str(), print_results, nullptr, &errmsg); // Synchronous callback 'print_results'
    if (res != SQLITE_OK) {
        RBLog(std::string("[DB] Cannot execute statement: ") + errmsg);
        sqlite3_free(errmsg);
        return close();
    }

    RBLog(std::string("[DB] ") + sql);
}

std::unordered_map<int, std::vector<std::string>> Database::query(const std::string & sql, const std::initializer_list<std::string> & params) {
    sqlite3_stmt *stmt;

    int res = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (res != SQLITE_OK) {
        RBLog(std::string("[DB] Prepare error: ") + sqlite3_errmsg(db));
        RBLog(std::string("[DB] Cannot execute statement: ") + sql);
        throw RBException("internal_server_error");
    }

    int i = 1;
    for (auto param : params) {
        res = sqlite3_bind_text(stmt, i, param.c_str(), param.length(), SQLITE_TRANSIENT);
        if (res != SQLITE_OK) {
            sqlite3_finalize(stmt);
            RBLog(std::string("[DB] Bind error: ") + sqlite3_errmsg(db));
            RBLog(std::string("[DB] Cannot execute statement: ") + sql);
            throw RBException("internal_server_error");
        }
        i++;
    }

    std::unordered_map<int, std::vector<std::string>> results; // Defaults to {}, i.e. an empty map
    
    // Populate map<row, vector of columns> with db results
    // Note: SELECT COUNT(*) always returns one row and one column, even if WHERE clause is not met (count is 0).
    //       SELECT <field> can return no rows (and therefore no columns), if WHERE clause is not met.
    int row = 0;
    while ((res = sqlite3_step(stmt)) == SQLITE_ROW) { // While there are rows in the result set
        for (int col = 0; col < sqlite3_column_count(stmt); col++) { // Iterate over the row's columns
            auto result = sqlite3_column_text(stmt, col);
            results[row].push_back(std::string(reinterpret_cast<const char*>(result)));
        }
        row++;
    }
    if (res != SQLITE_DONE) {
        RBLog(std::string("[DB] Step error: ") + sqlite3_errmsg(db));
    } else {
        RBLog(std::string("[DB] ") + sql);
    }

    sqlite3_finalize(stmt);

    // Print query results
    std::ostringstream oss;
    oss << "[DB] Query rows: " << results.size() << std::endl;
    for (auto & [key, value] : results) {
        oss << "       [row " << key << "] ";
        for (auto & col : value)
            oss << col << ", ";
        oss << std::endl;
    }
    RBLog(oss.str());

    return results;
}