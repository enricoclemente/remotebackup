#include "Server.h"

using namespace boost;

Service::Service(sockPtr_t sock, RBSrvCallback callback)
    : sock(sock),
      handler([callback, this](sockPtr_t sock) {
          AsioInputStream<boost::asio::ip::tcp::socket> ais(sock);
          AsioOutputStream<boost::asio::ip::tcp::socket> aos(sock);
          CopyingInputStreamAdaptor cis_adp(&ais);
          CopyingOutputStreamAdaptor cos_adp(&aos);

          bool final = false;
          while (!final) {
              RBRequest req;

              bool op = google::protobuf::io::readDelimitedFrom(&req, &cis_adp);

              if (!op)
                  throw RBException("request_receive_fail");

              RBResponse res = callback(req, shared_from_this());

              op = google::protobuf::io::writeDelimitedTo(res, &cos_adp);
              cos_adp.Flush();
              if (!op)
                  throw RBException("response_send_fail");

              final = req.final();
          }
      })
{
    RBLog("Service()");
}

std::shared_ptr<Service> Service::create(sockPtr_t sock, RBSrvCallback callback) {
    return std::shared_ptr<Service>(new Service(sock, callback));
}

void Service::handle_request() {
    try {
        handler(sock);
        sock.get()->close();
    } catch (RBException &e) {
        RBLog("Server >> RBProto failure: " + e.getMsg(), LogLevel::ERROR);
    } catch (std::exception &e) {
        RBLog("Server >> RBProto failure: " + std::string(e.what()), LogLevel::ERROR);
    }
}

using asio::ip::tcp;

Server::Server(unsigned short port_num, int n_workers, const RBSrvCallback & callback)
    : port(port_num), running(true), callback(callback), n_workers(n_workers),
    tcp_acceptor(ios, tcp::endpoint(tcp::v4(), port)){
        RBLog("Server(" + std::to_string(port) + ")", LogLevel::DEBUG);
    }

void Server::start() {
    thread_ptr.reset(new std::thread([this]() { run(); }));
    RBLog("Server >> Starting " + std::to_string(n_workers) + " workers...", LogLevel::INFO);
    for (int i = 0; i < n_workers; i++) {
        workers.emplace_back([this]() {
            while(running) {
                std::unique_lock ul(req_mutex);
                req_cv.wait(ul, [this]() { return requests.size() > 0 || !running; });
                if (!running) return;
                auto svc = requests.front();
                requests.pop_front();
                ul.unlock();
                svc->handle_request();
            }
        });
    }
    RBLog("Server >> Workers ready!", LogLevel::INFO);
}

void Server::stop() {
    if (!running) return;
    running = false;
    
    asio::io_service ios2;
    tcp::endpoint ep(asio::ip::address::from_string("127.0.0.1"), port);
    tcp::socket sock(ios2, tcp::v4());
    try {
        RBLog("SERVER >> Self-connecting to wake acceptor... (" + std::to_string(port) + ")", LogLevel::DEBUG);
        sock.connect(ep);
    } catch(std::exception &e) {
        RBLog("SERVER >> FAILED!", LogLevel::ERROR);
    }
    thread_ptr->join();
    RBLog("SERVER >> Acceptor thread joined.", LogLevel::DEBUG);
    RBLog("SERVER >> Waiting for workers to terminate...", LogLevel::INFO);
    req_cv.notify_all();
    for (auto & w : workers) {
        try {
            w.join();
        } catch (RBException &e) {
            RBLog("SERVER >> RBException while joining worker: " + e.getMsg(), LogLevel::ERROR);
        } catch (std::exception &e) {
            RBLog("SERVER >> std::exception while joining worker: " + std::string(e.what()), LogLevel::ERROR);
        }
    }
}

void Server::run() {
    RBLog("SERVER >> Server started", LogLevel::INFO);
    while (running) {
        sockPtr_t sock(new asio::ip::tcp::socket(ios));
        tcp_acceptor.accept(*sock); // This function blocks until a connection has been accepted
        if (running) {
            std::lock_guard lg(req_mutex);
            requests.emplace_back(Service::create(sock, callback));
            req_cv.notify_all();
        } else sock->close();
    }
}
