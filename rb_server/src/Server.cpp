#include "Server.h"

using namespace boost;

Service::Service(sockPtr_t sock, std::function<RBResponse(RBRequest&)> callback)
    : sock(sock),
      handler([callback](sockPtr_t sock) {
          AsioInputStream<boost::asio::ip::tcp::socket> ais(sock);
          AsioOutputStream<boost::asio::ip::tcp::socket> aos(sock);
          CopyingInputStreamAdaptor cis_adp(&ais);
          CopyingOutputStreamAdaptor cos_adp(&aos);

          RBRequest req;
          req.set_final(false);

          while (!req.final())
          {
              bool op = google::protobuf::io::readDelimitedFrom(&req, &cis_adp);

              if (!op)
                  throw RBException("reqRecv");

              RBResponse res = callback(req);

              op = google::protobuf::io::writeDelimitedTo(res, &cos_adp);
              cos_adp.Flush();
              if (!op)
                  throw RBException("resSend");
          }
      })
{
    RBLog("Service()");
}

void Service::serve(sockPtr_t sock, std::function<RBResponse(RBRequest&)> callback)
{
    auto svc_ptr = std::shared_ptr<Service>(new Service(sock, callback));

    std::thread th(([svc_ptr]() { svc_ptr->handleClient(); }));
    th.detach();
}

void Service::handleClient()
{
    try
    {
        RBLog("Handling client request");
        handler(sock);
        sock.get()->close();
    }
    catch (std::exception &e)
    {
        excHandler(e);
    }
    catch (RBException &e)
    {
        excHandler(e);
    }
}

using asio::ip::tcp;

Server::Server(unsigned short port_num, std::function<RBResponse(RBRequest&)> callback)
    : running(true),
      tcp_acceptor(ios, tcp::endpoint(tcp::v4(), port_num)),
      callback(callback) {}

void Server::start()
{
    thread_ptr.reset(new std::thread([this]() { run(); }));
}

void Server::stop()
{
    running.store(false);
    thread_ptr->join();
}

void Server::run()
{
    RBLog("Server started");
    while (running.load())
    {
        sockPtr_t sock(new asio::ip::tcp::socket(ios));
        tcp_acceptor.accept(*sock);
        Service::serve(sock, callback);
    }
}
