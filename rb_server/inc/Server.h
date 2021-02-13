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
  ~Service() {
    RBLog("~Service()\n");
  }

  static std::shared_ptr<Service> create(sockPtr_t sock, RBSrvCallback);
  void handle_request();
private:
  Service(sockPtr_t sock, RBSrvCallback);

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
  Server(unsigned short port_num, int n_workers, const RBSrvCallback &);

  void start();
  void stop();
  bool is_running() { return running; }

  ~Server() {
    RBLog("~Server()");
  }

private:
  void run();

  unsigned short port; // WARNING: if moved at the end the server breaks!!
  std::unique_ptr<std::thread> thread_ptr;
  std::atomic<bool> running;
  asio::io_service ios;
  asio::ip::tcp::acceptor tcp_acceptor;
  RBSrvCallback callback;
  std::vector<std::thread> workers;
  int n_workers = 16;

  std::condition_variable req_cv;
  std::mutex req_mutex;
  std::list<std::shared_ptr<Service>> requests;
};
