//
// Created by Enrico Clemente on 26/10/2020.
//

#include "OutputQueue.h"

void OutputQueue::push(file_operation element) {
    std::lock_guard lg(m);

    queue.push_back(element);
    dim++;
    free++;
    cv.notify_one();
}

std::optional<file_operation> OutputQueue::get() {
    std::unique_lock ul(m);
    cv.wait(ul, [this](){ return free > 0; });

    bool valid = true;

    for(auto i : queue) {
        if(!i.processing) {
            // check if there are not other process for the same file
            for(auto j : queue) {
                if(j.processing && j.path == i.path) {
                    valid = false;
                    break;
                }
            }
            if(valid) {
                i.processing = true;
                free--;
                return i;
            }
            valid = true;
        }
    }

    return std::nullopt;
}

void OutputQueue::pop(file_operation& fo) {
    std::unique_lock ul(m);
    cv.wait(ul, [this](){ return dim > 0; });

    auto it = queue.begin();
    while(it != queue.end()) {
        if(it->path == fo.path && it->time == fo.time ) {
            queue.erase(it);
            dim--;
            free--;
            break;
        }
    }
}

int OutputQueue::size() {
    std::lock_guard lg(m);
    return queue.size();
}