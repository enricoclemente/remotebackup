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

void test_auth_req() {
    auto& db = Database::get_instance();
    auto& auth_controller = AuthController::get_instance();
    
    std::string hash = AuthController::get_instance().sha256("test-pw");

    try {
        db.query("INSERT INTO users (username, password) VALUES ('test-user', ?);", {hash});
    } catch (RBException &e) {
        RBLog("RB >> " + e.getMsg(), LogLevel::ERROR);
    }
    
    std::string token;
    RBAuthRequest auth_request;
    auth_request.set_user("test-user");
    auth_request.set_pass("test-pw");
    try {
        auth_controller.auth_by_credentials(auth_request.user(), auth_request.pass());
        token = auth_controller.generate_token("test-user");
    } catch (RBException &e) {
        RBLog("RB >> " + e.getMsg(), LogLevel::ERROR);
    }

    RBRequest request;
    request.set_token(token);
    try {
        auth_controller.auth_by_token(request.token());
    } catch (RBException &e) {
        RBLog("RB >> " + e.getMsg(), LogLevel::ERROR);
    }
}

void add_user_u1() {
    auto& db = Database::get_instance();
    std::string hash = AuthController::get_instance().sha256("u1");
    db.query("INSERT INTO users (username, password) VALUES ('u1', ?);", {hash});
}

void test_fsm() {
    FileSystemManager fsm("./rbserver_data");
    auto res = fsm.file_exists("u1", fs::path("example.txt"));
}

void print_db() {
    auto& db = Database::get_instance();
    db.exec("SELECT * FROM users;");
    db.exec("SELECT * FROM fs;");
}

typedef atomic_map<std::string, std::shared_ptr<Service>> svc_atomic_map_t;

int main() {
    auto& db = Database::get_instance();
    db.open(); // Do this operation the first time an instance is retrieved
    
    RBLog("RB >> TESTS STARTING");
    add_user_u1();
    test_fsm();
    test_auth_req();
    print_db();
    RBLog("RB >> TESTS ENDED\n\n\n");

    FileSystemManager fsm("./rbserver_data");
    svc_atomic_map_t svc_map(8);

    Server srv(8888, [&svc_map, &fsm]
        (RBRequest req, std::shared_ptr<Service> worker) {

        RBResponse res;
        res.set_success(false);
        res.set_protover(3);
        res.set_type(req.type());

        try {
            if (req.type() == RBMsgType::AUTH) {
                validateRBProto(req, RBMsgType::AUTH, 3);
                RBLog("RB >> AUTH request received", LogLevel::INFO);

                std::string username = req.auth_request().user();
                std::string password = req.auth_request().pass();

                auto& auth_controller = AuthController::get_instance();
                auth_controller.auth_by_credentials(username, password);
                
                std::string token = auth_controller.generate_token(username);
                auto auth_response = std::make_unique<RBAuthResponse>();
                auth_response->set_token(token);
                res.set_allocated_auth_response(auth_response.release());
                res.set_success(true);
            } else if (req.type() == RBMsgType::UPLOAD) {
                validateRBProto(req, RBMsgType::UPLOAD, 3);
                RBLog("RB >> UPLOAD request received", LogLevel::INFO);

                // Authenticate the request
                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());
                RBLog("RB >> Request authenticated successfully");

                try {
                    std::string file_token = 
                        username + ">" + req.file_segment().path();
                    svc_atomic_map_t::guard sv_grd(svc_map, file_token, worker);

                    fsm.write_file(username, req);
                    res.set_success(true);
                } catch (svc_atomic_map_t::key_already_present  &e) {
                    throw RBException("concurrent_write");
                }

            } else if (req.type() == RBMsgType::REMOVE) {
                validateRBProto(req, RBMsgType::REMOVE, 3);
                RBLog("RB >> REMOVE request received", LogLevel::INFO);

                // Authenticate the request
                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());
                RBLog("RB >> Request authenticated successfully");

                try {
                    std::string file_token = 
                        username + ">" + req.file_segment().path();
                    svc_atomic_map_t::guard sv_grd(svc_map, file_token, worker);

                    fsm.remove_file(username, req);
                    res.set_success(true);
                } catch (svc_atomic_map_t::key_already_present  &e) {
                    throw RBException("concurrent_write");
                }
            } else if (req.type() == RBMsgType::ABORT) {
                // CHECK
                validateRBProto(req, RBMsgType::ABORT, 3);
                RBLog("RB >> ABORT request received", LogLevel::INFO);

                // Authenticate the request
                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());
                RBLog("RB >> Request authenticated successfully");

                try {
                    std::string file_token = 
                        username + ">" + req.file_segment().path();
                    svc_atomic_map_t::guard sv_grd(svc_map, file_token, worker);

                    fsm.remove_file(username, req);
                    res.set_success(true);
                } catch (svc_atomic_map_t::key_already_present  &e) {
                    throw RBException("concurrent_write");
                }
            } else if (req.type() == RBMsgType::PROBE) {
                validateRBProto(req, RBMsgType::PROBE, 3);
                RBLog("RB >> PROBE request received", LogLevel::INFO);

                // Authenticate the request
                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());
                RBLog("RB >> Request authenticated successfully");

                auto files = fsm.get_files(username);

                auto probe_res = std::make_unique<RBProbeResponse>();
                auto mutable_files = probe_res->mutable_files();
                mutable_files->insert(files.begin(), files.end());

                res.set_allocated_probe_response(probe_res.release());
                res.set_success(true);
            } else {
                throw RBException("invalid_request");
            }

            if (!res.success()) throw RBException("this_shouldnt_happen");
        } catch (RBException& e) {
            RBLog("RB >> RBProto handling error: " + e.getMsg(), LogLevel::ERROR);
            res.set_error(e.getMsg());
        } catch (std::exception &e) {
            RBLog(std::string("RB >> ") + e.what(), LogLevel::ERROR);
            res.set_error("internal_server_error");
        }

        return res;
    });

    srv.start();
    std::this_thread::sleep_for(std::chrono::seconds(3600));
    srv.stop();
    // TODO wait for thread join

    db.close();
}
