#include "ClientFlow.h"


ClientFlow::ClientFlow(
    const std::string & ip,
    const std::string & port,
    const std::string & root_path,
    int socket_timeout)
    : client(ip, port, socket_timeout), root_path(root_path) {
    keep_going.store(true);
}


void ClientFlow::authenticate(const std::string &username, const std::string &password) {
    client.authenticate(username, password);
}


void ClientFlow::upload_file(const std::shared_ptr<FileOperation> &file_operation) {
    if (file_operation->get_command() != FileCommand::UPLOAD)
        throw std::logic_error("ClientFlow->Wrong type of FileOperation command");

    filesystem::path file_path{root_path};
    file_path.append(file_operation->get_path());

    std::ifstream fl(file_path.string());
    if (fl.fail()) {
        if(filesystem::exists(file_path) && filesystem::is_regular_file(file_path))
            throw std::runtime_error("ClientFlow->Error opening file");
        else
            // TODO: how to handle this?
            return;
    }

    file_metadata metadata = file_operation->get_metadata();
    size_t file_size = filesystem::file_size(file_path);
    time_t last_write_time = filesystem::last_write_time(file_path);

    // TODO: how to handle this? it is better to delete the file operation
    if(file_size != metadata.size || last_write_time != metadata.last_write_time) // Check if metadata match
        return;

    int num_segments = count_segments(file_size);
    int chunk_size = 2048;
    std::vector<char> chunk(chunk_size, 0); // Buffer to hold 2048 characters
    crc_32_type crc;

    auto upload_channel = client.open_channel();    // opening protochannel

    try
    {   
        // ensure there's at least one segment, for empty files
        if (!num_segments) num_segments++;

        RBLog("Begin transfer of " + std::to_string(num_segments) + " chunks");

        // Fragment files that are larger than RB_MAX_SEGMENT_SIZE
        for (int i = 0; i < num_segments; i++) {
            RBLog ("Sending chunk " + std::to_string(i));

            RBRequest file_upload_request;
            file_upload_request.set_protover(3);
            file_upload_request.set_type(RBMsgType::UPLOAD);

            auto file_segment = std::make_unique<RBFileSegment>();
            auto file_metadata = std::make_unique<RBFileMetadata>();
            file_segment->set_path(file_operation->get_path());
            file_segment->set_segmentid(i);
            file_metadata->set_size(file_size);
            file_metadata->set_last_write_time(last_write_time);

            // Length of current file segment
            size_t segment_len = RB_MAX_SEGMENT_SIZE;
            if (i == num_segments - 1) { // If last segment
                if (file_size == 0)
                    segment_len = 0;
                else if (file_size % RB_MAX_SEGMENT_SIZE != 0)
                    segment_len = file_size % RB_MAX_SEGMENT_SIZE;
            }

            // Reading file segment with chunks
            size_t tot_read = 0;
            size_t current_read = 0;
            while (tot_read < segment_len) {
                // Check every time if something has changed for the file operation
                if (file_operation->get_abort())
                    throw RBException("ClientFlow->Abort");

                if (segment_len - tot_read >= chunk_size)
                    fl.read(&chunk[0], chunk_size);
                else
                    fl.read(&chunk[0], segment_len - tot_read);

                if (!fl) throw std::runtime_error("ClientFlow->Error reading file chunk");

                current_read = fl.gcount(); // Get the number of characters that have been read (always 2048, except the last time)
                file_segment->add_data(&chunk[0], current_read); // Push characters that have been read into data
                crc.process_bytes(&chunk[0], current_read);
                tot_read += current_read;
            }

            if (i == num_segments - 1) { // Final file segment
                if(crc.checksum() != metadata.checksum) // Check if checksums match
                    throw RBException("ClientFlow->different_checksums");
                file_upload_request.set_final(true);
                file_metadata->set_checksum(crc.checksum());
            } else if (!keep_going.load()) {
                throw RBException("ClientFlow->client_stopped");
            }

            file_segment->set_allocated_file_metadata(file_metadata.release());
            file_upload_request.set_allocated_file_segment(file_segment.release());

            auto res = upload_channel.run(file_upload_request);
            // in case the response is not valid the validator will throw an excaption, triggering the abort
            validateRBProto(res, RBMsgType::UPLOAD, 3);
        }
    }
    catch(RBException& e)
    {
        RBLog("File upload aborted: " + e.getMsg(), LogLevel::ERROR);

        RBRequest req;
        auto file_segment = std::make_unique<RBFileSegment>();
        file_segment->set_path(file_operation->get_path());
        req.set_allocated_file_segment(file_segment.release());
        req.set_type(RBMsgType::ABORT);
        req.set_final(true);
        req.set_protover(3);

        auto res = upload_channel.run(req);
        validateRBProto(res, RBMsgType::ABORT, 3);
        if (!res.success())
            throw RBException("ClientFlow->Server Response Error: " + res.error());
    }

    upload_channel.close();

    fl.close();
}


void ClientFlow::remove_file(const std::shared_ptr<FileOperation> &file_operation) {
    if (file_operation->get_command() != FileCommand::REMOVE)
        throw std::logic_error("ClientFlow->Wrong type of FileOperation command");

    RBRequest file_remove_request;
    file_remove_request.set_protover(3);
    file_remove_request.set_type(RBMsgType::REMOVE);
    file_remove_request.set_final(true);
    auto file_segment = std::make_unique<RBFileSegment>();
    file_segment->set_path(file_operation->get_path());
    file_remove_request.set_allocated_file_segment(file_segment.release());

    auto res = client.run(file_remove_request);
    validateRBProto(res, RBMsgType::REMOVE, 3);
    if (!res.success())
        throw RBException("ClientFlow->Server Response Error: " + res.error());
}


std::unordered_map<std::string, file_metadata> ClientFlow::get_server_files() {
    RBRequest probe_all_request;
    probe_all_request.set_protover(3);
    probe_all_request.set_final(true);
    probe_all_request.set_type(RBMsgType::PROBE);

    auto res = client.run(probe_all_request);
    validateRBProto(res, RBMsgType::PROBE, 3);
    if (!res.error().empty())
        throw RBException("ClientFlow->Server Response Error: " + res.error());
    if(!res.has_probe_response())
        throw RBException("ClientFlow->Missing probe response");

    auto server_files = res.probe_response().files();
    std::unordered_map<std::string, file_metadata> map;

    auto it = server_files.begin();
    while(it != server_files.end()) {
        map[it->first] = file_metadata{it->second.checksum(), it->second.size(), it->second.last_write_time()};
        it++;
    }

    return map;
}






