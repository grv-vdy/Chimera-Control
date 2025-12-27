#define _WINSOCKAPI_
#include "stdafx.h"
#include <WinSock2.h>
#include "WieserlabsDDSCore.h"
#include "ScriptedWieserlabsDDSWaveform.h"
#include "WieserlabsDDSStructures.h"
#include "ConfigurationSystems/ConfigSystem.h"
#include "ExperimentThread/ExpThreadWorker.h"
#include "DataLogging/DataLogger.h"
#include "DigitalOutput/DoCore.h"
#include <qdebug.h>
#include <algorithm>
#include <thread>
#include <chrono>

WieserlabsDDSCore::WieserlabsDDSCore(const wieserlabsDdsSettings& settings)
	: initSettings(settings), configDelim(settings.configurationFileDelimiter)
{
	currentSettings.resize(2); // 2 channels
	experimentActive = false;
	if (!initSettings.safemode) {
		connectToDevice();
	}
}

WieserlabsDDSCore::~WieserlabsDDSCore()
{
	disconnectFromDevice();
}

void WieserlabsDDSCore::initialize()
{
	if (!initSettings.safemode && !isConnected) {
		connectToDevice();
	}
}

bool WieserlabsDDSCore::connected()
{
	return isConnected;
}

void WieserlabsDDSCore::reconnect()
{
	disconnectFromDevice();
	connectToDevice();
}

std::pair<unsigned, unsigned> WieserlabsDDSCore::getTriggerLine()
{
	return initSettings.triggerLineCh0;
}

std::string WieserlabsDDSCore::getDeviceIdentity()
{
	if (initSettings.safemode) {
		return "Wieserlabs DDS (Safemode)";
	}
	return "Wieserlabs DDS at " + initSettings.ipAddress;
}

void WieserlabsDDSCore::logSettings(DataLogger& log, ExpThreadWorker* threadworker)
{
	try {
		if (!isConnected) {
			H5::Group ddsGroup(log.file.createGroup("/WieserlabsDDS:Off"));
			return;
		}
		H5::Group ddsGroup(log.file.createGroup("/WieserlabsDDS"));
		for (unsigned chan = 0; chan < currentSettings.size(); chan++) {
			H5::Group chanGroup(ddsGroup.createGroup("Channel_" + str(chan)));
			log.writeDataSet(currentSettings[chan].on, "On", chanGroup);
			log.writeDataSet(currentSettings[chan].frequency, "Frequency_Hz", chanGroup);
			log.writeDataSet(currentSettings[chan].amplitude, "Amplitude", chanGroup);
			log.writeDataSet(currentSettings[chan].phase, "Phase_Deg", chanGroup);
		}
	}
	catch (H5::Exception err) {
		log.logError(err);
		throwNested("ERROR: Failed to log Wieserlabs DDS parameters in HDF5 file: " + err.getDetailMsg());
	}
}

void WieserlabsDDSCore::convertInputToFinalSettings(unsigned chan, deviceOutputInfo& info,
	std::vector<parameterType>& variables)
{
	convertInputToFinalSettings(chan, info);
}

void WieserlabsDDSCore::convertInputToFinalSettings(unsigned chan, deviceOutputInfo& info)
{
	// For DDS, we don't need complex conversion like ArbGen
	// The settings are already in the correct format
}

std::vector<std::string> WieserlabsDDSCore::getStartupCommands()
{
	return {}; // No startup commands needed
}

void WieserlabsDDSCore::programSetupCommands()
{
	// Nothing to do here
}

std::string WieserlabsDDSCore::getDeviceInfo()
{
	return getDeviceIdentity();
}

void WieserlabsDDSCore::analyzeWieserlabsDDSScript(scriptedWieserlabsDDSInfo& infoObj, std::vector<parameterType>& vars, std::string& warnings)
{
	// Parse the script for DDS commands
	// This will be implemented when we create the scripting system
	infoObj.channels.resize(2); // 2 channels
	infoObj.isScript = true;

	// For now, just set some default values
	for (int i = 0; i < 2; i++) {
		infoObj.channels[i].on = false;
		infoObj.channels[i].frequency = 100e6; // 100 MHz default
		infoObj.channels[i].amplitude = 0.5;   // 50% amplitude
		infoObj.channels[i].phase = 0.0;       // 0 phase
	}
}

deviceOutputInfo WieserlabsDDSCore::getSettingsFromConfig(ConfigStream& file)
{
	deviceOutputInfo info;
	info.snapshot = std::vector<wieserlabsDdsChannel>(2);

	auto normalizeToken = [](std::string t) {
		// make lowercase to match ConfigStream behavior and strip surrounding quotes
		std::transform(t.begin(), t.end(), t.begin(), ::tolower);
		if (!t.empty() && t.front() == '"') {
			t.erase(0, 1);
		}
		if (!t.empty() && t.back() == '"') {
			t.pop_back();
		}
		return t;
	};
	
	std::array<std::string, 2> channelNames = { "channel_1", "channel_2" }; // lower to match ConfigStream lowercasing
	for (int chan = 0; chan < 2; chan++) {
		ConfigSystem::checkDelimiterLine(file, channelNames[chan]);
		
		// Read On - use ConfigStream >> to properly handle comments
		bool onVal;
		file >> onVal;
		info.snapshot[chan].on = onVal;
		
		// Read Frequency - use ConfigStream >> to properly handle comments
		std::string freqStr;
		file >> freqStr;
		freqStr = normalizeToken(freqStr);
		if (freqStr == "!#empty_string#!" || freqStr.empty()) {
			info.snapshot[chan].frequency = 0.0;
		}
		else {
			try {
				// Config stores MHz, system expects MHz (conversion to Hz happens in programSingleTone)
				info.snapshot[chan].frequency = std::stod(freqStr);
			}
			catch (...) {
				info.snapshot[chan].frequency = 0.0;
			}
		}
		
		// Read Amplitude - use ConfigStream >> to properly handle comments
		std::string ampStr;
		file >> ampStr;
		ampStr = normalizeToken(ampStr);
		if (ampStr == "!#empty_string#!" || ampStr.empty()) {
			info.snapshot[chan].amplitude = 0.0;
		}
		else {
			try {
				info.snapshot[chan].amplitude = std::stod(ampStr);
			}
			catch (...) {
				info.snapshot[chan].amplitude = 0.0;
			}
		}
		
		// Read Phase - use ConfigStream >> to properly handle comments
		std::string phaseStr;
		file >> phaseStr;
		phaseStr = normalizeToken(phaseStr);
		if (phaseStr == "!#empty_string#!" || phaseStr.empty()) {
			info.snapshot[chan].phase = 0.0;
		}
		else {
			try {
				info.snapshot[chan].phase = std::stod(phaseStr);
			}
			catch (...) {
				info.snapshot[chan].phase = 0.0;
			}
		}
	}
	
	// Read script address if present (added in later version - may not exist in old configs)
	// Peek at next token to see if it's the END delimiter or a script path
	std::streampos pos = file.tellg();
	std::string nextToken;
	file >> nextToken;
	
	// Check if this is the END delimiter (case-insensitive check)
	std::string lowerToken = nextToken;
	std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), ::tolower);
	
	if (lowerToken == "end_wieserlabs_dds" || lowerToken.find("camera_") == 0 || 
	    lowerToken.find("andor_") == 0 || lowerToken.find("master_") == 0 || nextToken.empty()) {
		// No script address in this config file (old format or empty), restore position
		file.seekg(pos);
		loadedScriptAddress = "";
		qDebug() << "WieserlabsDDSCore: No script address found in config (old format or next section:" << qstr(nextToken) << ")";
	} else {
		// This is the script address, keep it
		loadedScriptAddress = nextToken;
		// Strip quotes if present
		if (!loadedScriptAddress.empty() && loadedScriptAddress.front() == '"') {
			loadedScriptAddress.erase(0, 1);
		}
		if (!loadedScriptAddress.empty() && loadedScriptAddress.back() == '"') {
			loadedScriptAddress.pop_back();
		}
		qDebug() << "WieserlabsDDSCore: Loaded script address:" << QString::fromStdString(loadedScriptAddress);
	}
	
	// Store these as steady-state settings to return to after experiment
	steadyStateSettings = info.snapshot;
	return info;
}

void WieserlabsDDSCore::loadExpSettings(ConfigStream& script)
{
	// DDS is NOT configured in master script - it's only configured in the main config file
	// This function is called by the framework but should do nothing for DDS
	qDebug() << "WieserlabsDDSCore::loadExpSettings called - DDS does not use master script config, skipping";
	experimentActive = waveformLoaded; // Use the waveform that was loaded from the DDS script
	qDebug() << "WieserlabsDDSCore::loadExpSettings - experimentActive =" << experimentActive << ", waveformLoaded =" << waveformLoaded;
}

void WieserlabsDDSCore::calculateVariations(std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
	qDebug() << "WieserlabsDDSCore::calculateVariations called, waveformLoaded =" << waveformLoaded << ", experimentActive =" << experimentActive;
	// The waveform should already be set by the system via setScriptedWaveform
	// This is called from refreshScriptedWaveform when the experiment is prepared
	// experimentActive is set in setScriptedWaveform based on whether we have commands
	if (!experimentActive && !waveformLoaded) {
		qDebug() << "WieserlabsDDSCore: No waveform loaded, DDS will not be active in experiment";
	}
	else {
		qDebug() << "WieserlabsDDSCore: Waveform is loaded and ready for experiment";
	}
}

void WieserlabsDDSCore::checkTriggers(unsigned variationInc, DoCore& ttls, ExpThreadWorker* threadWorker)
{
	const auto triggerCount = ttls.countTriggers(initSettings.triggerLineCh0, variationInc);
	if (triggerCount == 0) {
		return;
	}

	if (!waveformLoaded) {
		if (!warnedMissingWaveform && threadWorker) {
			emit threadWorker->warn(qstr("Wieserlabs DDS received triggers but no script is loaded; skipping DDS actions.\r\n"), 0);
			warnedMissingWaveform = true;
		}
		return;
	}

	for (unsigned trig = 0; trig < triggerCount; ++trig) {
		executeScriptedCommands(activeWaveform, threadWorker);
	}
}

void WieserlabsDDSCore::setRunSettings(deviceOutputInfo newSettings)
{
	// Update current settings
}

void WieserlabsDDSCore::setWieserlabsDDS(unsigned variation, std::vector<parameterType>& params, deviceOutputInfo runSettings, ExpThreadWorker* expWorker)
{
	// Program the DDS for this variation
	if (!isConnected) return;

	auto& channels = runSettings.snapshot;
	for (unsigned chan = 0; chan < channels.size(); chan++) {
		if (channels[chan].on) {
			programSingleTone(chan, channels[chan].frequency, channels[chan].amplitude, channels[chan].phase, nullptr, false);
		}
		currentSettings[chan] = channels[chan];
	}
}

void WieserlabsDDSCore::programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
	if (hasScriptedWaveform()) {
		executeScriptedCommands(activeWaveform, threadworker);
	}
}

void WieserlabsDDSCore::connectToDevice()
{
	try {
		ddsClient = std::make_unique<WieserlabsClient>(initSettings.ipAddress, initSettings.ipPort);
		if (ddsClient->connect()) {
			isConnected = true;
			qDebug() << "Connected to Wieserlabs DDS at" << QString::fromStdString(initSettings.ipAddress);
		} else {
			isConnected = false;
			qDebug() << "Failed to connect to Wieserlabs DDS";
		}
	}
	catch (const std::exception& e) {
		isConnected = false;
		qDebug() << "Failed to connect to Wieserlabs DDS:" << e.what();
	}
}

void WieserlabsDDSCore::disconnectFromDevice()
{
	if (ddsClient) {
		ddsClient.reset();
		isConnected = false;
	}
}

void WieserlabsDDSCore::programSingleTone(unsigned channel, double freq, double amp, double phase, ExpThreadWorker* expWorker, bool waitForTrigger)
{
	if (!isConnected || !ddsClient) {
		qDebug() << "WieserlabsDDSCore::programSingleTone - Not connected or no client";
		return;
	}
	
	qDebug() << "WieserlabsDDSCore::programSingleTone - ch" << channel << "freq" << freq << "MHz, amp" << amp 
		<< "phase" << phase << "waitTrig" << waitForTrigger;
	
	try {
		ddsClient->singleToneNow(0, channel, freq * 1e6, amp, phase, waitForTrigger);
		qDebug() << "WieserlabsDDSCore::programSingleTone - Command sent successfully";
	}
	catch (const std::exception& e) {
		qDebug() << "WieserlabsDDSCore::programSingleTone - Exception:" << e.what();
	}
	catch (...) {
		qDebug() << "WieserlabsDDSCore::programSingleTone - Unknown exception";
	}
}

void WieserlabsDDSCore::executeScriptedCommands(const ScriptedWieserlabsDDSWaveform& waveform, ExpThreadWorker* expWorker)
{
	if (!isConnected) {
		return;
	}
	
	const auto& commands = waveform.getCommandList();
	
	// Build all DCP commands into one batch
	std::string batchCommands;
	bool firstCommand = true;
	
	for (size_t i = 0; i < commands.size(); i++) {
		const auto& cmd = commands[i];
		
		if (cmd.type == "tone") {
			// Build DCP commands for this tone
			double frequency = cmd.startFreq * 1e6; // Convert MHz to Hz
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:CFR1=0x402000\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:CFR2=0x1000080\n";
			
			// Calculate register values
			unsigned long long freq_val = static_cast<unsigned long long>(std::round((1ULL << 32) / 1e9 * frequency)) & 0xFFFFFFFFULL;
			int amp_val = static_cast<int>(std::round(std::max(0.0, std::min(16383.0, 16383.0 * cmd.amplitude))));
			double phase_norm = std::fmod(cmd.phase, 360.0);
			if (phase_norm < 0) phase_norm += 360.0;
			int phase_val = static_cast<int>(std::round((1 << 16) * phase_norm / 360.0));
			
			char stp0_buf[32];
			sprintf(stp0_buf, "0x%04x%04x%08llx", amp_val, phase_val, freq_val);
			
			
			// Add trigger wait for first command only
			if (firstCommand) {
				// Channel 0 uses BNC A, channel 1 uses BNC B
				std::string triggerLine = (cmd.channel == 0) ? "BNC_IN_A_RISING" : "BNC_IN_B_RISING";
				batchCommands += "dcp " + std::to_string(cmd.channel) + " wait::" + triggerLine + "\n";
				firstCommand = false;
			}
			
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:stp0=" + std::string(stp0_buf) + "\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " update:u\n";
			
			// Add delay if specified in script (convert ms to us for DCP wait command)
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(cmd.delayMs * 1000.0);
				batchCommands += "dcp " + std::to_string(cmd.channel) + " wait:" + std::to_string(delayUs) + ":\n";
			}
		}
		else if (cmd.type == "off") {
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:CFR1=0x402000\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:CFR2=0x1000080\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:stp0=0x000000000000\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " update:u\n";
		}
	}
	
	// Send all commands as one batch
	if (!batchCommands.empty()) {
		try {
			ddsClient->sendBatchCommands(batchCommands);
		} catch (const std::exception& e) {
			// Silently handle exception
		}
	}
}

void WieserlabsDDSCore::normalFinish()
{
	qDebug() << "normalFinish: Called";
	if (!isConnected || !ddsClient) {
		qDebug() << "normalFinish: Not connected or no client, returning";
		return;
	}
	
	// Return DDS to steady-state values from configuration
	for (unsigned chan = 0; chan < steadyStateSettings.size(); chan++) {
		if (steadyStateSettings[chan].on) {
			programSingleTone(chan, steadyStateSettings[chan].frequency, 
				steadyStateSettings[chan].amplitude, steadyStateSettings[chan].phase, nullptr, false);
		}
		currentSettings[chan] = steadyStateSettings[chan];
	}
	qDebug() << "Returned Wieserlabs DDS to steady-state values";
}

void WieserlabsDDSCore::errorFinish()
{
	// On abort/error, reset DDS to clear any pending trigger waits
	if (isConnected && ddsClient) {
		try {
			ddsClient->sendBatchCommands("dds reset\n");
			qDebug() << "Sent DDS reset on abort";
		} catch (...) {
			qDebug() << "Failed to send DDS reset";
		}
	}
	
	// Then return to steady state
	normalFinish();
}

void WieserlabsDDSCore::programSingleToneNow(unsigned channel, double freq, double amp, double phase)
{
	programSingleTone(channel, freq, amp, phase, nullptr, false);
}

void WieserlabsDDSCore::updateSteadyState(unsigned channel, double freq, double amp, double phase, bool on)
{
	if (channel < steadyStateSettings.size()) {
		steadyStateSettings[channel].frequency = freq;
		steadyStateSettings[channel].amplitude = amp;
		steadyStateSettings[channel].phase = phase;
		steadyStateSettings[channel].on = on;
		qDebug() << "Updated steady state for channel" << channel << ":" << freq << "MHz," << amp << "amp," << phase << "deg, on=" << on;
	}
}

void WieserlabsDDSCore::setScriptedWaveform(const ScriptedWieserlabsDDSWaveform& waveform)
{
	activeWaveform = waveform;
	waveformLoaded = !activeWaveform.getCommandList().empty();
	warnedMissingWaveform = false;
	experimentActive = waveformLoaded;
	qDebug() << "WieserlabsDDSCore: Waveform set with" << activeWaveform.getCommandList().size() 
		<< "commands, waveformLoaded =" << waveformLoaded;
}

void WieserlabsDDSCore::resetChannels()
{
	if (!isConnected || !ddsClient) return;
	
	// Turn off all channels to clear any previous state
	for (int ch = 0; ch < 2; ch++) {
		try {
			ddsClient->turnOffChannel(ch);
		}
		catch (...) {
			// Ignore errors during reset
		}
	}
}