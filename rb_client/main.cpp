#include "FileWatcher.h"
#include "OutputQueue.h"

#include <iostream>
#include <thread>
#include <mutex>

#include <boost/asio.hpp>

using namespace boost;

// Concurrent console printing
std::mutex pm;
void myprint(const std::string& output) {
    std::lock_guard lg(pm);
    std::cout<<output<<std::endl;
}


// Fake TCP stream
class Buffer{
    asio::streambuf buf;
    std::mutex bufm;

public:

    void write(std::string message) {
        std::lock_guard lg(bufm);
        std::ostream output(&buf);

        output<< message << "\n";
    }

    std::string read() {
        std::lock_guard lg(bufm);
        std::istream input(&buf);

        std::string message;
        std::getline(input, message);

        return message;
    }
};


int main() {

    FileWatcher fw{"/Users/enricoclemente/Downloads", std::chrono::milliseconds(5000)};

    OutputQueue<std::string> oq;

    std::vector<std::thread> sending_threads;
    bool running = true;

    Buffer buf;

    // File system watcher thread which fill every 5s the output queue if any change
    std::thread system([&fw, &oq](){
        fw.start([&oq](std::string path_to_watch, FileStatus status) -> void{
            if(!is_regular_file(path_to_watch) && status != FileStatus::erased ) {
                return;
            }

            switch(status) {
                case FileStatus::created:
                    //std::cout << "File created: " << path_to_watch << '\n';
                    myprint("File created: " + path_to_watch);
                    oq.push("CREATE " + path_to_watch);
                    break;
                case FileStatus::modified:
                    //std::cout << "File modified: " << path_to_watch << '\n';
                    myprint("File modified: " + path_to_watch);
                    oq.push("MODIFY " + path_to_watch);
                    break;
                case FileStatus::erased:
                    //std::cout << "File erased: " << path_to_watch << '\n';
                    myprint("File erased: " + path_to_watch);
                    oq.push("ERASE " + path_to_watch);
                    break;
                default:
                    //std::cout << "Error! Unknown file status.\n";
                    myprint("Error! Unknown file status.");
            }
        });
    });

    // Thread for sending files to the server
    for(int i=0; i<3; i++) {
        sending_threads.emplace_back([&oq, &buf, i, running](){
            while(running) {

                std::string operation = oq.pop();

                // probe del singolo file
                // se si mando il file.
                buf.write(operation);
                //myprint("I'm going to " + operation);


                //int queue_size = oq.size();
                //myprint("Queue size: " + std::to_string(queue_size));
                // altrimenti non faccio niente

            }
        });
    }


    // Fake server "receiving" every 10s
    std::thread receiving_thread([&buf, running](){
        while(running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));
            std::string message = buf.read();
            if(!message.empty())   myprint("I just received: " + message);
        }
    });


    for(auto &t: sending_threads) {
        if(t.joinable()) t.join();
    }

    receiving_thread.join();
    system.join();



    return 0;
}