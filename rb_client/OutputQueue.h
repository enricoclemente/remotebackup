//
// Created by Enrico Clemente on 26/10/2020.
//

#ifndef TCPCLIENT_OUTPUTQUEUE_H
#define TCPCLIENT_OUTPUTQUEUE_H

#include "FileWatcher.h"

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>


struct file_transfer {
    std::string file_path;
    std::time_t last_write_time;
    FileStatus command;
};

class OutputQueue {
    std::vector<file_transfer> queue;
    int dim;
    std::mutex m;
    std::condition_variable cv;

public:
    bool push(file_transfer element) {
        std::lock_guard lg(m);

        bool push = true;
        auto it = queue.begin();

        // check if there is a more recent operation on that file
        while(it != queue.end()) {
            if(it->file_path == element.file_path && it->last_write_time < element.last_write_time) {
                push = false;
                break;
            }
        }

        if(push) {
            queue.push_back(element);
            dim++;
            cv.notify_one();
        }

        return push;
    }

    file_transfer pop() {
        std::unique_lock ul(m);
        cv.wait(ul, [this](){ return !this->queue.empty(); });
        file_transfer output = queue.front();
        queue.erase(queue.begin());
        dim--;
        return output;
    }

    int size() {
        std::lock_guard lg(m);
        return queue.size();
    }
};


#endif //TCPCLIENT_OUTPUTQUEUE_H
