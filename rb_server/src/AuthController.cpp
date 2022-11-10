#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include "AuthController.h"

AuthController::AuthController() {
    RBLog("AuthController()");
}

void AuthController::auth_by_credentials(std::string username, std::string password) {
    auto& db = Database::get_instance();

    if (username.empty() || password.empty())
        throw RBException("login_failed_wrong_credentials");

    std::string hash = sha256(password);
    std::string sql = "SELECT COUNT(*) FROM users WHERE username = ? AND password = ?;";
    auto results = db.query(sql, {username, hash});

    auto count = std::stoi(results[0][0]);
    if (!count) throw RBException("login_failed_wrong_credentials");

    if (count > 1) RBLog("WARNING: DUPLICATED USER", LogLevel::INFO);
}

void AuthController::auth_by_token(std::string token) {
    auto& db = Database::get_instance();

    if (token.empty()) throw RBException("unauthorized");

    std::string sql = "SELECT COUNT(*) FROM users WHERE token = ?;";
    auto results = db.query(sql, {token});

    auto count = std::stoi(results[0][0]);
    if (!count) throw RBException("unauthorized");

    if (count > 1) RBLog("WARNING: DUPLICATED USER", LogLevel::INFO);
}

std::string AuthController::auth_get_user_by_token(const std::string& token) {
    auto& db = Database::get_instance();

    if (token.empty()) throw RBException("unauthenticated");

    std::string sql = "SELECT username FROM users WHERE token = ?;";
    auto results = db.query(sql, {token});

    if (results.empty())
        throw RBException("unauthenticated");

    auto username = results[0][0];

    return username;
}

std::string AuthController::generate_token(std::string username) {
    auto& db = Database::get_instance();

    using namespace std::chrono;
    auto time = system_clock::to_time_t(system_clock::now());

    boost::mt19937 ran;
    ran.seed(time);
    boost::uuids::basic_random_generator<boost::mt19937> gen(&ran);
    boost::uuids::uuid u = gen();
    std::string token = boost::lexical_cast<std::string>(u);
    db.query(
        "UPDATE users SET token = ? WHERE username = ?;",
        {token, username}
    );

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

void AuthController::add_user(const std::string & username, const std::string & pw) {
    auto& db = Database::get_instance();
    std::string hash = AuthController::get_instance().sha256(pw);
    db.query("INSERT INTO users (username, password) VALUES (?, ?);", 
        {username, hash}, true);
}
