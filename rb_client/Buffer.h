//
// Created by Enrico Clemente on 23/11/2020.
//

#ifndef TCPCLIENT_BUFFER_H
#define TCPCLIENT_BUFFER_H

#include "FileWatcher.h"
#include "packet.pb.h"
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;

// Fake TCP stream
class Buffer{
    asio::streambuf buf;
    std::mutex bufm;


public:
    std::string string_remove_pref(const std::string& pref, const std::string& input) {
        std::string output = input;
        output.erase(0,pref.size()+1);
        return output;
    }

    void send_probe_single(const std::string& root_path, const std::string& file_path) {
        ProbeSingleFileRequest packet;

        // put relative path of the file in the packet
        std::string path = string_remove_pref(root_path, file_path);
        packet.set_file_path(path);

        std::lock_guard lg(bufm);
        std::ostream output(&buf);

        packet.SerializeToOstream(&output);
    }

    FilePacket create_FilePacket(const std::string& root_path, const std::string& file_path,
            std::time_t last_write_time, FileStatus command) {
        FilePacket packet;

        // put relative path of the file in the packet
        std::string path = string_remove_pref(root_path, file_path);
        packet.set_file_path(path);

        if(command == FileStatus::created || command == FileStatus::modified) {
            // send the file
            std::ifstream fl(file_path);

            fl.seekg( 0, fl.end );
            std::size_t file_len = fl.tellg();
            fl.seekg(0, fl.beg);

            int chunck_size = 2048;
            std::vector<char> file_chunck(chunck_size, 0);

            packet.set_command(FilePacket::CREATE_MODIFY);
            packet.set_file_size(file_len);

            // file read by chuncks
            size_t tot_read = 0;
            size_t current_read = 0;
            while(tot_read < file_len) {
                if(file_len - tot_read >= chunck_size) {
                    fl.read(&file_chunck[0], chunck_size);
                } else {
                    fl.read(&file_chunck[0],file_len-tot_read);
                }
                current_read = fl.gcount();
                packet.add_file_chuncks(&file_chunck[0], current_read);
                tot_read += current_read;
            }
            fl.close();
        } else if(command == FileStatus::erased) {
            // just send the command to delete file on the server
            packet.set_command(FilePacket::ERASE);
        }

        return packet;
    }

    FilePacket receive_file() {
        std::lock_guard lg(bufm);
        std::istream input(&buf);

        FilePacket packet;
        packet.ParseFromIstream(&input);

        return packet;
    }
};



#endif //TCPCLIENT_BUFFER_H
