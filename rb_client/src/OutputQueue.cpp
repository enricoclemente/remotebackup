#include "OutputQueue.h"

// Output Queue Class implementations
OutputQueue::OutputQueue() : dim(0), free(0), id_counter(0) {};

void OutputQueue::add_file_operation(const std::string &path, file_metadata metadata, FileCommand command) {
    std::lock_guard lg(m);

    auto fo = std::make_shared<FileOperation>(path, metadata, command, id_counter);

    // if there are old operations on the same file, overwrite them with the new one if they are not in process
    // otherwise set abort flag true in order to interrupt the operation
    auto it = queue.begin();
    while (it != queue.end()) {
        if (it->get()->get_path() == path) {
            if (it->get()->get_processing()) {
                it->get()->set_abort(true);
            } else {
                queue.erase(it);
                dim--;
                free--;
            }
        }
        it++;
    }

    queue.push_back(fo);
    id_counter++;
    dim++;
    free++;

    cv.notify_one();
}

std::shared_ptr<FileOperation> OutputQueue::get_file_operation() {
    std::unique_lock ul(m);
    cv.wait(ul, [this]() { return free > 0; });

    bool valid = true;

    for (auto i : queue) {
        if (!i->get_processing()) {
            // check if there are not other process for the same file
            for (const auto &j : queue) {
                if (j->get_processing() && j->get_path() == i->get_path()) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                i->set_processing(true);
                free--;
                return i;
            }
            valid = true;
        }
    }

    return nullptr;
}

bool OutputQueue::remove_file_operation(int id) {
    // TODO remove if
    std::unique_lock ul(m);
    cv.wait(ul, [this]() { return dim > 0; });

    auto it = queue.begin();
    while (it != queue.end()) {
        if (it->get()->get_id() == id) {
            queue.erase(it);
            dim--;
            free--;
            return true;
        }
        it++;
    }

    return false;
}

int OutputQueue::size() {
    std::lock_guard lg(m);
    return queue.size();
}


// File Operation Class implementations
FileOperation::FileOperation(std::string path, file_metadata metadata, FileCommand command, int id) :
        path(std::move(path)), command(command), metadata(metadata), id(id) {
    processing = false;
    abort = false;
}

std::string FileOperation::get_path() const {
    return path;
}

FileCommand FileOperation::get_command() const {
    return command;
}

file_metadata FileOperation::get_metadata() const {
    return metadata;
}

int FileOperation::get_id() const {
    return id;
}

void FileOperation::set_processing(bool flag) {
    std::lock_guard lg(processing_m);

    processing = flag;
}

void FileOperation::set_abort(bool flag) {
    std::lock_guard lg(abort_m);

    abort = flag;
}

bool FileOperation::get_processing() const {
    return processing;
}

bool FileOperation::get_abort() const {
    return abort;
}



