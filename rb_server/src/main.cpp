#include <iostream>
#include <boost/asio.hpp>
#include <fstream>

#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "rbproto.pb.h"
#include "Server.h"
#include "Database.h"
#include "AuthController.h"

void test_db_and_auth() {
    auto& db = Database::get_instance();
    auto& auth_controller = AuthController::get_instance();
    
    std::string hash = AuthController::get_instance().sha256("test-pw");
    db.query("INSERT INTO users (username, password) VALUES ('test-user', ?);", {hash});

    db.exec("SELECT * FROM users;");
    
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

int main() {
    auto& db = Database::get_instance();
    db.open();

    test_db_and_auth();

    Server srv(8888, [](RBRequest req) {

        RBResponse res;

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

        res.set_type(req.type());

        return res;
    });

    srv.start();
    std::this_thread::sleep_for(std::chrono::seconds(60));
    srv.stop();

    db.close();
}
