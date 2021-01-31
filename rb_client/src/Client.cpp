#include "Client.h"

using boost::asio::ip::tcp;

Client::Client(const std::string &ip, const std::string &port, int timeout)
    :timeout(timeout) {
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

    token = res.auth_response().token();
}

RBResponse Client::run(RBRequest &req) {

    if (!req.final()) throw RBException("Client->Request is not final");

    ProtoChannel chan = open_channel();
    auto res = chan.run(req);
    chan.close();

    return std::move(res);
}

RBResponse ProtoChannel::run(RBRequest &req) {
    req.set_token(token);

    std::lock_guard<std::mutex> lock(mutex);

    if (!socket.is_open()) throw RBException("Client->Socket closed");
    deadline.expires_from_now(timeout);
    bool net_op = google::protobuf::io::writeDelimitedTo(req, &cos_adp);
    cos_adp.Flush();
    if (!net_op) throw RBException("Client->Request send fail");

    if (!socket.is_open()) throw RBException("Client->Socket closed");
    deadline.expires_from_now(timeout);
    RBResponse res;
    net_op = google::protobuf::io::readDelimitedFrom(&res, &cis_adp);
    if (!net_op) throw RBException("Client->Response receive fail");

    if (req.final()) socket.close();

    return res;
}


ProtoChannel::ProtoChannel(
    tcp::resolver::iterator &endpoints,
    boost::asio::io_service &io_service,
    std::string & token,
    boost::posix_time::time_duration timeout)
    : socket(io_service),
      deadline(io_service),
      timeout(timeout),
      ais(socket),
      cis_adp(&ais),
      aos(socket),
      cos_adp(&aos),
      token(token) {
    // No deadline is required until the first socket operation is started. We
    // set the deadline to positive infinity so that the actor takes no action
    // until a specific deadline is set.
    deadline.expires_at(boost::posix_time::pos_infin);
    // Start the persistent actor that checks for deadline expiry.
    check_deadline();

    deadline.expires_from_now(timeout);
    boost::asio::connect(socket, endpoints);
    if (!socket.is_open()) throw RBException("Client->Connection failed");

    RBLog("Protochannel()");
}

ProtoChannel Client::open_channel() {
    return ProtoChannel(endpoints, io_service, token, boost::posix_time::milliseconds(timeout));
}

ProtoChannel::~ProtoChannel() {
    if (socket.is_open()) {
        close();
        RBLog("ProtoChannel closed unexpectedly");
    }
    RBLog("~Protochannel()");
}

void ProtoChannel::close() {
    if (socket.is_open()) socket.close();
}
