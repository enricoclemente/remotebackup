#pragma once

#include <boost/asio.hpp>

#include <functional>
#include <mutex>
#include <string>

#include "AsioAdapting.h"
#include "ProtobufHelpers.h"
#include "RBHelpers.h"
#include "rbproto.pb.h"

class ProtoChannel;

class Client {
public:
    Client(
        const std::string & ip,
        const std::string & port,
        int protochan_pool_max_n
    );

    RBResponse run(RBRequest &);

    std::shared_ptr<ProtoChannel> open_channel();
    void authenticate(std::string, std::string);

private:
    friend class ProtoChannel;
    boost::asio::ip::tcp::resolver::iterator endpoints;
    boost::system::error_code ec;
    boost::asio::io_service io_service;
    std::string token;
};

using boost::asio::ip::tcp;

class ProtoChannel : public std::enable_shared_from_this<ProtoChannel> {
public:
    ProtoChannel(const ProtoChannel &) = delete;

    ProtoChannel(ProtoChannel &&) = default;

    ~ProtoChannel();

    RBResponse run(RBRequest &, bool do_try = false);

    void close();

    ProtoChannel(tcp::resolver::iterator &, boost::asio::io_service &, std::string &, Client &);

    bool is_open();

private:
    friend class Client;
    Client & client;
    tcp::socket socket;
    AsioInputStream<tcp::socket> ais;
    CopyingInputStreamAdaptor cis_adp;
    AsioOutputStream<tcp::socket> aos;
    CopyingOutputStreamAdaptor cos_adp;
    std::mutex mutex;
    std::string & token;
};
