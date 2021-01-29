#include "RBHelpers.h"
#include <iostream>
#include <mutex>
#include <boost/date_time/posix_time/posix_time.hpp>

void excHandler(RBException &e) {
    std::cout << "RBException:" << e.getMsg() << std::endl;
}

void excHandler(std::exception &e) {
    std::cout << "Error occured! Message: " << e.what();
}


void RBLog(const std::string & s, LogLevel level) {
    // catching time
    auto now = boost::posix_time::microsec_clock::local_time();
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    bool print = true;
    std::string level_to_print;
    // time calculus
    auto td = now.time_of_day();
    const long hours        = td.hours();
    const long minutes      = td.minutes();
    const long seconds      = td.seconds();
    const long milliseconds = td.total_milliseconds() -
                                ((hours * 3600 + minutes * 60 + seconds) * 1000);
    const long microseconds = td.total_microseconds() -
                                ((hours * 3600 + minutes * 60 + seconds) * 1000000);

    if(level == LogLevel::DEBUG) {
        level_to_print = "DEBUG";
        print = DEBUG_PRINT != 0;
    } else if(level == LogLevel::INFO) {
        level_to_print = "INFO";
        print = INFO_PRINT != 0;
    } else if(level == LogLevel::ERROR) {
        level_to_print = "ERROR";
        print = ERROR_PRINT != 0;
    }

    if(print)
        std:: cout << level_to_print <<": "<< hours <<":"<< minutes <<":"<< seconds <<"."<< microseconds <<
            " >> " << s << std::endl;
}


int count_segments(uint64_t size) {
    int num_segments = size / RB_MAX_SEGMENT_SIZE;
    return size % RB_MAX_SEGMENT_SIZE == 0
        ? num_segments
        : num_segments + 1;
}


std::uint32_t calculate_checksum(const fs::path &file_path) {
    std::ifstream ifs(file_path.string());
    if (ifs.fail()) throw std::runtime_error("Error opening file");

    std::size_t file_len = fs::file_size(file_path);

    int chunk_size = 1000000;
    std::vector<char> chunk(chunk_size, 0);
    boost::crc_32_type crc;

    size_t tot_read = 0;
    size_t current_read;
    while (tot_read < file_len) {
        if (file_len - tot_read >= chunk_size) {
            ifs.read(&chunk[0], chunk_size);
        } else {
            ifs.read(&chunk[0], file_len - tot_read);
        }

        if (!ifs) throw std::runtime_error("Error reading file chunk");

        current_read = ifs.gcount();
        crc.process_bytes(&chunk[0], current_read);

        tot_read += current_read;
    }

    return crc.checksum();
}

void validateRBProto(RBResponse & res, RBMsgType type, int ver, bool exactVer) {
    if (res.type() != type)
        throw RBProtoTypeException("unexpected_rbproto_response_type");
    if ((exactVer && res.protover() != ver) || res.protover() < ver)
        throw RBProtoVerException("unsupported_rbproto_version");

    if (!res.success()) {
        throw RBException(res.error());
    }

    if (res.type() == RBMsgType::AUTH && !res.has_auth_response())
        throw RBException("invalid_rbproto_auth_response");
}

void validateRBProto(RBRequest & req, RBMsgType type, int ver, bool exactVer) {
    if (req.type() != type)
        throw RBProtoTypeException("unexpected_rbproto_request_type");
    if ((exactVer && req.protover() != ver) || req.protover() < ver)
        throw RBProtoVerException("unsupported_rbproto_version");
    if (type == RBMsgType::AUTH && !req.has_auth_request())
        throw RBProtoTypeException("invalid_rbproto_auth_request");
    if ((type == RBMsgType::UPLOAD || type == RBMsgType::REMOVE)
        && !req.has_file_segment())
        throw RBProtoTypeException("invalid_rbproto_file_request");
}