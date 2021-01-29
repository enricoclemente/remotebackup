#include "Client.h"

using boost::asio::ip::tcp;

Client::Client(const std::string &ip, const std::string &port, int timeout)
    :timeout(timeout), ip(ip), port(port) {}

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

    if (!stream.socket().is_open()) throw RBException("Client->Socket closed");

    stream.expires_after(std::chrono::milliseconds(timeout));

    bool net_op = google::protobuf::io::writeDelimitedTo(req, &cos_adp);
    cos_adp.Flush();

    if (!net_op) throw RBException("Client->Request send fail");

    RBResponse res;

    net_op = google::protobuf::io::readDelimitedFrom(&res, &cis_adp);

    if (!net_op) throw RBException("Client->Response receive fail");

    if (req.final()) stream.close();

    return res;
}

typedef boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO> asio_socket_timeout_option;

ProtoChannel::ProtoChannel(
    std::string &ip,
    std::string &port,
    std::string & token)
    : stream(ip, port),
      ais(static_cast<boost::asio::ip::tcp::socket &>(stream.socket())),
      cis_adp(&ais),
      aos(static_cast<boost::asio::ip::tcp::socket &>(stream.socket())),
      cos_adp(&aos),
      token(token) {
    if (!stream.socket().is_open()) throw RBException("Client->Connection failed");
    RBLog("Protochannel()");
}

ProtoChannel Client::open_channel() {
    return ProtoChannel(ip, port, token);
}

ProtoChannel::~ProtoChannel() {
    if (stream.socket().is_open()) {
        close();
        RBLog("ProtoChannel closed unexpectedly");
    }
    RBLog("~Protochannel()");
}

void ProtoChannel::close() {
    if (stream.socket().is_open()) stream.close();
}
