#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "packet.pb.h"

#include <iostream>
#include <boost/asio.hpp>
#include <fstream>


using boost::asio::ip::tcp;
int main()
{
    boost::asio::io_service io_service;
    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 4001));
    tcp::socket socket(io_service);
    acceptor.accept(socket);
    AsioOutputStream<boost::asio::ip::tcp::socket> aos(socket);
    CopyingOutputStreamAdaptor cos_adp(&aos);
    AsioInputStream<tcp::socket> ais(socket);
    CopyingInputStreamAdaptor cis_adp(&ais);

    std::string client_folder = "/Users/enricoclemente/Desktop/Provastream/";
    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));

        FilePacket packet;
        google::protobuf::io::readDelimitedFrom(&cis_adp, &packet);

        // google::protobuf::MessageLite::GetTypeName può essere usato per capire i vari tipi di pacchetto
        if(!packet.file_path().empty()){
            std::cout<<"I just received: "<< packet.file_path()<<std::endl;

            std::ofstream fout(client_folder + packet.file_path() ,std::ios::out | std::ios::binary);

            int chuncks = packet.file_chuncks_size();
            for(int i=0; i<chuncks; i++) {
                fout.write((char*)&packet.file_chuncks(i)[0],
                           packet.file_chuncks(i).size() * sizeof(char));
            }

            fout.close();
        }
    } while (true);
    return 0;
}