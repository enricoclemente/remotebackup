#include <boost/filesystem.hpp>
#include <boost/crc.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <functional>

using namespace boost;

enum class FileStatus {
    CREATED = 0,
    MODIFIED = 1,
    REMOVED = 2,
};

struct file_metadata {
    uint32_t checksum;
    size_t size;
    time_t last_write_time;
};

class FileManager {
    filesystem::path path_to_watch;
    std::chrono::duration<int, std::milli> update_interval;
    std::unordered_map<std::string, file_metadata> files;
    std::atomic<bool> running = true;

    template<typename Map>
    bool contains(const Map& map, const std::string &key) {
        auto it = map.find(key);
        return it != map.end();
    }

public:
    FileManager(const filesystem::path &path, std::chrono::duration<int, std::milli> delay);
    void start_monitoring(const std::function<void(std::string&, const file_metadata&, FileStatus)> &action);
    void stop_monitoring();
    static std::uint32_t calculate_checksum(const filesystem::path &file_path);

    template<typename Map>
    void file_system_compare(const Map& map,
            const std::function<void(std::string, file_metadata, FileStatus)> &action);
};


