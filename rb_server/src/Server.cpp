#include "Server.h"

using namespace boost;

Service::Service(sockPtr_t sock, RBSrvCallback callback)
    : sock(sock),
      handler([callback, this](sockPtr_t sock) {
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
                  throw RBException("request_receive_fail");

              RBResponse res = callback(req, shared_from_this());

              op = google::protobuf::io::writeDelimitedTo(res, &cos_adp);
              cos_adp.Flush();
              if (!op)
                  throw RBException("response_send_fail");
          }
      })
{
    RBLog("Service()");
}

void Service::serve(sockPtr_t sock, RBSrvCallback callback) {
    auto svc_ptr = std::shared_ptr<Service>(new Service(sock, callback));

    std::thread th(([svc_ptr]() { svc_ptr->handleClient(); }));
    th.detach();
}

void Service::handleClient() {
    try {
        RBLog("Handling client request");
        handler(sock);
        sock.get()->close();
    } catch (RBException &e) {
        excHandler(e);
    }
    catch (std::exception &e) {
        excHandler(e);
    }
}

bool Service::accumulate_data(const RBRequest& req) {
    auto size = req.file_segment().file_metadata().size();
    if (size == 0) return false;

    if (file_data.size == 0)
        file_data.size = size;
    else if (file_data.size != size)
        return false;

    auto last_write_time = req.file_segment().file_metadata().last_write_time();

    if (file_data.last_write_time == 0)
        file_data.last_write_time = last_write_time;
    else if (file_data.last_write_time != last_write_time)
        return false;

    std::stringstream ss;
    auto data = req.file_segment().data();
    for(const std::string& datum : data)
        ss << datum;
    
    auto segment_id = req.file_segment().segmentid();
    if (file_data.segment_map.count(segment_id) != 0)
        return false;
    
    file_data.segment_map[segment_id] = ss.str();

    return true;
}

std::string Service::get_data() {
    std::stringstream ss;
    std::map<uint64_t, std::string>::iterator it;
    for (it = file_data.segment_map.begin(); it != file_data.segment_map.end(); it++) {
        ss << it->second;
    }

    // Reset file data
    file_data.segment_map.clear();
    file_data.size = 0;
    file_data.last_write_time = 0;
    
    return ss.str();
}

using asio::ip::tcp;

Server::Server(unsigned short port_num, RBSrvCallback callback)
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
