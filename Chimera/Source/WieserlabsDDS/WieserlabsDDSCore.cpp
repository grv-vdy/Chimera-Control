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
#include "Scripts/ScriptStream.h"
#include <qdebug.h>
#include <algorithm>
#include <thread>
#include <chrono>

namespace {
	double evaluateFrequencyMHz(wieserlabsDdsChannel& chan, std::vector<parameterType>& params, unsigned variation)
	{
		if (!chan.frequencyExpression.expressionStr.empty()) {
			std::vector<std::string> scopesToTry = {
				GLOBAL_PARAMETER_SCOPE,
				PARENT_PARAMETER_SCOPE,
				DDS_PARAMETER_SCOPE
			};
			for (const auto& param : params) {
				if (std::find(scopesToTry.begin(), scopesToTry.end(), param.parameterScope) == scopesToTry.end()) {
					scopesToTry.push_back(param.parameterScope);
				}
			}

			for (const auto& scope : scopesToTry) {
				try {
					chan.frequencyExpression.assertValid(params, scope);
					return chan.frequencyExpression.evaluate(params, variation);
				}
				catch (...) {
				}
			}

			try {
				return std::stod(chan.frequencyExpression.expressionStr);
			}
			catch (...) {
			}
		}
		return chan.frequency;
	}
}

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
		// strip surrounding quotes
		if (!t.empty() && t.front() == '"') {
			t.erase(0, 1);
		}
		if (!t.empty() && t.back() == '"') {
			t.pop_back();
		}
		return t;
	};
	auto lowerCopy = [](std::string t) {
		std::transform(t.begin(), t.end(), t.begin(), ::tolower);
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
		info.snapshot[chan].frequencyExpression.expressionStr = freqStr;
		if (lowerCopy(freqStr) == "!#empty_string#!" || freqStr.empty()) {
			info.snapshot[chan].frequency = 0.0;
			info.snapshot[chan].frequencyExpression.expressionStr = "0";
		}
		else {
			try {
				// Config stores MHz, system expects MHz (conversion to Hz happens in programSingleTone)
				info.snapshot[chan].frequency = std::stod(freqStr);
			}
			catch (...) {
				// Keep expression text even if it's not numeric.
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
	std::string lowerToken = lowerCopy(nextToken);

	// Optional control flag line.
	if (lowerToken == "0" || lowerToken == "1" || lowerToken == "true" || lowerToken == "false") {
		info.wieserlabsControl = (lowerToken == "1" || lowerToken == "true");
		file >> nextToken;
		lowerToken = lowerCopy(nextToken);
	}
 
	
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
	experimentActive = waveformLoaded || expRunSettings.wieserlabsControl; // Scripted or static-control mode
	if (waveformLoaded) {
		// Scripted runs may require reconnect before next manual tone programming.
		needsReinitializeAfterScript = true;
	}
	lastProgrammedScriptVariation = INVALID_SCRIPT_VARIATION;

	// Static mode behavior: with control OFF and no script waveform, apply steady-state once at run start.
	if (!waveformLoaded && !expRunSettings.wieserlabsControl && !initSettings.safemode) {
		if (!isConnected) {
			reconnect();
		}
		if (isConnected) {
			for (unsigned chan = 0; chan < steadyStateSettings.size(); chan++) {
				if (!steadyStateSettings[chan].on) {
					continue;
				}
				programSingleTone(chan, steadyStateSettings[chan].frequency,
					steadyStateSettings[chan].amplitude, steadyStateSettings[chan].phase, nullptr, false);
				currentSettings[chan] = steadyStateSettings[chan];
			}
		}
	}

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

	if (!expRunSettings.wieserlabsControl || waveformLoaded) {
		return;
	}

	size_t totalVariations = (params.size() == 0) ? 1 : params.front().keyValues.size();
	try {
		for (unsigned chan = 0; chan < steadyStateSettings.size(); chan++) {
			if (!steadyStateSettings[chan].on) {
				continue;
			}
			auto& expr = steadyStateSettings[chan].frequencyExpression;
			if (expr.expressionStr.empty()) {
				continue;
			}

			std::vector<std::string> scopesToTry = {
				GLOBAL_PARAMETER_SCOPE,
				PARENT_PARAMETER_SCOPE,
				DDS_PARAMETER_SCOPE
			};
			for (const auto& param : params) {
				if (std::find(scopesToTry.begin(), scopesToTry.end(), param.parameterScope) == scopesToTry.end()) {
					scopesToTry.push_back(param.parameterScope);
				}
			}

			bool validated = false;
			for (const auto& scope : scopesToTry) {
				try {
					expr.assertValid(params, scope);
					expr.internalEvaluate(params, static_cast<unsigned>(totalVariations));
					validated = true;
					break;
				}
				catch (...) {
				}
			}
			if (!validated) {
				thrower("Failed to validate expression \"" + expr.expressionStr + "\" in any known scope.");
			}

			if (expr.varies() && initSettings.safemode) {
				thrower("Error varying Wieserlabs DDS frequency in safemode for channel " + str(chan));
			}

			for (unsigned variation = 0; variation < totalVariations; variation++) {
				(void)expr.getValue(variation);
			}
		}
	}
	catch (ChimeraError&) {
		throwNested("Failed to evaluate Wieserlabs DDS frequency expression variations.");
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
	expRunSettings = newSettings;
	if (!newSettings.snapshot.empty()) {
		steadyStateSettings = newSettings.snapshot;
	}
	experimentActive = waveformLoaded || expRunSettings.wieserlabsControl;
}

void WieserlabsDDSCore::setScriptModeEnabled(bool enabled)
{
	Q_UNUSED(enabled);
}

void WieserlabsDDSCore::setWieserlabsDDS(unsigned variation, std::vector<parameterType>& params, deviceOutputInfo runSettings, ExpThreadWorker* expWorker)
{
	// Program the DDS for this variation
	if (!isConnected) return;

	auto& channels = runSettings.snapshot;
	for (unsigned chan = 0; chan < channels.size(); chan++) {
		if (channels[chan].on) {
			double freqMHz = evaluateFrequencyMHz(channels[chan], params, variation);
			programSingleTone(chan, freqMHz, channels[chan].amplitude, channels[chan].phase, nullptr, false);
		}
		currentSettings[chan] = channels[chan];
	}
}

void WieserlabsDDSCore::programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
	static unsigned programCallCount = 0;
	programCallCount++;
	qDebug() << "=== programVariation CALL #" << programCallCount << "===";
	qDebug() << "  variation =" << variation << ", lastProgrammed =" << lastProgrammedScriptVariation;

	if (hasScriptedWaveform()) {
		if (variation != lastProgrammedScriptVariation) {
			qDebug() << "  RE-PARSING script for variation" << variation;
			ScriptedWieserlabsDDSWaveform variationWaveform;
			std::string warnings;
			ScriptStream stream(activeWaveform.getScriptText());
			while (stream.peek() != EOF) {
				if (!variationWaveform.analyzeWieserlabsDDSScriptCommand(stream, params, warnings, variation)) {
					break;
				}
			}
			if (!warnings.empty()) {
				qDebug() << "Wieserlabs DDS variation" << variation << "script warnings:\n"
					<< QString::fromStdString(warnings);
			}
			activeWaveform = variationWaveform;
			lastProgrammedScriptVariation = variation;
		}
		else {
			qDebug() << "  REUSING cached waveform (same variation)";
		}
		qDebug() << "  Executing scripted commands, command count =" << activeWaveform.getCommandList().size();
		executeScriptedCommands(activeWaveform, threadworker);
		return;
	}

	if (!expRunSettings.wieserlabsControl || !isConnected) {
		return;
	}

	for (unsigned chan = 0; chan < steadyStateSettings.size(); chan++) {
		if (!steadyStateSettings[chan].on) {
			continue;
		}
		double freqMHz = evaluateFrequencyMHz(steadyStateSettings[chan], params, variation);
		programSingleTone(chan, freqMHz, steadyStateSettings[chan].amplitude, steadyStateSettings[chan].phase, nullptr, false);
		currentSettings[chan] = steadyStateSettings[chan];
		currentSettings[chan].frequency = freqMHz;
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
	if (needsReinitializeAfterScript && !initSettings.safemode) {
		qDebug() << "WieserlabsDDSCore::programSingleTone - reinitializing DDS connection after scripted mode";
		reconnect();
		needsReinitializeAfterScript = false;
	}

	if ((!isConnected || !ddsClient) && !initSettings.safemode) {
		reconnect();
	}

	if (!isConnected || !ddsClient) {
		qDebug() << "WieserlabsDDSCore::programSingleTone - Not connected or no client";
		return;
	}
	
	qDebug() << "WieserlabsDDSCore::programSingleTone - ch" << channel << "freq" << freq << "MHz, amp" << amp 
		<< "phase" << phase << "waitTrig" << waitForTrigger;
	
	try {
		bool sent = ddsClient->singleToneNow(0, channel, freq * 1e6, amp, phase, waitForTrigger);
		qDebug() << "WieserlabsDDSCore::programSingleTone first-send result ch" << channel << ":" << sent;
		if (!sent) {
			reconnect();
			if (isConnected && ddsClient) {
				sent = ddsClient->singleToneNow(0, channel, freq * 1e6, amp, phase, waitForTrigger);
				qDebug() << "WieserlabsDDSCore::programSingleTone retry result ch" << channel << ":" << sent;
			}
		}
		if (sent) {
			qDebug() << "WieserlabsDDSCore::programSingleTone - Command sent successfully";
		}
		else {
			qDebug() << "WieserlabsDDSCore::programSingleTone - Command failed after reconnect retry";
		}
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
	// Reconnect before every scripted batch to discard any unread response left
	// in the TCP receive buffer from the previous rep. Without this, unread responses
	// accumulate each rep until the TCP receive buffer fills (~6-7 reps), the DDS
	// blocks on its write, and the connection deadlocks.
	reconnect();
	if (!isConnected || !ddsClient) {
		return;
	}

	// Any scripted run can leave mode/wait state that affects subsequent static commands.
	// Force one reconnect before next static/manual tone programming.
	needsReinitializeAfterScript = true;
	
	const auto& commands = waveform.getCommandList();
	
	// Build all DCP commands into one batch
	std::string batchCommands;
	bool channelUsed[2] = { false, false };
	for (const auto& cmd : commands) {
		if (cmd.channel >= 0 && cmd.channel < 2) {
			channelUsed[cmd.channel] = true;
		}
	}
	const bool dualChannelSync = channelUsed[0] && channelUsed[1];
	if (dualChannelSync) {
		for (int ch = 0; ch < 2; ++ch) {
			batchCommands += "dcp " + std::to_string(ch) + " spi:CFR1=0x00402000\n";
		}
		for (int ch = 0; ch < 2; ++ch) {
			batchCommands += "dcp " + std::to_string(ch) + " wait::BNC_IN_A_RISING\n";
		}
		for (int ch = 0; ch < 2; ++ch) {
			batchCommands += "dcp " + std::to_string(ch) + " update:u\n";
		}
	}
	else {
		for (int ch = 0; ch < 2; ++ch) {
			if (!channelUsed[ch]) {
				continue;
			}
			std::string triggerLine = "BNC_IN_A_RISING";
			batchCommands += "dcp " + std::to_string(ch) + " wait::" + triggerLine + "\n";
		}
	}
	auto appendChannelWaitUs = [&](int channel, int waitUs) {
		if (waitUs <= 0) {
			return;
		}
		batchCommands += "dcp " + std::to_string(channel) + " wait:" + std::to_string(waitUs) + ":\n";
	};
	auto appendToneUpdate = [&](int channel, double freqMhz, double amplitude, double phaseDeg) {
		double frequency = freqMhz * 1e6; // Convert MHz to Hz
		batchCommands += "dcp " + std::to_string(channel) + " spi:CFR1=0x402000\n";
		batchCommands += "dcp " + std::to_string(channel) + " spi:CFR2=0x1000080\n";

		unsigned long long freq_val = static_cast<unsigned long long>(std::round((1ULL << 32) / 1e9 * frequency)) & 0xFFFFFFFFULL;
		int amp_val = static_cast<int>(std::round(std::max(0.0, std::min(16383.0, 16383.0 * amplitude))));
		double phase_norm = std::fmod(phaseDeg, 360.0);
		if (phase_norm < 0) phase_norm += 360.0;
		int phase_val = static_cast<int>(std::round((1 << 16) * phase_norm / 360.0));

		char stp0_buf[32];
		sprintf(stp0_buf, "0x%04x%04x%08llx", amp_val, phase_val, freq_val);

		batchCommands += "dcp " + std::to_string(channel) + " spi:stp0=" + std::string(stp0_buf) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " update:u\n";
	};
	// Setup for phase-continuous mode: CFR1 with autoclear DISABLED (bit 13 = 0)
	// Phase offset = 0 for phase-continuous operation (accumulator provides phase)
	auto appendPhaseContinuousSetup = [&](int channel, double freqMhz, double amplitude) {
		double frequency = freqMhz * 1e6;
		// CFR1=0x400000: bit 13 = 0 means phase accumulator is NOT cleared on I/O update
		batchCommands += "dcp " + std::to_string(channel) + " spi:CFR1=0x400000\n";
		batchCommands += "dcp " + std::to_string(channel) + " spi:CFR2=0x1000080\n";

		unsigned long long freq_val = static_cast<unsigned long long>(std::round((1ULL << 32) / 1e9 * frequency)) & 0xFFFFFFFFULL;
		int amp_val = static_cast<int>(std::round(std::max(0.0, std::min(16383.0, 16383.0 * amplitude))));
		int phase_val = 0; // Phase offset = 0 for phase-continuous ramps

		char stp0_buf[32];
		sprintf(stp0_buf, "0x%04x%04x%08llx", amp_val, phase_val, freq_val);
		batchCommands += "dcp " + std::to_string(channel) + " spi:stp0=" + std::string(stp0_buf) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " update:u\n";
	};
	// Phase-continuous frequency update: ONLY touches stp0, no CFR1/CFR2 changes
	// This avoids any glitches from reconfiguring the DDS during ramp
	auto appendFreqOnlyUpdate = [&](int channel, double freqMhz, double amplitude) {
		double frequency = freqMhz * 1e6;
		unsigned long long freq_val = static_cast<unsigned long long>(std::round((1ULL << 32) / 1e9 * frequency)) & 0xFFFFFFFFULL;
		int amp_val = static_cast<int>(std::round(std::max(0.0, std::min(16383.0, 16383.0 * amplitude))));
		int phase_val = 0; // Keep phase offset at 0 - accumulator provides the phase

		char stp0_buf[32];
		sprintf(stp0_buf, "0x%04x%04x%08llx", amp_val, phase_val, freq_val);
		batchCommands += "dcp " + std::to_string(channel) + " spi:stp0=" + std::string(stp0_buf) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " update:u\n";
	};
	auto appendFmGainOnly = [&](int channel, int gainBits) {
		int gain = std::max(0, std::min(15, gainBits));
		// Base CFR2 used by this integration, with parallel dataport enabled (bit 4)
		// and frequency gain bits [0..3] set to requested value.
		unsigned int cfr2 = 0x01000080;
		cfr2 |= 0x10;
		cfr2 = (cfr2 & ~0xFu) | static_cast<unsigned int>(gain);
		char cfr2Buf[16];
		sprintf(cfr2Buf, "0x%08x", cfr2);
		batchCommands += "dcp " + std::to_string(channel) + " spi:CFR2=" + std::string(cfr2Buf) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " update:u\n";
	};
	auto appendFmEnable = [&](int channel, double f0Mhz, double fPlusMhz, double fMinusMhz, int gainOverride) {
		auto ftwHz = [](double hz) -> unsigned int {
			unsigned long long val = static_cast<unsigned long long>(std::round((1ULL << 32) / 1e9 * hz)) & 0xFFFFFFFFULL;
			return static_cast<unsigned int>(val);
		};
		auto signedHex = [](int value) -> std::string {
			char buf[32];
			if (value < 0) {
				sprintf(buf, "-0x%x", -value);
			}
			else {
				sprintf(buf, "0x%x", value);
			}
			return std::string(buf);
		};

		double f0Hz = f0Mhz * 1e6;
		double fPlusHz = fPlusMhz * 1e6;
		double fMinusHz = fMinusMhz * 1e6;

		unsigned int w0 = ftwHz(f0Hz);
		unsigned int wPlus = ftwHz(fPlusHz);
		unsigned int wMinus = ftwHz(fMinusHz);

		int gain = gainOverride;
		if (gain < 0) {
			unsigned int wMax = std::max(w0, std::max(wPlus, wMinus));
			if (wMax == 0) {
				gain = 0;
			}
			else {
				double lg = std::log2(static_cast<double>(wMax));
				gain = static_cast<int>(std::ceil(lg) - 16.0);
			}
		}
		gain = std::max(0, std::min(15, gain));

		double out0 = static_cast<double>(w0 >> gain);
		double outPlus = static_cast<double>(wPlus >> gain);
		double outMinus = static_cast<double>(wMinus >> gain);

		// Match python VoltageToOutputMap convention: +1V -> 32767, -1V -> -32768.
		const double vPlus = 32767.0;
		const double vMinus = -32768.0;
		double slopeFromPlus = (outPlus - out0) * (4096.0 / vPlus);
		double slopeFromMinus = (outMinus - out0) * (4096.0 / vMinus);
		double slope = 0.5 * (slopeFromPlus + slopeFromMinus);

		int s0 = 0;
		int s1 = 0;
		if (channel == 0) {
			s0 = static_cast<int>(std::round(slope));
		}
		else {
			s1 = static_cast<int>(std::round(slope));
		}
		int offset = static_cast<int>(std::round(out0));

		batchCommands += "dcp " + std::to_string(channel) + " wr:AM_S0=" + signedHex(s0) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " wr:AM_S1=" + signedHex(s1) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " wr:AM_O0=0x0\n";
		batchCommands += "dcp " + std::to_string(channel) + " wr:AM_O1=0x0\n";
		batchCommands += "dcp " + std::to_string(channel) + " wr:AM_O=" + signedHex(offset) + "\n";
		batchCommands += "dcp " + std::to_string(channel) + " wr:AM_CFG=0x20000002\n";
		appendFmGainOnly(channel, gain);
	};
	auto appendFmDisable = [&](int channel) {
		// Disable parallel dataport path to stop external analog FM.
		batchCommands += "dcp " + std::to_string(channel) + " spi:CFR2=0x01000080\n";
		batchCommands += "dcp " + std::to_string(channel) + " update:u\n";
	};
	
	for (size_t i = 0; i < commands.size(); i++) {
		const auto& cmd = commands[i];
		if (cmd.channel < 0 || cmd.channel > 1) {
			continue;
		}

		if (cmd.preDelayMs > 0.0) {
			int preDelayUs = static_cast<int>(std::round(cmd.preDelayMs * 1000.0));
			appendChannelWaitUs(cmd.channel, preDelayUs);
		}
		
		if (cmd.type == "tone") {
			appendToneUpdate(cmd.channel, cmd.startFreq, cmd.amplitude, cmd.phase);
			
			// Add delay if specified in script (convert ms to us for DCP wait command)
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(cmd.channel, delayUs);
			}
		}
		else if (cmd.type == "ramp") {
			// Phase-continuous point-by-point ramp: program each frequency step
			// Uses CFR1 with bit 13 cleared so phase accumulator is NOT reset between updates
			double fstartMHz = cmd.startFreq;
			double fendMHz = cmd.endFreq;
			double durationMs = std::max(0.0, cmd.duration);

			if (std::abs(fendMHz - fstartMHz) < 1e-9 || durationMs < 0.001) {
				// No ramp needed, just set final frequency
				appendToneUpdate(cmd.channel, fendMHz, cmd.amplitude, cmd.phase);
			}
			else {
	
				const double stepTimeMs = 0.01; // change
				int numSteps = static_cast<int>(std::ceil(durationMs / stepTimeMs));
				numSteps = std::max(2, std::min(numSteps, 10000)); // Limit to reasonable range

				double actualStepTimeMs = durationMs / static_cast<double>(numSteps);
				int stepTimeUs = static_cast<int>(std::round(actualStepTimeMs * 1000.0));
				stepTimeUs = std::max(1, stepTimeUs);

				double freqStepMHz = (fendMHz - fstartMHz) / static_cast<double>(numSteps);

				qDebug() << "  RAMP (phase-continuous) ch" << cmd.channel 
					<< ": fstart=" << fstartMHz << "MHz, fend=" << fendMHz << "MHz"
					<< ", duration=" << durationMs << "ms, steps=" << numSteps
					<< ", stepTime=" << stepTimeUs << "us";

				// First point: setup phase-continuous mode (CFR1 without autoclear)
				// Phase offset = 0 throughout for true phase continuity
				appendPhaseContinuousSetup(cmd.channel, fstartMHz, cmd.amplitude);
				appendChannelWaitUs(cmd.channel, stepTimeUs);

				// Subsequent points: ONLY update stp0 - don't touch CFR1/CFR2
				// This keeps phase accumulator running continuously
				for (int step = 1; step <= numSteps; step++) {
					double currentFreqMHz = fstartMHz + freqStepMHz * static_cast<double>(step);
					appendFreqOnlyUpdate(cmd.channel, currentFreqMHz, cmd.amplitude);
					
					// Add wait between steps (except after the last one)
					if (step < numSteps) {
						appendChannelWaitUs(cmd.channel, stepTimeUs);
					}
				}
			}

			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(cmd.channel, delayUs);
			}
		}
		else if (cmd.type == "off") {
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:CFR1=0x402000\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:CFR2=0x1000080\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " spi:stp0=0x000000000000\n";
			batchCommands += "dcp " + std::to_string(cmd.channel) + " update:u\n";
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(cmd.channel, delayUs);
			}
		}
		else if (cmd.type == "bnc") {
			// BNC C output control via CFG_BNC_C register (0x082)
			// bncValue 0 = LOW (DIR=1, INV=0): 0x200
			// bncValue 1 = HIGH (DIR=1, INV=1): 0x300
			// Note: Only DCP channel 0 can write to BNC config registers
			uint32_t bncRegValue = (cmd.bncValue == 0) ? 0x200 : 0x300;
			char bncBuf[16];
			sprintf(bncBuf, "0x%03x", bncRegValue);
			batchCommands += "dcp 0 wr:0x082=" + std::string(bncBuf) + "\n";
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(0, delayUs);
			}
		}
		else if (cmd.type == "fmenable") {
			appendFmEnable(cmd.channel, cmd.startFreq, cmd.endFreq, cmd.amplitude, cmd.fmGain);
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(cmd.channel, delayUs);
			}
		}
		else if (cmd.type == "fmdisable") {
			appendFmDisable(cmd.channel);
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(cmd.channel, delayUs);
			}
		}
		else if (cmd.type == "fmgain") {
			appendFmGainOnly(cmd.channel, cmd.fmGain);
			if (cmd.delayMs > 0.0) {
				int delayUs = static_cast<int>(std::round(cmd.delayMs * 1000.0));
				appendChannelWaitUs(cmd.channel, delayUs);
			}
		}
	}
	
	// Send all commands as one batch; reconnect and retry once on failure.
	if (!batchCommands.empty()) {
		try {
			bool sent = ddsClient->sendBatchCommands(batchCommands);
			if (!sent) {
				reconnect();
				if (isConnected && ddsClient) {
					(void)ddsClient->sendBatchCommands(batchCommands);
				}
			}
		}
		catch (const std::exception&) {
			reconnect();
			if (isConnected && ddsClient) {
				(void)ddsClient->sendBatchCommands(batchCommands);
			}
		}
	}
}

void WieserlabsDDSCore::normalFinish()
{
	qDebug() << "normalFinish: Called";
	if ((!isConnected || !ddsClient) && !initSettings.safemode) {
		reconnect();
	}
	if (!isConnected || !ddsClient) {
		qDebug() << "normalFinish: Not connected or no client, returning";
		return;
	}
	
	// Return DDS to steady-state values from configuration
	for (unsigned chan = 0; chan < steadyStateSettings.size(); chan++) {
		if (steadyStateSettings[chan].on) {
			double freqMHz = steadyStateSettings[chan].frequency;
			if (!steadyStateSettings[chan].frequencyExpression.expressionStr.empty()) {
				try {
					freqMHz = std::stod(steadyStateSettings[chan].frequencyExpression.expressionStr);
				}
				catch (...) {
				}
			}
			programSingleTone(chan, freqMHz,
				steadyStateSettings[chan].amplitude, steadyStateSettings[chan].phase, nullptr, false);
			steadyStateSettings[chan].frequency = freqMHz;
		}
		currentSettings[chan] = steadyStateSettings[chan];
	}
	qDebug() << "Returned Wieserlabs DDS to steady-state values";
}

void WieserlabsDDSCore::errorFinish()
{
	bool needReconnect = false;
	if (isConnected && ddsClient) {
		for (int ch = 0; ch < 2; ++ch) {
			try {
				if (!ddsClient->abortChannel(ch)) {
					needReconnect = true;
				}
			}
			catch (...) {
				needReconnect = true;
			}
		}

		try {
			resetChannels();
			std::string cleanup;
			for (int ch = 0; ch < 2; ++ch) {
				cleanup += "dcp " + std::to_string(ch) + " spi:CFR1=0x402000\n";
				cleanup += "dcp " + std::to_string(ch) + " spi:CFR2=0x1000080\n";
				cleanup += "dcp " + std::to_string(ch) + " update:u\n";
			}
			if (!ddsClient->sendBatchCommands(cleanup)) {
				needReconnect = true;
			}
		}
		catch (...) {
			needReconnect = true;
		}
	}

	if (!initSettings.safemode) {
		reconnect();
	}
	needsReinitializeAfterScript = true;
	
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
		steadyStateSettings[channel].frequencyExpression.expressionStr = str(freq);
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
	lastProgrammedScriptVariation = INVALID_SCRIPT_VARIATION;
	experimentActive = waveformLoaded;
	qDebug() << "WieserlabsDDSCore: Waveform set with" << activeWaveform.getCommandList().size() 
		<< "commands, waveformLoaded =" << waveformLoaded;
}

void WieserlabsDDSCore::resetChannels()
{
	if ((!isConnected || !ddsClient) && !initSettings.safemode) {
		reconnect();
	}
	if (!isConnected || !ddsClient) {
		return;
	}

	bool resetOk = false;
	try {
		resetOk = ddsClient->sendBatchCommands("dds reset\n");
	}
	catch (...) {
		resetOk = false;
	}

	if (!resetOk && !initSettings.safemode) {
		reconnect();
		if (isConnected && ddsClient) {
			try {
				(void)ddsClient->sendBatchCommands("dds reset\n");
			}
			catch (...) {
			}
		}
	}

	needsReinitializeAfterScript = false;
}