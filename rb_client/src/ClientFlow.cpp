#include "ClientFlow.h"

ClientFlow::ClientFlow(const std::string & ip, const std::string & port) :client(ip, port) {
    // TODO start authentication
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

    size_t file_size = filesystem::file_size(file_path);
    time_t last_write_time = filesystem::last_write_time(file_path);
    if(file_size != metadata.size || last_write_time != metadata.last_write_time)
        return;

    int num_segments = count_segments(file_size);
    int chunck_size = 2048;
    std::vector<char> chunck(chunck_size, 0);
    crc_32_type crc;
    std::uint32_t checksum;

    // Fragment files larger than 1MB
    for (int i = 0; i < num_segments; i++) {
        RBRequest file_upload_request;
        file_upload_request.set_protover(3);
        file_upload_request.set_type(RBMsgType::UPLOAD);

        auto file_segment = std::make_unique<RBFileSegment>();
        auto file_metadata = std::make_unique<RBFileMetadata>();
        file_segment->set_path(file_operation->get_path());
        file_segment->set_segmentid(i);
        file_metadata->set_size(file_size);
        file_metadata->set_last_write_time(last_write_time);

        // File chuncks read
        size_t segment_len = RB_MAX_SEGMENT_SIZE;
        if (num_segments == 1) {
            segment_len = file_size;
        } else if ((num_segments - i) == 1) {
            segment_len = file_size % RB_MAX_SEGMENT_SIZE;
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
            file_upload_request.set_final(true);
            file_metadata->set_checksum(crc.checksum());
        }

        file_segment->set_allocated_file_metadata(file_metadata.release());
        file_upload_request.set_allocated_file_segment(file_segment.release());

        auto res = this->client.run(file_upload_request);
        validateRBProto(res, RBMsgType::UPLOAD, 3);
        if (!res.error().empty())
            throw RBException(res.error());
    }

    fl.close();
}


void ClientFlow::remove_file(const std::shared_ptr<FileOperation> &file_operation, const std::string &root_path) {
    if (file_operation->get_command() != FileCommand::REMOVE)
        throw std::logic_error("Wrong type of FileOperation command");

    RBRequest file_remove_request;
    file_remove_request.set_protover(3);
    file_remove_request.set_type(RBMsgType::REMOVE);
    file_remove_request.set_final(true);
    auto file_segment = std::make_unique<RBFileSegment>();
    file_segment->set_path(file_operation->get_path());
    file_remove_request.set_allocated_file_segment(file_segment.release());

    auto res = this->client.run(file_remove_request);
    validateRBProto(res, RBMsgType::REMOVE, 3);
    if (!res.error().empty())   throw RBException(res.error());
}

std::unordered_map<std::string, file_metadata> ClientFlow::get_server_files() {
    RBRequest probe_all_request;
    probe_all_request.set_protover(3);
    probe_all_request.set_final(true);
    probe_all_request.set_type(RBMsgType::PROBE);

    auto res = this->client.run(probe_all_request);
    validateRBProto(res, RBMsgType::PROBE, 3);
    if (!res.error().empty())
        throw RBException(res.error());
    if(!res.has_probe_response())
        throw RBException("missing probe response");

    auto server_files = res.probe_response().files();
    std::unordered_map<std::string, file_metadata> map;

    auto it = server_files.begin();
    while(it != server_files.end()) {
        map[it->first] = file_metadata{it->second.checksum(), it->second.size(), it->second.last_write_time()};
        it++;
    }

    return map;
}





