#include <signal.h>

#include <mutex>
#include <condition_variable>
#include <thread>

#include "ConfigMap.hpp"
#include "ClientFlow.h"


char CONFIG_FILE_PATH[] = "./rbclient.conf";

std::function<void(void)> sig_int_handler;

void handle_sig_int(int n) {
    sig_int_handler();
}

int main(int argc, char **argv) {

    int n_cpu_thrds = std::thread::hardware_concurrency();
    if (n_cpu_thrds > 4) n_cpu_thrds = 4;

    ConfigMap config;
    config["root_folder"] = "./data";
    config["port"] = "8888";
    config["host"] = "localhost";
    config["username"] = "";
    config["password"] = "";
    config["watcher_interval"] = "3000";
    config["sender_threads_num"] = std::to_string(n_cpu_thrds);

    RBLog("Main >> Reading config...", LogLevel::INFO);
    config.load(CONFIG_FILE_PATH);
    config.store(CONFIG_FILE_PATH);

    fs::path root_folder(config["root_folder"]);
    fs::create_directory(root_folder);   // directory is created only if not already present

    bool restore_option = false;
    if (argv[1] != nullptr) {
        if (std::string(argv[1]) == "--restore")
            restore_option = true;
        else
            RBLog("Main >> Possible arguments are: \"--restore\"");
    }

    ClientFlow client_logic(
        config["host"], config["port"],
        config["root_folder"],
        config["username"], config["password"],
        restore_option,
        std::chrono::milliseconds(config.get_numeric("watcher_interval")),
        config.get_numeric("sender_threads_num")
    );

    std::mutex waiter;

    void (*original_sigint_handler)(int) = signal(SIGINT, handle_sig_int);
    sig_int_handler = [&]() {
        client_logic.stop();
        signal(SIGINT, original_sigint_handler);
    };

    client_logic.start();

    RBLog("Main >> Client stopped!", LogLevel::INFO);
    exit(0);
}
