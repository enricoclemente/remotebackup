#include "OutputQueue.h"

// Output Queue Class implementations
OutputQueue::OutputQueue() : id_counter(0) {};


void OutputQueue::add_file_operation(const std::string &path, file_metadata metadata, FileCommand command) {
    std::lock_guard lg(m);

    auto fo = std::make_shared<FileOperation>(path, metadata, command, id_counter);

    // if there are old operations on the same file, overwrite them with the new one if they are not in processing
    // otherwise set abort flag true in order to interrupt the operation
    for (auto it = queue.begin(); it != queue.end(); it++) {
        if (it->get()->get_path() == path) {
            if (it->get()->get_processing()) {
                it->get()->set_abort(true);
            } else {
                queue.erase(it);
            }
        }
    }

    queue.push_back(fo);
    id_counter++;

    cv.notify_all();
}


std::shared_ptr<FileOperation> OutputQueue::get_file_operation() {
    std::unique_lock ul(m);
    cv.wait(ul, [this]() { return free() > 0; });

    bool valid = true;

    // TODO:  review this logic because the re could be the case that there is a file in processing
    //  and the same to be processed
    for (auto i : queue) {
        if (!i->get_processing()) {
            // check if there are not other processes for the same file
            for (const auto &j : queue) {
                if (j->get_processing() && j->get_path() == i->get_path()) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                i->set_processing(true);
                return i;
            }
            valid = true;
        }
    }

    throw std::logic_error("invalid_file_operation");
}


bool OutputQueue::free_file_operation(int id) {
    std::unique_lock ul(m);
    cv.wait(ul, [this]() { return queue.size() > 0; });

    for (const auto& val : queue) {
        if (val->get_id() == id) {
            val->set_processing(false);
            return true;
        }
    }

    // TODO: we have to make a notify, right?
    return false;
}


bool OutputQueue::remove_file_operation(int id) {
    std::unique_lock ul(m);
    cv.wait(ul, [this]() { return queue.size() > 0; });

    for (auto it = queue.begin(); it != queue.end(); it++) {
        if (it->get()->get_id() == id) {
            queue.erase(it);
            return true;
        }
    }
    return false;
}


int OutputQueue::size() {
    std::lock_guard lg(m);
    return queue.size();
}


int OutputQueue::free() {
    int sum(0);
    for (const auto& val : queue) {
        if (!(val->get_processing())) sum++;
    }
    return sum;
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
    processing = flag;
}

void FileOperation::set_abort(bool flag) {
    abort = flag;
}

bool FileOperation::get_processing() const {
    return processing.load();
}

bool FileOperation::get_abort() const {
    return abort.load();
}



