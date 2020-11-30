#include "Client.h"
#include "ProtobufHelpers.h"
#include "AsioAdapting.h"

using boost::asio::ip::tcp;

Client::Client(std::string ip, std::string port) {
    if (ec.value()) throw ec;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(ip, port);
    endpoints = resolver.resolve(query);
}

RBResponse Client::run(RBRequest req) {
  tcp::socket socket(io_service);
  boost::asio::connect(socket, endpoints);

  if (!socket.is_open()) throw RBNetException("can't open socket");

  AsioInputStream<boost::asio::ip::tcp::socket> ais(socket);
  CopyingInputStreamAdaptor cis_adp(&ais);
  AsioOutputStream<boost::asio::ip::tcp::socket> aos(socket);
  CopyingOutputStreamAdaptor cos_adp(&aos);

  bool net_op = google::protobuf::io::writeDelimitedTo(req, &cos_adp);
  cos_adp.Flush();

  if(!net_op) throw RBNetException("reqSend");

  RBResponse res;

  net_op = google::protobuf::io::readDelimitedFrom(&res, &cis_adp);

  if(!net_op) throw RBNetException("resRecv");

  socket.close();

  return res;
}
