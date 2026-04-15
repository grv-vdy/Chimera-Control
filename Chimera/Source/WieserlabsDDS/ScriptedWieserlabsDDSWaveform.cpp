#include "stdafx.h"
#include "ScriptedWieserlabsDDSWaveform.h"
#include "ParameterSystem/ParameterSystemStructures.h"
#include "ParameterSystem/Expression.h"
#include <boost/algorithm/string.hpp>
#include <qdebug.h>
#include <algorithm>

ScriptedWieserlabsDDSWaveform::ScriptedWieserlabsDDSWaveform()
{
}

bool ScriptedWieserlabsDDSWaveform::analyzeWieserlabsDDSScriptCommand(ScriptStream& script, std::vector<parameterType>& params,
	std::string& warnings, unsigned variation)
{
	std::string line = script.getline();
	boost::trim(line);
	if (line.empty()) {
		return true;
	}
	
	std::stringstream lineStream(line);
	std::string command;
	lineStream >> command;
	boost::to_lower(command);

	auto evalToken = [&](const std::string& token, double& valueOut) -> bool {
		try {
			valueOut = std::stod(token);
			return true;
		}
		catch (...) {
		}

		try {
			Expression expr(token);
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
					expr.assertValid(params, scope);
					valueOut = expr.evaluate(params, variation);
					return true;
				}
				catch (...) {
				}
			}
		}
		catch (...) {
		}

		return false;
	};

	auto finalizeCommand = [&](DdsCommand& cmdObj) {
		if (cmdObj.channel < 0 || cmdObj.channel > 1) {
			warnings += "DDS command channel must be 0 or 1\n";
			return;
		}

		double& channelTimelineEndMs = channelEndMs[cmdObj.channel];
		double scheduledStartMs = nextCommandStartMs;
		if (scheduledStartMs < channelTimelineEndMs) {
			warnings += "Requested DDS command start time is earlier than channel timeline; clamping to channel time\n";
			scheduledStartMs = channelTimelineEndMs;
		}

		cmdObj.preDelayMs = std::max(0.0, scheduledStartMs - channelTimelineEndMs);

		double intrinsicDurationMs = 0.0;
		if (cmdObj.type == "ramp") {
			intrinsicDurationMs = std::max(0.0, cmdObj.duration);
		}

		channelTimelineEndMs = scheduledStartMs + intrinsicDurationMs + std::max(0.0, cmdObj.delayMs);
		commandList.push_back(cmdObj);
	};

	if (command == "t") {
		std::string assignToken;
		lineStream >> assignToken;
		if (assignToken != "=" && assignToken != "+=") {
			warnings += "Invalid time assignment syntax. Use: t += 5, t = +5, or t = 50\n";
			return true;
		}

		std::string valueToken;
		lineStream >> valueToken;
		if (valueToken.empty()) {
			warnings += "Missing time value after time assignment\n";
			return true;
		}

		try {
			double valueMs = 0.0;
			if (!evalToken(valueToken, valueMs)) {
				warnings += "Invalid time value in time assignment\n";
				scriptText += line + "\n";
				return true;
			}
			if (assignToken == "+=" || valueToken[0] == '+') {
				nextCommandStartMs += valueMs;
			}
			else {
				nextCommandStartMs = valueMs;
			}
		}
		catch (...) {
			warnings += "Invalid time value in time assignment\n";
		}

		scriptText += line + "\n";
		return true;
	}

	if (command == "tone") {
		// Syntax: tone channel frequency amplitude [phase] [delay:X]
		std::string channelStr, freqStr, ampStr, phaseStr = "0", delayStr = "";
		lineStream >> channelStr >> freqStr >> ampStr;
		
		std::string token;
		while (lineStream >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			} else {
				phaseStr = token; // assume it's phase if not a delay spec
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "tone";
		cmdObj.preDelayMs = 0.0;
		cmdObj.channel = std::stoi(channelStr);
		if (!evalToken(freqStr, cmdObj.startFreq) || !evalToken(ampStr, cmdObj.amplitude)) {
			warnings += "Invalid expression in tone command\n";
			return true;
		}
		cmdObj.endFreq = cmdObj.startFreq;
		cmdObj.duration = 0.0;
		cmdObj.bncValue = 0;
		if (phaseStr.empty()) {
			cmdObj.phase = 0.0;
		}
		else if (!evalToken(phaseStr, cmdObj.phase)) {
			warnings += "Invalid phase expression in tone command\n";
			return true;
		}
		cmdObj.delayMs = 0.0;
		
		if (!delayStr.empty()) {
			try {
				if (!evalToken(delayStr.substr(6), cmdObj.delayMs)) {
					warnings += "Invalid delay format in tone command\n";
				}
			} catch (...) {
				warnings += "Invalid delay format in tone command\n";
			}
		}

		finalizeCommand(cmdObj);
		scriptText += command + " " + channelStr + " " + freqStr + " " + ampStr + 
			" " + phaseStr + (delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else if (command == "ramp") {
		// Syntax: ramp channel start_freq end_freq amplitude duration [phase] [delay:X]
		std::string channelStr, startFreqStr, endFreqStr, ampStr, durationStr, phaseStr = "0", delayStr = "";
		lineStream >> channelStr >> startFreqStr >> endFreqStr >> ampStr >> durationStr;
		
		std::string token;
		while (lineStream >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			} else {
				phaseStr = token; // assume it's phase if not a delay spec
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "ramp";
		cmdObj.preDelayMs = 0.0;
		cmdObj.channel = std::stoi(channelStr);
		if (!evalToken(startFreqStr, cmdObj.startFreq) || !evalToken(endFreqStr, cmdObj.endFreq)
			|| !evalToken(ampStr, cmdObj.amplitude) || !evalToken(durationStr, cmdObj.duration)) {
			warnings += "Invalid expression in ramp command\n";
			return true;
		}
		cmdObj.bncValue = 0;
		if (phaseStr.empty()) {
			cmdObj.phase = 0.0;
		}
		else if (!evalToken(phaseStr, cmdObj.phase)) {
			warnings += "Invalid phase expression in ramp command\n";
			return true;
		}
		cmdObj.delayMs = 0.0;
		
		if (!delayStr.empty()) {
			try {
				if (!evalToken(delayStr.substr(6), cmdObj.delayMs)) {
					warnings += "Invalid delay format in ramp command\n";
				}
			} catch (...) {
				warnings += "Invalid delay format in ramp command\n";
			}
		}

		finalizeCommand(cmdObj);
		scriptText += command + " " + channelStr + " " + startFreqStr + " " + endFreqStr + 
			" " + ampStr + " " + durationStr + " " + phaseStr + 
			(delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else if (command == "off") {
		// Syntax: off channel [delay:X]
		std::string channelStr, delayStr = "";
		lineStream >> channelStr;
		
		std::string token;
		while (lineStream >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "off";
		cmdObj.preDelayMs = 0.0;
		cmdObj.channel = std::stoi(channelStr);
		cmdObj.startFreq = 0.0;
		cmdObj.endFreq = 0.0;
		cmdObj.amplitude = 0.0;
		cmdObj.duration = 0.0;
		cmdObj.phase = 0.0;
		cmdObj.delayMs = 0.0;
		cmdObj.bncValue = 0;
		
		if (!delayStr.empty()) {
			try {
				if (!evalToken(delayStr.substr(6), cmdObj.delayMs)) {
					warnings += "Invalid delay format in off command\n";
				}
			} catch (...) {
				warnings += "Invalid delay format in off command\n";
			}
		}

		finalizeCommand(cmdObj);
		scriptText += command + " " + channelStr + (delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else if (command == "bnc") {
		// Syntax: bnc value [delay:X]
		// value: 0 = BNC C LOW, 1 = BNC C HIGH
		// Note: Only DCP channel 0 can write to BNC configuration registers
		std::string valueStr, delayStr = "";
		lineStream >> valueStr;
		
		std::string token;
		while (lineStream >> token) {
			boost::to_lower(token);
			if (token.find("delay:") == 0) {
				delayStr = token;
			}
		}

		DdsCommand cmdObj;
		cmdObj.type = "bnc";
		cmdObj.preDelayMs = 0.0;
		cmdObj.channel = 0; // BNC config registers only accessible via DCP channel 0
		cmdObj.startFreq = 0.0;
		cmdObj.endFreq = 0.0;
		cmdObj.amplitude = 0.0;
		cmdObj.duration = 0.0;
		cmdObj.phase = 0.0;
		cmdObj.delayMs = 0.0;
		cmdObj.bncValue = std::stoi(valueStr);
		
		if (!delayStr.empty()) {
			try {
				if (!evalToken(delayStr.substr(6), cmdObj.delayMs)) {
					warnings += "Invalid delay format in bnc command\n";
				}
			} catch (...) {
				warnings += "Invalid delay format in bnc command\n";
			}
		}

		finalizeCommand(cmdObj);
		scriptText += command + " " + valueStr + (delayStr.empty() ? "" : " " + delayStr) + "\n";
	}
	else {
		warnings += "Unrecognized Wieserlabs DDS command: " + command + "\n";
		return true;
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