#include "stdafx.h"
#include "ArbGenCore.h"
#include "DigitalOutput/DoCore.h"
#include <ExperimentThread/ExpThreadWorker.h>
#include <ConfigurationSystems/ConfigSystem.h>
#include <DataLogging/DataLogger.h>
#include <algorithm>
//#include <AnalogInput/CalibrationManager.h>

ArbGenCore::ArbGenCore(const arbGenSettings& settings) :
	visaFlume(settings.safemode, settings.address),
	sampleRate(settings.sampleRate),
	initSettings(settings),
	triggerRow(settings.triggerRow),
	triggerNumber(settings.triggerNumber),
	memoryLoc(settings.memoryLocation),
	configDelim(settings.configurationFileDelimiter),
	arbGenName(settings.deviceName),
	setupCommands(settings.setupCommands),
	isConnected(false)
{
	//calibrations[0].includesSqrt = false;
	calibrations[0].calibrationCoefficients = settings.calibrationCoeff;
	//calibrations[0].polynomialOrder = settings.calibrationCoeff.size();
	// could probably set these properly but in general wasn't doing this when was doing manual calibrations.
	calibrations[0].calmax = 1e6;
	calibrations[0].calmin = -1e6;

	//calibrations[1].includesSqrt = false;
	calibrations[1].calibrationCoefficients = settings.calibrationCoeff;
	//calibrations[1].polynomialOrder = settings.calibrationCoeff.size();
	calibrations[1].calmax = 1e6;
	calibrations[1].calmin = -1e6;


	try {
		visaFlume.open();
	}
	catch (ChimeraError& err) {
		errBox("Error seen while opening connection to " + arbGenName + " ArbGen:" + err.trace());
	}
}

ArbGenCore::~ArbGenCore() {
	visaFlume.close();
}

void ArbGenCore::initialize() {
	try {
		deviceInfo = visaFlume.identityQuery();
		isConnected = true;
	}
	catch (ChimeraError&) {
		deviceInfo = "Disconnected";
		isConnected = false;
	}

}

std::pair<unsigned, unsigned> ArbGenCore::getTriggerLine() {
	return { triggerRow, triggerNumber };
}

std::string ArbGenCore::getDeviceInfo() {
	return deviceInfo;
}

std::string ArbGenCore::getDeviceIdentity() {
	std::string msg;
	try {
		msg = visaFlume.identityQuery();
	}
	catch (ChimeraError& err) {
		msg = err.trace();
	}
	if (msg == "") {
		msg = "Disconnected...\n";
	}
	return msg;
}

void ArbGenCore::setArbGen(unsigned var, std::vector<parameterType>& params, deviceOutputInfo runSettings,
	ExpThreadWorker* expWorker) {
	if (!connected()) {
		return;
	}
	if (runSettings.siglentFm.control) {
		programSpecializedVariation(var, params, runSettings, expWorker);
		return;
	}
	//auto notify = expWorker != nullptr // if in expworker, emit notification, else no way to notify currently. 
	//	? std::function{ [expWorker](QString note, unsigned debugLevel) {emit expWorker->notification({note, debugLevel}); } }
	//	: std::function{ [expWorker] (QString note, unsigned debugLevel) { } };
	setSync(runSettings, expWorker);
	for (auto chan : range(unsigned(2))) {
		auto& channel = runSettings.channel[chan];
		auto stdNote = qstr("Programming ArbGen " + arbGenName + " Channel " + str(chan) + " ");
		setPolarity(chan + 1, channel.polarityInvert, expWorker);
		try {
			switch (channel.option) {
			case ArbGenChannelMode::which::No_Control:
				break;
			case ArbGenChannelMode::which::Output_Off:
				notify({ stdNote + "Output off.\n",1 }, expWorker);
				outputOff(chan + 1);
				break;
			case ArbGenChannelMode::which::DC:
				notify({ stdNote + " DC Voltage.\n", 1 }, expWorker);
				setDC(chan + 1, channel.dc, var);
				break;
			case ArbGenChannelMode::which::Sine:
				notify({ stdNote + " Sine Wave.\n", 1 }, expWorker);
				setSine(chan + 1, channel.sine, var);
				break;
			case ArbGenChannelMode::which::Square:
				notify({ stdNote + " Square Wave.\n", 1 }, expWorker);
				setSquare(chan + 1, channel.square, var);
				break;
			case ArbGenChannelMode::which::Preloaded:
				notify({ stdNote + " Preloaded Wave.\n", 1 }, expWorker);
				setExistingWaveform(chan + 1, channel.preloadedArb);
				break;
				break;
			default:
				thrower("Unrecognized channel " + str(chan) + " setting?!?!?!: "
					+ ArbGenChannelMode::toStr(channel.option));
			}
		}
		catch (ChimeraError& err) {
			throwNested("Error seen while programming agilent output for " + configDelim + " ArbGen channel "
				+ str(chan + 1) + ": " + err.whatBare());
		}
	}
	programSpecializedVariation(var, params, runSettings, expWorker);
}

//void ArbGenCore::setSync()
//{
//	try {
//		notify({ "Writing Agilent output sync option: " + qstr(runSettings.synced), 2 }, expWorker);
//		visaFlume.write("OUTPut:SYNC " + str(runSettings.synced));
//	}
//	catch (ChimeraError&) {
//		//errBox ("Failed to set agilent output synced?!");
//	}
//}

// stuff that only has to be done once.
//void ArbGenCore::prepAgilentSettings(unsigned channel) {
//	if (channel != 1 && channel != 2) {
//		thrower("Bad value for channel in prepAgilentSettings! Channel shoulde be 1 or 2.");
//	}
//	// Set timout, sample rate, filter parameters, trigger settings.
//	visaFlume.setAttribute(VI_ATTR_TMO_VALUE, 40000);
//	visaFlume.write("SOURCE1:FUNC:ARB:SRATE " + str(sampleRate));
//	visaFlume.write("SOURCE2:FUNC:ARB:SRATE " + str(sampleRate));
//}

/*
 * This function tells the agilent to use sequence # (varNum) and sets settings correspondingly.
 */
//void ArbGenCore::setScriptOutput(unsigned varNum, scriptedArbInfo scriptInfo, unsigned chan) {
//	if (scriptInfo.wave.isVaried() || varNum == 0) {
//		prepAgilentSettings(chan);
//		// check if effectively dc
//		if (scriptInfo.wave.minsAndMaxes.size() == 0) {
//			thrower("script wave min max size is zero???");
//		}
//		auto& minMaxs = scriptInfo.wave.minsAndMaxes[varNum];
//		if (fabs(minMaxs.first - minMaxs.second) < 1e-6) {
//			dcInfo tempDc;
//			tempDc.dcLevel = str(minMaxs.first);
//			tempDc.dcLevel.internalEvaluate(std::vector<parameterType>(), 1);
//			tempDc.useCal = scriptInfo.useCal;
//			setDC(chan, tempDc, 0);
//		}
//		else {
//			auto schan = "SOURCE" + str(chan);
//			// Load sequence that was previously loaded.
//			visaFlume.write("MMEM:LOAD:DATA" + str(chan) + " \"" + memoryLoc + ":\\sequence" + str(varNum) + ".seq\"");
//			visaFlume.write(schan + ":FUNC ARB");
//			visaFlume.write(schan + ":FUNC:ARB \"" + memoryLoc + ":\\sequence" + str(varNum) + ".seq\"");
//			// set the offset and then the low & high. this prevents accidentally setting low higher than high or high 
//			// higher than low, which causes agilent to throw annoying errors.
//			visaFlume.write(schan + ":VOLT:OFFSET " + str((minMaxs.first + minMaxs.second) / 2) + " V");
//			visaFlume.write(schan + ":VOLT:LOW " + str(minMaxs.first) + " V");
//			visaFlume.write(schan + ":VOLT:HIGH " + str(minMaxs.second) + " V");
//			visaFlume.write("OUTPUT" + str(chan) + " ON");
//		}
//	}
//}

//void ArbGenCore::outputOff(int channel) {
//	if (channel != 1 && channel != 2) {
//		thrower("bad value for channel inside outputOff! Channel shoulde be 1 or 2.");
//	}
//	channel++;
//	visaFlume.write("OUTPUT" + str(channel) + " OFF");
//}


bool ArbGenCore::connected() {
	return isConnected;
}

void ArbGenCore::reconnectFlume()
{
	visaFlume.open();

}


void ArbGenCore::convertInputToFinalSettings(unsigned chan, deviceOutputInfo& info, std::vector<parameterType>& params) {
	unsigned totalVariations = (params.size() == 0) ? 1 : params.front().keyValues.size();
	try {
		if (chan == 0 && info.siglentFm.control) {
			info.siglentFm.ch1AmplitudeVpp.assertValid(params, GLOBAL_PARAMETER_SCOPE);
			info.siglentFm.ch1StartPhaseDeg.assertValid(params, GLOBAL_PARAMETER_SCOPE);
			info.siglentFm.ch1BurstCycles.assertValid(params, GLOBAL_PARAMETER_SCOPE);
			info.siglentFm.ch2FrequencyMHz.assertValid(params, GLOBAL_PARAMETER_SCOPE);
			info.siglentFm.ch2AmplitudeVpp.assertValid(params, GLOBAL_PARAMETER_SCOPE);
			info.siglentFm.ch2PhaseDeg.assertValid(params, GLOBAL_PARAMETER_SCOPE);
			info.siglentFm.ch2FrequencyDeviationMHz.assertValid(params, GLOBAL_PARAMETER_SCOPE);

			info.siglentFm.ch1AmplitudeVpp.internalEvaluate(params, totalVariations);
			info.siglentFm.ch1StartPhaseDeg.internalEvaluate(params, totalVariations);
			info.siglentFm.ch1BurstCycles.internalEvaluate(params, totalVariations);
			info.siglentFm.ch2FrequencyMHz.internalEvaluate(params, totalVariations);
			info.siglentFm.ch2AmplitudeVpp.internalEvaluate(params, totalVariations);
			info.siglentFm.ch2PhaseDeg.internalEvaluate(params, totalVariations);
			info.siglentFm.ch2FrequencyDeviationMHz.internalEvaluate(params, totalVariations);
			return;
		}

		channelInfo& channel = info.channel[chan];
		switch (channel.option)
		{
		case ArbGenChannelMode::which::No_Control:
		case ArbGenChannelMode::which::Output_Off:
			break;
		case ArbGenChannelMode::which::DC:
			channel.dc.dcLevel.internalEvaluate(params, totalVariations);
			break;
		case ArbGenChannelMode::which::Sine:
			channel.sine.frequency.internalEvaluate(params, totalVariations);
			channel.sine.amplitude.internalEvaluate(params, totalVariations);
			channel.sine.phase.internalEvaluate(params, totalVariations);
			break;
		case ArbGenChannelMode::which::Square:
			channel.square.frequency.internalEvaluate(params, totalVariations);
			channel.square.amplitude.internalEvaluate(params, totalVariations);
			channel.square.offset.internalEvaluate(params, totalVariations);
			channel.square.dutyCycle.internalEvaluate(params, totalVariations);
			channel.square.phase.internalEvaluate(params, totalVariations);
			break;
		case ArbGenChannelMode::which::Preloaded:
			break;
		default:
			thrower("Unrecognized ArbGen Setting: " + ArbGenChannelMode::toStr(channel.option));
		}
	}
	catch (ChimeraError&) {
		throwNested("Failed to evaluate ArbGen expressions while converting input to final settings.");
	}
	catch (std::out_of_range&) {
		throwNested("unrecognized variable!");
	}
}

void ArbGenCore::convertInputToFinalSettings(unsigned chan, deviceOutputInfo& info)
{
	std::vector<parameterType> variables = std::vector<parameterType>();
	convertInputToFinalSettings(chan, info, variables);
}

/**
 * expects the inputted power to be in -MILI-WATTS!
 * returns set point in VOLTS
 */
double ArbGenCore::convertPowerToSetPoint(double requestedSetting, bool conversionOption,
	calResult calibration) {
	// requested setting is the voltage or power settting coming from the calibration, depending on how the agilent
	// was calibrated (power typically for tweezer powers, voltage on PD otherwise). 
	if (conversionOption) {
		// build the polynomial calibration.
		// fix this after add analog input + calibration -zzp 2021/2/24
		double setPointInVolts = requestedSetting/*CalibrationManager::calibrationFunction (requestedSetting, calibration)*/;
		return setPointInVolts;
	}
	else {
		// no conversion
		return requestedSetting;
	}
}


std::vector<std::string> ArbGenCore::getStartupCommands() {
	return setupCommands;
}

void ArbGenCore::programSetupCommands() {
	try {
		for (auto cmd : setupCommands) {
			visaFlume.write(cmd);
		}
	}
	catch (ChimeraError&) {
		throwNested("Failed to program setup commands for " + arbGenName + " ArbGen!");
	}
}


deviceOutputInfo ArbGenCore::getSettingsFromConfig(ConfigStream& file) {
	auto readFunc = ConfigSystem::getGetlineFunc(file.ver);
	deviceOutputInfo tempSettings;
	file >> tempSettings.synced;
	std::array<std::string, 2> channelNames = { "CHANNEL_1", "CHANNEL_2" };
	unsigned chanInc = 0;
	for (auto& channel : tempSettings.channel) {
		ConfigSystem::checkDelimiterLine(file, channelNames[chanInc]);
		// the extra step in all of the following is to remove the , at the end of each input.
		std::string input;
		file >> input;
		try {
			/*channel.option = file.ver < Version ("4.2") ?
				ArbGenChannelMode::which (boost::lexical_cast<int>(input) + 2) : ArbGenChannelMode::fromStr (input);*/
			channel.option = ArbGenChannelMode::fromStr(input);
		}
		catch (boost::bad_lexical_cast&) {
			throwNested("Bad channel " + str(chanInc + 1) + " option!");
		}
		file >> channel.polarityInvert;
		std::string calibratedOption;
		file.get();
		readFunc(file, channel.dc.dcLevel.expressionStr);
		//if (file.ver > Version ("2.3")){
		file >> channel.dc.useCal;
		file.get();
		//}
		readFunc(file, channel.sine.amplitude.expressionStr);
		readFunc(file, channel.sine.frequency.expressionStr);
		readFunc(file, channel.sine.phase.expressionStr);
		file >> channel.sine.burstMode;
		//if (file.ver > Version ("2.3")){
		file >> channel.sine.useCal;
		file.get();
		//}
		readFunc(file, channel.square.amplitude.expressionStr);
		readFunc(file, channel.square.frequency.expressionStr);
		readFunc(file, channel.square.offset.expressionStr);
		readFunc(file, channel.square.dutyCycle.expressionStr);
		readFunc(file, channel.square.phase.expressionStr);
		file >> channel.square.burstMode;
		//if (file.ver > Version ("2.3")){
		file >> channel.square.useCal;
		file.get();
		//}
		readFunc(file, channel.preloadedArb.address.expressionStr);
		//if (file.ver > Version ("2.3")){
		file >> channel.preloadedArb.useCal;
		file.get();
		//}
		//if (file.ver >= Version ("5.9")) {
		file >> channel.preloadedArb.burstMode;
		file.get();
		//}
		chanInc++;
	}

	auto lowerToken = [](std::string value) {
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});
		return value;
	};

	auto trimToken = [](std::string& value) {
		auto first = value.find_first_not_of(" \t\r");
		if (first == std::string::npos) {
			value.clear();
			return;
		}
		auto last = value.find_last_not_of(" \t\r");
		value = value.substr(first, last - first + 1);
	};

	auto readExpressionLine = [&](Expression& expression) {
		readFunc(file, expression.expressionStr);
		trimToken(expression.expressionStr);
		if (expression.expressionStr == ConfigStream::emptyStringTxt) {
			expression.expressionStr.clear();
		}
	};

	std::streampos pos = file.tellg();
	std::string nextToken;
	file >> nextToken;
	std::string lowered = lowerToken(nextToken);
	if (lowered == "0" || lowered == "1" || lowered == "true" || lowered == "false") {
		tempSettings.siglentFm.control = (lowered == "1" || lowered == "true");
		file >> tempSettings.siglentFm.useExternalClock;
		file.get();
		readExpressionLine(tempSettings.siglentFm.ch1AmplitudeVpp);
		readExpressionLine(tempSettings.siglentFm.ch1StartPhaseDeg);
		readExpressionLine(tempSettings.siglentFm.ch1BurstCycles);
		readExpressionLine(tempSettings.siglentFm.ch2FrequencyMHz);
		readExpressionLine(tempSettings.siglentFm.ch2AmplitudeVpp);
		readExpressionLine(tempSettings.siglentFm.ch2PhaseDeg);
		readExpressionLine(tempSettings.siglentFm.ch2FrequencyDeviationMHz);
	}
	else {
		file.clear();
		file.seekg(pos);
	}
	return tempSettings;
}


void ArbGenCore::logSettings(DataLogger& log, ExpThreadWorker* threadworker) {
	try {
		H5::Group arbGensGroup;
		try {
			arbGensGroup = log.file.createGroup("/ArbGens");
		}
		catch (H5::Exception&) {
			arbGensGroup = log.file.openGroup("/ArbGens");
		}

		H5::Group singleArbGen(arbGensGroup.createGroup(getDelim()));
		unsigned channelCount = 1;
		log.writeDataSet(getStartupCommands(), "Startup-Commands", singleArbGen);
		for (auto& channel : expRunSettings.channel) {
			H5::Group channelGroup(singleArbGen.createGroup("Channel-" + str(channelCount)));
			std::string outputModeName = ArbGenChannelMode::toStr(channel.option);
			log.writeDataSet(outputModeName, "Output-Mode", channelGroup);
			log.writeDataSet(channel.polarityInvert, "Polarity-Inverted", channelGroup);
			H5::Group dcGroup(channelGroup.createGroup("DC-Settings"));
			log.writeDataSet(channel.dc.dcLevel.expressionStr, "DC-Level", dcGroup);
			H5::Group sineGroup(channelGroup.createGroup("Sine-Settings"));
			log.writeDataSet(channel.sine.frequency.expressionStr, "Frequency", sineGroup);
			log.writeDataSet(channel.sine.amplitude.expressionStr, "Amplitude", sineGroup);
			log.writeDataSet(channel.sine.phase.expressionStr, "Phase", sineGroup);
			H5::Group squareGroup(channelGroup.createGroup("Square-Settings"));
			log.writeDataSet(channel.square.amplitude.expressionStr, "Amplitude", squareGroup);
			log.writeDataSet(channel.square.frequency.expressionStr, "Frequency", squareGroup);
			log.writeDataSet(channel.square.offset.expressionStr, "Offset", squareGroup);
			log.writeDataSet(channel.square.dutyCycle.expressionStr, "DutyCycle", squareGroup);
			log.writeDataSet(channel.square.phase.expressionStr, "Phase", squareGroup);
			H5::Group preloadedArbGroup(channelGroup.createGroup("Preloaded-Arb-Settings"));
			log.writeDataSet(channel.preloadedArb.address.expressionStr, "Address", preloadedArbGroup);
			channelCount++;
		}
	}
	catch (H5::Exception err) {
		log.logError(err);
		throwNested("ERROR: Failed to log ArbGen parameters in HDF5 file: " + err.getDetailMsg());
	}
}

void ArbGenCore::loadExpSettings(ConfigStream& script) {
	ConfigSystem::stdGetFromConfig(script, *this, expRunSettings);
	experimentActive = (expRunSettings.channel[0].option != ArbGenChannelMode::which::No_Control
		|| expRunSettings.channel[1].option != ArbGenChannelMode::which::No_Control
		|| expRunSettings.siglentFm.control);
}

void ArbGenCore::calculateVariations(std::vector<parameterType>& params, ExpThreadWorker* threadWorker) {
	if (!experimentActive && !expRunSettings.siglentFm.control) {
		return;
	}
	try {
		if (expRunSettings.siglentFm.control) {
			convertInputToFinalSettings(0, expRunSettings, params);
			return;
		}
		convertInputToFinalSettings(0, expRunSettings, params);
		convertInputToFinalSettings(1, expRunSettings, params);
	}
	catch (ChimeraError&) {
		throwNested("Failed to evaluate ArbGen expression varations!");
	}
}

void ArbGenCore::setRunSettings(deviceOutputInfo newSettings) {
	// used by "program now".
	expRunSettings = newSettings;
}

void ArbGenCore::programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadWorker) {
	setArbGen(variation, params, expRunSettings, threadWorker);
}

void ArbGenCore::checkTriggers(unsigned variationInc, DoCore& ttls, ExpThreadWorker* threadWorker) {
	(void)variationInc;
	(void)ttls;
	(void)threadWorker;
}

void ArbGenCore::setAgCalibration(calResult newCal, unsigned chan) {
	if (chan != 1 && chan != 2) {
		thrower("ERROR: ArbGen channel must be 1 or 2!");
	}
	calibrations[chan - 1] = newCal;
}
