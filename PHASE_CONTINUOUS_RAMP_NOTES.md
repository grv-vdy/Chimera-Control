# Phase-Continuous Frequency Ramps in Wieserlabs DDS

## Overview

Implemented point-by-point frequency ramps that maintain phase continuity throughout the sweep. This avoids the phase jumps that occur when reconfiguring the DDS on each step.

## Key Concept: CFR1 Bit 13 (Autoclear Phase Accumulator)

The AD9910 DDS has a control bit that determines what happens to the phase accumulator on I/O update:

| CFR1 Value | Bit 13 | Behavior |
|------------|--------|----------|
| `0x402000` | SET    | Phase accumulator **cleared** on update → phase jump |
| `0x400000` | CLEAR  | Phase accumulator **continues** → phase continuous |

## Implementation

### Helper Functions in `WieserlabsDDSCore.cpp`

```cpp
// 1. Regular tone (clears phase - use for single tones)
appendToneUpdate(channel, freqMHz, amplitude, phaseDeg)
  → CFR1=0x402000, sets phase offset from phaseDeg

// 2. Phase-continuous setup (first ramp point)
appendPhaseContinuousSetup(channel, freqMHz, amplitude)
  → CFR1=0x400000, phase_offset=0

// 3. Frequency-only update (subsequent ramp points)
appendFreqOnlyUpdate(channel, freqMHz, amplitude)
  → Only writes stp0, no CFR1/CFR2 changes, phase_offset=0
```

### Ramp Execution Sequence

```
Step 0: appendPhaseContinuousSetup()
        - Write CFR1=0x400000 (no autoclear)
        - Write CFR2=0x1000080  
        - Write stp0 with freq, amp, phase_offset=0
        - update:u
        - wait

Step 1-N: appendFreqOnlyUpdate()
        - Write stp0 ONLY (new freq, same amp, phase_offset=0)
        - update:u
        - wait (except last step)
```

## Why This Works

1. **CFR1 only written once** - No configuration changes mid-ramp
2. **Phase offset constant (0)** - The stp0 phase field stays at 0 throughout
3. **Phase accumulator continuous** - Bit 13 = 0 means accumulator keeps running
4. **Minimal register writes** - Only stp0 updated after first step

## Ramp Parameters

- **Step time**: 1 ms per frequency step
- **80 ms ramp example**: 80 steps, each 1 ms apart
- **Steps limited**: 2 to 10,000

## Script Syntax

```
ramp channel start_freq end_freq amplitude duration [phase] [delay:X]
```

Example:
```
ramp 0 100 150 0.5 80
// Sweeps channel 0 from 100 MHz to 150 MHz
// Amplitude 0.5, duration 80 ms
// Phase-continuous throughout
```

## Important Notes

- **Starting phase is not controllable** in phase-continuous mode - it depends on the accumulator state when the ramp begins
- The `phase` parameter in ramp commands is ignored for phase-continuous operation
- For ramps where starting phase matters, use a `tone` command first to set the phase, then start the ramp

## Files Modified

- `Chimera/Source/WieserlabsDDS/WieserlabsDDSCore.cpp`
  - Added `appendPhaseContinuousSetup()` helper
  - Added `appendFreqOnlyUpdate()` helper  
  - Modified ramp execution to use phase-continuous updates
