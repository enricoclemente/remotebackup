#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <boost/filesystem.hpp>

#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "rbproto.pb.h"
#include "Server.h"
#include "Database.h"
#include "AuthController.h"
#include "FileSystemManager.h"
#include "AtomicMap.hpp"

namespace fs = boost::filesystem;

void test_db_and_auth() {
    auto& db = Database::get_instance();
    auto& auth_controller = AuthController::get_instance();
    
    std::string hash = AuthController::get_instance().sha256("test-pw");
    db.query("INSERT INTO users (username, password) VALUES ('test-user', ?);", {hash});

    db.exec("SELECT * FROM users;");
    db.exec("SELECT * FROM fs;");
    
    std::string token;
    RBAuthRequest auth_request;
    auth_request.set_user("test-user");
    auth_request.set_pass("test-pw");
    if (auth_controller.auth_by_credentials(auth_request.user(), auth_request.pass())) {
        RBLog("User authenticated successfully by username and password");
        token = auth_controller.generate_token("test-user");
    }

    RBRequest request;
    request.set_token(token);
    if (auth_controller.auth_by_token(request.token()))
        RBLog("User authenticated successfully by token");
}

void add_user_u1() {
    auto& db = Database::get_instance();
    
    std::string hash = AuthController::get_instance().sha256("u1");
    db.query("INSERT INTO users (username, password) VALUES ('u1', ?);", {hash});

    db.exec("SELECT * FROM users;");
}

void test_fsmanager() {
    FileSystemManager fs;

    bool res = fs.write_file("u1", fs::path("./u1/ciao.txt"), "This is the first file", "0", "0", "0");
    std::cout << "File written: " << res << std::endl;

    res = fs.find_file("u1", fs::path("./u1/ciao.txt"));
    std::cout << "File found: " << res << std::endl;
}

int main() {
    auto& db = Database::get_instance();
    db.open();
    
    add_user_u1();
    test_fsmanager();

    test_db_and_auth();

    atomic_map<std::string, std::shared_ptr<Service>> svc_map(8);

    Server srv(8888, [&svc_map](RBRequest req, std::shared_ptr<Service> worker) {

        RBResponse res;

        try {
            std::string chk = "testString";
            atomic_map_guard<std::string, std::shared_ptr<Service>> amg(svc_map, chk, worker);

            // save stuff
        } catch (exception& e) {
            res.set_success(false);
            res.set_error("upload_refused");
            return res;
        }

        try {
            if (req.type() == RBMsgType::AUTH) {
                RBLog("Auth request!");

                std::string username = req.auth_request().user();
                std::string password = req.auth_request().pass();

                auto& auth_controller = AuthController::get_instance();
                if (!auth_controller.auth_by_credentials(username, password))
                    throw RBException("Invalid authentication");
                
                std::string token = auth_controller.generate_token(username);
                auto auth_response = std::make_unique<RBAuthResponse>();
                auth_response->set_token(token);
                res.set_allocated_auth_response(auth_response.release());
            } else if (req.type() == RBMsgType::UPLOAD) {
                RBLog("Upload request!");

                // Authenticate the request
                auto& auth_controller = AuthController::get_instance();
                auto username = auth_controller.auth_get_user_by_token(req.token());
                if (username.empty())
                    throw RBException("Invalid authentication");
                
                bool ok = worker->accumulate_data(req);
                if (!ok)
                    throw RBException("Invalid request data");
                
                if (req.final()) {
                    auto req_path = req.file_segment().path();
                    auto req_checksum = req.file_segment().file_metadata().checksum();
                    auto req_last_write_time = req.file_segment().file_metadata().last_write_time();
                    auto req_file_size = req.file_segment().file_metadata().size();

                    fs::path path(req_path);

                    std::string content = worker->get_data();

                    std::stringstream ss;
                    ss << req_file_size;
                    std::string file_size = ss.str();

                    ss.str(std::string());
                    ss.clear();

                    ss << req_last_write_time;
                    std::string last_write_time = ss.str();

                    ss.str(std::string());
                    ss.clear();

                    ss << req_last_write_time;
                    std::string checksum = ss.str();

                    ss.str(std::string());
                    ss.clear();

                    FileSystemManager fs;
                    fs.write_file(username, path, content, checksum, last_write_time, file_size);
                }
            } else if (req.type() == RBMsgType::REMOVE) {
                res.set_error("unimplemented:REMOVE");
                RBLog("Remove request!");
            } else if (req.type() == RBMsgType::PROBE) {
                res.set_error("unimplemented:PROBE");
                RBLog("Probe request!");
            } else {
                throw RBException("unknownReqType:"+ std::to_string(req.type()));
            }
        } catch (RBException& e) {
            RBLog(e.getMsg());
            res.set_error(e.getMsg());
        }

        svc_map.remove("testString");

        res.set_protover(3);
        res.set_type(req.type());
        if (res.error().empty()) res.set_success(true);

        return res;
    });

    srv.start();
    std::this_thread::sleep_for(std::chrono::seconds(60));
    srv.stop();

    db.close();
}
