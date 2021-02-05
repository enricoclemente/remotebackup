#include <iostream>
#include <boost/asio.hpp>
#include <fstream>
#include <string>

#include "ServerFlow.h"


std::function<void(void)> sig_int_handler;

void handle_sig_int(int n) {
    sig_int_handler();
}

std::atomic<bool> keep_going = true;

int main() {

    ServerFlow server_logic(8888, 8, "./rbserver_data");
    
    void (*original_sigint_handler)(int) = signal(SIGINT, handle_sig_int);

    sig_int_handler = [&server_logic, &original_sigint_handler]() {
        keep_going.store(false);
        RBLog("CONSOLE >> SIGINT intercepted!", LogLevel::INFO);
        signal(SIGINT, original_sigint_handler);
        server_logic.stop();
        exit(0);
    };

    RBLog("CONSOLE >> Server console ready, enter `help` for available commands");

    while(keep_going.load()) {
        try {
            std::string console_line_s;
            std::getline(std::cin, console_line_s);
            std::istringstream console_line(console_line_s);
            std::string command;
            console_line >> command;
            if (command == "stop") {
                break;
            }
            if (command == "clear") {
                server_logic.clear();
                RBLog("CONSOLE >> Server cleared correctly! Will now restart...");
                server_logic.start();
                continue;
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
            std::cin.clear();
        } catch(std::exception &e) {
            RBLog("CONSOLE >> std::exception: " + std::string(e.what()), LogLevel::ERROR);
            std::cin.clear();
        }
    }

    server_logic.stop();
}
