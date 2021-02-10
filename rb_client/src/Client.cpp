#include "Client.h"
#include <exception>

using boost::asio::ip::tcp;

Client::Client(const std::string &ip, const std::string &port, int timeout, int n)
    :timeout(timeout), protochan_pool_max_n(n) {
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

    RBResponse res = run(req, true);

    validateRBProto(res, RBMsgType::AUTH, 3);

    token = res.auth_response().token();
}

RBResponse Client::run(RBRequest &req, bool force_single) {

    if (force_single) {
        req.set_final(true);
        ProtoChannel chan = open_channel();
        auto res = chan.run(req);
        chan.close();

        return std::move(res);
    } else {
        std::unique_lock ul(pcp_m);
        while(true) {
            for (auto ps : protochan_pool) {
                try {
                    ul.unlock();
                    // true on doTry throws exception if busy
                    auto res = ps->run(req, true);
                    pcp_cv.notify_one();
                    return res;
                } catch (std::runtime_error &e) {
                    ul.lock();
                }
            }
            if (protochan_pool.size() < protochan_pool_max_n) {
                auto ps = std::make_shared<ProtoChannel>(endpoints, io_service, token, boost::posix_time::milliseconds(timeout), *this);
                protochan_pool.push_back(ps);
                ul.unlock();
                auto res = ps->run(req);
                pcp_cv.notify_one();
                return res;
            }
            pcp_cv.wait(ul);
        }
    }
}

ProtoChannel Client::open_channel() {
    return ProtoChannel(endpoints, io_service, token,
        boost::posix_time::milliseconds(timeout), *this);
}

void Client::clean_protochan_pool() {
    protochan_pool.remove_if([this](auto ps) {
        auto now = std::chrono::system_clock::now();
        auto timeout = ps->last_use + std::chrono::seconds(PROTOCHANNEL_POOL_TIMEOUT);
        if (now < timeout) return false;
        RBLog("Client >> Cleaning up unused ProtoChannel", LogLevel::DEBUG);
        RBRequest nop_req;
        nop_req.set_type(RBMsgType::NOP);
        nop_req.set_protover(3);
        nop_req.set_final(true);
        try {
            ps->run(nop_req, true);
        } catch (std::runtime_error &e) {
            return false;
        }
        return true;
    });
    pcp_cv.notify_all();
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

    if (!socket.is_open() && client.pcp_m.try_lock()) {
        try {
            std::unique_lock ul(client.pcp_m, std::adopt_lock);
            client.protochan_pool.remove(shared_from_this());
        } catch (std::bad_weak_ptr & e) {
            // we surely aren't in the protochan pool...
        }
    }

    last_use = std::chrono::system_clock::now();

    return res;
}

ProtoChannel::ProtoChannel(
    tcp::resolver::iterator &endpoints,
    boost::asio::io_service &io_service,
    std::string & token,
    boost::posix_time::time_duration timeout,
    Client & c)
    : socket(io_service),
      deadline(io_service),
      timeout(timeout),
      ais(socket),
      cis_adp(&ais),
      aos(socket),
      cos_adp(&aos),
      client(c),
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



ConnectionTimeout::ConnectionTimeout(std::chrono::system_clock::duration interval) : timeout_interval(interval) {}

void ConnectionTimeout::start_timer() {
    auto timer = std::thread([this]() {
        std::this_thread::sleep_for(this->timeout_interval);
        if(!this->stopped.load()) {
            throw RBException("ConnectionTimeout >> out of connection time without response!");
        }
    });

    timer.join();
}

void ConnectionTimeout::stop_timer() {
    stopped.store(true);
}