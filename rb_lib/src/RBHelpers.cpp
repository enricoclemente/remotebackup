#include "RBHelpers.h"
#include <iostream>
#include <mutex>

void excHandler(RBException &e) {
    std::cout << "RBException:" << e.getMsg() << std::endl;
}

void excHandler(std::exception &e) {
    std::cout << "Error occured! Message: " << e.what();
}

void RBLog(std::string s) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    std:: cout << ">> " << s << std::endl;
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
}

void validateRBProto(RBRequest & req, RBMsgType type, int ver, bool exactVer) {
    if (req.type() != type)
        throw RBProtoTypeException("unexpected_rbproto_request_type");
    if ((exactVer && req.protover() != ver) || req.protover() < ver)
        throw RBProtoVerException("unsupported_rbproto_version");
    if (type == RBMsgType::AUTH && !req.has_auth_request())
        throw RBProtoVerException("invalid_rbproto_auth_pack");
    if ((type == RBMsgType::UPLOAD || type == RBMsgType::UPLOAD)
        && !req.has_file_segment())
        throw RBProtoVerException("invalid_rbproto_auth_pack");
}