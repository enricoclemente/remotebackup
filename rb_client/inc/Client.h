#pragma once

#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lambda/bind.hpp>

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
        int timeout, 
        int protochan_pool_max_n
    );

    RBResponse run(RBRequest &, bool force_single = false);

    ProtoChannel open_channel();
    void authenticate(std::string, std::string);

    ~Client() {
        keep_going = false;
        RBLog("Waiting for client watchdog termination...", LogLevel::DEBUG);
        watchdog.join();
    }
private:
    friend class ProtoChannel;
    boost::asio::ip::tcp::resolver::iterator endpoints;
    boost::system::error_code ec;
    boost::asio::io_service io_service;
    std::string token;
    int timeout;

    std::list<std::shared_ptr<ProtoChannel>> protochan_pool;
    int protochan_pool_max_n;
    /** protochannel_pool mutex */
    std::mutex pcp_m;
    /** protochannel_pool condition variable */
    std::condition_variable pcp_cv;
    std::atomic<bool> keep_going = true;

    void clean_protochan_pool();
    std::thread watchdog = make_watchdog(
        std::chrono::seconds(10),
        [this]() { return keep_going.load(); },
        [this]() { clean_protochan_pool(); }
    );
};

using boost::asio::ip::tcp;
using boost::asio::deadline_timer;
using boost::lambda::bind;

class ProtoChannel : public std::enable_shared_from_this<ProtoChannel> {
public:
    ProtoChannel(const ProtoChannel &) = delete;

    ProtoChannel(ProtoChannel &&) = default;

    ~ProtoChannel();

    RBResponse run(RBRequest &, bool do_try = false);

    void close();

    ProtoChannel(tcp::resolver::iterator &, boost::asio::io_service &, std::string &, boost::posix_time::time_duration, Client &);

private:
    friend class Client;
    Client & client;
    std::chrono::system_clock::time_point last_use;
    tcp::socket socket;
    deadline_timer deadline;
    AsioInputStream<tcp::socket> ais;
    CopyingInputStreamAdaptor cis_adp;
    AsioOutputStream<tcp::socket> aos;
    CopyingOutputStreamAdaptor cos_adp;
    std::mutex mutex;
    std::string & token;
    boost::posix_time::time_duration timeout;

    void check_deadline()
    {
        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (deadline.expires_at() <= deadline_timer::traits_type::now())
        {
            RBLog("Timeout expired!");
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled. This allows the blocked
            // connect(), read_line() or write_line() functions to return.
            boost::system::error_code ignored_ec;
            socket.close(ignored_ec);

            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            deadline.expires_at(boost::posix_time::pos_infin);
        }

        // Put the actor back to sleep.
        deadline.async_wait(bind(&ProtoChannel::check_deadline, this));
    }
};

class ConnectionTimeout {
    std::chrono::system_clock::duration timeout_interval;
    std::atomic<bool> stopped = false;

public:
    ConnectionTimeout(std::chrono::system_clock::duration interval);
    void start_timer();
    void stop_timer();
};