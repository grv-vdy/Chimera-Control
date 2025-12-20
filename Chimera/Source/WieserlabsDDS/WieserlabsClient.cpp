#include <WinSock2.h>
#include "WieserlabsClient.h"
#include <iostream>
#include <boost/system/system_error.hpp>
#include <cmath>
#include <cstdio>
#include <algorithm>

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

bool WieserlabsClient::singleToneNow(int slot, int channel, double frequency, double amplitude, double phase) {
    if (!connected_) {
        return false;
    }

    // Build all commands following the exact sequence
    std::string commands;

    // Set CFR1 register for auto-clear phase on update (bit 13)
    commands += "dcp " + std::to_string(channel) + " spi:CFR1=0x402000\n";

    // Set CFR2 register for single tone ASF bit and matched latency
    commands += "dcp " + std::to_string(channel) + " spi:CFR2=0x1000080\n";

    // Compute stp0 register value: 0x[amp][phase][freq]
    std::string amp_w = amp_to_word(amplitude);
    std::string phase_w = phase_to_word(phase);
    std::string freq_w = freq_to_word(frequency);
    std::string stp0_value = "0x" + amp_w + phase_w + freq_w;

    // Send stp0 register write
    commands += "dcp " + std::to_string(channel) + " spi:stp0=" + stp0_value + "\n";

    // Send update immediately
    commands += "dcp " + std::to_string(channel) + " update:u\n";

    // Send all commands at once
    return sendCommand(commands);
}

bool WieserlabsClient::rampTone(int slot, int channel, double startFreq, double endFreq, double amplitude, double duration, double phase) {
    if (!connected_) {
        return false;
    }

    // Ramp implementation:
    // 1. Write CFR1 with ramp enable bits
    // 2. Write CFR2 with amplitude and phase
    // 3. Write frequency ramp start/stop/increment to RAM
    // 4. Trigger ramp with update command

    std::string commands;

    // Set CFR1 for ramp mode (enable sweep with no autoclear on update)
    // Bit 13: autoclear phase (0), Bit 7: linear sweep enable (1)
    commands += "dcp " + std::to_string(channel) + " spi:CFR1=0x400280\n";

    // Set CFR2 register for amplitude and phase
    std::string amp_w = amp_to_word(amplitude);
    std::string phase_w = phase_to_word(phase);
    commands += "dcp " + std::to_string(channel) + " spi:CFR2=0x" + amp_w + phase_w + "00\n";

    // Write start frequency to RAM address 0x00
    std::string startFreq_w = freq_to_word(startFreq);
    commands += "dcp " + std::to_string(channel) + " ram:0x00=0x" + startFreq_w + "\n";

    // Write end frequency to RAM address 0x01
    std::string endFreq_w = freq_to_word(endFreq);
    commands += "dcp " + std::to_string(channel) + " ram:0x01=0x" + endFreq_w + "\n";

    // Calculate ramp increment based on duration and clock (assuming 1 GHz reference)
    // Ramp rate = (endFreq - startFreq) / duration
    double rampInc = (endFreq - startFreq) / (duration * 1e9); // in steps per clock cycle
    unsigned long rampIncWord = static_cast<unsigned long>(std::round(rampInc)) & 0xFFFFFFFF;
    char rampIncBuf[11];
    sprintf(rampIncBuf, "%08lx", rampIncWord);
    
    // Write ramp increment to RAM address 0x02
    commands += "dcp " + std::to_string(channel) + " ram:0x02=0x" + std::string(rampIncBuf) + "\n";

    // Write ramp duration to RAM address 0x03
    unsigned long durationWord = static_cast<unsigned long>(std::round(duration * 1e6)) & 0xFFFFFFFF;
    char durationBuf[11];
    sprintf(durationBuf, "%08lx", durationWord);
    commands += "dcp " + std::to_string(channel) + " ram:0x03=0x" + std::string(durationBuf) + "\n";

    // Trigger ramp start
    commands += "dcp " + std::to_string(channel) + " update:u\n";

    return sendCommand(commands);
}

bool WieserlabsClient::sendCommand(const std::string& command) {
    try {
        // Split command into individual lines and send each one
        std::istringstream iss(command);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) {
                // Add \r\n for proper line ending
                std::string cmd = line + "\n\r";
                boost::asio::write(socket_, boost::asio::buffer(cmd));
                // Read response for each command
                std::string response = receiveResponse();
                if (response.find("error") != std::string::npos) {
                    std::cerr << "DDS error for command '" << line << "': " << response << std::endl;
                    return false;
                }
            }
        }
        return true;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Send command failed: " << e.what() << std::endl;
        return false;
    }
}

std::string WieserlabsClient::receiveResponse() {
    try {
        boost::asio::streambuf buffer;
        size_t n = boost::asio::read(socket_, buffer, boost::asio::transfer_at_least(1));
        std::istream is(&buffer);
        std::string response;
        std::getline(is, response);
        return response;
    } catch (const boost::system::system_error& e) {
        std::cerr << "Receive response failed: " << e.what() << std::endl;
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