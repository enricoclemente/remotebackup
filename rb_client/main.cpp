#include "FileWatcher.h"
#include "OutputQueue.h"
#include "Buffer.h"
#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "packet.pb.h"

#include <iostream>
#include <thread>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;
using boost::asio::ip::tcp;
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

struct file_transfer {
    std::string file_path;
    std::time_t last_write_time;
    FileStatus command;
};

int main() {

    FileWatcher fw{"/Users/enricoclemente/Downloads", std::chrono::milliseconds(5000)};
    OutputQueue<file_transfer> oq;

    bool running = true;
    Buffer buf;

    // Setting connection with the server
    const char* hostname = "192.168.1.3";
    const char* port = "4001";
    boost::asio::io_service io_service;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(hostname, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::socket socket(io_service);
    boost::asio::connect(socket, endpoint_iterator);
    AsioInputStream<tcp::socket> ais(socket);
    CopyingInputStreamAdaptor cis_adp(&ais);
    AsioOutputStream<boost::asio::ip::tcp::socket> aos(socket);
    CopyingOutputStreamAdaptor cos_adp(&aos);


    // Thread to monitor the file system watcher every 5s
    std::thread system([&fw, &oq](){
        fw.start_monitoring([&oq](std::string file_path, std::time_t last_write_time, FileStatus status) -> void{
            if(!is_regular_file(file_path) && status != FileStatus::erased ) {
                return;
            }

            file_transfer ft{file_path, last_write_time, status};
            oq.push(ft);
        });
    });

    // Thread for sending files to the server
    std::thread sender([&oq, &buf, &fw, &cis_adp, &cos_adp, running](){

        while(running) {
            auto file_operation = oq.pop();
            // probe del singolo file

            // se si mando il file.
            myprint("Sending: " + file_operation.file_path);
            FilePacket packet = buf.create_FilePacket(fw.get_path_to_watch(), file_operation.file_path,
                          file_operation.last_write_time, file_operation.command);
            google::protobuf::io::writeDelimitedTo(packet, &cos_adp);
            // Now we have to flush, otherwise the write to the socket won't happen until enough bytes accumulate
            cos_adp.Flush();

            // ricevo conferma
        }
    });


    // Fake server "receiving" every 10s
    std::thread receiving_thread([&buf, running](){
        while(running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10000));

            auto packet = buf.receive_file();

            if(!packet.file_path().empty()){
                myprint("I just received: " + packet.file_path());

                // fake write on server
                ofstream fout("/Users/enricoclemente/Desktop/Provastream/" + packet.file_path() ,
                        std::ios::out | std::ios::binary);

                int chuncks = packet.file_chuncks_size();
                for(int i=0; i<chuncks; i++) {
                    fout.write((char*)&packet.file_chuncks(i)[0],
                            packet.file_chuncks(i).size() * sizeof(char));
                }

                fout.close();
            }
        }
    });


    receiving_thread.join();
    sender.join();
    system.join();

    return 0;
}