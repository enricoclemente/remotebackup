#include "FileManager.h"

#include <utility>

FileManager::FileManager(
    filesystem::path path,
    std::chrono::duration<int, std::milli> delay)
    : path_to_watch(std::move(path)), update_interval(delay) {

    for (auto &file : filesystem::recursive_directory_iterator(path_to_watch)) {
        if(filesystem::is_regular_file(file.path())){
            file_metadata current_file_metadata{};
            current_file_metadata.last_write_time = last_write_time(file.path());
            current_file_metadata.size = filesystem::file_size(file.path());
            current_file_metadata.checksum = calculate_checksum(file.path());

            files[file.path().string()] = current_file_metadata;
        }
    }

    RBLog("File watcher set", LogLevel::INFO);
}

// Utils function
std::string string_remove_pref(const std::string &pref, const std::string &input) {
    std::string output = input;
    output.erase(0, pref.size() + 1);
    return output;
}

void FileManager::stop_monitoring() {
    running = false;
    files["^NIL^"] = file_metadata();
}


// Description: Start the monitoring of the setted path to watch
// Errors: It can throw a runtime error because of checksum calculation
void FileManager::start_monitoring(const std::function<void(const std::string&, const file_metadata&, FileStatus)> &action) {
    while (running) {
        std::this_thread::sleep_for(update_interval);
        std::string relative_path;
        // File erase
        auto it = files.begin();
        while (it != files.end()) {
            if (!filesystem::exists(filesystem::path(it->first))) {
                relative_path = string_remove_pref(path_to_watch.string(), it->first);
                action(relative_path, {}, FileStatus::REMOVED);
                it = files.erase(it);
            } else {
                it++;
            }
        }

        for (auto &file : filesystem::recursive_directory_iterator(path_to_watch)) {
            file_metadata current_file_metadata{};

            if(filesystem::is_regular_file(file.path())) {
                // File creation
                if (!contains(files, file.path().string())) {
                    current_file_metadata.last_write_time = last_write_time(file.path());
                    current_file_metadata.size = filesystem::file_size(file.path());
                    current_file_metadata.checksum = calculate_checksum(file.path());

                    files[file.path().string()] = current_file_metadata;
                    relative_path = string_remove_pref(path_to_watch.string(), file.path().string());
                    action(relative_path, files[file.path().string()], FileStatus::CREATED);
                } else {
                    // File modification
                    current_file_metadata.last_write_time = last_write_time(file.path());
                    if (files[file.path().string()].last_write_time != filesystem::last_write_time(file.path())) {

                        current_file_metadata.size = filesystem::file_size(file.path());
                        current_file_metadata.checksum = calculate_checksum(file.path());
                        if (files[file.path().string()].checksum != current_file_metadata.checksum) {

                            files[file.path().string()] = current_file_metadata;
                            relative_path = string_remove_pref(path_to_watch.string(), file.path().string());
                            action(relative_path, files[file.path().string()], FileStatus::MODIFIED);
                        }
                    }
                }
            }
        }
    }
}

// Description: compare a map containing a file system with the actual and make an action for the differences
// Input parameters: map <string(relative path), file_metadata>
//                   action function
void FileManager::file_system_compare(const std::unordered_map<std::string, file_metadata>& map,
                                      const std::function<void(const std::string&, const file_metadata&, FileStatus)> &action) {
    if(path_to_watch.empty())
        throw std::logic_error("FileManager->Function file_system_compare can be used only after the file watcher is set");

    auto it = map.begin();
    while(it != map.end()) {
        filesystem::path p{path_to_watch.string()};
        p /= it->first;
        std::string file_path = p.string();
        if(!contains(files, file_path)) {
            action(it->first, {}, FileStatus::REMOVED);
        } else {
            if(files[file_path].last_write_time != it->second.last_write_time &&
                files[file_path].checksum != it->second.checksum) {
                action(it->first, files[file_path], FileStatus::MODIFIED);
            }
        }
        it++;
    }

    it=files.begin();
    while(it != files.end()) {
        std::string relative_path = string_remove_pref(path_to_watch.string(), it->first);

        if(!contains(map, relative_path)) {
            action(relative_path, it->second, FileStatus::CREATED);
        }
        it++;
    }
}



