#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_SYSTEM_NO_LIB

#include <boost/asio.hpp>
#include <iostream>

int main() {
    using boost::asio::ip::tcp;

    const std::string ip = "192.168.105.5";  // ← set your DDS IP here
    const unsigned short port = 26000;       // slot 0

    std::cout << "Trying to connect to " << ip << ":" << port << "...\n";

    boost::asio::io_context io;

    // parse IP
    boost::system::error_code ec_addr;
    auto addr = boost::asio::ip::make_address(ip, ec_addr);
    if (ec_addr) {
        std::cerr << "Invalid IP address: " << ec_addr.message() << "\n";
        return 1;
    }

    tcp::endpoint endpoint(addr, port);
    tcp::socket socket(io);

    // connect
    boost::system::error_code ec;
    socket.connect(endpoint, ec);

    if (ec) {
        std::cerr << "Connect failed: " << ec.message() << "\n";
        return 1;
    }

    std::cout << "Connected successfully.\n";
    return 0;
}

