#pragma once

#include "WieserlabsDDSStructures.h"
#include "ParameterSystem/ParameterSystemStructures.h"
#include "GeneralObjects/IDeviceCore.h"
#include "ConfigurationSystems/ConfigStream.h"
#include "ArbGen/ArbGenStructures.h"
#include "WieserlabsClient.h"
#include <vector>
#include <string>
#include <memory>

class DoCore;
class WieserlabsDDSCore : public IDeviceCore {
public:
	// THIS CLASS IS NOT COPYABLE.
	WieserlabsDDSCore& operator=(const WieserlabsDDSCore&) = delete;
	WieserlabsDDSCore(const WieserlabsDDSCore&) = delete;

	WieserlabsDDSCore(const wieserlabsDdsSettings& settings);
	~WieserlabsDDSCore();
	void initialize();
	std::string getDelim() { return configDelim; }
	bool connected();
	void reconnect();
	std::pair<unsigned, unsigned> getTriggerLine();
	std::string getDeviceIdentity();
	void logSettings(DataLogger& log, ExpThreadWorker* threadworker);
	void convertInputToFinalSettings(unsigned chan, deviceOutputInfo& info,
		std::vector<parameterType>& variables);
	void convertInputToFinalSettings(unsigned chan, deviceOutputInfo& info);

	std::vector<std::string> getStartupCommands();
	void programSetupCommands();
	std::string getDeviceInfo();

	void analyzeWieserlabsDDSScript(scriptedWieserlabsDDSInfo& infoObj, std::vector<parameterType>& vars, std::string& warnings);

	deviceOutputInfo getSettingsFromConfig(ConfigStream& file);
	void loadExpSettings(ConfigStream& script);
	void calculateVariations(std::vector<parameterType>& params, ExpThreadWorker* threadworker);
	void checkTriggers(unsigned variationInc, DoCore& ttls, ExpThreadWorker* threadWorker);
	void normalFinish();
	void errorFinish() {};
	void setRunSettings(deviceOutputInfo newSettings);
	void setWieserlabsDDS(unsigned variation, std::vector<parameterType>& params, deviceOutputInfo runSettings, ExpThreadWorker* expWorker);
	void programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadworker);

	void programSingleToneNow(unsigned channel, double freq, double amp, double phase);
	void executeScriptedCommands(const class ScriptedWieserlabsDDSWaveform& waveform, ExpThreadWorker* expWorker);

private:
	const wieserlabsDdsSettings initSettings;
	std::string configDelim;
	std::unique_ptr<WieserlabsClient> ddsClient;
	bool isConnected = false;

	// Current settings for each channel
	std::vector<wieserlabsDdsChannel> currentSettings;
	
	// Steady-state settings to return to after experiment
	std::vector<wieserlabsDdsChannel> steadyStateSettings;

	void connectToDevice();
	void disconnectFromDevice();
	void programSingleTone(unsigned channel, double freq, double amp, double phase);
	void programRamp(unsigned channel, double startFreq, double endFreq, double amp, double phase, double duration);
};