#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <functional>

#include "RBHelpers.h"
#include "rbproto.pb.h"
#include "AsioAdapting.h"
#include "ProtobufHelpers.h"

using namespace boost;

class Service;

typedef
  std::function<RBResponse(RBRequest&, std::shared_ptr<Service>)>
  RBSrvCallback;

class Service : std::enable_shared_from_this<Service> {
public:
  ~Service()
  {
    RBLog("~Service()");
  }

  static void serve(sockPtr_t sock, RBSrvCallback);

private:
  Service(sockPtr_t sock, RBSrvCallback);

  void handleClient();

  sockPtr_t sock;
  std::function<void(sockPtr_t)> handler;
};

class Server
{
public:
  Server(unsigned short port_num, RBSrvCallback);

  void start();
  void stop();

private:
  void run();

  std::unique_ptr<std::thread> thread_ptr;
  std::atomic<bool> running;
  asio::io_service ios;
  asio::ip::tcp::acceptor tcp_acceptor;
  RBSrvCallback callback;
};
