#pragma once
#include "Client.h"
#include "OutputQueue.h"
#include <mutex>

#define PROTOCHANNEL_POOL_TIMEOUT_SECS 5

class ClientFlow {
private:
    class ClientFlowConsumer {
    private:
        std::thread sender;
        std::shared_ptr<ProtoChannel> pc;
        Client &client;
        std::chrono::system_clock::time_point last_use;
        std::mutex m;
        void clear_protochannel();
    public:
        ClientFlowConsumer(Client &, std::function<void(ClientFlowConsumer&)>);
        std::shared_ptr<ProtoChannel> get_protochannel();
        void clean_protochannel();
        void join();
    };

    Client client;
    OutputQueue out_queue;
    FileManager file_manager;

    std::string username;
    std::string password;
    bool restore_from_server;
    fs::path root_path;

    int senders_pool_n;

    std::thread watcher_thread;
    void watcher_loop();

    std::vector<std::unique_ptr<ClientFlowConsumer>> senders_pool;
    void sender_loop(ClientFlowConsumer &cfc);

    std::atomic<bool> keep_going = true;
    std::mutex waiter;
    std::condition_variable waiter_cv;

    std::unordered_map<std::string, file_metadata> get_server_state();
    void get_server_files(const std::unordered_map<std::string, file_metadata>&);
    bool upload_file(const std::shared_ptr<FileOperation> &file_operationh, ClientFlowConsumer &cfc);
    void remove_file(const std::shared_ptr<FileOperation> &file_operation, ClientFlowConsumer &cfc);

    std::thread watchdog;

public:
    ClientFlow(
        const std::string &ip,
        const std::string &port,
        const std::string &root_path,
        const std::string &username,
        const std::string &password,
        bool restore_from_server,
        std::chrono::system_clock::duration watcher_interval,
        int senders_pool_n
    );

    void stop();
    void start();
    void run();
};
