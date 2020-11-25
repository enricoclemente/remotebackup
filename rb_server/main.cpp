#include "ProtobufHelpers.h"
#include "AsioAdapting.h"
#include "packet.pb.h"
#include "CRC.h"

#include <iostream>
#include <boost/asio.hpp>
#include <fstream>


using boost::asio::ip::tcp;
int main()
{
    boost::asio::io_service io_service;
    int port = 4561;
    tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port));
    tcp::socket socket(io_service);
    acceptor.accept(socket);
    AsioOutputStream<boost::asio::ip::tcp::socket> aos(socket);
    CopyingOutputStreamAdaptor cos_adp(&aos);
    AsioInputStream<boost::asio::ip::tcp::socket> ais(socket);
    CopyingInputStreamAdaptor cis_adp(&ais);

    std::string client_folder = "/Users/enricoclemente/Desktop/Provastream/";
    bool net_op = false;

    std::cout<<"Server inizializzato sulla porta: "<<port<<std::endl;

    do {
        // std::this_thread::sleep_for(std::chrono::milliseconds(100000));

        ProbeSingleFileRequest psfr_request;
        net_op = google::protobuf::io::readDelimitedFrom(&psfr_request, &cis_adp);

        if(net_op) {
            std::cout<<"I just received ProbeSingleFileRequest for: "<< psfr_request.file_path()<<std::endl;

            ProbeSingleFileResponse psfr_response;
            psfr_response.set_file_path(psfr_request.file_path());
            psfr_response.set_send_file(true);
            net_op = google::protobuf::io::writeDelimitedTo(psfr_response, &cos_adp);
            cos_adp.Flush();

            if(net_op) {
                FilePacket packet;
                net_op = google::protobuf::io::readDelimitedFrom(&packet, &cis_adp);

                // google::protobuf::MessageLite::GetTypeName può essere usato per capire i vari tipi di pacchetto
                if(net_op){
                    std::cout<<"I just received: "<< packet.file_path()<<std::endl;

                    std::uint32_t checksum;
                    std::ofstream fout(client_folder + packet.file_path() ,std::ios::out | std::ios::binary);

                    int chuncks = packet.file_chuncks_size();
                    for(int i=0; i<chuncks; i++) {
                        fout.write((char*)&packet.file_chuncks(i)[0],
                                   packet.file_chuncks(i).size() * sizeof(char));
                        checksum = CRC::Calculate(&packet.file_chuncks(i)[0], sizeof(packet.file_chuncks(i)),
                                CRC::CRC_32(),checksum);
                    }

                    std::cout<<"File checksum: "<<checksum<<std::endl;
                    std::cout<<"Received checksum: "<<packet.file_checksum()<<std::endl;

                    FilePacketResponse fp_response;
                    fp_response.set_file_path(packet.file_path());
                    if(checksum != packet.file_checksum()) {
                        std::cout<<"File corrotto"<<std::endl;
                        fp_response.set_success(false);
                    } else {
                        std::cout<<"File ricevuto correttamente"<<std::endl;
                        fp_response.set_success(true);
                    }

                    net_op = google::protobuf::io::writeDelimitedTo(fp_response, &cos_adp);
                    cos_adp.Flush();
                    fout.close();
                }
            }
        } else {
            std::cout<<"Something went wrong"<<std::endl;
        }
    } while (true);

    return 0;
}