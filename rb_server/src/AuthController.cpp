#include <openssl/sha.h>

#include "AuthController.h"

AuthController::AuthController() {
    RBLog("AuthController()");
}

bool AuthController::authenticate(std::string username, std::string password) {
    auto& db = Database::get_instance();

    std::string hash = sha256(password);

    std::string stmt = std::string("SELECT COUNT(*) FROM USERS WHERE username = '") + username +
                       std::string("' AND password = '") + hash + "';"; // Vulnerable to SQL injection attacks

    db.query(stmt);

    return false;
}

bool AuthController::authenticate(std::string token) {
    auto& db = Database::get_instance();

    std::string stmt = std::string("SELECT COUNT(*) FROM USERS WHERE token = '") + token + "';"; // Vulnerable to SQL injection attacks

    db.query(stmt);

    return false;
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