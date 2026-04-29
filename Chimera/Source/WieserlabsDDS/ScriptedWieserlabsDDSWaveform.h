// created by Mark O. Brown
#pragma once
#include <string>
#include <vector>
#include <array>
#include "Scripts/ScriptStream.h"

struct DdsCommand {
	std::string type; // "tone", "ramp", "off", "bnc", "fmenable", "fmdisable", "fmgain"
	double preDelayMs; // delay before this command executes (in milliseconds)
	double delayMs;   // inter-command delay after this command (in milliseconds)
	int channel;
	double startFreq;
	double endFreq;
	double amplitude;
	double duration;  // ramp duration (in milliseconds)
	double phase;
	int bncValue;     // for bnc command: 0=LOW, 1=HIGH (controls BNC C output)
	int fmGain = -1;  // optional FM gain override (0-15), used by fmenable/fmgain
};

class ScriptedWieserlabsDDSWaveform
{
public:
	ScriptedWieserlabsDDSWaveform();
	bool analyzeWieserlabsDDSScriptCommand(ScriptStream& script, std::vector<parameterType>& params,
		std::string& warnings, unsigned variation = 0);
	bool isVaried();
	void calculateAllSegmentVariations(unsigned totalNumVariations, std::vector<parameterType>& variables);
	std::string returnSequenceString();
	std::string getScriptText();
	const std::vector<DdsCommand>& getCommandList() const { return commandList; }

protected:
	std::string scriptText;
	std::vector<std::string> commands;
	std::vector<DdsCommand> commandList;
	double nextCommandStartMs = 0.0;
	std::array<double, 2> channelEndMs = { 0.0, 0.0 };
};