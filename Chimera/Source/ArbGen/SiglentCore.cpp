#include "stdafx.h"
#include "SiglentCore.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>

SiglentCore::SiglentCore(const arbGenSettings& settings) :
	ArbGenCore(settings)
{

}

SiglentCore::~SiglentCore() {
	visaFlume.close();
}


// stuff that only has to be done once.
void SiglentCore::prepArbGenSettings(unsigned channel) {
	if (channel != 1 && channel != 2) {
		thrower("Bad value for channel in prepAgilentSettings! Channel shoulde be 1 or 2.");
	}
	// Set timout, sample rate, filter parameters, trigger settings.
	visaFlume.setAttribute(VI_ATTR_TMO_VALUE, 40000);
	/*once write this command, cannot set SWEEP or BURST anymore, have to turn to DDS and set them and then turn it back to TARB*/
	visaFlume.write("C1:SRATE MODE,TARB,VALUE," + str(sampleRate));
	visaFlume.write("C2:SRATE MODE,TARB,VALUE," + str(sampleRate));
}

/*
 * This function tells the agilent to use sequence # (varNum) and sets settings correspondingly.
 */
void SiglentCore::setScriptOutput(unsigned varNum, scriptedArbInfo scriptInfo, unsigned chan) {
	if (scriptInfo.wave.isVaried() || varNum == 0) {
		prepArbGenSettings(chan);
		// check if effectively dc
		if (scriptInfo.wave.minsAndMaxes.size() == 0) {
			thrower("script wave min max size is zero???");
		}
		auto& minMaxs = scriptInfo.wave.minsAndMaxes[varNum];
		if (fabs(minMaxs.first - minMaxs.second) < 1e-6) {
			dcInfo tempDc;
			tempDc.dcLevel = str(minMaxs.first);
			std::vector<parameterType> var = std::vector<parameterType>();
			tempDc.dcLevel.internalEvaluate(var, 1);
			tempDc.useCal = scriptInfo.useCal;
			setDC(chan, tempDc, 0);
		}
		else {
			auto schan = "C" + str(chan);
			visaFlume.write(schan + ":ARWV NAME,wave" + str(varNum));
			visaFlume.write(schan + ":BSWV WVTP,ARB");
			visaFlume.write(schan + ":BSWV OFST," + str((minMaxs.first + minMaxs.second) / 2) + "V");
			visaFlume.write(schan + ":BSWV LLEV," + str(minMaxs.first) + "V");
			visaFlume.write(schan + ":BSWV HLEV," + str(minMaxs.second) + "V");
			programBurstMode(chan, true);


			//visaFlume.write(schan + ":OUTPut ON");
			outputOn(chan);
		}
	}
}


void SiglentCore::outputOff(int channel) {
	if (channel != 1 && channel != 2) {
		thrower("bad value for channel inside outputOff! Channel shoulde be 1 or 2.");
	}
	//channel++;
	visaFlume.write("C" + str(channel) + ":OUTPut OFF");
}

void SiglentCore::outputOn(int channel)
{
	if (channel != 1 && channel != 2) {
		thrower("bad value for channel inside outputOn! Channel shoulde be 1 or 2.");
	}
	//channel++;
	std::string outputOn;
	visaFlume.query("C" + str(channel) + ":OUTP?", outputOn, "%t");
	if (outputOn.find("OUTP ON") == std::string::npos) {
		visaFlume.write("C" + str(channel) + ":OUTPut ON");
	}
}


void SiglentCore::setSync(const deviceOutputInfo& runSettings, ExpThreadWorker* expWorker)
{
	try {
		notify({ "Writing Siglent output sync option: " + qstr(runSettings.synced), 2 }, expWorker);
		visaFlume.write("C1:SYNC " + (runSettings.synced) ? "ON" : "OFF");
		/*default sync to CH1, siglent can also sync with mod, see if we need zzp 20210228*/
	}
	catch (ChimeraError&) {
		thrower("Failed to set Siglent output synced, check connection as well as Siglent command syntax in c++ code");
		//errBox ("Failed to set agilent output synced?!");
	}
}

void SiglentCore::setPolarity(int channel, bool polarityInverted, ExpThreadWorker* expWorker)
{
	if (channel != 1 && channel != 2) {
		thrower("Bad value for channel inside setDC! Channel shoulde be 1 or 2.");
	}
	//try {
	//	visaFlume.write("C" + str(channel) + ":INVT " + (polarityInverted ? "ON" : "OFF"));
	//}
	//catch (ChimeraError&) {
	//	throwNested("Seen while programming output polarity for channel " + str(channel) + " (1-indexed).");
	//}
}

void SiglentCore::setDC(int channel, dcInfo info, unsigned var) {
	if (channel != 1 && channel != 2) {
		thrower("Bad value for channel inside setDC! Channel shoulde be 1 or 2.");
	}
	try {
		visaFlume.write("C" + str(channel) + ":BSWV WVTP,DC,OFST,"
			+ str(convertPowerToSetPoint(info.dcLevel.getValue(var), info.useCal, calibrations[channel - 1])) 
			+ "V");
	}
	catch (ChimeraError&) {
		throwNested("Seen while programming DC for channel " + str(channel) + " (1-indexed).");
	}
}


void SiglentCore::setExistingWaveform(int channel, preloadedArbInfo info) {
	if (channel != 1 && channel != 2) {
		thrower("Bad value for channel in setExistingWaveform! Channel shoulde be 1 or 2.");
	}
	visaFlume.write("C" + str(channel) + ":ARWV NAME," + info.address.expressionStr);
	programBurstMode(channel, info.burstMode);
	//visaFlume.write("C" + str(channel) + ":OUTPut ON");
	outputOn(channel);
}


void SiglentCore::programBurstMode(int channel, bool burstOption) 
{
	std::string sStr = "C" + str(channel);
	if (burstOption) 
	{
		std::string arbMode;
		visaFlume.query(sStr + ":SRATE?", arbMode, "%t");
		bool TARB = false;
		if (arbMode.find("TARB") != std::string::npos) {
			TARB = true;
		}
		/*must change to DDS mode to be able to change burst state*/
		visaFlume.write(sStr + ":SRATE MODE,DDS");
		
		visaFlume.write(sStr + ":BTWV STATE,ON");
		visaFlume.write(sStr + ":BTWV TRSR,EXT");
		visaFlume.write(sStr + ":BTWV GATE_NCYC,NCYC");
		visaFlume.write(sStr + ":BTWV EDGE,RISE");
		visaFlume.write(sStr + ":BTWV TIME,1"); /*Value of Ncycle number*/
		visaFlume.write(sStr + ":BTWV STPS,0");
		visaFlume.write(sStr + ":BTWV DLAY,0");

		if (TARB) {
			visaFlume.write(sStr + ":SRATE MODE,TARB");
			//thrower("Agilent can not set Burst in TrueArb mode. Have changed the mode to DDS.");
		}

	}
	else {
		visaFlume.write(sStr + ":BTWV STATE,OFF");
	}
}

void SiglentCore::programNonArbBurstMode(int channel, bool burstOption)
{
	std::string sStr = "C" + str(channel);
	if (burstOption)
	{
		std::string arbMode;
		visaFlume.query(sStr + ":SRATE?", arbMode, "%t");
		bool TARB = false;
		if (arbMode.find("TARB") != std::string::npos) {
			TARB = true;
			/*must change to DDS mode to be able to change burst state*/
			visaFlume.write(sStr + ":SRATE MODE,DDS");
		}

		visaFlume.write(sStr + ":BTWV STATE,ON");
		//visaFlume.write(sStr + ":BTWV GATE_NCYC,NCYC");
		//visaFlume.write(sStr + ":BTWV DLAY,0"); // This need to be changed when gating is set to NCYC
		visaFlume.write(sStr + ":BTWV GATE_NCYC,GATE");
		visaFlume.write(sStr + ":BTWV TRSR,EXT");
		visaFlume.write(sStr + ":BTWV EDGE,RISE");
		visaFlume.write(sStr + ":BTWV PLRT,POS");
		visaFlume.write(sStr + ":BTWV STPS,0");

		if (TARB) {
			//visaFlume.write(sStr + ":SRATE MODE,TARB");
			thrower("Agilent can not set Burst in TrueArb mode. Have changed the mode to DDS.");
		}

	}
	else {
		visaFlume.write(sStr + ":BTWV STATE,OFF");
	}
}


// set the agilent to output a square wave.
void SiglentCore::setSquare(int channel, squareInfo info, unsigned var) {
	if (channel != 1 && channel != 2) {
		thrower("Bad Value for Channel in setSquare! Channel shoulde be 1 or 2.");
	}
	try {
		visaFlume.write("C" + str(channel) + ":BSWV WVTP,SQUARE"
			",FRQ," + str(info.frequency.getValue(var) * 1000) +"HZ"
			",AMP," + str(convertPowerToSetPoint(info.amplitude.getValue(var), info.useCal, calibrations[channel - 1])) + "VPP"
			",OFST," + str(convertPowerToSetPoint(info.offset.getValue(var), info.useCal, calibrations[channel - 1])) + "V"
			",DUTY," + str(info.dutyCycle.getValue(var)) + 
			",PHSE," + str(info.phase.getValue(var)));
		programNonArbBurstMode(channel, info.burstMode);
		if (info.burstMode) {
			visaFlume.write("C" + str(channel) + +":BTWV CARR,PHSE," + str(info.phase.getValue(var)));
			visaFlume.write("C" + str(channel) + +":BTWV STPS," + str(info.phase.getValue(var))); // both works the same way
		}
		outputOn(channel);
	}
	catch (ChimeraError&) {
		throwNested("Seen while programming Square Wave for channel " + str(channel) + " (1-indexed).");
	}
}


void SiglentCore::setSine(int channel, sineInfo info, unsigned var) {
	if (channel != 1 && channel != 2) {
		thrower("Bad value for channel in setSine! Channel shoulde be 1 or 2.");
	}
	try {
		visaFlume.write("C" + str(channel) + ":BSWV WVTP,SINE"
			",FRQ," + str(info.frequency.getValue(var) * 1000) + "HZ"
			",AMP," + str(convertPowerToSetPoint(info.amplitude.getValue(var), info.useCal, calibrations[channel - 1])) + "VPP"
			",OFST,0V"
			",PHSE," + str(info.phase.getValue(var)));
		programNonArbBurstMode(channel, info.burstMode);
		outputOn(channel);
		//visaFlume.write("C" + str(channel) + ":BTWV CARR,WVTP,SINE"
		//	",FRQ," + str(info.frequency.getValue(var) * 1000) + "HZ"
		//	",AMP," + str(convertPowerToSetPoint(info.amplitude.getValue(var), info.useCal, calibrations[channel - 1])) + "VPP"
		//	",OFST,0V,PHSE,0"); // this also works

	}
	catch (ChimeraError& e) {
		throwNested("Seen while programming Sine Wave for channel " + str(channel) + " (1-indexed). \r\n" + e.trace());
	}

}

/**
 * This function tells the agilent to put out the DC default waveform.
 */
void SiglentCore::setDefault(int channel) {
	try {
		// turn it to the default voltage...
		std::string setPointString = str(convertPowerToSetPoint(SIGLENT_DEFAULT_POWER, true, calibrations[channel - 1]));
		visaFlume.write("C" + str(channel) + ":BSWV WVTP,DC,OFST," + setPointString + "V");
		outputOff(channel);
	}
	catch (ChimeraError&) {
		throwNested("Seen while programming default voltage.");
	}
}

void SiglentCore::handleScriptVariation(unsigned variation, scriptedArbInfo& scriptInfo, unsigned channel,
	std::vector<parameterType>& params) {
	prepArbGenSettings(channel);
	/**********make sure be in DDS mode to change burst and sweep***********/
	programSetupCommands();
	if (scriptInfo.wave.isVaried() || variation == 0) 
	{
		unsigned totalSegmentNumber = scriptInfo.wave.getSegmentNumber();
		// Loop through all segments
		for (auto segNumInc : range(totalSegmentNumber)) {
			// Use that information to writebtn the data.
			try {
				scriptInfo.wave.calSegmentData(segNumInc, sampleRate, variation);
			}
			catch (ChimeraError&) {
				throwNested("IntensityWaveform.calSegmentData threw an error! Error occurred in segment #"
					+ str(totalSegmentNumber));
			}
		}
		// order matters.
		// loop through again and calc/normalize/writebtn values.
		scriptInfo.wave.convertPowersToVoltages(scriptInfo.useCal, calibrations[channel - 1]);
		scriptInfo.wave.calcMinMax();
		scriptInfo.wave.minsAndMaxes.resize(variation + 1);
		scriptInfo.wave.minsAndMaxes[variation].second = scriptInfo.wave.getMaxVolt();
		scriptInfo.wave.minsAndMaxes[variation].first = scriptInfo.wave.getMinVolt();
		scriptInfo.wave.normalizeVoltages();

		prepArbGenSettings(channel);
		std::string& totalSeq = scriptInfo.wave.getTotalSequence();
		totalSeq = "C" + str(channel) + ":WVDT WVNM,wave" + str(variation) + ",WAVEDATA,";
		for (unsigned segNumInc : range(totalSegmentNumber)) 
		{
			totalSeq += compileAndReturnDataSendString(scriptInfo, segNumInc, variation,
				totalSegmentNumber, channel);
		}
		//compileSequenceString(scriptInfo, totalSegmentNumber, variation, channel, variation);
		// submit the sequence
		visaFlume.write(totalSeq/*scriptInfo.wave.returnSequenceString()*/);

		//std::string tmp;
		//visaFlume.query("WVDT? USER,wave0", tmp);
	}
}


/*
 * This function takes the data points (that have already been converted and normalized) and puts them into a string
 * for the agilent to readbtn. segNum: this is the segment number that this data is for
 * varNum: This is the variation number for this segment (matters for naming the segments)
 * totalSegNum: This is the number of segments in the waveform (also matters for naming)
 * For siglent, cannot do sequencing. Have to stitch all seq together.
 */
std::string SiglentCore::compileAndReturnDataSendString(scriptedArbInfo& scriptInfo, int segNum, int varNum, int totalSegNum, unsigned chan) {
	std::vector<Segment> waveformSegments = scriptInfo.wave.getWaveformSegments();

	// must get called after data conversion
	std::string tempSendString;
	//tempSendString = "C" + str(chan) + ":WVDT WVNM,wave" + str(segNum + totalSegNum * varNum) + ",WAVEDATA,";
	unsigned numData = waveformSegments[segNum].returnDataSize();
	double scale = (1 << 15) - 1; //upper,lower of 16bit signed interger
	for (unsigned sendDataInc = 0; sendDataInc < numData; sendDataInc++) 
	{
		/*note the format is small endian for siglent*/
		signed short data = static_cast<signed short>(scale * waveformSegments[segNum].returnDataVal(sendDataInc));
		unsigned char hi = data >> 8;
		unsigned char lo = data & 0xff;
		tempSendString += static_cast<char>(data & 0xff);
		tempSendString += static_cast<char>(data >> 8);
		//tempSendString += ",";
	}
	/*the coma for the last data will be handled after write all seq*/
	return tempSendString;
}


/*
* This function compiles the sequence string which tells the agilent what waveforms to output when and with what trigger control. The sequence is stored
* as a part of the class.  Not used for siglent zzp20210301
*/
void SiglentCore::compileSequenceString(scriptedArbInfo& scriptInfo, int totalSegNum, int sequenceNum, unsigned channel, unsigned varNum) {
	std::vector<Segment> waveformSegments = scriptInfo.wave.getWaveformSegments();
	std::string& totalSequence = scriptInfo.wave.getTotalSequence();

	//std::string tempSequenceString, tempSegmentInfoString;
	//// Total format is  #<n><n digits><sequence name>,<arb name1>,<repeat count1>,<play control1>,<marker mode1>,<marker point1>,<arb name2>,<repeat count2>,
	//// <play control2>, <marker mode2>, <marker point2>, and so on.
	//tempSequenceString = "SOURce" + str(channel) + ":DATA:SEQ #";
	//tempSegmentInfoString = "sequence" + str(sequenceNum) + ",";
	//if (totalSegNum == 0) {
	//	thrower("No segments in agilent waveform???\r\n");
	//}
	//for (int segNumInc = 0; segNumInc < totalSegNum; segNumInc++) {
	//	tempSegmentInfoString += "segment" + str(segNumInc + totalSegNum * sequenceNum) + ",";
	//	tempSegmentInfoString += str(waveformSegments[segNumInc].getInput().repeatNum.getValue(varNum)) + ",";
	//	tempSegmentInfoString += SegmentEnd::toStr(waveformSegments[segNumInc].getInput().continuationType) + ",";
	//	tempSegmentInfoString += "highAtStart,4,";
	//}
	//// remove final comma.
	//tempSegmentInfoString.pop_back();
	//totalSequence = tempSequenceString + str((str(tempSegmentInfoString.size())).size())
	//	+ str(tempSegmentInfoString.size()) + tempSegmentInfoString;
}

unsigned SiglentCore::uploadBinWaveformToChannel1(const std::string& binFilePath, double durationMs, const std::string& waveformName)
{
    if (durationMs <= 0) {
        thrower("Binary upload failed: duration must be > 0 ms.");
    }

    // 1. Read the raw binary file in one instant chunk
    std::ifstream file(binFilePath, std::ios::binary | std::ios::ate);
    if (!file.is_open() || !file.good()) {
        thrower("Binary upload failed: could not open file " + binFilePath);
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        file.close();
        thrower("Binary upload failed: file is empty or unreadable (size=" + std::to_string(fileSize) + ").");
    }
    
    file.seekg(0, std::ios::beg);
    if (!file.good()) {
        file.close();
        thrower("Binary upload failed: could not seek to start of file.");
    }

    // Basic validation: A valid 16-bit binary file must have an even number of bytes
    if (fileSize % 2 != 0) {
        file.close();
        thrower("Binary upload failed: file size (" + std::to_string(fileSize) + " bytes) is not even.");
    }

    // Read the exact byte payload directly into a string buffer
    std::string payload(fileSize, '\0');
    file.read(payload.data(), fileSize);
    
    std::streamsize bytesRead = file.gcount();
    file.close();
    
    if (bytesRead != fileSize) {
        thrower("Binary upload failed: read " + std::to_string(bytesRead) + " bytes but expected " + std::to_string(fileSize) + ".");
    }

    // 2. Calculate Sample Rate (Every point is 2 bytes)
    size_t numPoints = fileSize / 2;
    double durationSeconds = durationMs * 1e-3;
    auto calculatedRate = static_cast<unsigned>(std::llround(numPoints / durationSeconds));
    
    if (calculatedRate == 0) {
        thrower("Binary upload failed: calculated sample rate is zero.");
    }
    
    const unsigned MAX_SAMPLE_RATE = 75000000u; // 75 MSa/s
    if (calculatedRate > MAX_SAMPLE_RATE) {
        double minDurationMs = (numPoints / static_cast<double>(MAX_SAMPLE_RATE)) * 1e3;
        thrower("Calculated sample rate (" + std::to_string(calculatedRate) + " Sa/s) exceeds 75 MSa/s.\n"
            "Minimum pulse duration for this waveform is " + std::to_string(minDurationMs) + " ms.");
    }

	// Program the waveform sample rate before selecting the uploaded arb.
	this->visaFlume.write("C1:SRATE MODE,TARB,VALUE," + str(calculatedRate));

    // 3. Build Header and Send Command
    std::string safeName = waveformName.empty() ? "USERBIN" : waveformName;
    if (safeName.size() > 16) {
        safeName = safeName.substr(0, 16);
    }

    // Siglent WVDT command format: C1:WVDT WVNM,<name>,TYPE,6,LENGTH,<bytes>B,WAVEDATA,<binary_data>
    // Send header first
    std::string commandHeader = "C1:WVDT WVNM," + safeName + ",TYPE,6,LENGTH," + std::to_string(fileSize) + "B,WAVEDATA,";
    
    // Build complete command with binary payload
    std::string command;
    command.reserve(commandHeader.size() + fileSize);
    command.append(commandHeader);
    command.append(payload);
    
    // Send via VISA - note: this sends the binary data directly as part of the string
    this->visaFlume.write(command);
    
    return calculatedRate;
}

void SiglentCore::selectWaveformOnChannel1(const std::string& waveformName)
{
    // Select the waveform by name on CH1
    std::string safeName = waveformName.empty() ? "USERBIN" : waveformName;
    if (safeName.size() > 16) {
        safeName = safeName.substr(0, 16);
    }
    
	// Put CH1 into arbitrary waveform mode, then select the uploaded waveform.
	this->visaFlume.write("C1:BSWV WVTP,ARB");
    this->visaFlume.write("C1:ARWV NAME," + safeName);
    
    // Enable output on CH1
    outputOn(1);
}

void SiglentCore::setArbSampleRateCh1(unsigned sampleRateSaS)
{
	if (sampleRateSaS == 0) {
		thrower("Sample rate must be greater than zero.");
	}
	visaFlume.write("C1:SRATE MODE,TARB,VALUE," + str(sampleRateSaS));
}

void SiglentCore::programFmModulationProfile(double ch1AmplitudeVpp, double ch1StartPhaseDeg,
	double ch2FrequencyMHz, double ch2AmplitudeVpp, double ch2PhaseDeg, double frequencyDeviationMHz,
	bool useExternalClock, unsigned ch1BurstCycles)
{
	if (ch1BurstCycles == 0) {
		thrower("CH1 burst cycle count must be greater than zero.");
	}

	if (useExternalClock) {
		visaFlume.write("CLKSRC EXT");
	}
	else {
		visaFlume.write("CLKSRC INT");
	}

	visaFlume.write("C2:BSWV WVTP,SINE,FRQ," + std::to_string(ch2FrequencyMHz * 1e6) + 
                "HZ,AMP," + std::to_string(ch2AmplitudeVpp) + 
                "V,OFST,0V,PHSE," + std::to_string(ch2PhaseDeg));

	

	visaFlume.write("C2:MDWV FM,STATE,ON,FM,MDSP,SINE,SRC,CH1,DEVI," + 
                std::to_string(frequencyDeviationMHz * 1e6) + "HZ");

	//visaFlume.write("C2:BSWV WVTP,SINE,FRQ," + str(ch2FrequencyMHz * 1e6) + "HZ,AMP," + str(ch2AmplitudeVpp)
	//	+ "VPP,PHSE," + str(ch2PhaseDeg));
	//visaFlume.write("C2:MDWV FM");
	//visaFlume.write("C2:MDWV FM,STATE,ON,MDSP,SRCE,CH1,DEVI," + str(frequencyDeviationMHz * 1e6) + "HZ");

	// Do not set CH1 frequency here; CH1 waveform timing is determined by uploaded arb/sample-rate settings.
	visaFlume.write("C1:BSWV AMP," + str(ch1AmplitudeVpp) + "VPP,PHSE," + str(ch1StartPhaseDeg));

	visaFlume.write("C1:BTWV STATE,ON");
	visaFlume.write("C1:BTWV GATE_NCYC,NCYC");
	visaFlume.write("C1:BTWV TIME," + str(ch1BurstCycles));
	// Keep this period sequence as the final CH1 burst setup step.
	visaFlume.write("C1:BTWV TRSR,INT");
	visaFlume.write("C1:BTWV PRD,MIN");
	// Keep external trigger as the final state.
	visaFlume.write("C1:BTWV TRSR,EXT");

	outputOn(1);
	outputOn(2);
}

void SiglentCore::programSpecializedVariation(unsigned variation, std::vector<parameterType>& params,
	deviceOutputInfo& runSettings, ExpThreadWorker* expWorker)
{
	if (!runSettings.siglentFm.control) {
		return;
	}

	auto ch1BurstCyclesValue = runSettings.siglentFm.ch1BurstCycles.getValue(variation);
	auto roundedCycles = std::llround(ch1BurstCyclesValue);
	if (roundedCycles <= 0 || std::fabs(ch1BurstCyclesValue - roundedCycles) > 1e-6) {
		thrower("Siglent FM CH1 burst cycles must evaluate to a positive integer. Expression: "
			+ runSettings.siglentFm.ch1BurstCycles.expressionStr + ", value: " + str(ch1BurstCyclesValue));
	}

	notify({ "Programming Siglent FM workflow variation " + qstr(str(variation)) + "\n", 1 }, expWorker);
	programFmModulationProfile(runSettings.siglentFm.ch1AmplitudeVpp.getValue(variation),
		runSettings.siglentFm.ch1StartPhaseDeg.getValue(variation),
		runSettings.siglentFm.ch2FrequencyMHz.getValue(variation),
		runSettings.siglentFm.ch2AmplitudeVpp.getValue(variation),
		runSettings.siglentFm.ch2PhaseDeg.getValue(variation),
		runSettings.siglentFm.ch2FrequencyDeviationMHz.getValue(variation),
		runSettings.siglentFm.useExternalClock, static_cast<unsigned>(roundedCycles));

	// Block until the instrument reports all pending operations are complete.
	long opc = 0;
	visaFlume.query("*OPC?\n", opc);
	if (opc != 1) {
		thrower("Siglent did not report operation complete after FM variation programming (OPC=" + str(opc) + ").");
	}
}

