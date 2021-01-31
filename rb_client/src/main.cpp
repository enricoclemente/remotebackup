#include <signal.h>

#include <mutex>
#include <thread>

#include "ConfigMap.hpp"
#include "ClientFlow.h"

using namespace boost;

char CONFIG_FILE_PATH[] = "./rbclient.conf";

std::mutex waiter;
std::atomic<bool> keep_going = true;
// function
void stop_client(int n) {
    keep_going = false;
    RBLog("Stopping client...", LogLevel::INFO);
    waiter.unlock();
}


int main(int argc, char **argv) {
    ConfigMap config;
    config["root_folder"] = "./data";
    config["port"] = "8888";
    config["host"] = "localhost";
    config["username"] = "u1";
    config["password"] = "u1";
    config["connection_timeout"] = "5000";
    config["watcher_interval"] = "3000";

    RBLog("Reading config...", LogLevel::INFO);
    config.load(CONFIG_FILE_PATH);
    config.store(CONFIG_FILE_PATH);

    boost::filesystem::path root_folder(config["root_folder"]);
    boost::filesystem::create_directory(root_folder);   // directory is created only if not already present

    ClientFlow client_logic(
        config["host"], config["port"], config["root_folder"],
        config.get_numeric("connection_timeout")
    );

    FileManager file_manager(
        root_folder, 
        std::chrono::milliseconds(config.get_numeric("watcher_interval"))
    );

    OutputQueue out_queue;


    RBLog("Authenticating...", LogLevel::INFO);
    try {
        client_logic.authenticate(config["username"], config["password"]);
    } catch (RBException &e) {
        RBLog("Authentication failed: " + e.getMsg(), LogLevel::ERROR);
        exit(-1);
    }


    RBLog("Starting file watcher...", LogLevel::INFO);
    // thread for keep running file watcher and inital probe
    std::thread system([&]() {
        // lambda function to handle file system changes and file system compare
        auto update_handler = [&](const std::string &path, const file_metadata &meta, FileStatus status) {
            try {
                if (!filesystem::is_regular_file(root_folder.string() + '/' + path) && status != FileStatus::REMOVED)
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

                RBLog("Added file operation for " + path + " status: " + status_to_print, LogLevel::INFO);
                RBLog("File metadata: size " + std::to_string(meta.size) +
                    " last write time " + std::to_string(meta.last_write_time) +
                    " checksum " + std::to_string(meta.checksum));

                out_queue.add_file_operation(path, meta, command);
            } catch (RBException &e) {
                RBLog("RBException:" + e.getMsg(), LogLevel::ERROR);
            } catch (std::exception &e) {
                RBLog("exception:" + std::string(e.what()), LogLevel::ERROR);
            }
        };

        std::unordered_map server_files = client_logic.get_server_files();      // probing server
        file_manager.file_system_compare(server_files, update_handler);     // comparing client and server files
        file_manager.start_monitoring(update_handler);      // running file watcher to monitor client fs
    });


    RBLog("Starting RBProto client...", LogLevel::INFO);
    int attempt_count = 0;
    int max_attempts = 3;
    // thread for handling file operation: sending requests and receiving responses
    std::thread sender([&]() {
        while (true) {
            if(attempt_count == max_attempts) {
                RBLog("Reached max attempts number. Terminating client", LogLevel::ERROR);
                stop_client(0);
                break;
            }

            RBLog("Processing");
            auto op = out_queue.get_file_operation();
            std::string command_to_print;

            if (!keep_going.load()) break;
            try {
                switch (op->get_command()) {
                    case FileCommand::UPLOAD:
                        command_to_print = "UPLOAD";
                        RBLog("Got operation for: " + op->get_path() + " command: " + command_to_print,
                                LogLevel::INFO);
                        client_logic.upload_file(op);
                        break;
                    case FileCommand::REMOVE:
                        command_to_print = "REMOVE;";
                        RBLog("Got operation for: " + op->get_path() + " command: " + command_to_print,
                                LogLevel::INFO);
                        client_logic.remove_file(op);
                        break;
                    default:
                        break;
                }

                RBLog("Operation for: " + op->get_path() + " command: " + command_to_print + " correctly ended",
                        LogLevel::INFO);

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
    });

    signal(SIGINT, stop_client);
    waiter.lock();

    RBLog("Client started!", LogLevel::INFO);
    waiter.lock();

    RBLog("Stopping monitoring...", LogLevel::INFO);
    file_manager.stop_monitoring();

    RBLog("Stopping RBProto client...", LogLevel::INFO);
    client_logic.stop();

    RBLog("Waiting for watcher thread to finish...", LogLevel::INFO);
    system.join();

    RBLog("Waiting for RBProto thread to finish...", LogLevel::INFO);
    sender.join();

    RBLog("Client stopped!", LogLevel::INFO);
    return 0;
}
