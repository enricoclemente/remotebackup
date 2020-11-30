#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <functional>
#include "RBHelpers.h"
#include "rb_request.pb.h"
#include "rb_response.pb.h"

using namespace boost;

void excHandler(system::system_error & e);

class Service {
 public:
  static Service* serve(sockPtr_t sock, std::function<void(sockPtr_t)>&);

 private:
  Service(sockPtr_t sock,std::function<void(sockPtr_t)>&);
  void startHandle();
  void handleClient();

  sockPtr_t sock;
  std::function<void(sockPtr_t)> & acceptor;
};

class Server {
 public:
  Server(unsigned short port_num, std::function<void(sockPtr_t)>);
  Server(unsigned short port_num, std::function<RBResponse(RBRequest)>);

  void start();

  void stop();

 private:
  void run();

  std::unique_ptr<std::thread> m_thread;
  std::atomic<bool> m_stop;
  asio::io_service m_ios;
  asio::ip::tcp::acceptor tcp_acceptor;
  std::function<void(sockPtr_t)> acceptor;
};
