#include "flexdds.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <algorithm> 


// ---------- helpers for tone & ramp ----------

static std::string hexWidth(std::uint32_t value, int width) {
    std::ostringstream ss;
    ss << std::hex << std::nouppercase << std::setfill('0') << std::setw(width)
       << static_cast<std::uint32_t>(value);
    return ss.str();
}

static std::uint32_t setBit(std::uint32_t v, int index, bool x) {
    std::uint32_t mask = (1u << index);
    if (x) v |= mask;
    else   v &= ~mask;
    return v;
}

// freq in Hz → 32-bit tuning word (1 GHz system clock)
static std::string freqToWord(double f) {
    if (f < 0.0 || f >= 1e9) {
        std::cerr << "freq needs to be in [0, 1e9), clamping\n";
        f = std::max(0.0, std::min(999999999.0, f));
    }
    double factor = 4294967296.0 / 1e9; // 2^32 / 1e9
    std::uint32_t num = static_cast<std::uint32_t>(
        std::llround(factor * f)) & 0xffffffffu;
    return hexWidth(num, 8);
}

// amp 0..1 → 14-bit word
static std::string ampToWord(double amp) {
    double a = std::max(0.0, std::min(1.0, amp));
    int val = static_cast<int>(std::llround(0x3fff * a));
    return hexWidth(static_cast<std::uint32_t>(val), 4);
}

// dBm -> normalized amplitude (0..1)
// maxDbm is the power that corresponds to amp = 1.0
static double dbmToAmp(double dbm, double maxDbm = 14.0) {
    // relative amplitude: A / A_max = 10^((P_dBm - P_max_dBm)/20)
    double rel = std::pow(10.0, (dbm - maxDbm) / 20.0);
    if (rel < 0.0) rel = 0.0;
    if (rel > 1.0) rel = 1.0;
    return rel;
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

static std::string stp0Value(double freq, double amp, double phaseDeg) {
    std::string amp_w   = ampToWord(amp);
    std::string phase_w = phaseToWord(phaseDeg);
    std::string freq_w  = freqToWord(freq);
    std::ostringstream ss;
    ss << "0x" << amp_w << "_" << phase_w << "_" << freq_w;
    return ss.str();
}

static bool parseBool(const std::string& s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    return (t == "1" || t == "true" || t == "yes" || t == "y");
}


// send sequence to set a tone on one channel
static void sendTone(FlexDDS& dds, int ch, double freqHz, double amp, double phaseDeg = 0.0) {
    if (ch < 0 || ch > 1) {
        throw std::runtime_error("channel must be 0 or 1");
    }

    const std::uint32_t CFR1_BASE = 0x00410002;
    const std::uint32_t CFR2_BASE = 0x004008C0;

    // CFR1
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:CFR1=0x" << hexWidth(CFR1_BASE, 8);
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "CFR1 -> " << reply << "\n";
    }

    // CFR2: amp control on, parallel port off, ramp off
    std::uint32_t cfr2 = CFR2_BASE;
    cfr2 = setBit(cfr2, 24, true);
    cfr2 = setBit(cfr2, 4,  false);
    cfr2 = setBit(cfr2, 19, false);

    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:CFR2=0x" << hexWidth(cfr2, 8);
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "CFR2 -> " << reply << "\n";
    }

    // STP0: amp / phase / freq
    {
        std::string stp0 = stp0Value(freqHz, amp, phaseDeg);
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:stp0=" << stp0;
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "STP0 -> " << reply << "\n";
    }

    // Update
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " update:u";
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "UPDATE -> " << reply << "\n";
    }
}

// clear ramp accumulator like Python _clear_ramp_accumulator()
static void clearRampAccumulator(FlexDDS& dds, int ch) {
    const std::uint32_t CFR1_BASE = 0x00410002;

    // set bit 12
    {
        std::uint32_t cfr1 = setBit(CFR1_BASE, 12, true);
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:CFR1=0x" << hexWidth(cfr1, 8);
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "CFR1 (set bit12) -> " << reply << "\n";
    }
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " update:u";
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "UPDATE (clear acc 1) -> " << reply << "\n";
    }

    // clear bit 12
    {
        std::uint32_t cfr1 = setBit(CFR1_BASE, 12, false);
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:CFR1=0x" << hexWidth(cfr1, 8);
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "CFR1 (clr bit12) -> " << reply << "\n";
    }
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " update:u";
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "UPDATE (clear acc 2) -> " << reply << "\n";
    }
}

// frequency ramp like Python frequency_ramp (no filter mode)
static void sendRamp(FlexDDS& dds,
                     int ch,
                     double fstart,
                     double fend,
                     double amp,
                     double phaseDeg,
                     double tramp,
                     double fstep)
{
    if (ch < 0 || ch > 1)
        throw std::runtime_error("channel must be 0 or 1");

    if (fstart == fend)
        throw std::runtime_error("fstart and fend cannot be the same");

    // Mirror trick for downward ramps (hardware quirk)
    if (fend < fstart) {
        fstart = 1e9 - fstart;
        fend   = 1e9 - fend;
    }

    std::string up_ramp_limit   = freqToWord(std::max(fstart, fend));
    std::string down_ramp_limit = freqToWord(std::min(fstart, fend));

    // time per step
    double t_step_ns = fstep / std::fabs(fstart - fend) * tramp * 1e9;
    int time_in_dds_clock = static_cast<int>(t_step_ns / 4.0);  // 250 MHz DDS clock

    if (time_in_dds_clock > 0xffff) {
        throw std::runtime_error("Either tramp is too big or fstep is too large");
    }

    std::string DRL  = "0x" + up_ramp_limit + down_ramp_limit;
    std::string DRSS = "0x" + freqToWord(fstep) + freqToWord(fstep);
    std::ostringstream drr;
    drr << "0x"
        << hexWidth(static_cast<std::uint32_t>(time_in_dds_clock), 4)
        << hexWidth(static_cast<std::uint32_t>(time_in_dds_clock), 4);
    std::string DRR = drr.str();

    // 1) Set amplitude and phase using single tone at freq=0
    sendTone(dds, ch, 0.0, amp, phaseDeg);

    // 2) Clear ramp accumulator
    clearRampAccumulator(dds, ch);

    // 3) Enable ramp in CFR2 (frequency ramp)
    const std::uint32_t CFR2_BASE = 0x004008C0;
    std::uint32_t cfr2 = CFR2_BASE;
    cfr2 = setBit(cfr2, 24, true);   // amplitude control on
    cfr2 = setBit(cfr2, 4,  false);  // parallel port off
    cfr2 = setBit(cfr2, 19, true);   // enable ramp
    cfr2 = setBit(cfr2, 20, false);  // frequency ramp
    cfr2 = setBit(cfr2, 21, false);  // frequency ramp

    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:CFR2=0x" << hexWidth(cfr2, 8);
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "CFR2 (ramp) -> " << reply << "\n";
    }

    // 4) Program ramp registers DRL, DRSS, DRR
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:DRL=" << DRL;
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "DRL -> " << reply << "\n";
    }
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:DRSS=" << DRSS;
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "DRSS -> " << reply << "\n";
    }
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " spi:DRR=" << DRR;
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "DRR -> " << reply << "\n";
    }

    // 5) Fire the ramp (u-d, then u+d)
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " update:u-d";
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "UPDATE u-d -> " << reply << "\n";
    }
    {
        std::ostringstream cmd;
        cmd << "dcp " << ch << " update:u+d";
        std::string reply = dds.sendCommand(cmd.str());
        std::cerr << "UPDATE u+d -> " << reply << "\n";
    }
}



// ---------- main CLI ----------

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage:\n";
        std::cout << "  flexcli <ip> raw <command...>\n";
        std::cout << "  flexcli <ip> tone <channel> <freq_Hz> <amp_0to1>\n";
        std::cout << "  flexcli <ip> tone_dbm <channel> <freq_Hz> <power_dBm>\n";
        std::cout << "  flexcli <ip> ramp <channel> <fstart_Hz> <fend_Hz> <amp_0to1> <phase_deg> <tramp_s> <fstep_Hz> <continuous>\n";
        std::cout << "  flexcli <ip> ramp_dbm <channel> <fstart_Hz> <fend_Hz> <power_dBm> <phase_deg> <tramp_s> <fstep_Hz>\n";
        std::cout << "Examples:\n";
        std::cout << "  flexcli 192.168.105.5 raw \"dcp 0 update:u\"\n";
        std::cout << "  flexcli 192.168.105.5 tone 0 8.5e7 0.5\n";
        return 0;
    }

    std::string ip   = argv[1];
    std::string mode = argv[2];

    try {
        FlexDDS dds(ip);

        if (mode == "raw") {
            if (argc < 4) {
                std::cerr << "raw mode needs a command string\n";
                return 1;
            }
            std::ostringstream oss;
            for (int i = 3; i < argc; ++i) {
                if (i > 3) oss << " ";
                oss << argv[i];
            }
            std::string cmd = oss.str();
            std::string reply = dds.sendCommand(cmd);
            std::cout << "DDS -> " << reply << "\n";
        }
        else if (mode == "tone") {
            if (argc < 6) {
                std::cerr << "tone mode needs: <channel> <freq_Hz> <amp_0to1>\n";
                return 1;
            }
            int ch        = std::stoi(argv[3]);
            double freqHz = std::stod(argv[4]);
            double amp    = std::stod(argv[5]);

            std::cout << "Setting tone: ch=" << ch
                      << " f=" << freqHz << " Hz"
                      << " amp=" << amp << "\n";

            sendTone(dds, ch, freqHz, amp);
            std::cout << "Tone command sequence finished.\n";
        }
        else if (mode == "tone_dbm") {
            // flexcli ip tone_dbm ch freq_Hz dBm
            if (argc < 6) {
                std::cerr << "tone_dbm needs: <channel> <freq_Hz> <power_dBm>\n";
                return 1;
            }

            int ch         = std::stoi(argv[3]);
            double freqHz  = std::stod(argv[4]);
            double powerDbm = std::stod(argv[5]);

            double amp = dbmToAmp(powerDbm);  // convert dBm -> 0..1

            std::cout << "Setting tone (dBm): ch=" << ch
                    << " f=" << freqHz << " Hz"
                    << " P=" << powerDbm << " dBm"
                    << " -> amp=" << amp << "\n";

            sendTone(dds, ch, freqHz, amp, 0.0);
            std::cout << "Tone (dBm) command sequence finished.\n";
        }
        else if (mode == "ramp") {
            // flexcli ip ramp ch fstart_Hz fend_Hz amp_0to1 phase_deg tramp_s fstep_Hz continuous
            if (argc < 11) {
                std::cerr << "ramp mode needs: <channel> <fstart_Hz> <fend_Hz> <amp_0to1> <phase_deg> <tramp_s> <fstep_Hz> <continuous>\n";
                return 1;
            }

            int ch           = std::stoi(argv[3]);
            double fstart    = std::stod(argv[4]);
            double fend      = std::stod(argv[5]);
            double amp       = std::stod(argv[6]);
            double phaseDeg  = std::stod(argv[7]);
            double tramp     = std::stod(argv[8]);
            double fstep     = std::stod(argv[9]);
            bool continuous  = parseBool(argv[10]);

            std::cout << "Ramp: ch=" << ch
                    << " fstart=" << fstart
                    << " fend="   << fend
                    << " amp="    << amp
                    << " phase="  << phaseDeg
                    << " tramp="  << tramp
                    << " fstep="  << fstep
                    << " continuous=" << (continuous ? "true" : "false") << "\n";

            if (!continuous) {
                sendRamp(dds, ch, fstart, fend, amp, phaseDeg, tramp, fstep);
                std::cout << "Ramp command sequence finished.\n";
            } else {
                std::cout << "Running ramp continuously, press Ctrl-C to stop...\n";
                while (true) {
                    sendRamp(dds, ch, fstart, fend, amp, phaseDeg, tramp, fstep);
                    // optional tiny pause if needed
                    // std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }

        else if (mode == "ramp_dbm") {
            // flexcli ip ramp_dbm ch fstart_Hz fend_Hz dBm phase_deg tramp_s fstep_Hz continuous
            if (argc < 11) {
                std::cerr << "ramp_dbm needs: <channel> <fstart_Hz> <fend_Hz> <power_dBm> <phase_deg> <tramp_s> <fstep_Hz> <continuous>\n";
                return 1;
            }

            int ch            = std::stoi(argv[3]);
            double fstart     = std::stod(argv[4]);
            double fend       = std::stod(argv[5]);
            double powerDbm   = std::stod(argv[6]);
            double phaseDeg   = std::stod(argv[7]);
            double tramp      = std::stod(argv[8]);
            double fstep      = std::stod(argv[9]);
            bool continuous   = parseBool(argv[10]);      // NEW

            double amp = dbmToAmp(powerDbm);

            std::cout << "Ramp (dBm): ch=" << ch
                    << " fstart=" << fstart
                    << " fend="   << fend
                    << " P=" << powerDbm << " dBm"
                    << " -> amp=" << amp
                    << " phase="  << phaseDeg
                    << " tramp="  << tramp
                    << " fstep="  << fstep
                    << " continuous=" << (continuous ? "true" : "false") << "\n";

            if (!continuous) {
                // one-shot ramp
                sendRamp(dds, ch, fstart, fend, amp, phaseDeg, tramp, fstep);
                std::cout << "Ramp (dBm) command sequence finished.\n";
            } 
            else {
                // continuous ramp
                std::cout << "Running ramp (dBm) continuously, press Ctrl-C to stop...\n";
                while (true) {
                    sendRamp(dds, ch, fstart, fend, amp, phaseDeg, tramp, fstep);
                    // optional tiny pause:
                    // std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }

        else {
            std::cerr << "Unknown mode: " << mode << "\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
