//
// Created by Enrico Clemente on 05/12/2020.
//
#include "Client.h"
#include "OutputQueue.h"


class ClientFlow {
private:
    Client client;
    filesystem::path root_path;
    std::atomic_bool keep_going;

public:
    ClientFlow(
        const std::string& ip,
        const std::string& port,
        const std::string &root_path,
        int socket_timeout
    );

    void authenticate(const std::string &username, const std::string &password);

    std::unordered_map<std::string, file_metadata> get_server_files();

    void upload_file(const std::shared_ptr<FileOperation> &file_operationh);

    void remove_file(const std::shared_ptr<FileOperation> &file_operation);

    void stop() { keep_going.store(false); }
};
