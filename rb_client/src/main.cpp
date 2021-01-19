#include <signal.h>

#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

#include "ConfigMap.hpp"
#include "ClientFlow.h"

using namespace boost;

char CONFIG_FILE_PATH[] = "./rbclient.conf";

std::mutex waiter;
std::atomic<bool> keep_going = true;
void stop_client(int n) {
    keep_going = false;
    RBLog("Stopping client...");
    waiter.unlock();
}

int main(int argc, char **argv) {
    ConfigMap config;

    config["root_folder"] = "./data";
    config["port"] = "8888";
    config["host"] = "localhost";
    config["username"] = "test-user";
    config["password"] = "test-pw";
    config["connection_timeout"] = "5000";
    config["watcher_interval"] = "3000";

    std::cout << "Reading config..." << std::endl;

    config.load(CONFIG_FILE_PATH);
    config.store(CONFIG_FILE_PATH);

    boost::filesystem::path root_folder(config["root_folder"]);
    // directory is created only if not already present
    boost::filesystem::create_directory(root_folder);

    ClientFlow client_logic(
        config["host"], config["port"], config["root_folder"],
        config.get_numeric("connection_timeout")
    );
    FileManager file_manager(
        root_folder, 
        std::chrono::milliseconds(config.get_numeric("watcher_interval"))
    );
    OutputQueue out_queue;

    std::cout << "Authenticating..." << std::endl;
    try {
        client_logic.authenticate(config["username"], config["password"]);
    } catch (RBException &e) {
        RBLog("Authentication failed: " + e.getMsg());
        exit(-1);
    }

    std::cout << "Starting file watcher..." << std::endl;
    std::thread system([&]() {
        // lambda function to handle file system changes and file system compare
        auto update_handler = [&](const std::string &path, const file_metadata &meta, FileStatus status) {
            try {
                if (!filesystem::is_regular_file(root_folder.string() + '/' + path) && status != FileStatus::REMOVED)
                    return;

                FileCommand command = status == FileStatus::REMOVED 
                    ? FileCommand::REMOVE 
                    : FileCommand::UPLOAD;

                RBLog("Added file operation for " + path + " status: " + std::to_string(static_cast<int>(status)));
                out_queue.add_file_operation(path, meta, command);
            } catch (RBException &e) {
                RBLog("RBException:" + e.getMsg());
            } catch (std::exception &e) {
                RBLog("exception:" + std::string(e.what()));
            }
        };

        std::unordered_map server_files = client_logic.get_server_files();
        file_manager.file_system_compare(server_files, update_handler);
        file_manager.start_monitoring(update_handler);
    });


    RBLog("Starting RBProto client...");
    int attempt_count = 0;
    int max_attempts = 3;
    std::thread sender([&]() {
        while (true) {
            if(attempt_count == max_attempts) {
                RBLog("Reached max attempts number. Terminating client");
                stop_client(0);
                break;
            }

            RBLog("Processing");
            auto op = out_queue.get_file_operation();
            RBLog("Got operation for: " + op->get_path() + " command: " +
                std::to_string(static_cast<int>(op->get_command())));

            if (!keep_going) break;
            try {
                switch (op->get_command()) {
                case FileCommand::UPLOAD:

                    client_logic.upload_file(op);
                    break;
                case FileCommand::REMOVE:
                    client_logic.remove_file(op);
                    break;
                default:
                    break;
                }

                RBLog("Operation for: " + op->get_path() + " command: " +
                      std::to_string(static_cast<int>(op->get_command())) + " correctly ended");

                attempt_count = 0;
                out_queue.remove_file_operation(op->get_id());
            } catch (RBException &e) {
                attempt_count++;
                RBLog("RBException:" + e.getMsg());
                out_queue.free_file_operation(op->get_id());
            } catch (std::exception &e) {
                attempt_count++;
                RBLog("exception:" + std::string(e.what()));
                out_queue.free_file_operation(op->get_id());
            }
        }
    });

    signal(SIGINT, stop_client);
    waiter.lock();

    RBLog("Client started!");
    waiter.lock();

    RBLog("Stopping monitoring...");
    file_manager.stop_monitoring();

    RBLog("Waiting for watcher thread to finish...");
    system.join();

    RBLog("Waiting for RBProto thread to finish...");
    sender.join();

    RBLog("Client stopped!");
    return 0;
}
