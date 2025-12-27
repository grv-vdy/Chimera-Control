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

    bool singleToneNow(int slot, int channel, double frequency, double amplitude, double phase = 0.0, bool waitForTrigger = false);
    bool turnOffChannel(int channel);
    bool abortChannel(int channel);
    bool sendBatchCommands(const std::string& commands);

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