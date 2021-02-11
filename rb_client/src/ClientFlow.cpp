#include "ClientFlow.h"

ClientFlow::ClientFlowConsumer::ClientFlowConsumer(Client &client, std::function<void(ClientFlowConsumer&)> handler) 
    : client(client), sender([handler, this](){ 
        handler(*this);
    }) {}

void ClientFlow::ClientFlowConsumer::clear_protochannel() {
    if (pc == nullptr) return;
    if (pc->is_open()) {
        RBRequest nop_req;
        nop_req.set_type(RBMsgType::NOP);
        nop_req.set_protover(3);
        nop_req.set_final(true);
        pc->run(nop_req, true);
    }
    pc = nullptr;
}

ProtoChannel & ClientFlow::ClientFlowConsumer::get_protochannel() {
    std::lock_guard lg(m);
    if (pc == nullptr || !pc->is_open()) {
        pc = client.open_channel();
    }
    last_use = std::chrono::system_clock::now();
    return *pc;
}

void ClientFlow::ClientFlowConsumer::clean_protochannel() {
    std::lock_guard lg(m);
    auto timeout = last_use + std::chrono::seconds(PROTOCHANNEL_POOL_TIMEOUT_SECS);
    auto now = std::chrono::system_clock::now();
    if (now > timeout) {
        RBLog("ClientFlow >> Cleaning unused ProtoChannel...", LogLevel::DEBUG);
        clear_protochannel();
    }
}

void ClientFlow::ClientFlowConsumer::join() {
    sender.join();
}

ClientFlow::ClientFlow(
    const std::string &ip,
    const std::string &port,
    const std::string &root_path,
    const std::string &username,
    const std::string &password,
    bool restore_option,
    std::chrono::system_clock::duration watcher_interval,
    int socket_timeout,
    int senders_pool_n)
    : client(ip, port, socket_timeout, senders_pool_n),
      root_path(root_path),
      username(username),
      password(password),
      restore_from_server(restore_option),
      senders_pool_n(senders_pool_n),
      watchdog(make_watchdog(
        std::chrono::seconds(PROTOCHANNEL_POOL_TIMEOUT_SECS),
        [this]() { return keep_going.load(); },
        [this]() { 
            for(auto &cfc : senders_pool) cfc->clean_protochannel();
        }
      )),
      file_manager(root_path, watcher_interval) {}

bool ClientFlow::upload_file(const std::shared_ptr<FileOperation> &file_operation, ProtoChannel &pc) {
    if (file_operation->get_command() != FileCommand::UPLOAD)
        throw std::logic_error("ClientFlow->Wrong type of FileOperation command");

    fs::path file_path{root_path};
    file_path.append(file_operation->get_path());

    std::ifstream fl(file_path.string(), std::ios::binary);
    if (fl.fail()) {
        RBLog("ClientFlow >> Can't open file <" + file_path.string() + "> for upload", LogLevel::ERROR);
        return false;
    }

    file_metadata metadata = file_operation->get_metadata();
    size_t file_size = fs::file_size(file_path);
    time_t last_write_time = fs::last_write_time(file_path);

    // Skip if metadata don't match
    if (file_size != metadata.size || last_write_time != metadata.last_write_time)
        return false;

    int num_segments = count_segments(file_size);
    int chunk_size = 2048;
    std::vector<char> chunk(chunk_size, 0);  // Buffer to hold 2048 characters
    boost::crc_32_type crc;

    try {
        // ensure there's at least one segment, for empty files
        if (!num_segments) num_segments++;

        RBLog("Begin outbound transfer of " + std::to_string(num_segments) + " segments");

        // Fragment files that are larger than RB_MAX_SEGMENT_SIZE
        for (int i = 0; i < num_segments; i++) {
            RBLog("Sending segment " + std::to_string(i));

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
            if (i == num_segments - 1) {  // If last segment
                if (file_size == 0)
                    segment_len = 0;
                else if (file_size % RB_MAX_SEGMENT_SIZE != 0)
                    segment_len = file_size % RB_MAX_SEGMENT_SIZE;
            }

            // Reading file segment by 2048-character long chunks
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

                current_read = fl.gcount();                       // Get the number of characters that have been read (always 2048, except the last time)
                file_segment->add_data(&chunk[0], current_read);  // Push characters that have been read into data
                crc.process_bytes(&chunk[0], current_read);
                tot_read += current_read;
            }

            if (i == num_segments - 1) {                  // Final file segment
                if (crc.checksum() != metadata.checksum)  // Check if checksums match
                    throw RBException("ClientFlow->different_checksums");
                file_metadata->set_checksum(crc.checksum());
            } else if (!keep_going.load()) {
                throw RBException("ClientFlow->client_stopped");
            }

            file_segment->set_allocated_file_metadata(file_metadata.release());
            file_upload_request.set_allocated_file_segment(file_segment.release());

            auto res = pc.run(file_upload_request);
            // in case the response is not valid the validator will throw an excaption, triggering the abort
            validateRBProto(res, RBMsgType::UPLOAD, 3);
        }
    } catch (RBException &e) {
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

void ClientFlow::remove_file(const std::shared_ptr<FileOperation> &file_operation, ProtoChannel &pc) {
    if (file_operation->get_command() != FileCommand::REMOVE)
        throw std::logic_error("ClientFlow->Wrong type of FileOperation command");

    RBRequest file_remove_request;
    file_remove_request.set_protover(3);
    file_remove_request.set_type(RBMsgType::REMOVE);
    auto file_segment = std::make_unique<RBFileSegment>();
    file_segment->set_path(file_operation->get_path());
    file_remove_request.set_allocated_file_segment(file_segment.release());

    auto res = pc.run(file_remove_request);
    validateRBProto(res, RBMsgType::REMOVE, 3);
    if (!res.success())
        throw RBException("ClientFlow->Server Response Error: " + res.error());
}

void ClientFlow::get_server_files(const std::unordered_map<std::string, file_metadata> &server_map) {
    for (const auto &pair : server_map) {
        RBLog("Path: " + pair.first);
        RBLog("Size: " + std::to_string(pair.second.size));
        int num_segments = count_segments(pair.second.size);
        RBLog("Segments: " + std::to_string(num_segments));

        // ensure there's at least one segment, for empty files
        if (!num_segments) num_segments++;

        RBLog("Begin inbound transfer of " + std::to_string(num_segments) + " segments");

        // Files larger than RB_MAX_SEGMENT_SIZE are requested in segments
        for (int i = 0; i < num_segments; i++) {
            if (!keep_going.load())
                throw RBException("ClientFlow->client_stopped");

            RBLog("Requesting segment " + std::to_string(i));

            auto file_segment_info = std::make_unique<RBFileSegment>();
            file_segment_info->set_path(pair.first);
            file_segment_info->set_segmentid(i);

            RBRequest restore_request;
            restore_request.set_protover(3);
            restore_request.set_type(RBMsgType::RESTORE);
            restore_request.set_allocated_file_segment(file_segment_info.release());

            auto res = client.run(restore_request);
            validateRBProto(res, RBMsgType::RESTORE, 3);

            // write segment received from server
            const auto &file_segment = res.file_segment();
            const auto &res_path = file_segment.path();  // CHECK not checking for forbidden paths
            auto path = root_path / fs::path(res_path).lexically_normal();
            if (path.filename().empty()) {
                RBLog("Client >> The path provided is not formatted as a valid file path", LogLevel::ERROR);
                throw RBException("ClientFlow->malformed_path");
            }

            auto segment_id = file_segment.segmentid();
            if (segment_id != i) {
                RBLog("Client >> Wrong segment received");
                throw RBException("ClientFlow->wrong_segment");
            }
            // CHECK What if we don't check for wrong segment, but we accept them in
            // any order and place them in a file at the right position, even if
            // the file doesn't exist and the segment_id is greater than 0

            // Create directories containing the file
            fs::create_directories(path.parent_path());

            // Create or overwrite file if it's the first segment (segment_id == 0), otherwise append to file
            auto open_mode = segment_id == 0
                ? std::ios::trunc
                : std::ios::app;
            std::ofstream ofs{path.string(), open_mode | std::ios::binary};

            if (!ofs) {
                RBLog("Client >> Cannot open file", LogLevel::ERROR);
                // fs::remove(path); // CHECK Delete file?
                throw RBException("ClientFlow->cannot_open_file");
            }

            for (const std::string &datum : file_segment.data())
                ofs << datum;
            ofs.close();

            if (i != num_segments - 1) continue;

            // CHECK This checks against original checksum received from server
            // Check if checksums match
            auto checksum = calculate_checksum(path);
            if (checksum != pair.second.checksum) {
                RBLog("Client >> Checksums don't match");  //", deleting file..."
                // fs::remove(path); // CHECK Delete file?
                throw RBException("ClientFlow->invalid_checksum");
            }
        }
    }

    // CHECK directly throwing an exception in order to prevent from doing the file_system_compare
}

std::unordered_map<std::string, file_metadata> ClientFlow::get_server_state() {
    RBRequest probe_all_request;
    probe_all_request.set_protover(3);
    probe_all_request.set_type(RBMsgType::PROBE);

    auto res = client.run(probe_all_request);
    validateRBProto(res, RBMsgType::PROBE, 3);
    if (!res.error().empty())
        throw RBException("ClientFlow->Server Response Error: " + res.error());
    if (!res.has_probe_response())
        throw RBException("ClientFlow->Missing probe response");

    auto server_files = res.probe_response().files();
    std::unordered_map<std::string, file_metadata> map;

    auto it = server_files.begin();
    while (it != server_files.end()) {
        map[it->first] = file_metadata{it->second.checksum(), it->second.size(), it->second.last_write_time()};
        it++;
    }

    return map;
}

void ClientFlow::watcher_loop() {
    auto update_handler = [&](const std::string &path, const file_metadata &meta, FileStatus status) {
        try {
            if (!keep_going) return;
            if (!fs::is_regular_file(root_path / path) && status != FileStatus::REMOVED)
                return;

            FileCommand command;
            std::string status_to_print;
            if (status == FileStatus::REMOVED) {
                command = FileCommand::REMOVE;
                status_to_print = "REMOVED";
            } else if (status == FileStatus::CREATED) {
                command = FileCommand::UPLOAD;
                status_to_print = "CREATED";
            } else if (status == FileStatus::MODIFIED) {
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

    // probing server
    std::unordered_map server_files = get_server_state();

    // restoring files from server's backup
    if (restore_from_server) {
        RBLog("Watcher >> Syncing client to server's state...", LogLevel::INFO);
        get_server_files(server_files);
        RBLog("Client >> RESTORE DONE", LogLevel::INFO);
    }

    RBLog("Watcher >> Syncing server to client's state...", LogLevel::INFO);
    // scanning current files
    file_manager.initial_scan();
    // comparing client and server files
    file_manager.file_system_compare(server_files, update_handler);

    RBLog("Watcher >> Start monitoring...", LogLevel::INFO);
    // running file watcher to monitor client fs
    file_manager.start_monitoring(update_handler);
}

void ClientFlow::sender_loop(ClientFlowConsumer &cfc) {
    int attempt_count = 0;
    int max_attempts = 3;
    try {
        while (true) {
            if (attempt_count == max_attempts) {
                RBLog("Reached max attempts number. Terminating client", LogLevel::ERROR);  // TODO Solve client not terminating
                stop();
                break;
            } else if (attempt_count > 0) {
                // from the second attempt the thread have to wait a little bit in order to not solve the problem
                std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            }

            auto op = out_queue.get_file_operation();
            const auto &path = op->get_path();

            if (!keep_going) return;

            try {
                switch (op->get_command()) {
                case FileCommand::UPLOAD:
                    RBLog("Client >> UPLOADING: " + path, LogLevel::INFO);
                    if (upload_file(op, cfc.get_protochannel()))
                        RBLog("Client >> UPLOADED: " + path, LogLevel::INFO);
                    else
                        RBLog("Client >> SKIPPED: " + path, LogLevel::DEBUG);
                    break;
                case FileCommand::REMOVE:
                    RBLog("Client >> REMOVING: " + path, LogLevel::INFO);
                    remove_file(op, cfc.get_protochannel());
                    RBLog("Client >> REMOVED: " + path, LogLevel::INFO);
                    break;
                default:
                    RBLog("Client >> unhandled file operation!", LogLevel::ERROR);
                    break;
                }

                attempt_count = 0;                              // resetting attempt count
                out_queue.remove_file_operation(op->get_id());  // deleting file operation because completed correctly
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

    RBLog("ClientFLow >> Starting file watcher...", LogLevel::INFO);

    // thread for making some initial operations and then keeping file watcher running
    watcher_thread = std::thread([this]() { watcher_loop(); });

    // threads for handling file operation: sending requests and receiving responses
    for (int i = 0; i < senders_pool_n; i++) {
        RBLog("Main >> Starting sender thread " + std::to_string(i), LogLevel::INFO);
        senders_pool.emplace_back(std::make_unique<ClientFlowConsumer>(
            client, 
            [this](ClientFlowConsumer &cfc) { sender_loop(cfc); }
        ));
    }

    RBLog("ClientFLow >> RB client started!", LogLevel::INFO);

    std::unique_lock<std::mutex> waiter_lk(waiter);
    waiter_cv.wait(waiter_lk, [this]() { return !keep_going; });

    RBLog("ClientFLow >> Stopping RB client...", LogLevel::INFO);
    keep_going = false;
    RBLog("ClientFLow >> Stopping file watcher...", LogLevel::INFO);
    file_manager.stop_monitoring();
    RBLog("ClientFLow >> Waiting for watcher thread to finish...", LogLevel::INFO);
    watcher_thread.join();

    out_queue.stop();

    RBLog("ClientFLow >> Waiting for sender threads to finish...", LogLevel::INFO);
    for (auto &s : senders_pool) {
        try {
            s->join();
        } catch (std::exception &e) {
        }
    }
}

void ClientFlow::stop() {
    keep_going = false;
    waiter_cv.notify_all();
}
