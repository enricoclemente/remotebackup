#pragma once
#include "Server.h"
#include "FileSystemManager.h"
#include "AuthController.h"
#include "AtomicMap.hpp"


typedef atomic_map<std::string, std::shared_ptr<Service>> svc_atomic_map_t;

class ServerFlow {
public:
    ServerFlow(unsigned short port, int workersLimit, const std::string & rootPath) 
        : svc_map(workersLimit), fsm(rootPath), srv(port, [&](RBRequest req, std::shared_ptr<Service> worker) {
            return flow(req, worker);
        }) {
        db.open();
        test_all();
        add_u1();
        srv.start();
    }

    void add_u1() {
        try {
            auth_controller.add_user("u1", "u1");
        } catch (std::exception &e) {
            RBLog("SRV >> user u1 already exists");
        }
    }

    void test_auth_req() {
        try {
        
            std::string hash = AuthController::get_instance().sha256("test-pw");

            auth_controller.add_user("test-user", "test-pw");
            
            std::string token;
            RBAuthRequest auth_request;
            auth_request.set_user("test-user");
            auth_request.set_pass("test-pw");
            auth_controller.auth_by_credentials(auth_request.user(), auth_request.pass());
            token = auth_controller.generate_token("test-user");
            RBRequest request;
            request.set_token(token);
            auth_controller.auth_by_token(request.token());
        } catch (RBException &e) {
            RBLog("RB >> " + e.getMsg(), LogLevel::ERROR);
        }
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

    void test_all() {
        RBLog("RB >> TESTS STARTING");
        test_fsm();
        test_auth_req();
        print_db();
        RBLog("RB >> TESTS ENDED\n\n\n");
    }

    void stop() {
        RBLog("[SRV] Stopping server...", LogLevel::INFO);
        srv.stop();
        db.close();
    }

    void clear() {
        stop();
        RBLog("[SRV] Clearing server...", LogLevel::INFO);
        fsm.clear();
        db.clear();
        RBLog("[SRV] Server cleared!", LogLevel::INFO);
    }
private:
    Server srv;
    FileSystemManager fsm;
    svc_atomic_map_t svc_map;
    Database & db = Database::get_instance();
    AuthController & auth_controller = AuthController::get_instance();

    RBResponse inline flow(RBRequest req, std::shared_ptr<Service> worker) {
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
                auto username = auth_controller.auth_get_user_by_token(req.token());
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
                auto username = auth_controller.auth_get_user_by_token(req.token());
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
                validateRBProto(req, RBMsgType::ABORT, 3);
                RBLog("RB >> ABORT request received", LogLevel::INFO);

                // Authenticate the request
                auto username = auth_controller.auth_get_user_by_token(req.token());
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
                auto username = auth_controller.auth_get_user_by_token(req.token());
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
    }
};
