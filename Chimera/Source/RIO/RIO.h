#pragma once
#include <vector>
#include <array>
#include "AnalogOutput/AoStructures.h"
#include "DigitalOutput/DoStructures.h"
#include "NiFpga.h"
#include "NiFpga_chimerasequencer.h"

class RIO {
public:
    RIO();
    ~RIO();

    // FPGA session management
    void initialize(const char* resource = "RIO0");
    void close();

    // TTL / DAC writing
    int writeTTL(const std::vector<uint64_t>& times, const std::vector<uint64_t>& data);
    int writeDACs(const std::vector<AoChannelSnapshot>& dacSnapshots);

    // Sequencer control
    void trigger();
    int clearFifo();
	void untrigger();
    void reset();
    void waitForMemLoaded();
    void waitForFinish();

private:
    NiFpga_Session session;
};
