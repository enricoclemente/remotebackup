#include "FileManager.h"

// Utils function
std::string string_remove_pref(const std::string &pref, const std::string &input) {
    std::string output = input;
    output.erase(0, pref.size() + 1);
    return output;
}

// Description: Set file watcher parameters
// Errors: It can throw a runtime error because of metadata calculation
void FileManager::set_file_watcher(const std::string &path, std::chrono::duration<int, std::milli> delay) {
    this->path_to_watch = filesystem::path(path);
    this->update_interval = delay;

    for (auto &file : filesystem::recursive_directory_iterator(path_to_watch)) {
        if(filesystem::is_regular_file(file.path())){
            file_metadata current_file_metadata{};
            current_file_metadata.last_write_time = last_write_time(file.path());
            current_file_metadata.size = filesystem::file_size(file.path());
            current_file_metadata.checksum = calculate_checksum(file.path());

            files[file.path().string()] = current_file_metadata;
        }
    }
    std::cout<<"File watcher setted"<<std::endl;
}

// Description: Start the monitoring of the setted path to watch
// Errors: It can throw a runtime error because of checksum calculation
void FileManager::start_monitoring(const std::function<void(std::string, file_metadata, FileStatus)> &action) {
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
                    relative_path = string_remove_pref(path_to_watch.string(), it->first);
                    action(relative_path, files[file.path().string()], FileStatus::CREATED);
                } else {
                    // File modification
                    current_file_metadata.last_write_time = last_write_time(file.path());
                    if (files[file.path().string()].last_write_time != filesystem::last_write_time(file.path())) {

                        current_file_metadata.size = filesystem::file_size(file.path());
                        current_file_metadata.checksum = calculate_checksum(file.path());
                        if (files[file.path().string()].checksum != current_file_metadata.checksum) {

                            files[file.path().string()] = current_file_metadata;
                            relative_path = string_remove_pref(path_to_watch.string(), it->first);
                            action(relative_path, files[file.path().string()], FileStatus::MODIFIED);
                        }
                    }
                }
            }
        }
    }
}

// Description: calculate file checksum
// Errors: It can throw a runtime error because of file errors
std::uint32_t FileManager::calculate_checksum(const filesystem::path &file_path) {
    std::ifstream fl(file_path.string());
    if (fl.fail()) throw std::runtime_error("Error opening file");

    std::size_t file_len = filesystem::file_size(file_path);

    int chunck_size = 1000000;
    std::vector<char> chunck(chunck_size, 0);
    crc_32_type crc;

    size_t tot_read = 0;
    size_t current_read;
    while (tot_read < file_len) {
        if (file_len - tot_read >= chunck_size) {
            fl.read(&chunck[0], chunck_size);
        } else {
            fl.read(&chunck[0], file_len - tot_read);
        }

        if (!fl) throw std::runtime_error("Error reading file chunck");

        current_read = fl.gcount();
        crc.process_bytes(&chunck[0], current_read);

        tot_read += current_read;
    }

    return crc.checksum();
}

// Description: compare a map containing a file system with the actual and make an action for the differences
// Input parameters: map <string(relative path), file_metadata>
//                   action function
template<typename Map>
void FileManager::file_system_compare(const Map& map,
                                      const std::function<void(std::string, file_metadata, FileStatus)> &action) {
    if(path_to_watch.empty())
        throw std::logic_error("Function file_system_compare can be used only after setting the file watcher");

    auto it = map.begin();
    while(it != map.end()) {
        std::string file_path = path_to_watch.string() + it->first;
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
    }
}



