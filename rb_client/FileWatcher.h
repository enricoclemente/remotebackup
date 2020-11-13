//
// Created by Enrico Clemente on 26/10/2020.
//

#ifndef TCPCLIENT_FILEWATCHER_H
#define TCPCLIENT_FILEWATCHER_H

#include <boost/filesystem.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <functional>

using namespace boost::filesystem;

enum class FileStatus{ created, modified, erased };

class FileWatcher {
    std::unordered_map<std::string, std::time_t> files;
    bool running = true;

    bool contains(const std::string &key) {
        auto el = files.find(key);
        return el != files.end();
    }

public:
    std::string path_to_watch;
    std::chrono::duration<int, std::milli> delay;

    FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch(path_to_watch),
                                                                                           delay(delay) {
        for (auto &file : recursive_directory_iterator(path_to_watch)) {
            std::cout<<file.path().string()<<std::endl;
            files[file.path().string()] = last_write_time(file.path());
        }
    }

    void start_monitoring(const::std::function<void (std::string, FileStatus)> &action) {
        while(running) {
            std::this_thread::sleep_for(delay);

            auto it = files.begin();
            while(it != files.end()) {
                if(!exists(it->first)) {
                    action(it->first, FileStatus::erased);
                    it = files.erase(it);
                } else {
                    it++;
                }
            }

            for(auto &file : recursive_directory_iterator(path_to_watch)) {
                auto current_file_last_time_write = last_write_time(file.path());

                // File creation
                if(!contains(file.path().string())) {
                    files[file.path().string()] = last_write_time(file.path());
                    action(file.path().string(), FileStatus::created);
                    // File modification
                } else {
                    if(files[file.path().string()] != current_file_last_time_write) {
                        files[file.path().string()] = last_write_time(file.path());
                        action(file.path().string(), FileStatus::modified);
                    }
                }
            }
        }
    }

    std::vector<char> read_file_bytes(const std::string &file_path)
    {
        std::ifstream fl(file_path);
        fl.seekg( 0, std::ios::end );

        size_t file_len = fl.tellg();

        std::vector<char> bytes(file_len);
        fl.seekg(0, std::ios::beg);

        if (file_len)
            fl.read(&bytes[0], file_len);

        fl.close();
        return std::move(bytes);
    }
};

#endif //TCPCLIENT_FILEWATCHER_H
