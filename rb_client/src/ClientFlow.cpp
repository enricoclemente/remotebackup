//
// Created by Enrico Clemente on 05/12/2020.
//

#include "ClientFlow.h"

#include <boost/filesystem.hpp>


std::string string_remove_pref(const std::string& pref, const std::string& input) {
    std::string output = input;
    output.erase(0,pref.size()+1);
    return output;
}

std::string ClientFlow::authenticate(const std::string &username, const std::string &password) {
    return std::string();
}

void ClientFlow::upload_file(const std::shared_ptr<FileOperation>& file_operation, const std::string &root_path) {

    if(file_operation->get_command() != FileCommand::UPLOAD)
        throw std::logic_error("Wrong type of FileOperation command");

    std::ifstream fl(file_operation->get_path());
    if(fl.fail())   throw std::runtime_error("Error opening file");

    fl.seekg( 0, fl.end );
    std::size_t file_len = fl.tellg();
    fl.seekg(0, fl.beg);


    int num_segments = file_len / MAXFILESEGMENT +1;
    int chunck_size = 2048;
    std::vector<char> chunck(chunck_size, 0);
    std::uint32_t checksum;

    std::string file_path = string_remove_pref(root_path,file_operation->get_path());

    // Fragment files larger than 1MB
    for(int i=0; i<num_segments; i++) {
        RBRequest file_upload_request;
        file_upload_request.set_type(RBMsgType::UPLOAD);

        auto *file_segment = new RBFileSegment();
        file_segment->set_path(file_path);
        file_segment->set_size(file_len);
        file_segment->set_totsegments(num_segments);
        file_segment->set_segmentid(i);

        // File chuncks read
        size_t segment_len = MAXFILESEGMENT;
        if(num_segments == 1) {
            segment_len = file_len;
        } else if((num_segments - i) == 1) {
            segment_len = file_len - ((i+1)*MAXFILESEGMENT);
        }

        size_t tot_read = 0;
        size_t current_read = 0;
        while(tot_read < segment_len) {
            // check every time is something has changed for the file operation
            if(file_operation->get_abort())     throw std::runtime_error("FileOperation aborted");// error or better return a code?
            if(segment_len - tot_read >= chunck_size) {
                fl.read(&chunck[0], chunck_size);
                checksum = CRC::Calculate(&chunck[0], chunck_size, CRC::CRC_32(),checksum);
            } else {
                fl.read(&chunck[0],segment_len-tot_read);
                checksum = CRC::Calculate(&chunck[0], segment_len-tot_read, CRC::CRC_32(),checksum);
            }

            if(!fl)    throw std::runtime_error("Error reading file chunck");

            file_segment->add_data(&chunck[0], current_read);

            current_read = fl.gcount();
            tot_read += current_read;
        }

        if(i==num_segments-1) {
            file_segment->set_checksum(checksum);
        }
        file_upload_request.set_allocated_filesegment(file_segment);

        auto res = this->client.run(file_upload_request);
        if(!res.error().empty())    throw std::runtime_error(res.error());
    }

    fl.close();
}

void ClientFlow::remove_file(const std::shared_ptr<FileOperation>& file_operation, const std::string &root_path) {
    if(file_operation->get_command() != FileCommand::REMOVE)
        throw std::logic_error("Wrong type of FileOperation command");

    RBRequest file_upload_request;
    file_upload_request.set_type(RBMsgType::REMOVE);

    std::string file_path = string_remove_pref(root_path,file_operation->get_path());
    auto *file_segment = new RBFileSegment();
    file_segment->set_path(file_path);

    file_upload_request.set_allocated_filesegment(file_segment);

    auto res = this->client.run(file_upload_request);
    if(!res.error().empty())    throw std::runtime_error(res.error());
}




