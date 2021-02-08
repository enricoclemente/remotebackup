#pragma once
#include "Client.h"
#include "OutputQueue.h"

class ClientFlow {
private:
    Client client;
    OutputQueue out_queue;
    FileManager file_manager;

    std::string username;
    std::string password;
    fs::path root_path;

    int senders_pool_n;

    std::thread watcher_thread;
    void watcher_loop();

    std::vector<std::thread> senders_pool;
    void sender_loop();

    std::atomic<bool> keep_going = true;

    std::unordered_map<std::string, file_metadata> get_server_files();
    bool upload_file(const std::shared_ptr<FileOperation> &file_operationh);
    void remove_file(const std::shared_ptr<FileOperation> &file_operation);

public:
    ClientFlow(
        const std::string& ip,
        const std::string& port,
        const std::string &root_path,
        const std::string &username,
        const std::string &password,
        std::chrono::system_clock::duration watcher_interval,
        int socket_timeout,
        int senders_pool_n
    );

    void stop();
    void start();
};
