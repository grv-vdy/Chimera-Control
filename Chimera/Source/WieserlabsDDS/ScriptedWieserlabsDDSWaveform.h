// created by Mark O. Brown
#pragma once
#include <string>
#include <vector>
#include "Scripts/ScriptStream.h"

struct DdsCommand {
	std::string type; // "tone", "ramp", "off"
	double preDelayMs; // delay before this command executes (in milliseconds)
	double delayMs;   // inter-command delay after this command (in milliseconds)
	int channel;
	double startFreq;
	double endFreq;
	double amplitude;
	double duration;  // ramp duration (in milliseconds)
	double phase;
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
	double lastCommandEndMs = 0.0;
};