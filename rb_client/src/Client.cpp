#include "Client.h"
#include <exception>

using boost::asio::ip::tcp;

Client::Client(const std::string &ip, const std::string &port, int timeout, int n)
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

    RBResponse res = run(req);

    validateRBProto(res, RBMsgType::AUTH, 3);

    token = res.auth_response().token();
}

RBResponse Client::run(RBRequest &req) {
    req.set_final(true);
    ProtoChannel chan(endpoints, io_service, token, *this);
    auto res = chan.run(req);
    chan.close();
    return std::move(res);
}

std::shared_ptr<ProtoChannel> Client::open_channel() {
    return std::make_shared<ProtoChannel>(endpoints, io_service, token, *this);
}

RBResponse ProtoChannel::run(RBRequest &req, bool do_try) {
    if (do_try) {
        if (!mutex.try_lock()) {
            throw std::runtime_error("busy_protochannel");
        }
    } else {
        mutex.lock();
    }

    std::lock_guard lg(mutex, std::adopt_lock);

    req.set_token(token);

    if (!socket.is_open()) {
        if (do_try) throw std::runtime_error("closed_protochannel");
        throw RBException("Client->Socket closed");
    }

    bool net_op = google::protobuf::io::writeDelimitedTo(req, &cos_adp);
    cos_adp.Flush();
    if (!net_op) throw RBException("Client->Request send fail");

    if (!socket.is_open()) throw RBException("Client->Socket closed");

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
    Client & c)
    : socket(io_service),
      ais(socket),
      cis_adp(&ais),
      aos(socket),
      cos_adp(&aos),
      client(c),
      token(token) {
    boost::asio::connect(socket, endpoints);
    if (!socket.is_open()) throw RBException("Client->Connection failed");

    RBLog("Protochannel()");
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

bool ProtoChannel::is_open() {
    std::lock_guard lg(mutex);
    return socket.is_open();
}
