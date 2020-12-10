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
    int nSegments = size / RB_MAX_SEGMENT_SIZE;
    if (size % RB_MAX_SEGMENT_SIZE == 0) return nSegments;
    return nSegments + 1;
}

void validateRBProto(RBResponse & res, RBMsgType type, int ver, bool exactVer) {
    if (res.type() != type)
        throw RBProtoTypeException("unexpected response type");
    if ((exactVer && res.protover() != ver) || res.protover() < ver)
        throw RBProtoVerException("unsupported proto version");
}

void validateRBProto(RBRequest & req, RBMsgType type, int ver, bool exactVer) {
    if (req.type() != type)
        throw RBProtoTypeException("unexpected response type");
    if ((exactVer && req.protover() != ver) || req.protover() < ver)
        throw RBProtoVerException("unsupported proto version");
}