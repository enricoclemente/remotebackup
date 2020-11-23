//
// Created by Enrico Clemente on 23/11/2020.
//

#ifndef TCPCLIENT_BUFFER_H
#define TCPCLIENT_BUFFER_H

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

    void send_file(const std::string& file_path) {
        std::ifstream fl(file_path);

        fl.seekg( 0, fl.end );
        std::size_t file_len = fl.tellg();
        fl.seekg(0, fl.beg);

        int chunck_size = 2048;
        std::vector<char> file_chunck(chunck_size, 0);

        FilePacket packet;
        packet.set_path(file_path);
        packet.set_file_size(file_len);

        size_t tot_read = 0;
        size_t current_read = 0;
        while(tot_read < file_len) {
            if(file_len - tot_read >= chunck_size) {
                fl.read(&file_chunck[0], chunck_size);
            } else {
                fl.read(&file_chunck[0],file_len-tot_read);
            }
            current_read = fl.gcount();
            packet.add_file_chunck(&file_chunck[0], current_read);
            tot_read += current_read;
        }

        fl.close();

        std::lock_guard lg(bufm);
        std::ostream output(&buf);

        packet.SerializeToOstream(&output);
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
