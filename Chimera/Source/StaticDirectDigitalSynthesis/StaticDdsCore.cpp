#include "stdafx.h"
#include "StaticDdsCore.h"
#include <ExperimentThread/ExpThreadWorker.h>
#include <ConfigurationSystems/ConfigSystem.h>
#include <DataLogging/DataLogger.h>

StaticDdsCore::StaticDdsCore(bool safemode, const std::string& port, unsigned baudrate) :
	safemode(safemode), 
	sddsFlume(port, baudrate, safemode)
{
}

void StaticDdsCore::loadExpSettings(ConfigStream& stream)
{
	ConfigSystem::stdGetFromConfig(stream, *this, expSettings);
	experimentActive = expSettings.ctrlDDS;
}

void StaticDdsCore::logSettings(DataLogger& logger, ExpThreadWorker* threadworker)
{}

void StaticDdsCore::calculateVariations(std::vector<parameterType>&params, ExpThreadWorker * threadworker)
{
	if (!experimentActive && !expSettings.ctrlDDS) {
		return;
	}
	size_t totalVariations = (params.size() == 0) ? 1 : params.front().keyValues.size();
	try {
		for (auto port : range(size_t(StaticDDSGrid::total))) {
			for (auto ch : range(size_t(StaticDDSGrid::total))) {
				expSettings.staticDDSs[port*ch+ch][0].assertValid(params, GLOBAL_PARAMETER_SCOPE);
				expSettings.staticDDSs[port * ch + ch][0].internalEvaluate(params, totalVariations);
				expSettings.staticDDSs[port * ch + ch][1].assertValid(params, GLOBAL_PARAMETER_SCOPE);
				expSettings.staticDDSs[port * ch + ch][1].internalEvaluate(params, totalVariations);
				if (expSettings.staticDDSs[port * ch + ch][0].varies() && safemode) {
					thrower("Error in varying static DDS for channel " + str(ch) +
						". The DDS is in SAFEMODE in constant.h but is varied given expression " +
						expSettings.staticDDSs[port * ch + ch][0].expressionStr);
				}
				if (expSettings.staticDDSs[port * ch + ch][1].varies() && safemode) {
					thrower("Error in varying static DDS for channel " + str(ch) +
						". The DDS is in SAFEMODE in constant.h but is varied given expression " +
						expSettings.staticDDSs[port * ch + ch][1].expressionStr);
				}
				for (auto variation : range(totalVariations)) {
					auto ddsfreqVal = expSettings.staticDDSs[port * ch + ch][0].getValue(variation);
					auto ddslevelVal = expSettings.staticDDSs[port * ch + ch][1].getValue(variation);
					if (!checkBoundFreq(ddsfreqVal)) {
						thrower("Error in varying static DDS for channel " + str(ch) + " and variation" + str(variation) +
							". The DDS is limited to " + str(minFreqVal) + " MHz to " + str(maxFreqVal) +
							" MHz but is set to an outside value given expression " +
							expSettings.staticDDSs[port * ch + ch][0].expressionStr + " and its evaluation: " + str(ddsfreqVal));
					}

					if (!checkBoundLevel(ddslevelVal)) {
						thrower("Error in varying static DDS for channel " + str(ch) + " and variation" + str(variation) +
							". The DDS is limited to " + str(minLevelVal) + " dB to " + str(maxLevelVal) +
							" dB but is set to an outside value given expression " +
							expSettings.staticDDSs[port * ch + ch][1].expressionStr + " and its evaluation: " + str(ddsfreqVal));
					}
				}
			}
		}
		
	}
	catch (ChimeraError&) {
		throwNested("Failed to evaluate staticAO expression varations!");
	}
}

void StaticDdsCore::programVariation(unsigned variation, std::vector<parameterType>& params, ExpThreadWorker* threadworker)
{
	if (!experimentActive && !expSettings.ctrlDDS) {
		return;
	}
	std::array<std::array<double, 2>, size_t(StaticDDSGrid::total)> outputs_frequency;
	std::array<std::array<double, 2>, size_t(StaticDDSGrid::total)> outputs_level;
	for (auto port : range(size_t(StaticDDSGrid::total))) {
		for (auto ch : range(size_t(2))) {
			outputs_frequency[port][ch] = expSettings.staticDDSs[port*ch+ch][0].getValue(variation);
			outputs_level[port][ch] = expSettings.staticDDSs[port*ch+ch][1].getValue(variation);
		}
	}
	writeDDSs(outputs_frequency, outputs_level);
}

StaticDDSSettings StaticDdsCore::getSettingsFromConfig(ConfigStream& file)
{
	StaticDDSSettings tempSettings;
	auto getlineF = ConfigSystem::getGetlineFunc(file.ver);
	//file.get();
	for (auto port : range(size_t(StaticDDSGrid::total))) {
		for (auto ch : range(size_t(2))) {
			getlineF(file, tempSettings.staticDDSs[port*ch+ch][0].expressionStr);
			getlineF(file, tempSettings.staticDDSs[port*ch+ch][1].expressionStr);
		}
	}
	file >> tempSettings.ctrlDDS;
	file.get();
	return tempSettings;
}

std::string StaticDdsCore::getDeviceInfo()
{
	return sddsFlume.getSerialNumberOnly();
}

void StaticDdsCore::setStaticDDSExpSetting(StaticDDSSettings tmpSetting)
{
	expSettings = tmpSetting;
}

std::string StaticDdsCore::getDDSCommand(double ddsfreqVal)
{
	std::string buffCmd;
	buffCmd += "(" + str(0/*getCmdChannelIdx(channelSnap.channel)*/) + ","
		+ str(ddsfreqVal/*channelSnap.val*/, numFreqDigits) + ","
		+ str(ddsfreqVal/*channelSnap.endVal*/, numFreqDigits) + "," + str(1/*channelSnap.numSteps*/) + ","
		+ str(1.0/*channelSnap.rampTime*/, 2/*numTimeDigits(channelSnap.channel)*/) + ")";
	buffCmd += "e";
	return buffCmd;
}

void StaticDdsCore::writeDDSs(std::array<std::array<double, 2>, size_t(StaticDDSGrid::total)> outputs_frequency, 
std::array<std::array<double, 2>, size_t(StaticDDSGrid::total)> outputs_level)
{
	//std::string command;
	//for (auto ch : range(size_t(StaticDDSGrid::total))) {
	//	command = getDDSCommand(outputs_frequency[ch]);
	//}
	//sddsFlume.write(command);
	//if (!safemode) {
	//	std::string recv = sddsFlume.read();
	//	std::transform(recv.begin(), recv.end(), recv.begin(), ::tolower); /*:: without namespace select from global namespce, see https://stackoverflow.com/questions/5539249/why-cant-transforms-begin-s-end-s-begin-tolower-be-complied-successfu*/
	//	if (recv.find("error") != std::string::npos) {
	//		thrower("Error in static DDS programming, from Arduino: " + recv);
	//	}
	//}
}

bool StaticDdsCore::checkBoundFreq(double ddsfreqVal)
{
	if (ddsfreqVal > maxFreqVal) {
		return false;
	}
	else if (ddsfreqVal < minFreqVal) {
		return false;
	}
	else {
		return true;
	}
}

bool StaticDdsCore::checkBoundLevel(double ddslevelVal)
{
	if (ddslevelVal > maxLevelVal) {
		return false;
	}
	else if (ddslevelVal < minLevelVal) {
		return false;
	}
	else {
		return true;
	}
}
