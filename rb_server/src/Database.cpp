#include <sqlite3.h>

#include "Database.h"

/*
int callback(void *, int, char **, char **);

int callback(void* unused, int num_cols, char** row_fields, char** col_names)
{
    unused = 0;

    for (int i = 0; i < num_cols; i++)
    {
        printf("%s: %s | ", col_names[i], row_fields[i] ? row_fields[i] : "NULL");
    }

    printf("\n");

    return 0;
}
*/

Database::Database()
{
    RBLog("Database()");
}

void Database::init()
{
    // query("SELECT name FROM sqlite_master WHERE type='table' AND name='<table_name>';") // Another way to check for the existance of tables
    query("CREATE TABLE IF NOT EXISTS USERS(id INT PRIMARY KEY NOT NULL, username TEXT NOT NULL, password TEXT NOT NULL, TOKEN TEXT);");
    query("CREATE TABLE IF NOT EXISTS FS(id INT PRIMARY KEY NOT NULL, filename TEXT NOT NULL, path TEXT NOT NULL, tmp_chunks INT);");
}

void Database::open()
{
    RBLog(std::string("SQLite version: ") + sqlite3_libversion());

    int res = sqlite3_open("test.db", &db);
    if (res != SQLITE_OK)
    {
        RBLog(std::string("Cannot open database: ") + sqlite3_errmsg(db));
        return close();
    }

    RBLog("DB opened successfully");

    init();
}

void Database::close()
{
    int res = sqlite3_close(db);
    if (res != SQLITE_OK)
    {
        RBLog(std::string("Cannot close database: ") + sqlite3_errmsg(db));
    }
}

void Database::query(std::string sql)
{
    char *errmsg = 0;
    int res = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (res != SQLITE_OK)
    {
        RBLog(std::string("Cannot execute statement: ") + errmsg);
        sqlite3_free(errmsg);
        return close();
    }

    RBLog(std::string("Executed: ") + sql);
}
