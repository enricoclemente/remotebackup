#include <sqlite3.h>

#include "Database.h"

int print_results(void* unused, int num_cols, char** row_fields, char** col_names)
{
    unused = 0;

    std::ostringstream oss;
    for (int i = 0; i < num_cols; i++)
        oss << col_names[i] << ": " << (row_fields[i] ? row_fields[i] : "NULL") << ", ";
    oss << std::endl;

    RBLog("DB >> " + oss.str());

    return 0;
}

Database::Database()
{
    RBLog("DB >> Database()");
}

void Database::init()
{
    exec("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT UNIQUE NOT NULL, password TEXT NOT NULL, token TEXT);");
    exec("CREATE TABLE IF NOT EXISTS fs (id INTEGER PRIMARY KEY, username TEXT NOT NULL, path TEXT NOT NULL, hash TEXT NOT NULL DEFAULT '', last_write_time TEXT NOT NULL DEFAULT '', size TEXT NOT NULL DEFAULT '', last_chunk TEXT NOT NULL DEFAULT '', UNIQUE(username, path) ON CONFLICT REPLACE);");
}

void Database::open()
{
    RBLog(std::string("DB >> SQLite version: ") + sqlite3_libversion(), LogLevel::INFO);

    int res = sqlite3_open(db_path.c_str(), &db);
    if (res != SQLITE_OK) {
        RBLog(std::string("DB >> Cannot open database: ") + sqlite3_errmsg(db), LogLevel::ERROR);
        // CHECK
        close();
        throw RBException("db_open_error");
    }

    RBLog("DB >> Database opened successfully", LogLevel::INFO);

    init();
}

void Database::close()
{
    if (db == nullptr) return;
    int res = sqlite3_close(db);
    if (res != SQLITE_OK) {
        RBLog(std::string("DB >> Cannot close database: ") + sqlite3_errmsg(db), LogLevel::ERROR);
        throw RBException("db_close_error");
    }
    db = nullptr;

    RBLog("DB >> Database closed successfully", LogLevel::INFO);
}

void Database::clear() {
    if (db != nullptr) throw RBException("no_clear_open_db");
    boost::filesystem::remove(db_path);
}

void Database::exec(std::string sql) {
    char *errmsg = 0;

    int res = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (res != SQLITE_OK) {
        RBLog(std::string("DB >> Cannot execute statement: ") + errmsg, LogLevel::ERROR);
        sqlite3_free(errmsg);
        throw RBException("internal_server_error");
    }

    RBLog("DB >> " + sql, LogLevel::DEBUG);
}

std::unordered_map<int, std::vector<std::string>> Database::query(
    const std::string & sql, const std::initializer_list<std::string> & params, bool throwOnStep) {
    Statement stmt{sql, params, throwOnStep};

    auto &db = Database::get_instance();
    stmt.prepare(db);
    stmt.bind(db);
    const auto& results = stmt.step(db); // Extending the lifetime of the object created by step
    // stmt.print_results(results); // Not looking good as a Statement's function

    return results;
}

Statement::Statement(const std::string& sql, const std::initializer_list<std::string>& params, bool throwOnStep)
    : sql(sql), params(params), throwOnStep(throwOnStep) {
    RBLog("Statement()");
}

Statement::~Statement() {
    RBLog("~Statement()");
    sqlite3_finalize(stmt);
}

void Statement::prepare(Database& db_instance) {
    int res = sqlite3_prepare_v2(db_instance.db, sql.c_str(), -1, &stmt, nullptr);
    if (res != SQLITE_OK) {
        RBLog(std::string("DB >> Prepare error: ") + sqlite3_errmsg(db_instance.db), LogLevel::ERROR);
        RBLog("DB >> Cannot execute statement: " + sql, LogLevel::ERROR);
        throw RBException("internal_server_error");
    }
}

void Statement::bind(Database& db_instance) {
    int i = 1;
    for (auto param : params) {
        int res = sqlite3_bind_text(stmt, i, param.c_str(), param.length(), SQLITE_TRANSIENT);
        if (res != SQLITE_OK) {
            sqlite3_finalize(stmt);
            RBLog(std::string("DB >> Bind error: ") + sqlite3_errmsg(db_instance.db), LogLevel::ERROR);
            RBLog("DB >> Cannot execute statement: " + sql, LogLevel::ERROR);
            sqlite3_finalize(stmt);
            throw RBException("internal_server_error");
        }
        i++;
    }
}

std::unordered_map<int, std::vector<std::string>> Statement::step(Database& db_instance) {
  std::unordered_map<int, std::vector<std::string>> results; // Defaults to {}, i.e. an empty map

  // Populate map<row, vector of columns> with db results
  // Note: SELECT COUNT(*) always returns one row and one column, even if WHERE clause is not met (count is 0).
  //       SELECT <field> can return no rows (and therefore no columns), if WHERE clause is not met.
  int res;
  int row = 0;
  while ((res = sqlite3_step(stmt)) == SQLITE_ROW) { // While there are rows in the result set
      
      for (int col = 0; col < sqlite3_column_count(stmt); col++) { // Iterate over the row's columns
          auto result = sqlite3_column_text(stmt, col);
          results[row].push_back(std::string(reinterpret_cast<const char*>(result)));
      }
      row++;
  }

  if (res != SQLITE_DONE) {
      if (throwOnStep) {
          sqlite3_finalize(stmt);
          throw RBException(
              std::string("db_step_error:") + sqlite3_errmsg(db_instance.db));
      } else
          RBLog(std::string("DB >> Step error: ") + 
              sqlite3_errmsg(db_instance.db), LogLevel::ERROR);    
  } else
      RBLog("DB >> " + sql, LogLevel::DEBUG);

  return results;
}

void Statement::print_results(const std::unordered_map<int, std::vector<std::string>>& results ) {
  std::ostringstream oss;
  oss << "Query rows: " << results.size() << std::endl;
  for (auto & [key, value] : results) {
      oss << "       [row " << key << "] ";
      for (auto & col : value)
          oss << col << ", ";
      oss << std::endl;
  }
  RBLog("DB >> " + oss.str());
}