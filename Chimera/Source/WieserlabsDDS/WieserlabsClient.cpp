#include <WinSock2.h>
#include "WieserlabsClient.h"
#include <iostream>
#include <boost/system/system_error.hpp>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <thread>
#include <chrono>
#include <QDebug>

WieserlabsClient::WieserlabsClient(const std::string& ip, int port)
    : ip_(ip), port_(port), socket_(io_context_), connected_(false) {
}

WieserlabsClient::~WieserlabsClient() {
    disconnect();
}

bool WieserlabsClient::connect() {
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(ip_, std::to_string(port_));
        boost::asio::connect(socket_, endpoints);
        socket_.set_option(boost::asio::ip::tcp::no_delay(true));
        // Set a receive timeout so receiveResponse() never blocks forever
        // (e.g. DDS stuck waiting for a BNC trigger that never comes)
        DWORD rcvTimeout = 10000; // 10 seconds
        setsockopt(socket_.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&rcvTimeout), sizeof(rcvTimeout));
        connected_ = true;
        // Send authentication for slot 0
        std::string auth = "75f4a4e10dd4b6b0\r\n";
        boost::asio::write(socket_, boost::asio::buffer(auth));
        std::string auth_response = receiveResponse();
        if (auth_response.find("error") != std::string::npos) {
            std::cerr << "Auth failed: " << auth_response << std::endl;
            connected_ = false;
            return false;
        }
        
        return true;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void WieserlabsClient::disconnect() {
    if (connected_) {
        try {
            socket_.close();
        } catch (...) {
            // Ignore errors during disconnect
        }
        connected_ = false;
    }
}

bool WieserlabsClient::isConnected() const {
    return connected_;
}

bool WieserlabsClient::singleToneNow(int slot, int channel, double frequency, double amplitude, double phase, bool waitForTrigger) {
    if (!connected_) {
        qDebug() << "WieserlabsClient::singleToneNow - NOT CONNECTED";
        return false;
    }

    std::string commands;
    commands += "dcp " + std::to_string(channel) + " spi:CFR1=0x402000\n";
    commands += "dcp " + std::to_string(channel) + " spi:CFR2=0x1000080\n";

    std::string amp_w = amp_to_word(amplitude);
    std::string phase_w = phase_to_word(phase);
    std::string freq_w = freq_to_word(frequency);
    std::string stp0_value = "0x" + amp_w + phase_w + freq_w;

    commands += "dcp " + std::to_string(channel) + " spi:stp0=" + stp0_value + "\n";

    if (waitForTrigger) {
        commands += "dcp " + std::to_string(channel) + " wait::BNC_IN_A_RISING\n";
    }

    commands += "dcp " + std::to_string(channel) + " update:u\n";

    qDebug() << "=== DCP COMMANDS ===";
    qDebug() << QString::fromStdString(commands);
    qDebug() << "====================";

    return sendCommand(commands);
}

bool WieserlabsClient::turnOffChannel(int channel) {
    if (!connected_) {
        return false;
    }
    
    // Send commands to turn off channel (amplitude = 0)
    std::string commands;
    commands += "dcp " + std::to_string(channel) + " spi:CFR1=0x402000\n";
    commands += "dcp " + std::to_string(channel) + " spi:CFR2=0x1000080\n";
    commands += "dcp " + std::to_string(channel) + " spi:stp0=0x000000000000\n";  // Zero amplitude
    commands += "dcp " + std::to_string(channel) + " update:u\n";
    
    return sendCommand(commands);
}

bool WieserlabsClient::abortChannel(int channel) {
    if (!connected_) {
        return false;
    }
    
    // Clear pending wait/trigger state and return channel to normal tone mode.
    std::string commands;
    commands += "dcp " + std::to_string(channel) + " update::immediate\n";
    commands += "dcp " + std::to_string(channel) + " spi:CFR1=0x402000\n";
    commands += "dcp " + std::to_string(channel) + " spi:CFR2=0x1000080\n";
    commands += "dcp " + std::to_string(channel) + " update:u\n";
    
    qDebug() << "Aborting channel" << channel << "clearing queued waits and restoring tone mode";
    return sendCommand(commands);
}

bool WieserlabsClient::sendBatchCommands(const std::string& commands) {
    // Fire-and-forget: send the batch to the DDS hardware and return immediately
    // without waiting for the response. The DDS executes the DCP program
    // asynchronously (waiting for hardware triggers, running ramp steps, etc.).
    // Blocking here would freeze the experiment thread for the entire script duration.
    // The socket is reconnected before next static use (needsReinitializeAfterScript),
    // so any unread response in the TCP buffer is discarded cleanly.
    if (!connected_) {
        return false;
    }
    try {
        std::string payload;
        payload.reserve(commands.size() + 64);
        std::istringstream iss(commands);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                payload += line;
                payload += "\r\n";
            }
        }
        if (payload.empty()) {
            return true;
        }
        payload += "dcp flush\r\n";
        boost::asio::write(socket_, boost::asio::buffer(payload));
        // Do NOT read the response — the DDS is now running its program.
        return true;
    } catch (const boost::system::system_error& e) {
        std::cerr << "sendBatchCommands failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

bool WieserlabsClient::sendCommand(const std::string& command) {
    if (!connected_) {
        return false;
    }

    try {
        // Build the entire payload in one string with \r\n line endings,
        // then send as a single TCP write so all lines arrive in one packet.
        // Append "dcp flush" at the end to ensure all instructions are executed
        // and get a single response for the whole batch.
        std::string payload;
        payload.reserve(command.size() + 64);
        std::istringstream iss(command);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                payload += line;
                payload += "\r\n";
            }
        }
        if (payload.empty()) {
            return true;
        }
        // Append flush to execute all queued DCP instructions
        payload += "dcp flush\r\n";
        
        boost::asio::write(socket_, boost::asio::buffer(payload));
        // Read the single response for the whole batch
        std::string response = receiveResponse();
        if (!connected_) {
            return false;
        }
        if (response.find("error") != std::string::npos) {
            std::cerr << "DDS batch error: " << response << std::endl;
            return false;
        }
        return true;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Send command failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

std::string WieserlabsClient::receiveResponse() {
    try {
        boost::asio::streambuf buffer;
        boost::asio::read(socket_, buffer, boost::asio::transfer_at_least(1));
        std::istream is(&buffer);
        std::string response;
        std::getline(is, response);
        return response;
    } catch (const boost::system::system_error& e) {
        // WSAETIMEDOUT means the SO_RCVTIMEO expired (DDS never sent a response,
        // e.g. stuck waiting for a BNC trigger). Disconnect so the next operation
        // triggers a reconnect and starts fresh.
        if (e.code().value() == WSAETIMEDOUT || e.code().value() == WSAECONNRESET) {
            std::cerr << "DDS receive timeout/reset - reconnecting: " << e.what() << std::endl;
        } else {
            std::cerr << "Receive response failed: " << e.what() << std::endl;
        }
        connected_ = false;
        return "";
    }
}

std::string WieserlabsClient::freq_to_word(double f) {
    if (f < 0 || f >= 1e9) {
        return "00000000";
    }
    unsigned long long num = static_cast<unsigned long long>(std::round((1ULL << 32) / 1e9 * f)) & 0xFFFFFFFFULL;
    char buf[9];
    sprintf(buf, "%08llx", num);
    return std::string(buf);
}

std::string WieserlabsClient::amp_to_word(double amp) {
    int val = static_cast<int>(std::round(max(0.0, min(16383.0, 16383.0 * amp))));
    char buf[5];
    sprintf(buf, "%04x", val);
    return std::string(buf);
}

std::string WieserlabsClient::phase_to_word(double phase) {
    phase = std::fmod(phase, 360.0);
    if (phase < 0) phase += 360.0;
    int p = static_cast<int>(std::round((1 << 16) * phase / 360.0));
    char buf[5];
    sprintf(buf, "%04x", p);
    return std::string(buf);
}