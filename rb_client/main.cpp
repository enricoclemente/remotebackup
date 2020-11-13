#include "FileWatcher.h"
#include "OutputQueue.h"

#include <iostream>
#include <thread>
#include <mutex>

#include <boost/asio.hpp>
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

// Fake TCP stream
class Buffer{
    asio::streambuf buf;
    std::mutex bufm;

public:

    std::pair<std::vector<char>, size_t> read_file_bytes(const std::string &file_path)
    {
        std::ifstream fl(file_path);
        fl.seekg( 0, std::ios::end );

        std::size_t file_len = fl.tellg();

        std::vector<char> bytes(file_len);
        fl.seekg(0, std::ios::beg);

        if (file_len)
            fl.read(&bytes[0], file_len);

        fl.close();
        return std::pair<std::vector<char>, size_t>(std::move(bytes), file_len);
    }

    void write_file(std::string comand, std::string file_path) {
        std::lock_guard lg(bufm);
        std::ostream output(&buf);

        auto file = std::move(read_file_bytes(file_path));


        output<< comand << ": " << file_path << " Lenght: " << file.second << "\n";

        output.write(&file.first[0], file.second);

    }

    std::string read_header() {
        std::lock_guard lg(bufm);
        std::istream input(&buf);

        std::string message;
        std::getline(input, message);

        return message;
    }

    std::vector<char> read_file(std::size_t file_len) {
        std::lock_guard lg(bufm);
        std::istream input(&buf);

        std::vector<char> file(file_len);
        input.read(&file[0], file_len);

        return file;
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
        fw.start_monitoring([&oq](std::string path_to_watch, FileStatus status) -> void{
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
                std::vector<std::string> arguments = split_string(operation, " ");
                buf.write_file(arguments[0], arguments[1]);
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
            std::string message = buf.read_header();
            if(!message.empty()){
                myprint("I just received: " + message);
                std::vector<std::string> header = split_string(message, " ");
                auto file = buf.read_file(std::stoi(header[3], nullptr, 10));


                // fake write on server
                std::vector<std::string> path = split_string(header[1], "/");
                ofstream fout("/Users/enricoclemente/Desktop/Provastream/" + path[4] , std::ios::out | std::ios::binary);
                fout.write((char*)&file[0], file.size() * sizeof(char));
                fout.close();


            }
        }
    });


    for(auto &t: sending_threads) {
        if(t.joinable()) t.join();
    }

    receiving_thread.join();
    system.join();



    return 0;
}