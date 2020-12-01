//
// Created by Enrico Clemente on 26/10/2020.
//

#ifndef TCPCLIENT_OUTPUTQUEUE_H
#define TCPCLIENT_OUTPUTQUEUE_H

#include "FileWatcher.h"

#include <iostream>
#include <optional>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>


struct file_operation {
    std::string path;
    std::uint32_t checksum;
    FileStatus command;
    std::time_t time;
    bool processing;
};

class OutputQueue {
    std::vector<file_operation> queue;
    int dim;
    int free;
    std::mutex m;
    std::condition_variable cv;

public:
    void push(file_operation element);

    std::optional<file_operation> get();

    void pop(file_operation& fo);

    int size();
};


#endif //TCPCLIENT_OUTPUTQUEUE_H
