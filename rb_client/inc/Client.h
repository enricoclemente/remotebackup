#pragma once

#include "RBHelpers.h"
#include <boost/asio.hpp>
#include <string>
#include <functional>
#include "rb_request.pb.h"
#include "rb_response.pb.h"

class Client {
public:
    Client(std::string, std::string);
    RBResponse run(RBRequest);

private:
    boost::asio::ip::tcp::resolver::iterator endpoints;
    boost::system::error_code ec;
    boost::asio::io_service io_service;
};
