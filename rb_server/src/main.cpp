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
    try {
        auth_controller.auth_by_credentials(auth_request.user(), auth_request.pass());
        RBLog("User authenticated successfully by username and password");
        token = auth_controller.generate_token("test-user");
    } catch (RBException &e) {
        RBLog("CAN'T AUTHENTICATE USER");
    }

    RBRequest request;
    request.set_token(token);
    try {
        auth_controller.auth_by_token(request.token());
        RBLog("User authenticated successfully by token");
    } catch (RBException &e) {
        RBLog("CAN'T AUTHENTICATE USER");
    }
}

void add_user_u1() {
    auto& db = Database::get_instance();
    
    std::string hash = AuthController::get_instance().sha256("u1");
    db.query("INSERT INTO users (username, password) VALUES ('u1', ?);", {hash});

    db.exec("SELECT * FROM users;");
}

void test_fsmanager() {
    std::cout << "Skipping test_fsmanager";
    /*FileSystemManager fs("./rbserver_data");

    bool res = fs.write_file("u1", fs::path("./u1/ciao.txt"), "This is the first file", "0", "0", "0");
    std::cout << "File written: " << res << std::endl;

    res = fs.file_exits("u1", fs::path("./u1/ciao.txt"));
    std::cout << "File found: " << res << std::endl;
    */
}

typedef atomic_map<std::string, std::shared_ptr<Service>> svc_atomic_map_t;

int main() {
    auto& db = Database::get_instance();
    db.open();
    
    add_user_u1();
    // test_fsmanager();

    // test_db_and_auth();

    FileSystemManager fsm("./rbserver_data");
    svc_atomic_map_t svc_map(8);

    Server srv(8888, [&svc_map, &fsm]
        (RBRequest req, std::shared_ptr<Service> worker) {

        RBResponse res;

        try {
            if (req.type() == RBMsgType::AUTH) {
                validateRBProto(req, RBMsgType::AUTH, 3);
                RBLog("Auth request!");

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
                RBLog("Upload request!");

                // Authenticate the request
                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());

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
                RBLog("Remove request [unimplemented]!");

                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());

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
                RBLog("Probe request!");

                // Authenticate the request
                auto username = 
                    AuthController::get_instance()
                    .auth_get_user_by_token(req.token());

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
            RBLog(e.getMsg());
            res.set_error(e.getMsg());
            res.set_success(false);
        }

        res.set_protover(3);
        res.set_type(req.type());

        return res;
    });

    srv.start();
    std::this_thread::sleep_for(std::chrono::seconds(60));
    srv.stop();

    db.close();
}
