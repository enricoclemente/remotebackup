#include <iostream>
#include <optional>
#include <list>
#include <mutex>
#include <condition_variable>

enum class FileCommand {
    UPLOAD = 1,
    REMOVE = 3,
    RENAME = 4,
};

class FileOperation {
private:
    std::string path;
    FileCommand command;
    std::time_t last_write_time;
    int id;
    bool processing;
    std::mutex processing_m;
    bool abort;
    std::mutex abort_m;

public:
    FileOperation(std::string path, FileCommand command, std::time_t last_write_time, int id);
    std::string get_path() const;
    FileCommand get_command() const;
    std::time_t get_last_write_time() const;
    int get_id() const;
    void set_processing(bool flag);
    bool get_processing() const;
    void set_abort(bool flag);
    bool get_abort() const;
};

class OutputQueue {
private:
    std::list<std::shared_ptr<FileOperation>> queue;
    int dim;
    int free;
    int id_counter;
    std::mutex m;
    std::condition_variable cv;

public:
    OutputQueue();
    void add_file_operation(const std::string& path, FileCommand command, std::time_t last_write_time);
    std::shared_ptr<FileOperation> get_file_operation();
    void remove_file_operation(int id);
    int size();
};

