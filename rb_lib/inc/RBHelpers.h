#pragma once
#include <memory>
#include <string>
#include <boost/asio.hpp>


typedef std::shared_ptr<boost::asio::ip::tcp::socket> sockPtr_t;

class RBNetException : std::exception {
  public:

  RBNetException(std::string msg) : msg(msg) {}

  const char * what() {
    return "RBNetException";
  }

  const std::string & getMsg() {
    return msg;
  }

  private:
  const std::string msg;
};
