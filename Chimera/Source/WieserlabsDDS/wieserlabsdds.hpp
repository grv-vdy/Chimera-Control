#pragma once

// Make Boost.System header-only so we don't need to link libboost_system
#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_ERROR_CODE_HEADER_ONLY
#endif
#ifndef BOOST_SYSTEM_NO_LIB
#define BOOST_SYSTEM_NO_LIB
#endif

#include <boost/asio.hpp>

#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class WieserlabsClient {
public:
    // Simple constructor: one slot (index 0) at given IP
    explicit WieserlabsClient(const std::string& ip, double maxAmpDbm = 10.0)
        : ip_(ip),
          maxAmpDbm_(maxAmpDbm),
          io_(),
          socket_(io_)
    {
        using tcp = boost::asio::ip::tcp;

        // Connect to slot 0 (port 26000)
        tcp::endpoint endpoint(
            boost::asio::ip::make_address(ip_),
            static_cast<unsigned short>(26000));

        socket_.connect(endpoint);

        // Authenticate for slot 0
        sendLine(authMessage(0));

        // Initialize CFR values (same defaults as Python code)
        cfr_[0][0] = 0x00410002; // ch0, CFR1
        cfr_[0][1] = 0x004008C0; // ch0, CFR2
        cfr_[1][0] = 0x00410002; // ch1, CFR1
        cfr_[1][1] = 0x004008C0; // ch1, CFR2

        // Write CFR1 and CFR2 for both channels
        for (int ch = 0; ch < 2; ++ch) {
            writeCFR(ch, 1);
            writeCFR(ch, 2);
        }
    }

    // Convenience: immediately set a single tone and send to hardware
    // slotIndex is accepted for API compatibility but ignored (we only support slot 0)
    void singleToneNow(int slotIndex, int channel,
                       double freqHz, double amplitude, double phaseDeg = 0.0)
    {
        (void)slotIndex; // unused

        // 1) Configure CFR bits: amplitude control on, parallel port off, ramp off
        setCFRBit(channel, 2, 24, true);   // amplitude control on
        setCFRBit(channel, 2, 4,  false);  // parallel data port off
        setCFRBit(channel, 2, 19, false);  // ramp off
        writeCFR(channel, 2);              // send new CFR2

        // 2) Program STP0 with amplitude, phase, frequency
        std::string stp0 = getStp0Value(freqHz, amplitude, phaseDeg);
        messageStack_.push_back(ad9910WriteMessage(channel, "stp0", stp0));

        // 3) Issue an update command
        messageStack_.push_back(updateMessage(channel, "u"));

        // 4) Send everything to the board
        run();
    }

private:
    // ====== Types / Members ======
    using tcp = boost::asio::ip::tcp;

    std::string ip_;
    double maxAmpDbm_;

    boost::asio::io_context io_;
    tcp::socket socket_;
    std::vector<std::string> messageStack_;
    std::uint32_t cfr_[2][2]; // [channel][cfrIndex: 0->CFR1, 1->CFR2]

    // ====== Low-level helpers ======

    static std::string hexWidth(std::uint32_t value, int width) {
        std::ostringstream ss;
        ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(width)
           << static_cast<std::uint32_t>(value);
        return ss.str();
    }

    static std::string authMessage(int slotIndex) {
        std::ostringstream ss;
        ss << "75f4a4e10dd4b6b" << slotIndex;
        return ss.str();
    }

    // freq in Hz, mapped to 32-bit tuning word (1 GHz system clock)
    static std::string freqToWord(double f) {
        if (f < 0.0 || f >= 1e9) {
            std::cerr << "freq needs to be in range [0, 1e9)\n";
            f = 0.0;
        }
        double factor = 4294967296.0 / 1e9; // 2^32 / 1e9
        std::uint32_t num = static_cast<std::uint32_t>(
            std::llround(factor * f)) & 0xffffffffu;
        return hexWidth(num, 8);
    }

    // amplitude 0..1 → 14-bit word
    static std::string ampToWord(double amp) {
        double a = std::max(0.0, std::min(1.0, amp));
        int val = static_cast<int>(std::llround(0x3fff * a));
        return hexWidth(static_cast<std::uint32_t>(val), 4);
    }

    // phase in degrees → 16-bit word
    static std::string phaseToWord(double phaseDeg) {
        double phase = std::fmod(phaseDeg, 360.0);
        if (phase < 0.0) phase += 360.0;
        double factor = 65536.0 / 360.0; // 2^16 / 360
        std::uint32_t p = static_cast<std::uint32_t>(
            std::llround(factor * phase)) & 0xffffu;
        return hexWidth(p, 4);
    }

    std::string getStp0Value(double freq, double amp, double phaseDeg) {
        std::string amp_w   = ampToWord(amp);
        std::string phase_w = phaseToWord(phaseDeg);
        std::string freq_w  = freqToWord(freq);

        std::ostringstream ss;
        ss << "0x" << amp_w << "_" << phase_w << "_" << freq_w;
        return ss.str();
    }

    static std::string ad9910WriteMessage(int channel,
                                          const std::string& reg,
                                          const std::string& value)
    {
        std::ostringstream ss;
        ss << "dcp " << channel << " spi:" << reg << "=" << value;
        return ss.str();
    }

    static std::string updateMessage(int channel, const std::string& type) {
        std::ostringstream ss;
        ss << "dcp " << channel << " update:" << type;
        return ss.str();
    }

    // Set or clear bit in CFR array and keep local mirror
    void setCFRBit(int channel, int cfrNumber, int bitIndex, bool bitValue) {
        if (channel < 0 || channel > 1) {
            std::cerr << "Invalid channel\n";
            return;
        }
        if (cfrNumber != 1 && cfrNumber != 2) {
            std::cerr << "Invalid CFR number\n";
            return;
        }
        if (bitIndex < 0 || bitIndex > 31) {
            std::cerr << "Invalid bit index\n";
            return;
        }

        int idx = cfrNumber - 1;
        std::uint32_t mask = (1u << bitIndex);
        if (bitValue) {
            cfr_[channel][idx] |= mask;
        } else {
            cfr_[channel][idx] &= ~mask;
        }
    }

    // Write CFR1 or CFR2 for a channel to the DDS
    void writeCFR(int channel, int cfrNumber) {
        int idx = cfrNumber - 1;
        std::ostringstream ssVal;
        ssVal << "0x" << hexWidth(cfr_[channel][idx], 8);
        messageStack_.push_back(
            ad9910WriteMessage(channel, "CFR" + std::to_string(cfrNumber), ssVal.str())
        );
        run(); // send immediately
    }

    // Send accumulated messages as one block
    void run() {
        if (messageStack_.empty()) return;

        std::ostringstream payload;
        for (const auto& m : messageStack_) {
            payload << m << "\n";
        }
        std::string msg = payload.str();
        messageStack_.clear();

        sendLine(msg);
    }

    // Send line(s) and read a single response line
    void sendLine(const std::string& line) {
        // Ensure it ends with newline
        std::string msg = line;
        if (msg.empty() || msg.back() != '\n')
            msg.push_back('\n');

        boost::asio::write(socket_, boost::asio::buffer(msg));

        // Read one response line (like "OK" or error)
        boost::asio::streambuf buf;
        boost::system::error_code ec;
        boost::asio::read_until(socket_, buf, '\n', ec);

        if (ec && ec != boost::asio::error::eof) {
            std::cerr << "read error: " << ec.message() << "\n";
            return;
        }

        std::istream is(&buf);
        std::string response;
        std::getline(is, response);

        if (!response.empty()) {
            std::cerr << "FlexDDS -> " << response << "\n";
        }
    }
};

