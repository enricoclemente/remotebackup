#include "ClientFlow.h"


ClientFlow::ClientFlow(
    const std::string & ip,
    const std::string & port,
    const std::string & root_path,
    const std::string &username,
    const std::string &password,
    std::chrono::system_clock::duration watcher_interval,
    int socket_timeout, 
    int senders_pool_n)
    :
    client(ip, port, socket_timeout, senders_pool_n),
    root_path(root_path),
    username(username),
    password(password),
    senders_pool_n(senders_pool_n),
    file_manager(root_path, watcher_interval) {}

bool ClientFlow::upload_file(const std::shared_ptr<FileOperation> &file_operation) {
    if (file_operation->get_command() != FileCommand::UPLOAD)
        throw std::logic_error("ClientFlow->Wrong type of FileOperation command");

    filesystem::path file_path{root_path};
    file_path.append(file_operation->get_path());

    std::ifstream fl(file_path.string(), std::ios::binary);
    if (fl.fail()) {
        RBLog("ClientFlow >> Can't open file <" 
            + file_path.string() + "> for upload", LogLevel::ERROR);
        return false;
    }

    file_metadata metadata = file_operation->get_metadata();
    size_t file_size = filesystem::file_size(file_path);
    time_t last_write_time = filesystem::last_write_time(file_path);

    // Skip if metadata don't match
    if(file_size != metadata.size || last_write_time != metadata.last_write_time)
        return false;

    int num_segments = count_segments(file_size);
    int chunk_size = 2048;
    std::vector<char> chunk(chunk_size, 0); // Buffer to hold 2048 characters
    crc_32_type crc;

    try {   
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
                file_metadata->set_checksum(crc.checksum());
            } else if (!keep_going.load()) {
                throw RBException("ClientFlow->client_stopped");
            }

            file_segment->set_allocated_file_metadata(file_metadata.release());
            file_upload_request.set_allocated_file_segment(file_segment.release());

            auto res = client.run(file_upload_request);
            // in case the response is not valid the validator will throw an excaption, triggering the abort
            validateRBProto(res, RBMsgType::UPLOAD, 3);
        }
    } catch(RBException& e) {
        RBLog("File upload aborted: " + e.getMsg(), LogLevel::ERROR);

        RBRequest req;
        auto file_segment = std::make_unique<RBFileSegment>();
        file_segment->set_path(file_operation->get_path());
        req.set_allocated_file_segment(file_segment.release());
        req.set_type(RBMsgType::ABORT);
        req.set_protover(3);

        auto res = client.run(req);
        validateRBProto(res, RBMsgType::ABORT, 3);
    }

    // upload_channel.close();

    fl.close();
    return true;
}


void ClientFlow::remove_file(const std::shared_ptr<FileOperation> &file_operation) {
    if (file_operation->get_command() != FileCommand::REMOVE)
        throw std::logic_error("ClientFlow->Wrong type of FileOperation command");

    RBRequest file_remove_request;
    file_remove_request.set_protover(3);
    file_remove_request.set_type(RBMsgType::REMOVE);
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
    probe_all_request.set_type(RBMsgType::PROBE);

    auto res = client.run(probe_all_request, true);
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

void ClientFlow::watcher_loop() {
    auto update_handler = [&](const std::string &path, const file_metadata &meta, FileStatus status) {
        try {
            if (!keep_going) return;
            if (!filesystem::is_regular_file(root_path / path) && status != FileStatus::REMOVED)
                return;

            FileCommand command;
            std::string status_to_print;
            if(status == FileStatus::REMOVED) {
                command = FileCommand::REMOVE;
                status_to_print = "REMOVED";
            } else if(status == FileStatus::CREATED) {
                command = FileCommand::UPLOAD;
                status_to_print = "CREATED";
            } else if(status == FileStatus::MODIFIED) {
                command = FileCommand::UPLOAD;
                status_to_print = "MODIFIED";
            }

            out_queue.add_file_operation(path, meta, command);
            RBLog("Client >> Watcher: " + status_to_print + ": " + path, LogLevel::INFO);
            /* RBLog("File metadata: size " + std::to_string(meta.size) +
                " last write time " + std::to_string(meta.last_write_time) +
                " checksum " + std::to_string(meta.checksum), LogLevel::DEBUG); */
        } catch (RBException &e) {
            RBLog("RBException:" + e.getMsg(), LogLevel::ERROR);
        } catch (std::exception &e) {
            RBLog("exception:" + std::string(e.what()), LogLevel::ERROR);
        }
    };

    RBLog("Watcher >> Syncing...");
    // probing server
    std::unordered_map server_files = get_server_files();
    // comparing client and server files
    file_manager.file_system_compare(server_files, update_handler);
    RBLog("Watcher >> Start monitoring");
    // running file watcher to monitor client fs
    file_manager.start_monitoring(update_handler);
}

void ClientFlow::sender_loop() {
    int attempt_count = 0;
    int max_attempts = 3;
    try {

        while (true) {
            if(attempt_count == max_attempts) {
                RBLog("Reached max attempts number. Terminating client", LogLevel::ERROR);
                stop();
                break;
            }

            auto op = out_queue.get_file_operation();
            const auto & path = op->get_path();

            if (!keep_going) return;

            try {
                switch (op->get_command()) {
                    case FileCommand::UPLOAD:
                        RBLog("Client >> UPLOADING: " + path, LogLevel::INFO);
                        if (upload_file(op)) 
                            RBLog("Client >> UPLOADED: " + path , LogLevel::INFO);
                        else
                            RBLog("Client >> SKIP: " + path, LogLevel::DEBUG);
                        break;
                    case FileCommand::REMOVE:
                        RBLog("Client >> REMOVING: " + path, LogLevel::INFO);
                        remove_file(op);
                        RBLog("Client >> REMOVED: " + path, LogLevel::INFO);
                        break;
                    default:
                        RBLog("Client >> unhandled file operation!", LogLevel::ERROR);
                        break;
                }

                attempt_count = 0;      // resetting attempt count
                out_queue.remove_file_operation(op->get_id());      // deleting file operation because completed correctly
            } catch (RBException &e) {
                attempt_count++;
                RBLog("RBException:" + e.getMsg(), LogLevel::ERROR);
                out_queue.free_file_operation(op->get_id());
            } catch (std::exception &e) {
                attempt_count++;
                RBLog("exception:" + std::string(e.what()), LogLevel::ERROR);
                out_queue.free_file_operation(op->get_id());
            }
        }
    } catch (RBException &e) {
        if (keep_going) 
            RBLog("Client >> unexpected termination of sender thread: " + e.getMsg());
    } catch (std::exception &e) {
        RBLog("Client >> unexpected termination of sender thread: " + std::string(e.what()));
    }
}

void ClientFlow::start() {

    RBLog("Main >> Authenticating...", LogLevel::INFO);
    try {
        client.authenticate(username, password);
    } catch (RBException &e) {
        RBLog("Authentication failed: " + e.getMsg(), LogLevel::ERROR);
        exit(-1);
    } catch (std::exception &e) {
        RBLog("Authentication failed: " + std::string(e.what()), LogLevel::ERROR);
        exit(-1);
    }

    RBLog("Main >> Starting file watcher...", LogLevel::INFO);
    file_manager.initial_scan();

    // thread for keep running file watcher and inital probe
    watcher_thread = std::thread([this]() { watcher_loop(); });

    // threads for handling file operation: sending requests and receiving responses
    for (int i = 0; i < senders_pool_n; i++) {
        RBLog("Main >> Starting sender thread " + std::to_string(i), LogLevel::INFO);
        senders_pool.emplace_back(std::thread([this]() { sender_loop(); }));
    }
}

void ClientFlow::stop() {
    RBLog("ClientFLow >> Stopping RB client...", LogLevel::INFO);
    keep_going = false;
    RBLog("ClientFLow >> Stopping file watcher...", LogLevel::INFO);
    file_manager.stop_monitoring();
    RBLog("ClientFLow >> Waiting for watcher thread to finish...", LogLevel::INFO);
    watcher_thread.join();

    out_queue.stop();

    RBLog("ClientFLow >> Waiting for sender threads to finish...", LogLevel::INFO);
    for (auto &s : senders_pool) try {
        s.join();
    } catch (std::exception &e) {}


}
