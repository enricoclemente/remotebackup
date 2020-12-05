#include "FileWatcher.h"
#include "ClientFlow.h"

#include <iostream>
#include <thread>
#include <mutex>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;

// Concurrent console printing
std::mutex pm;
void myprint(const std::string& output) {
    std::lock_guard lg(pm);
    std::cout<<output<<std::endl;
}

std::vector<std::string> split_string(const std::string& str, const std::string& delimitator) {
    std::vector<std::string> container;
    split(container, str, is_any_of(delimitator));

    return container;
}


int main() {

    // Client pippo("192.168.1.3", "8888");
    FileWatcher fw{"/Users/enricoclemente/Downloads", std::chrono::milliseconds(5000)};
    OutputQueue oq;

    bool running = true;

    // Thread to monitor the file system watcher every 5s
    std::thread system([&fw, &oq](){
        fw.start_monitoring([&oq](std::string file_path, std::time_t last_write_time, FileStatus status) -> void{
            if(!is_regular_file(file_path) && status != FileStatus::REMOVED ) {
                return;
            }

            oq.add_file_operation(file_path, static_cast<FileCommand>(status), last_write_time);
        });
    });


    // Thread for sending files to the server
    std::thread sender([&oq, &fw, running](){
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