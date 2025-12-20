#include "stdafx.h"
#include "RIO.h"
#include <stdexcept>
#include <thread>
#include <chrono>

RIO::RIO() : session(0) {}

RIO::~RIO() { close(); }

void RIO::initialize(const char* resource) {
    NiFpga_Initialize();
    NiFpga_Status status = NiFpga_Open(
        NiFpga_chimerasequencer_Bitfile,
        NiFpga_chimerasequencer_Signature,
        resource,
        NiFpga_OpenAttribute_NoRun,
        &session);
    if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to open FPGA session");
    }
    NiFpga_Abort(session);
    NiFpga_Run(session, 0);
    status = NiFpga_StartFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time);
    if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to open FIFO");
    }
    status = NiFpga_StartFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data);
    if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to open FIFO");
    }
    /*clearFifo();*/
    NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_start_copy_to_ram, 0);

}

void RIO::close() {
    if (session) {
        NiFpga_StopFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time);
        NiFpga_StopFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data);
        NiFpga_Close(session, 0);
        NiFpga_Finalize();
        session = 0;
    }
}

int RIO::writeTTL(const std::vector<uint64_t>& times, const std::vector<uint64_t>& data) {
    if (times.size() != data.size()) return 1;
    size_t empty; // Change the type of 'empty' to size_t

    NiFpga_Status status_time;
    NiFpga_Status status_data;

    status_time = NiFpga_WriteFifoU64(session,
        NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time,
        times.data(), (size_t)times.size(), 1000, &empty);

    status_data = NiFpga_WriteFifoU64(session,
        NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data,
        data.data(), (size_t)data.size(), 1000, &empty);

    if (status_time != NiFpga_Status_Success || status_data != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to write to FIFO");
    }

    NiFpga_Status status;
    status = NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_start_copy_to_ram, 1);
    if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to Start Copy to RAM");
    }

    return 0;
}

int RIO::writeDACs(const std::vector<AoChannelSnapshot>& dacSnapshots) {
    // not yet implemented, depends on how DAC is mapped in FPGA
    return 0;
}

void RIO::trigger() {
    NiFpga_Status status;
    NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_start_copy_to_ram, 0);
    status = NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_trigger_i, 1);
	if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to trigger FPGA");
    }
    
}

void RIO::untrigger() {
   NiFpga_Status status;
    status = NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_trigger_i, 0);
	if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to trigger FPGA");
    }
}

void RIO::set_reprogram(int reprogram) {
   NiFpga_Status status;
    status = NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_skip_program, reprogram);
	if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to trigger FPGA");
    }
}

void RIO::reset() {
    NiFpga_Abort(session);
    NiFpga_Run(session, 0);
    NiFpga_Status status;
    status = NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_reset, 1);
	if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to reset FPGA");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // wait 5ms for reset to complete
    status = NiFpga_WriteBool(session, NiFpga_chimerasequencer_ControlBool_reset, 0);
	if (status != NiFpga_Status_Success) {
        throw std::runtime_error("Failed to reset FPGA");
    }

    clearFifo();
}

void RIO::waitForMemLoaded() {
    NiFpga_Bool loaded = 0;
    NiFpga_Status status;

    //for debugging use index count
    int16_t  index_count = 0;
    while (!loaded) {
        status = NiFpga_ReadBool(session, NiFpga_chimerasequencer_IndicatorBool_mem_loaded, &loaded);
        NiFpga_ReadI16(session, NiFpga_chimerasequencer_IndicatorI16_index_count, &index_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int RIO::clearFifo()
{
    NiFpga_Status status;

  
    // Flush contents by reading until empty
    size_t remaining = 100;
    uint64_t buffer[1000]; // dummy buffer

    /*do {
        status = NiFpga_ReadFifoU64(session,
            NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time,
            buffer, 1000, 0, &remaining);
        if (status != NiFpga_Status_Success) return status;
    } while (remaining > 0);

    do {
        status = NiFpga_ReadFifoU64(session,
            NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data,
            buffer, 1000, 0, &remaining);
        if (status != NiFpga_Status_Success) return status;
    } while (remaining > 0);*/

    // Restart FIFOs
    // Stop FIFOs
    status = NiFpga_StopFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time);
    if (status != NiFpga_Status_Success) return status;

    status = NiFpga_StopFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data);
    if (status != NiFpga_Status_Success) return status;

    status = NiFpga_StartFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Time);
    if (status != NiFpga_Status_Success) return status;

    status = NiFpga_StartFifo(session, NiFpga_chimerasequencer_HostToTargetFifoU64_FIFO_Data);
    if (status != NiFpga_Status_Success) return status;

    return NiFpga_Status_Success;
}

void RIO::waitForFinish() {
    NiFpga_Bool done = 0;
    while (!done) {
        NiFpga_ReadBool(session, NiFpga_chimerasequencer_IndicatorBool_finish_stb_o, &done);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
