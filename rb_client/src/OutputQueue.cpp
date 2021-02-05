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

    // TODO:  review this logic because there could be the case that there is a file in processing and the same to be processed
    auto not_found = processing_files.end();
    for (auto i : queue) {
        if (!i->get_processing()) {
            // check if there are not other processes for the same file
            if (processing_files.find(i->get_path()) != not_found) {
                valid = false;
            }

            if (valid) {
                i->set_processing(true);
                processing_files.emplace(i->get_path());
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
            processing_files.erase(val->get_path());
            cv.notify_all();
            return true;
        }
    }

    return false;
}


bool OutputQueue::remove_file_operation(int id) {
    std::unique_lock ul(m);
    cv.wait(ul, [this]() { return queue.size() > 0; });
    
    queue.remove_if([&](auto &e) {
        if (e->get_id() == id) {
            processing_files.erase(e->get_path());
            return true;
        }
        return false;
    });

    /* for (auto it = queue.begin(); it != queue.end(); it++) {
        if (it->get()->get_id() == id) {
            processing_files.erase(it->get()->get_path());
            queue.erase(it);
            return true;
        }
    } */
    return false;
}


int OutputQueue::size() {
    std::lock_guard lg(m);
    return queue.size();
}

// a, a*, b

int OutputQueue::free() {
    int sum(0);
    auto not_found = processing_files.end();

    for (const auto& val : queue) {
        if (!(val->get_processing())
            && processing_files.find(val->get_path()) == not_found) {
            sum++;
        }
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



