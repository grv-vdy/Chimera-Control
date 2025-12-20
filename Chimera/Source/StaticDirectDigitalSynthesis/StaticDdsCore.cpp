#include "stdafx.h"
#include "StaticDdsCore.h"
#include <ExperimentThread/ExpThreadWorker.h>
#include <ConfigurationSystems/ConfigSystem.h>
#include <DataLogging/DataLogger.h>

StaticDdsCore::StaticDdsCore(
    const bool safemode,
    const std::array<std::string, STATICDDS_NUMBER>& port,
    const std::array<unsigned int, STATICDDS_NUMBER>& baudrate
) :
    safemode(safemode),
	sddsFlume{ StaticDDSFlume(port[0], baudrate[0], safemode),
			   StaticDDSFlume(port[1], baudrate[1], safemode),
			   StaticDDSFlume(port[2], baudrate[2], safemode) }
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
		for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
			for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
				expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].assertValid(params, GLOBAL_PARAMETER_SCOPE);
				expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].internalEvaluate(params, totalVariations);
				expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].assertValid(params, GLOBAL_PARAMETER_SCOPE);
				expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].internalEvaluate(params, totalVariations);
				if (expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].varies() && safemode) {
					thrower("Error in varying static DDS for channel " + str(ch) +
						". The DDS is in SAFEMODE in constant.h but is varied given expression " +
						expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].expressionStr);
				}
				if (expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].varies() && safemode) {
					thrower("Error in varying static DDS for channel " + str(ch) +
						". The DDS is in SAFEMODE in constant.h but is varied given expression " +
						expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].expressionStr);
				}
				for (auto variation : range(totalVariations)) {
					auto ddsfreqVal = expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].getValue(variation);
					auto ddslevelVal = expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].getValue(variation);
					if (!checkBoundFreq(ddsfreqVal)) {
						thrower("Error in varying static DDS for channel " + str(ch) + " and variation" + str(variation) +
							". The DDS is limited to " + str(minFreqVal) + " MHz to " + str(maxFreqVal) +
							" MHz but is set to an outside value given expression " +
							expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].expressionStr + " and its evaluation: " + str(ddsfreqVal));
					}

					if (!checkBoundLevel(ddslevelVal)) {
						thrower("Error in varying static DDS for channel " + str(ch) + " and variation" + str(variation) +
							". The DDS is limited to " + str(minLevelVal) + " dB to " + str(maxLevelVal) +
							" dB but is set to an outside value given expression " +
							expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].expressionStr + " and its evaluation: " + str(ddsfreqVal));
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
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> outputs_frequency;
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> outputs_level;
	for (auto port : range(size_t(StaticDDSGrid::numOFunit)) ){
		for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
			outputs_frequency[port][ch] = expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].getValue(variation);
			outputs_level[port][ch] = expSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].getValue(variation);
		}
	}
	try {
		writeDDSs(outputs_frequency, outputs_level);
	}
	catch (ChimeraError& e) {
		throwNested("Failed to program Static DDS (Valon communication error): " + e.trace());
	}
}

StaticDDSSettings StaticDdsCore::getSettingsFromConfig(ConfigStream& file)
{
	StaticDDSSettings tempSettings;
	auto getlineF = ConfigSystem::getGetlineFunc(file.ver);
	//file.get();
	for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
		for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
			getlineF(file, tempSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].expressionStr);
			getlineF(file, tempSettings.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].expressionStr);
		}
	}
	file >> tempSettings.ctrlDDS;
	file.get();
	return tempSettings;
}

std::string StaticDdsCore::getDeviceInfo(unsigned int port)
{
	return sddsFlume[port].getSerialNumberOnly();
}

void StaticDdsCore::setStaticDDSExpSetting(StaticDDSSettings tmpSetting)
{
	expSettings = tmpSetting;
}

void StaticDdsCore::startFrequencySweep(unsigned port, unsigned ch, double startMHz, double stopMHz, double sweepSeconds, unsigned desiredSteps)
{
	if (port >= size_t(StaticDDSGrid::numOFunit) || ch >= size_t(StaticDDSGrid::numPERunit)) {
		thrower("Invalid port/ch for sweep");
	}
	if (sweepSeconds <= 0.0) {
		thrower("Sweep time must be > 0");
	}

	// Choose steps/rate so that RATE >= 10 ms (recommended by manual)
	unsigned steps = desiredSteps;
	unsigned rateMs = 10; // default min
	if (steps == 0) {
		// aim for ~100 steps by default
		steps = 100;
		double candidateRate = (sweepSeconds / static_cast<double>(steps)) * 1000.0;
		if (candidateRate < 10.0) {
			rateMs = 10;
			steps = static_cast<unsigned>(std::ceil((sweepSeconds * 1000.0) / static_cast<double>(rateMs)));
		}
		else {
			rateMs = static_cast<unsigned>(std::round(candidateRate));
		}
	}
	else {
		// compute rate to meet sweepSeconds with desiredSteps
		double candidateRate = (sweepSeconds / static_cast<double>(steps)) * 1000.0;
		rateMs = static_cast<unsigned>(std::max(10.0, candidateRate));
	}

	double stepMHz = (stopMHz - startMHz) / static_cast<double>(steps);
	if (stepMHz == 0.0) {
		// nothing to do
		return;
	}

	// Use device-native sweep command via flume
	try {
		sddsFlume[port].startSweep(startMHz, stopMHz, stepMHz, rateMs, static_cast<int>(ch));
	}
	catch (ChimeraError& e) {
		throwNested("Failed to start Valon sweep: " + e.trace());
	}
}

void StaticDdsCore::stopFrequencySweep(unsigned port, unsigned ch)
{
	if (port >= size_t(StaticDDSGrid::numOFunit) || ch >= size_t(StaticDDSGrid::numPERunit)) return;
	try {
		sddsFlume[port].stopSweep(static_cast<int>(ch));
	}
	catch (ChimeraError& e) {
		throwNested("Failed to stop Valon sweep: " + e.trace());
	}
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

void StaticDdsCore::writeDDSs(
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> outputs_frequency,
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> outputs_level)
{
	try {
		for (size_t port = 0; port < size_t(StaticDDSGrid::numOFunit); ++port) {
			for (size_t ch = 0; ch < size_t(StaticDDSGrid::numPERunit); ++ch) {
				double freq = outputs_frequency[port][ch];
				double level = outputs_level[port][ch];
				try {
					sddsFlume[port].setFrequency(freq, static_cast<int>(ch));
				}
				catch (ChimeraError& e) {
					throwNested("Failed to set frequency on Static DDS port " + str(port) + " channel " + str(ch) + ": " + e.trace());
				}
				try {
					sddsFlume[port].setOutputLevel(level, static_cast<int>(ch));
				}
				catch (ChimeraError& e) {
					throwNested("Failed to set output level on Static DDS port " + str(port) + " channel " + str(ch) + ": " + e.trace());
				}
			}
		}
	}
	catch (ChimeraError&) {
		throw;
	}
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
