#include <iostream>
#include <boost/asio.hpp>
#include <fstream>

#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "rbproto.pb.h"
#include "Server.h"
#include "Database.h"
#include "AuthController.h"

int main() {
    auto& db = Database::get_instance();
    db.open();

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

