#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_SYSTEM_NO_LIB

#include <boost/asio.hpp>
#include <iostream>
#include <string>

int main() {
    using boost::asio::ip::tcp;

    const std::string ip   = "192.168.105.5";  // <-- your DDS IP
    const unsigned short port = 26000;         // slot 0
    const int slot_index = 0;                  // we're talking to slot 0

    std::cout << "Connecting to " << ip << ":" << port << "...\n";

    boost::asio::io_context io;
    tcp::socket socket(io);

    // parse IP
    boost::system::error_code ec_addr;
    auto addr = boost::asio::ip::make_address(ip, ec_addr);
    if (ec_addr) {
        std::cerr << "Invalid IP address: " << ec_addr.message() << "\n";
        return 1;
    }

    // connect
    tcp::endpoint endpoint(addr, port);
    boost::system::error_code ec;
    socket.connect(endpoint, ec);
    if (ec) {
        std::cerr << "Connect failed: " << ec.message() << "\n";
        return 1;
    }

    std::cout << "Connected. Sending auth...\n";

    // -------- AUTH STRING --------
    // same as Python: "75f4a4e10dd4b6b<slot_index>\n"
    std::string auth = "75f4a4e10dd4b6b" + std::to_string(slot_index) + "\n";

    boost::asio::write(socket, boost::asio::buffer(auth), ec);
    if (ec) {
        std::cerr << "Write failed: " << ec.message() << "\n";
        return 1;
    }

    // -------- READ RESPONSE --------
    boost::asio::streambuf buf;
    boost::asio::read_until(socket, buf, '\n', ec);

    if (ec && ec != boost::asio::error::eof) {
        std::cerr << "Read failed: " << ec.message() << "\n";
        return 1;
    }

    std::istream is(&buf);
    std::string line;
    std::getline(is, line);

    std::cout << "DDS reply: \"" << line << "\"\n";

    return 0;
}
