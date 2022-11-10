#include "FileManager.h"

#include <iostream>
#include <optional>
#include <list>
#include <set>
#include <mutex>
#include <condition_variable>
#include <atomic>


enum class FileCommand {
    UPLOAD = 1,
    REMOVE = 3,
    RENAME = 4,
};


class FileOperation {
private:
    std::string path;
    FileCommand command;
    file_metadata metadata;
    int id;
    std::atomic<bool> processing{};
    std::atomic<bool> abort{};
    void set_processing(bool flag);
    bool get_processing() const;
    friend class OutputQueue;       // only outputqueue can handle processing

public:
    FileOperation(std::string path, file_metadata metadata, FileCommand command, int id);
    std::string get_path() const;
    FileCommand get_command() const;
    file_metadata get_metadata() const;
    int get_id() const;
    void set_abort(bool flag);
    bool get_abort() const;
};


class OutputQueue {
private:
    std::list<std::shared_ptr<FileOperation>> queue;
    std::unordered_set<std::string> processing_files;
    int id_counter;
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> keep_going = true;
    std::thread watchdog = make_watchdog(
        std::chrono::seconds(3),
        [this]() { return keep_going.load(); },
        [this]() { cv.notify_all(); }
    );

public:
    OutputQueue();
    void add_file_operation(const std::string &path, file_metadata metadata, FileCommand command);
    std::shared_ptr<FileOperation> get_file_operation();
    bool free_file_operation(int id);
    bool remove_file_operation(int id);
    int size();
    void stop() {
        keep_going = false;
        cv.notify_all();
    }
};
