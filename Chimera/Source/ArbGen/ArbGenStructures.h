// created by Mark O. Brown
#pragma once

#include "ScriptedArbGenWaveform.h"
#include "ArbGenChannelMode.h"
#include "ArbGenSettings.h"
#include "ParameterSystem/Expression.h"
#include "WieserlabsDDS/WieserlabsDDSStructures.h"
#include <string>
#include <array>

//class Agilent;

struct minMaxDoublet{
	double min;
	double max;
};


struct generalArbGenOutputInfo{
	bool useCal = false;
};

struct scriptedArbInfo : public generalArbGenOutputInfo {
	Expression fileAddress = "";
	ScriptedArbGenWaveform wave;
};


struct dcInfo : public generalArbGenOutputInfo {
	Expression dcLevel;
};


struct squareInfo : public generalArbGenOutputInfo {
	Expression frequency;
	Expression amplitude;
	Expression offset;
	Expression dutyCycle;
	Expression phase;
	bool burstMode = false;
};


struct sineInfo : public generalArbGenOutputInfo {
	Expression frequency;
	Expression amplitude;
	Expression phase;
	bool burstMode = false;
};


struct preloadedArbInfo : public generalArbGenOutputInfo {
	// The only reason at this time to make this an expression instead of a normal string is to make sure it gets 
	// outputted to the config file correctly in case it's empty. 
	Expression address = "";
	bool burstMode = false;
};


struct siglentFmInfo {
	bool control = false;
	bool useExternalClock = true;
	Expression ch1PulseDurationMs = "1.0";
	Expression ch1AmplitudeVpp = "1.0";
	Expression ch1StartPhaseDeg = "0";
	Expression ch1BurstCycles = "1";
	Expression ch2FrequencyMHz = "100.0";
	Expression ch2AmplitudeVpp = "2.0";
	Expression ch2PhaseDeg = "0";
	Expression ch2FrequencyDeviationMHz = "0.05";
};


struct channelInfo{
	ArbGenChannelMode::which option = ArbGenChannelMode::which::No_Control;
	bool polarityInvert;
	dcInfo dc;
	sineInfo sine;
	squareInfo square;
	preloadedArbInfo preloadedArb;
	scriptedArbInfo scriptedArb;
};


struct deviceOutputInfo{
	// first ([0]) is channel 1, second ([1]) is channel 2.
	std::array<channelInfo, 2> channel;
	bool synced = false;
	siglentFmInfo siglentFm;
	bool wieserlabsControl = false;
	std::vector<wieserlabsDdsChannel> snapshot;
};

