#include "ClientFlow.h"

#include <iostream>
#include <thread>
#include <mutex>

#include <utility>

using namespace boost;

// Concurrent console printing
std::mutex pm;

void myprint(const std::string &output) {
    std::lock_guard lg(pm);
    std::cout << output << std::endl;
}


int main() {

    // Client pippo("192.168.1.3", "8888");
    auto file_manager = std::make_shared<FileManager>();

    OutputQueue oq;
    bool running = true;

    // Thread to monitor the file system watcher every 5s
    std::thread system([&file_manager, &oq]() {
        file_manager->set_file_watcher("/Users/enricoclemente/Downloads", std::chrono::milliseconds(5000));

        file_manager->start_monitoring([&oq] (const std::string &relative_file_path, file_metadata metadata,
                                                FileStatus status) -> void {
            if (!filesystem::is_regular_file(relative_file_path) && status != FileStatus::REMOVED) {
                return;
            }
            myprint("File " + relative_file_path + " event " + std::to_string(static_cast<int>(status)));
            oq.add_file_operation(relative_file_path, metadata, static_cast<FileCommand>(status));
        });
    });


    // Thread for sending files to the server
    std::thread sender([&oq, &file_manager, running](){
        bool net_op = false;
        while(running) {
            auto file_operation = oq.get_file_operation();
            myprint("System modified: " + file_operation->get_path());

            if(file_operation->get_abort()) {
                // Loggo che è stato abortito
            } else {


            }
        }
    });

    sender.join();

    system.join();

    return 0;
}

int main_test() {
    Client pippo("127.0.0.1", "8888");

    RBRequest test;

    test.set_type(RBMsgType::AUTH);
    test.set_final(true);

    std::cout << "Running test request" << std::endl;

    try {
        RBResponse res = pippo.run(test);
        std::cout << "RES: " << res.type() << std::endl;
        if (res.error().size()) {
            RBLog(res.error());
        }
    } catch (RBException &e) {
        excHandler(e);
    }


    return 0;

}