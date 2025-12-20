# Wieserlabs DDS Timing Control

## Overview

The Wieserlabs DDS module now supports explicit timing control for all commands (tone, ramp, and off). Commands can be executed immediately or delayed by a specified number of seconds.

## Script Syntax

### Tone Command with Optional Delay
```
tone <channel> <frequency_MHz> <amplitude> [phase_degrees] [delay:seconds]
```

**Parameters:**
- `channel`: DDS channel (0 or 1)
- `frequency_MHz`: Output frequency in MHz (e.g., 80, 120.5)
- `amplitude`: Output amplitude as fraction (0.0 to 1.0, e.g., 0.5 = 50%)
- `phase_degrees` (optional): Output phase in degrees (default: 0)
- `delay:seconds` (optional): Delay before execution in seconds (default: 0)

**Examples:**
```
tone 0 80 0.5           # Immediate: 80 MHz on channel 0, 50% amplitude
tone 0 80 0.5 45        # Immediate: 80 MHz with 45° phase
tone 0 80 0.5 45 delay:0.5   # After 0.5s: 80 MHz with 45° phase
```

### Ramp Command with Optional Delay
```
ramp <channel> <start_freq_MHz> <end_freq_MHz> <amplitude> <duration_s> [phase_degrees] [delay:seconds]
```

**Parameters:**
- `channel`: DDS channel (0 or 1)
- `start_freq_MHz`: Starting frequency in MHz
- `end_freq_MHz`: Ending frequency in MHz
- `amplitude`: Output amplitude as fraction (0.0 to 1.0)
- `duration_s`: Ramp duration in seconds
- `phase_degrees` (optional): Output phase in degrees (default: 0)
- `delay:seconds` (optional): Delay before ramp starts in seconds (default: 0)

**Examples:**
```
ramp 0 100 150 0.7 1.0       # Immediate: Ramp 100→150 MHz over 1 second
ramp 0 100 150 0.7 1.0 45    # Immediate ramp with 45° phase
ramp 0 100 150 0.7 1.0 45 delay:0.5   # After 0.5s: Ramp with phase and delay
```

### Off Command with Optional Delay
```
off <channel> [delay:seconds]
```

**Parameters:**
- `channel`: DDS channel (0 or 1)
- `delay:seconds` (optional): Delay before turning off in seconds (default: 0)

**Examples:**
```
off 0           # Immediate: Turn off channel 0
off 0 delay:1.5 # After 1.5s: Turn off channel 0
```

## Execution Model

### Software Timing
Commands are executed sequentially with software-based delays using `std::this_thread::sleep_for()`. This provides millisecond-precision timing for non-critical delays and is sufficient for most experimental sequences.

### Timing Accuracy
- **Precision**: ~1-10 milliseconds depending on system load
- **Latency**: Ramp commands are hardware-controlled once triggered, but the initial trigger has software latency
- **Use Case**: Ideal for experiment synchronization, sample preparation, and sequential RF operations

## Complete Example

```
# Prepare sample with first tone
tone 0 80 0.3 0

# Wait 2 seconds for thermal equilibration
tone 0 80 0.3 0 delay:2.0

# Perform Rabi oscillation ramp starting at t=3s
ramp 0 80 90 0.8 2.0 0 delay:3.0

# Turn off at end of ramp (t=5s)
off 0 delay:5.0
```

**Timeline:**
- t=0s: 80 MHz tone at 30% amplitude
- t=2s: Same tone continues (or new instance)
- t=3s: Ramp starts from 80 MHz → 90 MHz over 2 seconds
- t=5s: Channel turns off (ramp complete + turn-off delay)

## Implementation Details

### Classes Modified
1. **ScriptedWieserlabsDDSWaveform.h**: Added `DdsCommand` struct with `delayMs` field and `getCommandList()` method
2. **ScriptedWieserlabsDDSWaveform.cpp**: Updated parser to extract `delay:X` tokens from commands
3. **WieserlabsDDSCore.h**: Added `executeScriptedCommands()` method
4. **WieserlabsDDSCore.cpp**: Implemented timed execution with `std::this_thread::sleep_for()`

### Data Flow
```
Script Text
    ↓
ScriptedWieserlabsDDSWaveform::analyzeWieserlabsDDSScriptCommand()
    ↓
std::vector<DdsCommand> with timing info
    ↓
WieserlabsDDSCore::executeScriptedCommands()
    ↓
Sleep(delayMs) → programSingleTone() / programRamp() / off
```

## Future Enhancements

- **Hardware Triggering**: Synchronize ramp start with TTL pulse from experiment controller
- **Multi-Channel Synchronization**: Coordinate ramps across both channels
- **Parametric Sequences**: Vary delays based on experiment parameters
- **Phase Continuity**: Ensure phase coherence when chaining tones and ramps

## Troubleshooting

**Timing seems off:**
- Verify system load and CPU usage during experiment
- Increase delays by 10-50ms if precision is critical
- Use hardware triggering for critical timing (<1ms accuracy required)

**Commands execute in wrong order:**
- Ensure delays are specified correctly (in seconds)
- Check for negative values or typos in delay specification
- Review console output for parsing warnings
