#include "Server.h"
using namespace boost;

Service::Service(sockPtr_t sock, std::function<void(sockPtr_t)> &acceptor)
    : sock(sock), acceptor(acceptor) {
        RBLog("Service()");
    }

Service *Service::serve(sockPtr_t sock,
                        std::function<void(sockPtr_t)> &acceptor) {
    Service *svc = new Service(sock, acceptor);
    svc->startHandle();
    return svc;
}

void Service::startHandle() {
    std::thread th(([this]() { handleClient(); }));
    th.detach();
}

void Service::handleClient() {
    try {
        acceptor(sock);
        sock.get()->close();
    } catch (std::exception &e) {
        excHandler(e);
    } catch (RBException &e) {
        excHandler(e);
    }

    delete this;
}

using asio::ip::tcp;

Server::Server(unsigned short port_num, std::function<void(sockPtr_t)> acceptor)
    : m_stop(false),
      tcp_acceptor(m_ios, tcp::endpoint(tcp::v4(), port_num)),
      acceptor(acceptor) {}

Server::Server(unsigned short port_num, std::function<RBResponse (RBRequest)> func)
    : m_stop(false),
      tcp_acceptor(m_ios, tcp::endpoint(tcp::v4(), port_num)) ,
    acceptor([func](sockPtr_t sock) {
        AsioInputStream<boost::asio::ip::tcp::socket> ais(*sock.get());
        AsioOutputStream<boost::asio::ip::tcp::socket> aos(*sock.get());
        CopyingInputStreamAdaptor cis_adp(&ais);
        CopyingOutputStreamAdaptor cos_adp(&aos);

        RBRequest req;
        req.set_final(false);
        
        while(!req.final()) {
            bool op = google::protobuf::io::readDelimitedFrom(&req, &cis_adp);
            
            if (!op) throw RBException("reqRecv");

            RBResponse res = func(req);

            op = google::protobuf::io::writeDelimitedTo(res, &cos_adp);
            cos_adp.Flush();
            if (!op) throw RBException("resSend");
        }

    }){}

void Server::start() {
    m_thread.reset(new std::thread([this]() { run(); }));
}

void Server::stop() {
    m_stop.store(true);
    m_thread->join();
}

void Server::run() {
    RBLog("Server started");
    while (!m_stop.load()) {
        sockPtr_t sock(new asio::ip::tcp::socket(m_ios));
        tcp_acceptor.accept(*sock.get());
        Service::serve(sock, acceptor);
    }
}
