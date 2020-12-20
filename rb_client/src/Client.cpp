#include "Client.h"

using boost::asio::ip::tcp;

Client::Client(const std::string &ip, const std::string &port) {
    if (ec.value()) throw ec;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(ip, port);
    endpoints = resolver.resolve(query);
}

void Client::authenticate(std::string username, std::string password) {
    RBRequest req;
    auto authReq = std::make_unique<RBAuthRequest>();

    authReq->set_user(username);
    authReq->set_pass(password);

    req.set_protover(3);
    req.set_type(RBMsgType::AUTH);
    req.set_allocated_auth_request(authReq.release());
    req.set_final(true);

    RBResponse res = run(req);

    validateRBProto(res, RBMsgType::AUTH, 3);
    if (!res.has_auth_response())
        throw RBException("missing auth response");
    token = res.auth_response().token();
}

RBResponse Client::run(RBRequest &req) {

    if (!req.final()) throw RBException("notFinalClientRun");

    ProtoChannel chan = open_channel();
    return chan.run(req);
}

RBResponse ProtoChannel::run(RBRequest &req) {
    req.set_token(token);

    std::lock_guard<std::mutex> lock(mutex);

    if (!socket.is_open()) throw RBException("socketClosed");

    bool net_op = google::protobuf::io::writeDelimitedTo(req, &cos_adp);
    cos_adp.Flush();

    if (!net_op) throw RBException("reqSend");

    RBResponse res;

    net_op = google::protobuf::io::readDelimitedFrom(&res, &cis_adp);

    if (!net_op) throw RBException("resRecv");

    if (req.final()) socket.close();

    return res;
}

ProtoChannel::ProtoChannel(
    tcp::resolver::iterator &endpoints,
    boost::asio::io_service &io_service,
    std::string & token)
    : socket(io_service),
      ais(socket),
      cis_adp(&ais),
      aos(socket),
      cos_adp(&aos),
      token(token) {
    boost::asio::connect(socket, endpoints);
}

ProtoChannel Client::open_channel() {
    return ProtoChannel(endpoints, io_service, token);
}

ProtoChannel::~ProtoChannel() {
    if (socket.is_open()) socket.close();
}
