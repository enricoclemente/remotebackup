#include "AuthController.h"

AuthController::AuthController() {
    RBLog("AuthController()");
}

bool AuthController::auth_by_credentials(std::string username, std::string password) {
    auto& db = Database::get_instance();

    std::string hash = sha256(password);
    std::string sql = "SELECT COUNT(*) FROM users WHERE username = ? AND password = ?;";

    auto results = db.query(sql, {username, hash});
    if (results.empty() || results[0].empty()) {
        RBLog("Error executing the statement");
        return false;
    }

    auto count = std::stoi(results[0].at(0));

    return count == 1;
}

bool AuthController::auth_by_token(std::string token) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT COUNT(*) FROM users WHERE token = ?;";
    
    auto results = db.query(sql, {token});
    if (results.empty() || results[0].empty()) {
        RBLog("Error executing the statement");
        return false;
    }

    auto count = std::stoi(results[0].at(0));

    return count == 1;
}

std::string AuthController::auth_get_user_by_token(const std::string& token) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT username FROM users WHERE token = ?;";

    auto results = db.query(sql, {token});
    if (results.empty() || results[0].empty()) {
        RBLog("Error executing the statement");
        return "";
    }

    auto username = results[0].at(0);

    return username;
}

std::string AuthController::generate_token(std::string username) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT password FROM users WHERE username = ?;";
    
    auto results = db.query(sql, {username});
    if (results.empty() || results[0].empty()) {
        RBLog("Error executing the statement");
        return "";
    }

    auto password = results[0].at(0);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::string token = sha256(username + password + ctime(&time));
    sql = "UPDATE users SET token = ? WHERE username = ?;";
    db.query(sql, {token, username});

    return token;
}

std::string AuthController::sha256(const std::string str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::hex << (int)hash[i];

    return ss.str();
}