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
    std::cout << "Stopping client..." << std::endl;
    waiter.unlock();
}

int main(int argc, char **argv) {
    ConfigMap config;

    config["root_folder"] = "./data";
    config["port"] = "8888";
    config["host"] = "localhost";
    config["username"] = "";
    config["password"] = "";
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
        config.getNumeric("connection_timeout")
    );
    FileManager file_manager(
        root_folder, 
        std::chrono::milliseconds(config.getNumeric("watcher_interval"))
    );
    OutputQueue out_queue;

    std::cout << "Authenticating..." << std::endl;
    client_logic.authenticate(config["username"], config["password"]);

    std::cout << "Starting file watcher..." << std::endl;
    std::thread system([&]() {
        file_manager.start_monitoring([&](std::string &path, const file_metadata &meta, FileStatus status) {
            try {
                if (!filesystem::is_regular_file(path) && status != FileStatus::REMOVED)
                    return;

                FileCommand command = FileCommand::UPLOAD;
                if (status == FileStatus::REMOVED)
                    command = FileCommand::REMOVE;
                out_queue.add_file_operation(path, meta, command);
            } catch (RBException &e) {
                RBLog("RBException:" + e.getMsg());
            } catch (std::exception &e) {
                RBLog("exception:" + std::string(e.what()));
            }
        });
    });

    std::cout << "Starting RBProto client..." << std::endl;
    std::thread sender([&]() {
        while (true) {
            std::cout << "Processing\n";
            auto op = out_queue.get_file_operation();
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
                std::cout << "OK\n";
            } catch (RBException &e) {
                RBLog("RBException:" + e.getMsg());
                out_queue.reinsert_file_operation(op);
            } catch (std::exception &e) {
                RBLog("exception:" + std::string(e.what()));
                out_queue.reinsert_file_operation(op);
            }
        }
    });

    signal(SIGINT, stop_client);
    waiter.lock();

    std::cout << "Client started!" << std::endl;
    waiter.lock();

    std::cout << "Stopping..." << std::endl;
    file_manager.stop_monitoring();

    std::cout << "Waiting for watcher thread to finish..." << std::endl;
    system.join();

    std::cout << "Waiting for RBProto thread to finish..." << std::endl;
    sender.join();

    std::cout << "Client stopped!" << std::endl;
    return 0;
}
