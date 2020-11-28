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



int main() {

    FileWatcher fw{"/Users/enricoclemente/Downloads", std::chrono::milliseconds(5000)};
    OutputQueue oq;

    bool running = true;
    Buffer buf;

    // Setting connection with the server
    const char* hostname = "192.168.1.3";
    const char* port = "4561";
    boost::asio::io_service io_service;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(hostname, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::socket socket(io_service);
    boost::asio::connect(socket, endpoint_iterator);
    AsioInputStream<boost::asio::ip::tcp::socket> ais(socket);
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
        bool net_op = false;
        while(running) {
            auto file_operation = oq.pop();
            myprint("System modified: " + file_operation.file_path);
            // probe del singolo file
            ProbeSingleFileRequest psfr_request = buf.create_ProbeSingleFileRequest(fw.get_path_to_watch(),
                    file_operation.file_path, file_operation.last_write_time);
            net_op = google::protobuf::io::writeDelimitedTo(psfr_request, &cos_adp);
            cos_adp.Flush();

            if(net_op) {
                myprint("Sent ProbeSingleFileRequest for: " + psfr_request.file_path());
                ProbeSingleFileResponse psfr_response;
                net_op = google::protobuf::io::readDelimitedFrom(&psfr_response, &cis_adp);

                if(net_op) {
                    myprint("Just received ProbeSingleFileResponse for: " + psfr_response.file_path());

                    if(psfr_response.send_file()) {
                        // se si mando il file.
                        myprint("Sending: " + file_operation.file_path);
                        FilePacket f_packet = buf.create_FilePacket(fw.get_path_to_watch(), file_operation.file_path,
                                                                    file_operation.last_write_time, file_operation.command);
                        myprint("Packet checksum: " + std::to_string(f_packet.file_checksum()));
                        google::protobuf::io::writeDelimitedTo(f_packet, &cos_adp);
                        // Now we have to flush, otherwise the write to the socket won't happen until enough bytes accumulate
                        cos_adp.Flush();

                        // ricevo conferma
                        FilePacketResponse fp_response;
                        google::protobuf::io::readDelimitedFrom(&fp_response, &cis_adp);
                        if(fp_response.success()) {
                            myprint("File successfully transfered");
                        } else {
                            // add again the operation in the queue
                            oq.push(file_operation);
                        }

                    }
                }
            }

        }
    });

    sender.join();
    system.join();

    return 0;
}