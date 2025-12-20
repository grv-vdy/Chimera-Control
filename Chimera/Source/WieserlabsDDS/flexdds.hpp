#pragma once

#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_SYSTEM_NO_LIB

#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <stdexcept>

class FlexDDS {
public:
    FlexDDS(const std::string& ip, int slot = 0)
        : ip_(ip),
          slot_(slot),
          socket_(io_) 
    {
        using tcp = boost::asio::ip::tcp;

        // parse IP
        boost::system::error_code ec;
        auto addr = boost::asio::ip::make_address(ip, ec);
        if (ec) {
            throw std::runtime_error("Invalid IP: " + ec.message());
        }

        // connect (port = 26000 + slot)
        tcp::endpoint endpoint(addr, static_cast<unsigned short>(26000 + slot_));
        socket_.connect(endpoint, ec);
        if (ec) {
            throw std::runtime_error("Connect failed: " + ec.message());
        }

        // authenticate
        std::string auth = "75f4a4e10dd4b6b" + std::to_string(slot_) + "\n";
        boost::asio::write(socket_, boost::asio::buffer(auth));
        std::string reply = readLine();  // should be "OK" or similar
        std::cerr << "Auth reply: " << reply << "\n";
    }

    // send an arbitrary line and get one reply line
    std::string sendCommand(const std::string& cmd) {
        std::string msg = cmd;
        if (msg.empty() || msg.back() != '\n')
            msg.push_back('\n');

        boost::asio::write(socket_, boost::asio::buffer(msg));
        return readLine();
    }

private:
    std::string readLine() {
        boost::asio::streambuf buf;
        boost::system::error_code ec;
        boost::asio::read_until(socket_, buf, '\n', ec);

        if (ec && ec != boost::asio::error::eof) {
            throw std::runtime_error("Read failed: " + ec.message());
        }

        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        return line;
    }

    std::string ip_;
    int slot_;
    boost::asio::io_context io_;
    boost::asio::ip::tcp::socket socket_;
};
