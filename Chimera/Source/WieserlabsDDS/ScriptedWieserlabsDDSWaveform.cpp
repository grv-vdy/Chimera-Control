#include "stdafx.h"
#include "ScriptedWieserlabsDDSWaveform.h"
#include "ParameterSystem/ParameterSystemStructures.h"
#include <boost/algorithm/string.hpp>
#include <qdebug.h>

ScriptedWieserlabsDDSWaveform::ScriptedWieserlabsDDSWaveform()
{
}

bool ScriptedWieserlabsDDSWaveform::analyzeWieserlabsDDSScriptCommand(ScriptStream& script, std::vector<parameterType>& params,
	std::string& warnings)
{
	std::string command;
	script >> command;
	boost::to_lower(command);

	if (command == "tone") {
		// Syntax: tone channel frequency amplitude [phase] [delay:X]
		std::string channelStr, freqStr, ampStr, phaseStr = "0", delayStr = "";
		script >> channelStr >> freqStr >> ampStr;
		
		std::string token;
		while (script >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			} else {
				phaseStr = token; // assume it's phase if not a delay spec
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "tone";
		cmdObj.channel = std::stoi(channelStr);
		cmdObj.startFreq = std::stod(freqStr);
		cmdObj.amplitude = std::stod(ampStr);
		cmdObj.phase = phaseStr.empty() ? 0.0 : std::stod(phaseStr);
		cmdObj.delayMs = 0.0;
		
		if (!delayStr.empty()) {
			try {
				cmdObj.delayMs = std::stod(delayStr.substr(6)) * 1000.0; // delay:X in seconds -> ms
			} catch (...) {
				warnings += "Invalid delay format in tone command\n";
			}
		}

		commandList.push_back(cmdObj);
		scriptText += command + " " + channelStr + " " + freqStr + " " + ampStr + 
			" " + phaseStr + (delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else if (command == "ramp") {
		// Syntax: ramp channel start_freq end_freq amplitude duration [phase] [delay:X]
		std::string channelStr, startFreqStr, endFreqStr, ampStr, durationStr, phaseStr = "0", delayStr = "";
		script >> channelStr >> startFreqStr >> endFreqStr >> ampStr >> durationStr;
		
		std::string token;
		while (script >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			} else {
				phaseStr = token; // assume it's phase if not a delay spec
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "ramp";
		cmdObj.channel = std::stoi(channelStr);
		cmdObj.startFreq = std::stod(startFreqStr);
		cmdObj.endFreq = std::stod(endFreqStr);
		cmdObj.amplitude = std::stod(ampStr);
		cmdObj.duration = std::stod(durationStr);
		cmdObj.phase = phaseStr.empty() ? 0.0 : std::stod(phaseStr);
		cmdObj.delayMs = 0.0;
		
		if (!delayStr.empty()) {
			try {
				cmdObj.delayMs = std::stod(delayStr.substr(6)) * 1000.0; // delay:X in seconds -> ms
			} catch (...) {
				warnings += "Invalid delay format in ramp command\n";
			}
		}

		commandList.push_back(cmdObj);
		scriptText += command + " " + channelStr + " " + startFreqStr + " " + endFreqStr + 
			" " + ampStr + " " + durationStr + " " + phaseStr + 
			(delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else if (command == "off") {
		// Syntax: off channel [delay:X]
		std::string channelStr, delayStr = "";
		script >> channelStr;
		
		std::string token;
		if (script >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "off";
		cmdObj.channel = std::stoi(channelStr);
		cmdObj.delayMs = 0.0;
		
		if (!delayStr.empty()) {
			try {
				cmdObj.delayMs = std::stod(delayStr.substr(6)) * 1000.0; // delay:X in seconds -> ms
			} catch (...) {
				warnings += "Invalid delay format in off command\n";
			}
		}

		commandList.push_back(cmdObj);
		scriptText += command + " " + channelStr + (delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else {
		warnings += "Unrecognized Wieserlabs DDS command: " + command + "\n";
		return false;
	}

	return true;
}

bool ScriptedWieserlabsDDSWaveform::isVaried()
{
	// Check if any parameters are varied
	return false; // For now, assume not varied
}

void ScriptedWieserlabsDDSWaveform::calculateAllSegmentVariations(unsigned totalNumVariations, std::vector<parameterType>& variables)
{
	// Calculate variations if needed
}

std::string ScriptedWieserlabsDDSWaveform::returnSequenceString()
{
	return scriptText;
}

std::string ScriptedWieserlabsDDSWaveform::getScriptText()
{
	return scriptText;
}