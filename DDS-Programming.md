# Wieserlabs DDS Programming Guide

This document explains how single‑tone programming and steady‑state programming work in the current Chimera integration, along with trigger/timing behavior and what happens on normal end vs abort. It also outlines quick recovery steps if something fails.

## Overview
- Device: Wieserlabs DDS (2 channels assumed).
- Client code: see [Chimera/Source/WieserlabsDDS/WieserlabsClient.cpp](Chimera/Source/WieserlabsDDS/WieserlabsClient.cpp) and [Chimera/Source/WieserlabsDDS/WieserlabsDDSCore.cpp](Chimera/Source/WieserlabsDDS/WieserlabsDDSCore.cpp).
- Amplitude scaling: GUI amplitude 0.0–1.0 → 14‑bit word 0–16383.
- Frequency scaling: Hz → 32‑bit tuning word using $\text{word} = \left\lfloor \frac{2^{32}}{1\,\text{GHz}} \cdot f_{\text{Hz}} \right\rfloor$.

## Single‑Tone Programming (Immediate)
- Entry point: `WieserlabsDDSCore::programSingleTone()` → `WieserlabsClient::singleToneNow()`.
- Command sequence per channel:
  1. `spi:CFR1=0x402000`
  2. `spi:CFR2=0x1000080`
  3. Compute `stp0 = 0x[AMP][PHASE][FREQ]` where:
     - `AMP = amp_to_word(amplitude)` → `round(16383 * clamp(amp,0..1))` → 4 hex chars
     - `PHASE = phase_to_word(phase_deg)` → `round(2^16 * phase/360)` → 4 hex chars
     - `FREQ = freq_to_word(freq_Hz)` → `round(2^32/1e9 * freq)` → 8 hex chars
  4. `spi:stp0=<value>`
  5. Optional: `wait::<trigger>` if `waitForTrigger=true`
  6. `update:u` (apply update)

- Triggers (optional): If requested, `wait::BNC_IN_A_RISING` for channel 0, `wait::BNC_IN_B_RISING` for channel 1.

## Scripted / Timed Programming
- Entry point: `WieserlabsDDSCore::executeScriptedCommands()`.
- The system builds a batch of DCP commands from a script waveform (e.g., `tone 0 100.000000 0.75 40 delay:10`).

  Field meanings:
  - `tone`: single-tone operation for one channel; writes CFR1/CFR2, sets `stp0` (amplitude, phase, frequency), then applies `update:u`.
  - `0`: DDS channel index. Channel 0 uses `BNC_IN_A_RISING` for the first-command trigger wait; channel 1 uses `BNC_IN_B_RISING`.
  - `100.000000`: frequency in MHz. Converted to Hz, then to the 32-bit tuning word using the $2^{32}/1\,\text{GHz}$ scaling and placed into `stp0`.
  - `0.75`: amplitude (normalized 0–1). Converted to a 14-bit word via `round(16383 × amp)` and placed into `stp0` (full-scale is 16383).
  - `40`: phase in degrees. Converted to a 16-bit word via `round((2^{16}/360) × phase)` and placed into `stp0`.
  - `delay:10`: delay after this tone, in milliseconds. Inserted as a DCP wait of 10,000 microseconds between commands before the next update.

  For each tone:
  - Writes CFR1/CFR2, `stp0` as above.
  - For the first command in the batch, inserts a trigger wait (`BNC_IN_A_RISING` or `BNC_IN_B_RISING`).
  - Issues `update:u` after each `stp0`.
  - If a delay is specified (ms), inserts `wait:<microseconds>:` between commands.
- Off commands: `spi:stp0=0x000000000000` and `update:u` (amplitude 0).

## Steady‑State Programming (End of Sequence)
- On normal finish: `WieserlabsDDSCore::normalFinish()` restores the DDS to steady‑state values loaded from the configuration file.
  - For each channel with `on==true`, it calls `programSingleTone()` using the configured `frequency (MHz)`, `amplitude (0..1)`, and `phase (deg)`.
  - The current settings cache is updated.

## Abort / Error Behavior
- On abort/error: `WieserlabsDDSCore::errorFinish()` first sends a `dds reset` batch to the device, then calls `normalFinish()`.
  - The reset clears any pending `wait::` triggers and queued updates so the DDS is left in a clean state.

## Quick Recovery
If DDS commands fail or the device appears stuck (e.g., waiting on a trigger that never arrives):
- Reset the DDS and restart Chimera:
  1. Quit Chimera.
  2. Power‑cycle or reset the Wieserlabs DDS if needed.
  3. Relaunch Chimera.
- Re‑load your DDS script and re‑run.

## Parameter Scaling
- Amplitude:
  - GUI value `amp` in [0,1] → raw word `round(16383 * amp)`.
  - Example: `amp=1.0` → `16383` (full scale); `amp=0.5` → `8192`.
- Frequency:
  - Frequency in Hz → tuning word `round((2^32 / 1e9) * f_Hz)`.
  - Example: `f=100 MHz` → `round(2^32 * 0.1)`.
- Phase:
  - Degrees → word `round((2^16 / 360) * phase_deg)`.

## Where Things Happen (Code)
- Single‑tone immediate: [Chimera/Source/WieserlabsDDS/WieserlabsDDSCore.cpp](Chimera/Source/WieserlabsDDS/WieserlabsDDSCore.cpp)
  - `programSingleTone()` → sets one channel right away (optionally waits for external trigger).
- Low‑level DCP building: [Chimera/Source/WieserlabsDDS/WieserlabsClient.cpp](Chimera/Source/WieserlabsDDS/WieserlabsClient.cpp)
  - `singleToneNow()`
  - `amp_to_word()`, `freq_to_word()`, `phase_to_word()`
- Scripted batch execution: `WieserlabsDDSCore::executeScriptedCommands()`.
- Normal finish: `WieserlabsDDSCore::normalFinish()` restores steady‑state from config.
- Abort/error: `WieserlabsDDSCore::errorFinish()` sends `dds reset` then calls `normalFinish()`.

## Notes
- Triggers must be present on the expected BNC inputs to advance scripted sequences that include `wait::` statements.
- If using immediate programming (no `waitForTrigger`), updates apply as soon as `update:u` is sent.
