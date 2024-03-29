#include "OutputQueue.h"

// Output Queue Class implementations
OutputQueue::OutputQueue() : id_counter(0){};

void OutputQueue::add_file_operation(const std::string& path, file_metadata metadata, FileCommand command) {
    std::unique_lock ul(m);

    cv.wait(ul, [this]() { return queue.size() < 100 || !keep_going; });

    if (!keep_going) throw RBException("stop");
    
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
    auto not_found = processing_files.end();

    while (true) {
        if (!keep_going) throw RBException("stop");
        for (const auto& val : queue) {
            if (!(val->get_processing()) && processing_files.find(val->get_path()) == not_found) {
                val->set_processing(true);
                processing_files.emplace(val->get_path());
                return val;
            }
        }
        cv.wait(ul);
    }
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

    queue.remove_if([&](auto& e) {
        if (e->get_id() == id) {
            processing_files.erase(e->get_path());
            if (queue.size() < 75)
                cv.notify_all();
            return true;
        }
        return false;
    });

    return false;
}

int OutputQueue::size() {
    std::lock_guard lg(m);
    return queue.size();
}

// File Operation Class implementations
FileOperation::FileOperation(std::string path, file_metadata metadata, FileCommand command, int id) : path(std::move(path)), command(command), metadata(metadata), id(id) {
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
