#pragma once
#include <string>
#include <boost/asio.hpp>

class WieserlabsClient {
public:
    WieserlabsClient(const std::string& ip, int port);
    ~WieserlabsClient();

    bool connect();
    void disconnect();
    bool isConnected() const;

    // Program a single tone on a specific channel
    bool singleToneNow(int slot, int channel, double frequency, double amplitude, double phase = 0.0);

    // Placeholder for ramp functionality (to be implemented)
    bool rampTone(int slot, int channel, double startFreq, double endFreq, double amplitude, double duration, double phase = 0.0);

private:
    std::string ip_;
    int port_;
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
    bool connected_;

    bool sendCommand(const std::string& command);
    std::string receiveResponse();

    std::string freq_to_word(double f);
    std::string amp_to_word(double amp);
    std::string phase_to_word(double phase);
};