#include "AuthController.h"

AuthController::AuthController() {
    RBLog("AuthController()");
}

bool AuthController::auth_by_credentials(std::string username, std::string password) {
    auto& db = Database::get_instance();

    std::string hash = sha256(password);
    std::string sql = "SELECT COUNT(*) FROM users WHERE username = ? AND password = ?;";
    auto count = std::stoi(db.query(sql, {username, hash}).at(0));

    return count == 1;
}

bool AuthController::auth_by_token(std::string token) {
    auto& db = Database::get_instance();

    std::string sql = "SELECT COUNT(*) FROM users WHERE token = ?;";
    auto count = std::stoi(db.query(sql, {token}).at(0));

    return count == 1;
}

std::string AuthController::sha256(const std::string str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)    
        ss << std::hex << (int) hash[i];

    return ss.str();
}