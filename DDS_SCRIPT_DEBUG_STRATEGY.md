# Wieserlabs DDS Script Debugging Strategy

## Issue Found
The `programVariation()` function in WieserlabsDDSCore.cpp was empty - it wasn't calling `executeScriptedCommands()`.

## Fix Applied
Added execution of scripted waveform in `programVariation()`:
```cpp
void WieserlabsDDSCore::programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
    // Execute scripted waveform if present
    if (hasScriptedWaveform()) {
        qDebug() << "Executing Wieserlabs DDS scripted waveform for variation" << variation;
        executeScriptedCommands(activeWaveform, threadworker);
    }
    else {
        qDebug() << "No Wieserlabs DDS scripted waveform loaded";
    }
}
```

## How to Verify the Fix

### 1. Check Script Loading
Add debug output to `WieserlabsDDSSystem::refreshScriptedWaveform()` to verify script is being parsed:
- Check if `getScriptText()` returns content
- Verify `analyzeWieserlabsDDSScriptCommand()` is parsing commands
- Check `commandList` size after parsing

### 2. Check Script Availability in Core
In `WieserlabsDDSCore::setScriptedWaveform()`, add:
```cpp
qDebug() << "Setting scripted waveform with" << waveform.getCommandList().size() << "commands";
```

### 3. Monitor Execution Flow
Watch Qt Debug output for:
- "Executing Wieserlabs DDS scripted waveform for variation X"
- Or "No Wieserlabs DDS scripted waveform loaded"

### 4. Test Sequence
1. Open TEST_WIESERLABS_TONE_CHANGE.ddsScript in GUI
2. Select "Scripting" mode in WieserlabsDDS dropdown
3. Verify script text appears in window
4. Run experiment
5. Check debug console for execution messages
6. Verify DDS hardware changes frequency

## Debugging Points to Add

### A. Script Text Retrieval
In `WieserlabsDDSSystem.cpp`, line ~215 add before parsing:
```cpp
std::string scriptContent = wieserlabsDdsScript->getScriptText();
qDebug() << "DDS Script content length:" << scriptContent.length();
qDebug() << "DDS Script text:" << QString::fromStdString(scriptContent);
```

### B. Command Parsing
In `ScriptedWieserlabsDDSWaveform::analyzeWieserlabsDDSScriptCommand()`, add:
```cpp
qDebug() << "Parsed DDS command:" << QString::fromStdString(command) 
         << "channel:" << cmdObj.channel 
         << "type:" << QString::fromStdString(cmdObj.type);
```

### C. Execution Timing
In `WieserlabsDDSCore::executeScriptedCommands()`, add timing info:
```cpp
qDebug() << "Executing" << commands.size() << "DDS commands";
for (const auto& cmd : commands) {
    qDebug() << "  Command:" << QString::fromStdString(cmd.type)
             << "channel:" << cmd.channel 
             << "delay:" << cmd.delayMs << "ms";
}
```

## Common Issues to Check

1. **Script mode not selected**: Verify dropdown is set to "Scripting" mode
2. **Script not refreshed**: Call `refreshScriptedWaveform()` after loading script
3. **Connection issues**: Verify DDS is connected before programming
4. **Empty command list**: Check if parsing succeeded

## Test Commands
```cpp
// In debugger, check:
activeWaveform.getCommandList().size()  // Should be > 0
activeWaveform.getScriptText()          // Should contain script text
hasScriptedWaveform()                   // Should return true
```
