#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

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

    bool authenticate(std::string, std::string); // Username and password authentication
    bool authenticate(std::string); // Token authentication
    
private:
    AuthController();
    
    std::string sha256(const std::string);
};