#pragma once
#include "LowLevel/constants.h"
//#include "ArbGenStructures.h"

struct arbGenSettings {
	bool safemode;
	std::string address;
	unsigned long sampleRate;
	// "INT" or "USB"
	std::string memoryLocation;
	std::string deviceName;

	unsigned /*long*/ triggerRow;
	unsigned /*long*/ triggerNumber;
	std::string configurationFileDelimiter;

	std::vector<double> calibrationCoeff;
	std::vector<std::string> setupCommands;
};

const arbGenSettings UWAVE_SIGLENT_SETTINGS = {
	// safemode option											
	UWAVE_SAFEMODE_SIG,
	// tcpip address
	UWAVE_SIGLENT_ADDRESSES[0],
	// sample rate in hertz
	20e6,
	// Memory location, whether the device will save waveforms to 
	// the internal 64MB Memory buffer or to an external USB drive, which
	// can (obviously) have much more space.
	"INT",
	// device name (just a convenience, so that the class instance knows 
	// which device it is
	"siglent_AWG",
	UWAVE_SIGLENT_TRIGGER_LINE.first, UWAVE_SIGLENT_TRIGGER_LINE.second,
	// Configuration file delimiter, used for saving settings for this 
	// agilent.
	"SIGLENT_AWG",
	// Calibration coefficients (arb length)
	{ },
	/**********make sure be in DDS mode to change burst and sweep***********/
	{ "C1:OUTPUT OFF", "C2:OUTPUT OFF","C1:SRATE MODE,DDS","C2:SRATE MODE,DDS",
	"MODE PHASE-LOCKED",/*Both DDS reset when changing frequency. Phase deviation between Ch1&2 is maintained. This command somehow does not work*/
	"C1:BTWV STATE,ON", "C1:BTWV TRSR,EXT,GATE_NCYC,NCYC,EDGE,RISE,TIME,1,STPS,0","C1:BTWV STATE,OFF",
	"C2:BTWV STATE,ON", "C2:BTWV TRSR,EXT,GATE_NCYC,NCYC,EDGE,RISE,TIME,1,STPS,0","C2:BTWV STATE,OFF",
	/*"C1:SRATE MODE,TARB","C2:SRATE MODE,TARB",*/"C1:OUTPUT LOAD,HZ,PLRT,NOR", "C2:OUTPUT LOAD,HZ,PLRT,NOR" } // In TrueArb mode, can not use burst
};

const arbGenSettings UWAVE_SIGLENT_SETTINGS_2 = {
	UWAVE_SAFEMODE_SIG,
	UWAVE_SIGLENT_ADDRESSES[1],
	20e6,
	"INT",
	"siglent_AWG_2",
	UWAVE_SIGLENT_TRIGGER_LINE.first, UWAVE_SIGLENT_TRIGGER_LINE.second,
	"SIGLENT_AWG_2",
	{ },
	{ "C1:OUTPUT OFF", "C2:OUTPUT OFF","C1:SRATE MODE,DDS","C2:SRATE MODE,DDS",
	"MODE PHASE-LOCKED",
	"C1:BTWV STATE,ON", "C1:BTWV TRSR,EXT,GATE_NCYC,NCYC,EDGE,RISE,TIME,1,STPS,0","C1:BTWV STATE,OFF",
	"C2:BTWV STATE,ON", "C2:BTWV TRSR,EXT,GATE_NCYC,NCYC,EDGE,RISE,TIME,1,STPS,0","C2:BTWV STATE,OFF",
	"C1:OUTPUT LOAD,HZ,PLRT,NOR", "C2:OUTPUT LOAD,HZ,PLRT,NOR" }
};

