````markdown
# Wieserlabs DDS scripting

This DDS block understands a **very small command set** parsed by `ScriptedWieserlabsDDSWaveform`.
Commands are free-form text (one command per line) and are **case-insensitive**.
Frequencies below are in **Hz**, amplitudes are **0–1**, phases in **degrees**.
Optional delays are specified as `delay:<seconds>` and are converted to milliseconds.

## Commands

### tone
```
tone <channel> <frequency_hz> <amplitude_0-1> [phase_deg] [delay:<seconds>]
```
- Programs a single tone immediately after the optional delay.
- Example: `tone 0 80e6 0.6 45 delay:0.002` (Ch0, 80 MHz, 60% amp, 45°, start after 2 ms).

### ramp
```
ramp <channel> <start_freq_hz> <end_freq_hz> <amplitude_0-1> <duration_s> [phase_deg] [delay:<seconds>]
```
- Linear frequency sweep from start to end over the duration after the optional delay.
- Example: `ramp 1 70e6 90e6 0.5 0.010 0 delay:0.001` (Ch1, 70→90 MHz in 10 ms, 50% amp, 0°, start after 1 ms).

### off
```
off <channel> [delay:<seconds>]
```
- Turns a channel off after the optional delay.
- Example: `off 0 delay:0.005` (turn off Ch0 five milliseconds later).

## Sample Script
Here's a complete example script that could be used in the DDS GUI:

```
# Channel 0: Frequency sweep then off
tone 0 80e6 0.6 45
ramp 0 80e6 100e6 0.6 0.010
tone 0 90e6 0.3 delay:0.005
off 0 delay:0.002

# Channel 1: Simultaneous sequence
tone 1 70e6 0.5 0
ramp 1 70e6 85e6 0.5 0.010
off 1 delay:0.003
```

This script:
- **Channel 0:** Starts 80 MHz tone at 60% amp, sweeps to 100 MHz over 10 ms, drops to 90 MHz at 30% amp after 5 ms, turns off after 2 ms more
- **Channel 1:** Starts 70 MHz tone at 50% amp, sweeps to 85 MHz over 10 ms, turns off after 3 ms
- Both channels execute **simultaneously** when triggered on **BNC_IN_A**

## Trigger Configuration
**Hardware-triggered via FlexDDS-NG external inputs** (not software-driven):
- **Channel 0:** `(1, 0)` → **BNC_IN_A** on DDS hardware  
- **Channel 1:** `(1, 0)` → **BNC_IN_A** on DDS hardware

Each channel responds independently to TTL rising edges. Pre-load waveforms via TCP/IP, then:
```
dcp <channel> wait::BNC_IN_A_RISING   # for Channel 0
dcp <channel> wait::BNC_IN_A_RISING   # for Channel 1
```
The hardware waits for the physical TTL edge and executes the pre-loaded sequence autonomously with no software latency.

## Implementation Status
- **Parsing:** ✓ Scripts parse into `DdsCommand` lists (tone, ramp, off with per-command delays).
- **Scripting mode:** ✓ Set UI buttons work; use Script toggle in GUI to enable scripting.
- **Hardware trigger:** ✓ **HARDWARE-DRIVEN** — Both channels wait for physical TTL on BNC_IN_A.
- **Pre-load via software:** ✓ `WieserlabsDDSCore` sends tone/ramp/off commands over TCP/IP; use `dcp <ch> wait::BNC_IN_*_RISING` to arm waiting.

## How to Use (Hardware Trigger Mode)
1. In the Wieserlabs DDS GUI: Select **Script** mode and write commands (tone, ramp, off).
2. At experiment startup via TCP/IP, pre-load waveforms using standard commands.
3. Issue hardware trigger arm command: `dcp <channel> wait::BNC_IN_A_RISING` for both channels.
4. Wire TTL lines to the DDS:
   - Channel 0 script: TTL line `(1, 0)` → DDS **BNC_IN_A**
   - Channel 1 script: TTL line `(1, 0)` → DDS **BNC_IN_A**
5. During experiment: Physical TTL edges trigger hardware to execute pre-loaded waveforms with **no software latency**.

````
