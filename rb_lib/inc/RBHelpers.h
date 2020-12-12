#pragma once

#include <boost/asio.hpp>
#include <exception>
#include <memory>
#include <string>
#include "rbproto.pb.h"

#define RB_MAX_SEGMENT_SIZE 1048576  // 1MiB

int count_segments(uint64_t size);

typedef std::shared_ptr<boost::asio::ip::tcp::socket> sockPtr_t;

class RBException : std::exception {
public:
    RBException(std::string msg) : msg(msg) {}

    const char *what() { return "RBException: "; }

    const std::string &getMsg() { return msg; }

private:
    const std::string msg;
};

class RBProtoTypeException : RBException {
public:
    using RBException::RBException; // inherit constructor
    const char *what() { return "RBProtoTypeException: "; }
};

class RBProtoVerException : RBException {
public:
    using RBException::RBException; // inherit constructor
    const char *what() { return "RBProtoVerException: "; }
};


void validateRBProto(RBRequest &, RBMsgType, int ver, bool exactVer = false);
void validateRBProto(RBResponse &, RBMsgType, int ver, bool exactVer = false);

void excHandler(std::exception &e);
void excHandler(RBException &e);
void RBLog(std::string s);
