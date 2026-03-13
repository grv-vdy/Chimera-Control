#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <boost/asio.hpp>

/**
 * Binary protocol client for FlexDDS-NG (port 26010+slot).
 * 
 * Uses the 8-byte frame binary protocol which is ~10x faster than the text DCP protocol.
 * All frames are little-endian 64-bit values.
 * 
 * Frame types:
 *   0x0... NULL  - ignored
 *   0x1... SLOT  - DCP instruction to a slot/channel
 *   0x2... RACK  - rack command
 *   0x3... AUTH  - authentication token
 */
class WieserlabsBinaryClient {
public:
    WieserlabsBinaryClient(const std::string& ip, int slot = 0);
    ~WieserlabsBinaryClient();

    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    // High-level API matching text client
    bool singleToneNow(int channel, double freqHz, double amplitude, double phaseDeg = 0.0);
    bool turnOffChannel(int channel);
    
    // Queue commands for batch send
    void queueWriteCFR1(int channel, uint32_t value);
    void queueWriteCFR2(int channel, uint32_t value);
    void queueWriteSTP0(int channel, uint64_t value);  // 64-bit: [amp:14][phase:16][freq:32]
    void queueWriteDRL(int channel, uint64_t value);   // Digital Ramp Limit
    void queueWriteDRSS(int channel, uint64_t value);  // Digital Ramp Step Size
    void queueWriteDRR(int channel, uint32_t value);   // Digital Ramp Rate
    void queueUpdate(int channel, bool ioUpdate = true, uint8_t drctl = 0, uint8_t drhold = 0);
    void queueWaitBncARising(int channel, uint32_t timeoutUs = 0);
    void queueWaitUs(int channel, uint32_t delayUs);
    
    // Send all queued frames
    bool sendQueue();
    void clearQueue();

    // Low-level: build STP0 value from components
    static uint64_t buildSTP0(double freqHz, double amplitude, double phaseDeg);

private:
    // AD9910 register addresses
    static constexpr uint8_t REG_CFR1 = 0x00;
    static constexpr uint8_t REG_CFR2 = 0x01;
    static constexpr uint8_t REG_CFR3 = 0x02;
    static constexpr uint8_t REG_FTW  = 0x07;
    static constexpr uint8_t REG_POW  = 0x08;
    static constexpr uint8_t REG_ASF  = 0x09;
    static constexpr uint8_t REG_DRL  = 0x0B;  // Digital Ramp Limit (64-bit)
    static constexpr uint8_t REG_DRS  = 0x0C;  // Digital Ramp Step Size (64-bit)
    static constexpr uint8_t REG_DRR  = 0x0D;  // Digital Ramp Rate (32-bit)
    static constexpr uint8_t REG_STP0 = 0x0E;  // Single Tone Profile 0 (64-bit)

    // Event codes for WAIT instruction
    static constexpr uint8_t EVT_NONE = 0;
    static constexpr uint8_t EVT_BNC_IN_A_RISING = 16;  // External trigger A rising
    static constexpr uint8_t EVT_BNC_IN_A_FALLING = 17;
    static constexpr uint8_t EVT_BNC_IN_B_RISING = 18;
    static constexpr uint8_t EVT_BNC_IN_B_FALLING = 19;

    // Build binary frames
    uint64_t makeSlotFrame(int channel, uint64_t dcpInstruction);
    uint64_t makeAuthFrame();
    
    // DCP instruction builders (48-bit, returned in low 48 bits)
    uint64_t makeDdsWrite32(uint8_t regAddr, uint32_t data, bool waitComplete = false);
    uint64_t makeDdsWriteLongPrefix(uint32_t lowData);
    uint64_t makeDdsWrite64High(uint8_t regAddr, uint32_t highData, bool waitComplete = false);
    uint64_t makeUpdate(bool ioUpdate, uint8_t drctl = 0, uint8_t drhold = 0);
    uint64_t makeWait(uint8_t event1, uint8_t event2, bool andMode, uint8_t resMode, uint32_t timeout);

    void queueFrame(uint64_t frame);
    void queueDdsWrite32(int channel, uint8_t regAddr, uint32_t data);
    void queueDdsWrite64(int channel, uint8_t regAddr, uint64_t data);

    std::string ip_;
    int slot_;
    int binaryPort_;
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
    bool connected_;
    
    std::vector<uint64_t> frameQueue_;
};
