//
// Created by Enrico Clemente on 26/10/2020.
//

#ifndef TCPCLIENT_OUTPUTQUEUE_H
#define TCPCLIENT_OUTPUTQUEUE_H

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class OutputQueue {
    std::queue<T> queue;
    int dim;
    std::mutex m;
    std::condition_variable cv;

public:
    void push(T element) {
        std::lock_guard lg(m);
        queue.push(element);
        dim++;
        cv.notify_one();
    }

    T pop() {
        std::unique_lock ul(m);
        cv.wait(ul, [this](){ return !this->queue.empty(); });
        T output = queue.front();
        queue.pop();
        dim--;
        return output;
    }

    int size() {
        std::lock_guard lg(m);
        return queue.size();
    }
};


#endif //TCPCLIENT_OUTPUTQUEUE_H
