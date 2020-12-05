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
    Client(const std::string&, const std::string&);
    RBResponse run(const RBRequest&);
    ProtoChannel openChannel();

private:
    boost::asio::ip::tcp::resolver::iterator endpoints;
    boost::system::error_code ec;
    boost::asio::io_service io_service;
};

using boost::asio::ip::tcp;

class ProtoChannel {
public:
    ProtoChannel(ProtoChannel &) = delete;
    ProtoChannel(ProtoChannel &&) = default;
    ~ProtoChannel();
    RBResponse run(const RBRequest&);

private:
    ProtoChannel(tcp::resolver::iterator &, boost::asio::io_service &);
    friend ProtoChannel Client::openChannel();
    tcp::socket socket;

    AsioInputStream<tcp::socket> ais;
    CopyingInputStreamAdaptor cis_adp;
    AsioOutputStream<tcp::socket> aos;
    CopyingOutputStreamAdaptor cos_adp;
    std::mutex mutex;
};
