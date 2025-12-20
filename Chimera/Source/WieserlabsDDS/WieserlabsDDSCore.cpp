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
	return initSettings.triggerLine;
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
				info.snapshot[chan].frequency = std::stod(freqStr) * 1e6; // Convert MHz to Hz
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
	
	// Store these as steady-state settings to return to after experiment
	steadyStateSettings = info.snapshot;
	return info;
}

void WieserlabsDDSCore::loadExpSettings(ConfigStream& script)
{
	// Load settings from script
}

void WieserlabsDDSCore::calculateVariations(std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
	// Calculate variations for the experiment
}

void WieserlabsDDSCore::checkTriggers(unsigned variationInc, DoCore& ttls, ExpThreadWorker* threadWorker)
{
	// Check trigger consistency
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
			programSingleTone(chan, channels[chan].frequency, channels[chan].amplitude, channels[chan].phase);
		}
		currentSettings[chan] = channels[chan];
	}
}

void WieserlabsDDSCore::programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
	// Program variation
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

void WieserlabsDDSCore::programSingleTone(unsigned channel, double freq, double amp, double phase)
{
	if (!ddsClient) return;
	try {
		ddsClient->singleToneNow(0, channel, freq, amp, phase);
	}
	catch (const std::exception& e) {
		qDebug() << "Failed to program single tone:" << e.what();
	}
}

void WieserlabsDDSCore::programRamp(unsigned channel, double startFreq, double endFreq, double amp, double phase, double duration)
{
	// Program ramp using WieserlabsClient
	if (!ddsClient) return;
	try {
		ddsClient->rampTone(0, channel, startFreq, endFreq, amp, duration, phase);
	}
	catch (const std::exception& e) {
		qDebug() << "Failed to program ramp:" << e.what();
	}
}

	void WieserlabsDDSCore::executeScriptedCommands(const ScriptedWieserlabsDDSWaveform& waveform, ExpThreadWorker* expWorker)
{
	if (!isConnected) return;
	
	const auto& commands = waveform.getCommandList();
	
	for (const auto& cmd : commands) {
		// Wait for the specified delay before executing the command
		if (cmd.delayMs > 0.0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(cmd.delayMs)));
		}
		
		if (cmd.type == "tone") {
			programSingleTone(cmd.channel, cmd.startFreq, cmd.amplitude, cmd.phase);
		}
		else if (cmd.type == "ramp") {
			programRamp(cmd.channel, cmd.startFreq, cmd.endFreq, cmd.amplitude, cmd.phase, cmd.duration);
		}
		else if (cmd.type == "off") {
			// Program zero amplitude to turn off
			programSingleTone(cmd.channel, cmd.startFreq, 0.0, 0.0);
		}
	}
}

void WieserlabsDDSCore::normalFinish()
{
	if (!isConnected) return;
	
	// Return DDS to steady-state values from configuration
	for (unsigned chan = 0; chan < steadyStateSettings.size(); chan++) {
		if (steadyStateSettings[chan].on) {
			programSingleTone(chan, steadyStateSettings[chan].frequency, 
				steadyStateSettings[chan].amplitude, steadyStateSettings[chan].phase);
		}
		currentSettings[chan] = steadyStateSettings[chan];
	}
	qDebug() << "Returned Wieserlabs DDS to steady-state values";
}

void WieserlabsDDSCore::programSingleToneNow(unsigned channel, double freq, double amp, double phase)
{
	programSingleTone(channel, freq, amp, phase);
}