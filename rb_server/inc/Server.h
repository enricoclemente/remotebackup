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

class Service : public std::enable_shared_from_this<Service> {
public:
  ~Service()
  {
    RBLog("~Service()\n");
  }

  static void serve(sockPtr_t sock, RBSrvCallback);

  bool accumulate_data(const RBRequest&);
  std::string get_data();

private:
  Service(sockPtr_t sock, RBSrvCallback);

  void handleClient();

  sockPtr_t sock;
  std::function<void(sockPtr_t)> handler;

  struct FileData {
    std::map<uint64_t, std::string> segment_map;
    uint64_t size = 0;
    uint64_t last_write_time = 0;
  } file_data;
};

class Server {
public:
  Server(unsigned short port_num, RBSrvCallback);

  void start();
  void stop();

  ~Server() {
    RBLog("~Server()");
  }

private:
  void run();

  std::unique_ptr<std::thread> thread_ptr;
  std::atomic<bool> running;
  asio::io_service ios;
  asio::ip::tcp::acceptor tcp_acceptor;
  RBSrvCallback callback;
  unsigned short port;
};
