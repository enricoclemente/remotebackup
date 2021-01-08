#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <filesystem>

#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "rbproto.pb.h"
#include "Server.h"
#include "Database.h"
#include "AuthController.h"
#include "FileSystemManager.h"
#include "AtomicMap.hpp"

namespace fs = std::filesystem;

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

    bool res = fs.write_file("u1", fs::path("./u1/ciao.txt"), "This is the first file");
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

    AtomicMap<std::string, std::shared_ptr<Service>> svc_map(8);
    

    Server srv(8888, [&svc_map](RBRequest req, std::shared_ptr<Service> worker) {

        RBResponse res;

        if (svc_map.has("testString")) {
            res.set_success(false);
            res.set_error("already_served");
        } else {
            svc_map.add("testString", worker);
        }

        if (req.type() == RBMsgType::AUTH) {
            res.set_error("unimplemented:AUTH");
            RBLog("Auth request!");
        } else if (req.type() == RBMsgType::UPLOAD) {
            res.set_error("unimplemented:UPLOAD");
            RBLog("Upload request!");
        } else if (req.type() == RBMsgType::REMOVE) {
            res.set_error("unimplemented:REMOVE");
            RBLog("Remove request!");
        } else if (req.type() == RBMsgType::PROBE) {
            res.set_error("unimplemented:PROBE");
            RBLog("Probe request!");
        } else {
            throw RBException("unknownReqType:"+req.type());
        }

        svc_map.remove("testString");

        res.set_type(req.type());

        return res;
    });

    srv.start();
    std::this_thread::sleep_for(std::chrono::seconds(60));
    srv.stop();

    db.close();
}
