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
