#pragma once

#include <boost/crc.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "rbproto.pb.h"

#define RB_MAX_SEGMENT_SIZE 1048576  // 1MiB

#define DEBUG_PRINT 1
#define INFO_PRINT 1
#define ERROR_PRINT 1


namespace fs = boost::filesystem;
typedef std::shared_ptr<boost::asio::ip::tcp::socket> sockPtr_t;


enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    ERROR = 2
};


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

// Utils function shared between client and server
int count_segments(uint64_t size);
// It can throw a runtime error because of file errors
std::uint32_t calculate_checksum(const fs::path &file_path);

void validateRBProto(RBRequest &, RBMsgType, int ver, bool exactVer = false);
void validateRBProto(RBResponse &, RBMsgType, int ver, bool exactVer = false);

void RBLog(const std::string & s, LogLevel level = LogLevel::DEBUG);

std::thread make_watchdog(
    std::chrono::system_clock::duration interval,
    std::function<bool(void)> cond_checker,
    std::function<void(void)> wd_function
);
