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
        int timeout
    );

    RBResponse run(RBRequest &);

    ProtoChannel open_channel();
    void authenticate(std::string, std::string);

private:
    std::string ip;
    std::string port;
    std::string token;
    int timeout;
};

using boost::asio::ip::tcp;

class ProtoChannel {
public:
    ProtoChannel(ProtoChannel &) = delete;

    ProtoChannel(ProtoChannel &&) = default;

    ~ProtoChannel();

    RBResponse run(RBRequest &);

    void close();

private:
    ProtoChannel(std::string & ip, std::string & port, std::string & token);

    friend ProtoChannel Client::open_channel();

    tcp::iostream stream;

    AsioInputStream<tcp::socket> ais;
    CopyingInputStreamAdaptor cis_adp;
    AsioOutputStream<tcp::socket> aos;
    CopyingOutputStreamAdaptor cos_adp;
    std::mutex mutex;
    std::string & token;
    int timeout;
};
