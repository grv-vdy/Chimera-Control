#define NOMINMAX
#include <WinSock2.h>
#include "WieserlabsBinaryClient.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <QDebug>

WieserlabsBinaryClient::WieserlabsBinaryClient(const std::string& ip, int slot)
    : ip_(ip), slot_(slot), binaryPort_(26010 + slot), socket_(io_context_), connected_(false)
{
}

WieserlabsBinaryClient::~WieserlabsBinaryClient()
{
    disconnect();
}

bool WieserlabsBinaryClient::connect()
{
    try {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(ip_, std::to_string(binaryPort_));
        boost::asio::connect(socket_, endpoints);
        socket_.set_option(boost::asio::ip::tcp::no_delay(true));
        connected_ = true;

        // Send binary auth frame (8 bytes, little-endian)
        uint64_t authFrame = makeAuthFrame();
        boost::asio::write(socket_, boost::asio::buffer(&authFrame, 8));
        
        qDebug() << "WieserlabsBinaryClient: Connected to" << QString::fromStdString(ip_) 
                 << "port" << binaryPort_ << "(binary protocol)";
        return true;
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "Binary client connection failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void WieserlabsBinaryClient::disconnect()
{
    if (connected_) {
        try {
            socket_.close();
        }
        catch (...) {}
        connected_ = false;
    }
}

// AUTH frame: send token 0x75f4a4e10dd4b6bX directly as 8 bytes (little-endian)
uint64_t WieserlabsBinaryClient::makeAuthFrame()
{
    // The authentication token is 0x75f4a4e10dd4b6bX where X = slot number (0-5)
    // Sent as-is in little-endian byte order (native on x86)
    return 0x75f4a4e10dd4b6b0ULL | static_cast<uint64_t>(slot_);
}

// SLOT frame wrapper: 0001 000F SSS1 CCCC + 48-bit DCP instruction
// F=0 for normal, SSS=slot, CCCC=channel bitmask
uint64_t WieserlabsBinaryClient::makeSlotFrame(int channel, uint64_t dcpInstruction)
{
    // Header bits 63:48 = 0001 000F SSS1 CCCC
    // For slot 0, channel 0: 0001 0000 0001 0001 = 0x1011
    // For slot 0, channel 1: 0001 0000 0001 0010 = 0x1012
    uint16_t header = 0x1000;  // 0001 0000 0000 0000
    header |= (static_cast<uint16_t>(slot_ & 0x7) << 5);  // SSS in bits 7:5
    header |= 0x10;  // The '1' bit before CCCC
    header |= (1 << channel) & 0x0F;  // Channel bitmask in bits 3:0
    
    return (static_cast<uint64_t>(header) << 48) | (dcpInstruction & 0xFFFFFFFFFFFFULL);
}

// DDS_WRITE 32-bit: 0001 00W0 FEAAAAAA DDDDDDDD DDDDDDDD dddddddd dddddddd
uint64_t WieserlabsBinaryClient::makeDdsWrite32(uint8_t regAddr, uint32_t data, bool waitComplete)
{
    uint64_t instr = 0;
    // Bits 47:44 = 0001
    instr |= (0x1ULL << 44);
    // Bit 41 = W (wait for completion)
    if (waitComplete) {
        instr |= (1ULL << 41);
    }
    // Bit 40 = 0 (short write)
    // Bits 39:38 = FE (completion events, set to 0)
    // Bits 37:32 = AAAAAA (register address)
    instr |= (static_cast<uint64_t>(regAddr & 0x3F) << 32);
    // Bits 31:0 = data
    instr |= static_cast<uint64_t>(data);
    return instr;
}

// DDS_WRITE long prefix (for 64-bit registers): 0001 0001 DDDDDDDD ...
// This latches the low 32 bits
uint64_t WieserlabsBinaryClient::makeDdsWriteLongPrefix(uint32_t lowData)
{
    uint64_t instr = 0;
    // Bits 47:44 = 0001
    instr |= (0x1ULL << 44);
    // Bit 40 = 1 (long write prefix)
    instr |= (1ULL << 40);
    // Bits 39:8 = low 32 bits of data (shifted into position)
    instr |= (static_cast<uint64_t>(lowData) << 8);
    return instr;
}

// DDS_WRITE 64-bit high part: follows long prefix, contains high 32 bits + address
uint64_t WieserlabsBinaryClient::makeDdsWrite64High(uint8_t regAddr, uint32_t highData, bool waitComplete)
{
    // Same format as 32-bit write, but data contains high 32 bits
    return makeDdsWrite32(regAddr, highData, waitComplete);
}

// UPDATE: 0100 0000 ........ ........ .......C CBBAAPPP PHHRROOU
uint64_t WieserlabsBinaryClient::makeUpdate(bool ioUpdate, uint8_t drctl, uint8_t drhold)
{
    uint64_t instr = 0;
    // Bits 47:40 = 0100 0000
    instr |= (0x40ULL << 40);
    // Bit 0 = U (IO_UPDATE pulse)
    if (ioUpdate) {
        instr |= 1;
    }
    // Bits 2:1 = OO (OSK) - not used
    // Bits 4:3 = RR (DRCTL): 11=HIGH, 10=LOW, 01=toggle, 00=no change
    instr |= (static_cast<uint64_t>(drctl & 0x3) << 3);
    // Bits 6:5 = HH (DRHOLD)
    instr |= (static_cast<uint64_t>(drhold & 0x3) << 5);
    return instr;
}

// WAIT: 0011 0URH xXXARRRR RRSSSSSS TTTTTTTT TTTTTTTT TTTTTTTT
uint64_t WieserlabsBinaryClient::makeWait(uint8_t event1, uint8_t event2, bool andMode, uint8_t resMode, uint32_t timeout)
{
    uint64_t instr = 0;
    // Bits 47:44 = 0011
    instr |= (0x3ULL << 44);
    // Bit 43 = 0
    // Bit 42 = U (unused?)
    // Bits 41:40 = RH (resolution): 00=infinite, 01=8ns, 10=1.024μs, 11=extended
    instr |= (static_cast<uint64_t>(resMode & 0x3) << 40);
    // Bits 39:37 = xXX (unused, set to 0)
    // Bit 36 = A (AND mode)
    if (andMode) {
        instr |= (1ULL << 36);
    }
    // Bits 35:30 = RRRRRR (event 1)
    instr |= (static_cast<uint64_t>(event1 & 0x3F) << 30);
    // Bits 29:24 = SSSSSS (event 2)
    instr |= (static_cast<uint64_t>(event2 & 0x3F) << 24);
    // Bits 23:0 = timeout
    instr |= static_cast<uint64_t>(timeout & 0xFFFFFF);
    return instr;
}

void WieserlabsBinaryClient::queueFrame(uint64_t frame)
{
    frameQueue_.push_back(frame);
}

void WieserlabsBinaryClient::queueDdsWrite32(int channel, uint8_t regAddr, uint32_t data)
{
    uint64_t dcp = makeDdsWrite32(regAddr, data, false);
    queueFrame(makeSlotFrame(channel, dcp));
}

void WieserlabsBinaryClient::queueDdsWrite64(int channel, uint8_t regAddr, uint64_t data)
{
    // 64-bit write requires two frames: long prefix with low 32 bits, then high 32 bits + address
    uint32_t lowData = static_cast<uint32_t>(data & 0xFFFFFFFFULL);
    uint32_t highData = static_cast<uint32_t>((data >> 32) & 0xFFFFFFFFULL);
    
    uint64_t prefix = makeDdsWriteLongPrefix(lowData);
    queueFrame(makeSlotFrame(channel, prefix));
    
    uint64_t high = makeDdsWrite64High(regAddr, highData, false);
    queueFrame(makeSlotFrame(channel, high));
}

void WieserlabsBinaryClient::queueWriteCFR1(int channel, uint32_t value)
{
    queueDdsWrite32(channel, REG_CFR1, value);
}

void WieserlabsBinaryClient::queueWriteCFR2(int channel, uint32_t value)
{
    queueDdsWrite32(channel, REG_CFR2, value);
}

void WieserlabsBinaryClient::queueWriteSTP0(int channel, uint64_t value)
{
    queueDdsWrite64(channel, REG_STP0, value);
}

void WieserlabsBinaryClient::queueWriteDRL(int channel, uint64_t value)
{
    queueDdsWrite64(channel, REG_DRL, value);
}

void WieserlabsBinaryClient::queueWriteDRSS(int channel, uint64_t value)
{
    queueDdsWrite64(channel, REG_DRS, value);
}

void WieserlabsBinaryClient::queueWriteDRR(int channel, uint32_t value)
{
    queueDdsWrite32(channel, REG_DRR, value);
}

void WieserlabsBinaryClient::queueUpdate(int channel, bool ioUpdate, uint8_t drctl, uint8_t drhold)
{
    uint64_t dcp = makeUpdate(ioUpdate, drctl, drhold);
    queueFrame(makeSlotFrame(channel, dcp));
}

void WieserlabsBinaryClient::queueWaitBncARising(int channel, uint32_t timeoutUs)
{
    // Resolution mode 2 = 1.024μs per tick
    // Event 1 = BNC_IN_A_RISING (16), Event 2 = 0 (none)
    uint8_t resMode = (timeoutUs > 0) ? 2 : 0;  // 0 = infinite wait if no timeout
    uint64_t dcp = makeWait(EVT_BNC_IN_A_RISING, EVT_NONE, false, resMode, timeoutUs);
    queueFrame(makeSlotFrame(channel, dcp));
}

void WieserlabsBinaryClient::queueWaitUs(int channel, uint32_t delayUs)
{
    // Use timeout as delay: resolution mode 2 = 1.024μs per tick
    // No events (0), just timeout
    if (delayUs == 0) return;
    uint64_t dcp = makeWait(EVT_NONE, EVT_NONE, false, 2, delayUs);
    queueFrame(makeSlotFrame(channel, dcp));
}

bool WieserlabsBinaryClient::sendQueue()
{
    if (!connected_ || frameQueue_.empty()) {
        return connected_;
    }

    try {
        // Send all frames in one write (each frame is 8 bytes, little-endian)
        boost::asio::write(socket_, boost::asio::buffer(frameQueue_.data(), frameQueue_.size() * 8));
        qDebug() << "WieserlabsBinaryClient: Sent" << frameQueue_.size() << "binary frames";
        frameQueue_.clear();
        return true;
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "Binary send failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void WieserlabsBinaryClient::clearQueue()
{
    frameQueue_.clear();
}

// Build 64-bit STP0 value: [amplitude:14][phase:16][frequency:32] with padding
uint64_t WieserlabsBinaryClient::buildSTP0(double freqHz, double amplitude, double phaseDeg)
{
    // Frequency tuning word (32-bit): FTW = freq * 2^32 / 1e9
    uint32_t ftw = 0;
    if (freqHz >= 0.0 && freqHz < 1e9) {
        ftw = static_cast<uint32_t>(std::llround((4294967296.0 / 1e9) * freqHz)) & 0xFFFFFFFFu;
    }

    // Phase offset word (16-bit): POW = phase * 2^16 / 360
    double phase = std::fmod(phaseDeg, 360.0);
    if (phase < 0.0) phase += 360.0;
    uint16_t pow = static_cast<uint16_t>(std::llround((65536.0 / 360.0) * phase)) & 0xFFFFu;

    // Amplitude scale factor (14-bit): ASF = amplitude * 0x3FFF
    double amp = std::max(0.0, std::min(1.0, amplitude));
    uint16_t asf = static_cast<uint16_t>(std::llround(0x3FFF * amp)) & 0x3FFFu;

    // STP0 format (64-bit): [ASF:14][0:2][POW:16][FTW:32]
    // Bits 63:50 = ASF (14 bits)
    // Bits 49:48 = 0 (reserved)
    // Bits 47:32 = POW (16 bits)
    // Bits 31:0  = FTW (32 bits)
    uint64_t stp0 = 0;
    stp0 |= (static_cast<uint64_t>(asf) << 48);
    stp0 |= (static_cast<uint64_t>(pow) << 32);
    stp0 |= static_cast<uint64_t>(ftw);
    return stp0;
}

bool WieserlabsBinaryClient::singleToneNow(int channel, double freqHz, double amplitude, double phaseDeg)
{
    if (!connected_) return false;

    clearQueue();
    
    // CFR1: amplitude scale from single tone profiles
    queueWriteCFR1(channel, 0x00402000);
    // CFR2: enable amplitude control
    queueWriteCFR2(channel, 0x01000080);
    // STP0: frequency, amplitude, phase
    queueWriteSTP0(channel, buildSTP0(freqHz, amplitude, phaseDeg));
    // IO_UPDATE pulse
    queueUpdate(channel, true);

    return sendQueue();
}

bool WieserlabsBinaryClient::turnOffChannel(int channel)
{
    if (!connected_) return false;

    clearQueue();
    queueWriteCFR1(channel, 0x00402000);
    queueWriteCFR2(channel, 0x01000080);
    queueWriteSTP0(channel, 0);  // Zero amplitude
    queueUpdate(channel, true);

    return sendQueue();
}
