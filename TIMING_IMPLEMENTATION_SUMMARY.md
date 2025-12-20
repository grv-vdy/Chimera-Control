# Wieserlabs DDS Timing Control - Implementation Summary

## Changes Made

### 1. Data Structure Extension
**File**: `ScriptedWieserlabsDDSWaveform.h`

Added `DdsCommand` struct to store parsed command information with timing:
```cpp
struct DdsCommand {
    std::string type;      // "tone", "ramp", "off"
    double delayMs;        // delay before executing (milliseconds)
    int channel;
    double startFreq;
    double endFreq;
    double amplitude;
    double duration;
    double phase;
};
```

Also added:
- `std::vector<DdsCommand> commandList` member to store all parsed commands
- `getCommandList()` accessor method to retrieve commands with timing info

### 2. Script Parser Enhancement
**File**: `ScriptedWieserlabsDDSWaveform.cpp`

Enhanced `analyzeWieserlabsDDSScriptCommand()` to:
- Parse optional `delay:X` syntax from all command types
- Extract delay value (in seconds) and convert to milliseconds
- Create `DdsCommand` objects with all parameters including timing
- Populate `commandList` vector for execution layer

**Supported Syntax**:
- `tone channel frequency amplitude [phase] [delay:X]`
- `ramp channel start_freq end_freq amplitude duration [phase] [delay:X]`
- `off channel [delay:X]`

Where `delay:X` is optional and X is in seconds (e.g., `delay:0.5` for 500ms delay).

### 3. Execution Layer
**File**: `WieserlabsDDSCore.h`

Added public method:
```cpp
void executeScriptedCommands(const ScriptedWieserlabsDDSWaveform& waveform, ExpThreadWorker* expWorker);
```

**File**: `WieserlabsDDSCore.cpp`

Implemented `executeScriptedCommands()` that:
1. Retrieves command list from waveform object
2. Iterates through each command sequentially
3. Sleeps for specified delay using `std::this_thread::sleep_for()`
4. Executes the command (tone, ramp, or off) via appropriate hardware methods
5. Continues to next command

Also fixed bug in `programRamp()` where it referenced undefined `client` object (corrected to `ddsClient`).

### 4. Documentation
**Files Created**:
- `TIMING_CONTROL_README.md`: Complete user guide with examples
- `WIESERLABS_DDS_TIMED_RAMP_EXAMPLE.ddsScript`: Example script demonstrating timing features

## Execution Model

```
Script Parsing Phase:
  Script text with "delay:X" syntax
  ↓
  ScriptedWieserlabsDDSWaveform parses and stores DdsCommand objects with timing

Execution Phase:
  WieserlabsDDSCore::executeScriptedCommands() called
  ↓
  For each DdsCommand:
    - std::this_thread::sleep_for(delayMs milliseconds)
    - Call programSingleTone() / programRamp() / off
    - Move to next command
```

## Software Timing Characteristics

- **Precision**: ~1-10 milliseconds (system dependent)
- **Latency**: Command execution begins after delay expires
- **Blocking**: Commands execute sequentially in order
- **Thread-Safe**: Uses standard C++ thread utilities

## Example Usage

```
# Immediate 80 MHz tone
tone 0 80 0.5 0

# Ramp starting after 0.5 seconds (100→150 MHz over 1 second)
ramp 1 100 150 0.7 1.0 0 delay:0.5

# Turn off after total 1.5 seconds (0.5 delay + 1.0 ramp)
off 1 delay:1.5
```

## Integration Points

To use timed commands in the experiment workflow:

1. Parse script with `ScriptedWieserlabsDDSWaveform::analyzeWieserlabsDDSScriptCommand()`
2. Call `WieserlabsDDSCore::executeScriptedCommands(waveform, expWorker)` during experiment

The timing control is transparent to the rest of the system - delays are handled internally before hardware commands are issued.

## Future Enhancements

- **Hardware Triggering**: Replace software delays with TTL trigger synchronization
- **Parameter Sweep**: Make delays parametric based on experiment variables
- **Multi-Channel Sync**: Synchronize start times across both channels
- **Event Logging**: Record actual execution times for diagnostics

## Files Modified

1. `ScriptedWieserlabsDDSWaveform.h` - Added DdsCommand struct and getCommandList()
2. `ScriptedWieserlabsDDSWaveform.cpp` - Enhanced parser to extract and store timing
3. `WieserlabsDDSCore.h` - Added executeScriptedCommands() declaration
4. `WieserlabsDDSCore.cpp` - Implemented executeScriptedCommands() with timing logic, fixed programRamp() bug

## Compilation Status

All changes compile successfully. Includes:
- Standard C++ chrono/thread headers for timing
- Boost algorithm for string operations
- Existing WieserlabsClient interface for hardware control
