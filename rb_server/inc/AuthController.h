#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <openssl/sha.h>
#include <chrono>
#include <ctime>

#include "AsioAdapting.h"
#include "ProtobufHelpers.h"
#include "RBHelpers.h"
#include "rbproto.pb.h"
#include "Database.h"

// Singleton implementation
class AuthController {
public:
    static AuthController &get_instance()
    {
        static AuthController instance;
        return instance;
    }

    AuthController(AuthController const &) = delete;
    void operator=(AuthController const &) = delete;

    void auth_by_credentials(std::string, std::string);
    void auth_by_token(std::string);
    std::string auth_get_user_by_token(const std::string&);
    std::string generate_token(std::string);
    std::string sha256(const std::string);

    void add_user(const std::string & username, const std::string & pw);

private:
    AuthController();
};