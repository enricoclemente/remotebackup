//
// Created by Enrico Clemente on 05/12/2020.
//
#include "Client.h"
#include "OutputQueue.h"
#include "CRC.h"

#define MAXFILESEGMENT 1000000

class ClientFlow {
private:
    Client client;

public:
    ClientFlow(Client client);
    std::string authenticate(const std::string& username, const std::string& password);
    RBResponse get_server_status();
    void upload_file(const std::shared_ptr<FileOperation>& file_operation, const std::string& root_path);
    void remove_file(const std::shared_ptr<FileOperation>& file_operation, const std::string& root_path);

};


