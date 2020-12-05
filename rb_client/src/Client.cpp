#include "Client.h"

using boost::asio::ip::tcp;

Client::Client(const std::string& ip, const std::string& port) {
    if (ec.value()) throw ec;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(ip, port);
    endpoints = resolver.resolve(query);
}

RBResponse Client::run(const RBRequest& req) {

  if (!req.final()) throw RBException("notFinalClientRun");

  ProtoChannel chan = openChannel();
  return chan.run(req);
}

RBResponse ProtoChannel::run(const RBRequest& req) {

  std::lock_guard<std::mutex> lock(mutex);

  if (!socket.is_open()) throw RBException("socketClosed");

  bool net_op = google::protobuf::io::writeDelimitedTo(req, &cos_adp);
  cos_adp.Flush();

  if(!net_op) throw RBException("reqSend");

  RBResponse res;

  net_op = google::protobuf::io::readDelimitedFrom(&res, &cis_adp);

  if(!net_op) throw RBException("resRecv");

  if (req.final()) socket.close();

  return res;
}

ProtoChannel::ProtoChannel(
  tcp::resolver::iterator &endpoints,
  boost::asio::io_service &io_service)
  : socket(io_service),
  ais(socket),
  cis_adp(&ais),
  aos(socket),
  cos_adp(&aos) {
  boost::asio::connect(socket, endpoints);
}

ProtoChannel Client::openChannel() {
  return ProtoChannel(endpoints, io_service);
}

ProtoChannel::~ProtoChannel() {
  if (socket.is_open()) socket.close();
}
