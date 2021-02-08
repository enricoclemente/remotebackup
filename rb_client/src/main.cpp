#include <signal.h>

#include <mutex>
#include <thread>

#include "ConfigMap.hpp"
#include "ClientFlow.h"


char CONFIG_FILE_PATH[] = "./rbclient.conf";

std::function<void(void)> sig_int_handler;

void handle_sig_int(int n) {
    sig_int_handler();
}

int main(int argc, char **argv) {

    ConfigMap config;
    config["root_folder"] = "./data";
    config["port"] = "8888";
    config["host"] = "localhost";
    config["username"] = "u1";
    config["password"] = "u1";
    config["connection_timeout"] = "5000";
    config["watcher_interval"] = "3000";
    config["sender_threads_num"] = "4";

    RBLog("Main >> Reading config...", LogLevel::INFO);
    config.load(CONFIG_FILE_PATH);
    config.store(CONFIG_FILE_PATH);

    fs::path root_folder(config["root_folder"]);
    fs::create_directory(root_folder);   // directory is created only if not already present

    ClientFlow client_logic(
        config["host"], config["port"],
        config["root_folder"],
        config["username"], config["password"],
        std::chrono::milliseconds(config.get_numeric("watcher_interval")),
        config.get_numeric("connection_timeout"),
        config.get_numeric("sender_threads_num")
    );

    std::mutex waiter;

    void (*original_sigint_handler)(int) = signal(SIGINT, handle_sig_int);
    sig_int_handler = [&]() {
        waiter.unlock();
        signal(SIGINT, original_sigint_handler);
    };

    client_logic.start();

    waiter.lock();
    RBLog("Main >> Client started!", LogLevel::INFO);
    waiter.lock();

    RBLog("Main >> Stopping client...", LogLevel::INFO);
    client_logic.stop();

    RBLog("Main >> Client stopped!", LogLevel::INFO);
    exit(0);
}
