#pragma once

#include <openssl/md5.h>

#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "Database.h"
#include "RBHelpers.h"

namespace fs = boost::filesystem;
namespace ch = std::chrono;

class FileSystemManager {
public:
    std::unordered_map<std::string, RBFileMetadata> get_files(const std::string&);
    bool find_file(std::string, const fs::path&);
    bool write_file(std::string, const fs::path&, const std::string&,
                    const std::string&, const std::string&, const std::string&);
    bool remove_file(std::string, fs::path);
    std::string md5(fs::path);
    std::string get_hash(std::string, const fs::path&);
    std::string get_size(std::string, const fs::path&);
    std::string get_last_write_time(std::string, const fs::path&);

private:
    std::string to_string(unsigned char*);
};