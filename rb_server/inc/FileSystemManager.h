#pragma once

#include <openssl/md5.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "Database.h"
#include "RBHelpers.h"

namespace fs = std::filesystem;
namespace ch = std::chrono;

class FileSystemManager {
public:
    bool find_file(std::string, fs::path);
    bool write_file(std::string, fs::path, std::string);
    bool remove_file(std::string, fs::path);
    std::string md5(fs::path);
    std::string get_hash(std::string, fs::path);
    std::string get_size(std::string, fs::path);
    std::string get_last_write_time(std::string, fs::path);

private:
    std::string to_string(unsigned char*);
};