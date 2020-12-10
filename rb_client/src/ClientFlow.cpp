#include "ClientFlow.h"

ClientFlow::ClientFlow(const std::string & ip, const std::string & port) {
    this->client = std::make_shared<Client>(ip, port);
}

std::string ClientFlow::authenticate(const std::string &username, const std::string &password) {
    return std::string();
}



void ClientFlow::upload_file(const std::shared_ptr<FileOperation> &file_operation, const std::string &root_path) {

    if (file_operation->get_command() != FileCommand::UPLOAD)
        throw std::logic_error("Wrong type of FileOperation command");

    filesystem::path file_path{file_operation->get_path()+root_path};
    file_metadata metadata = file_operation->get_metadata();

    std::ifstream fl(file_path.string());
    if (fl.fail()) {
        if(filesystem::exists(file_path) && filesystem::is_regular_file(file_path))
            throw std::runtime_error("Error opening file");
        else
            return;
    }

    int file_size = filesystem::file_size(file_path);
    if(file_size != metadata.size ||
        filesystem::last_write_time(file_path) != metadata.last_write_time)
        return;

    int num_segments = file_size / MAXFILESEGMENT + 1;
    int chunck_size = 2048;
    std::vector<char> chunck(chunck_size, 0);
    crc_32_type crc;
    std::uint32_t checksum;

    // Fragment files larger than 1MB
    for (int i = 0; i < num_segments; i++) {
        RBRequest file_upload_request;
        file_upload_request.set_type(RBMsgType::UPLOAD);

        auto *file_segment = new RBFileSegment();
        file_segment->set_path(file_operation->get_path());
        file_segment->set_size(file_size);
        file_segment->set_totsegments(num_segments);
        file_segment->set_segmentid(i);

        // File chuncks read
        size_t segment_len = MAXFILESEGMENT;
        if (num_segments == 1) {
            segment_len = file_size;
        } else if ((num_segments - i) == 1) {
            segment_len = file_size - ((i + 1) * MAXFILESEGMENT);
        }

        size_t tot_read = 0;
        size_t current_read = 0;
        while (tot_read < segment_len) {
            // check every time is something has changed for the file operation
            if (file_operation->get_abort())    return;
            if (segment_len - tot_read >= chunck_size) {
                fl.read(&chunck[0], chunck_size);
            } else {
                fl.read(&chunck[0], segment_len - tot_read);
            }

            if (!fl)    throw std::runtime_error("Error reading file chunck");

            file_segment->add_data(&chunck[0], current_read);

            current_read = fl.gcount();
            crc.process_bytes(&chunck[0], current_read);
            tot_read += current_read;
        }

        if (i == num_segments - 1) {
            if(crc.checksum() != metadata.checksum)
                return;

            file_segment->set_checksum(crc.checksum());
        }
        file_upload_request.set_allocated_filesegment(file_segment);

        auto res = this->client->run(file_upload_request);
        if (!res.error().empty())   throw RBException(res.error());
    }

    fl.close();
}

void ClientFlow::remove_file(const std::shared_ptr<FileOperation> &file_operation, const std::string &root_path) {
    if (file_operation->get_command() != FileCommand::REMOVE)
        throw std::logic_error("Wrong type of FileOperation command");

    RBRequest file_remove_request;
    file_remove_request.set_type(RBMsgType::REMOVE);
    auto *file_segment = new RBFileSegment();
    file_segment->set_path(file_operation->get_path());
    file_remove_request.set_allocated_filesegment(file_segment);

    auto res = this->client->run(file_remove_request);
    if (!res.error().empty())   throw RBException(res.error());
}

std::unordered_map<std::string, file_metadata> ClientFlow::get_server_files() {

    RBRequest probe_all_request;
    probe_all_request.set_type(RBMsgType::PROBE);

    auto res = this->client->run(probe_all_request);
    if (!res.error().empty())   throw RBException(res.error());

    res.proberesponse();

    return std::unordered_map<std::string, file_metadata>();
}






