#pragma once

#include <openssl/md5.h>

#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <shared_mutex>

#include "Database.h"
#include "RBHelpers.h"

namespace fs = boost::filesystem;
namespace ch = std::chrono;

class FileSystemManager {
public:
    FileSystemManager(const fs::path & root) : root(root) {
        cleanup_empty_folders();
    };
    std::unordered_map<std::string, RBFileMetadata> get_files(const std::string&);
    bool file_exists(std::string, const fs::path&);
    void write_file(const std::string & username, const RBRequest & req);
    void remove_file(const std::string & username, const RBRequest & req);
    void read_file_segment(const std::string&, const RBRequest&, RBResponse&);
    std::string md5(fs::path);
    std::string get_hash(std::string, const fs::path&);
    std::string get_size(std::string, const fs::path&);
    std::string get_last_write_time(std::string, const fs::path&);
    void clear();
    ~FileSystemManager() {
        keep_going = false;
        watchdog.join();
    }

private:
    std::string to_string(unsigned char*);
    fs::path root;
    std::shared_mutex mutex;
    void cleanup_empty_folders();
    std::atomic<bool> keep_going;
    std::thread watchdog = make_watchdog(std::chrono::seconds(600),
        [this]() { return keep_going.load(); },
        [this]() { cleanup_empty_folders(); }
    );  
};
