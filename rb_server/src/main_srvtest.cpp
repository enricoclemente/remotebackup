#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "rb_request.pb.h"
#include "rb_response.pb.h"
#include "packet.pb.h"
#include "CRC.h"

#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <Server.h>

int main() {
    Server srv(8888, [](RBRequest req) {

        if (req.type() == "auth") {
            std::cout << "Auth request!";
        } else if (req.type() == "upload") {
            std::cout << "Upload request!";
        } else if (req.type() == "remove") {
            std::cout << "Remove request!";
        }

        RBResponse res;
        res.set_type("res_to_" + req.type());

        return res;
    });

    srv.start();
    std::this_thread::sleep_for(std::chrono::seconds(60));
    srv.stop();
}

