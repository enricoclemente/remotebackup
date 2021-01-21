#pragma once

#include <boost/crc.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "rbproto.pb.h"

namespace fs = boost::filesystem;

#define RB_MAX_SEGMENT_SIZE 1048576  // 1MiB

typedef std::shared_ptr<boost::asio::ip::tcp::socket> sockPtr_t;

class RBException : public std::exception {
public:
    RBException(const std::string& msg) : msg(msg) {}

    const char *what() const throw() { return "RBException: "; }

    const std::string & getMsg() { return msg; }

private:
    const std::string msg;
};

class RBProtoTypeException : public RBException {
public:
    using RBException::RBException; // inherit constructor
    const char *what() const throw() { return "RBProtoTypeException: "; }
};

class RBProtoVerException : public RBException {
public:
    using RBException::RBException; // inherit constructor
    const char *what() const throw() { return "RBProtoVerException: "; }
};

int count_segments(uint64_t size);
// It can throw a runtime error because of file errors
std::uint32_t calculate_checksum(const fs::path &file_path);

void validateRBProto(RBRequest &, RBMsgType, int ver, bool exactVer = false);
void validateRBProto(RBResponse &, RBMsgType, int ver, bool exactVer = false);

void excHandler(std::exception &e);
void excHandler(RBException &e);
void RBLog(const std::string & s);
