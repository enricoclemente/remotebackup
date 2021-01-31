#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <boost/filesystem.hpp>
#include <string>

#include "ServerFlow.h"


namespace fs = boost::filesystem;

int main() {

    ServerFlow server_logic(8888, 8, "./rbserver_data");
    

    RBLog("CONSOLE >> Server console ready, enter `help` for available commands");
    while(true) {
        try {
            std::string console_line_s;
            std::getline(std::cin, console_line_s);
            std::istringstream console_line(console_line_s);
            std::string command;
            console_line >> command;
            if (command == "stop") {
                server_logic.stop();
                exit(0);
            }
            if (command == "clear") {
                server_logic.clear();
                RBLog("CONSOLE >> Will now exit.");
                // Since the server listener is unstoppable we have to exit here.
                exit(10);
            }
            if (command == "adduser") {
                std::string uname, pw;
                console_line >> uname >> pw;
                if (uname.empty() || pw.empty()) {
                    RBLog("CONSOLE >> `adduser` expects 2 argument.\n"
                        "Usage: `adduser <username> <passowrd>`", LogLevel::ERROR);
                    continue;
                }
                AuthController::get_instance().add_user(uname, pw);
                RBLog("CONSOLE >> user added correctly!");
            } else if (command == "help") {
                RBLog("CONSOLE >> available commands: `stop`, `clear`, `adduser`");
            } else {
                throw RBException("unknown_command: <" + command + ">");
            }
        } catch(RBException &e) {
            RBLog("CONSOLE >> RBException: " + e.getMsg(), LogLevel::ERROR);
        } catch(std::exception &e) {
            RBLog("CONSOLE >> std::exception: " + std::string(e.what()), LogLevel::ERROR);
        }
    }
}
