#pragma once

#include <memory>
#include <string>
#include <exception>
#include <boost/asio.hpp>

typedef std::shared_ptr<boost::asio::ip::tcp::socket> sockPtr_t;

class RBException : std::exception {
  public:

  RBException(std::string msg) : msg(msg) {}

  const char * what() {
    return "RBException";
  }

  const std::string & getMsg() {
    return msg;
  }

  private:
  const std::string msg;
};

void excHandler(std::exception &e);
void excHandler(RBException &e);
void RBLog(std::string s);
