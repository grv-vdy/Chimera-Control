#pragma once
#include "imageParameters.h"
#include <string>
#include <vector>
#include <Windows.h>

// this structure contains all of the main options which are necessary to set when starting a camera acquisition. All
// of these settings should be possibly modified by the user of the UI.
struct HamamatsuRunSettings
{
	imageParameters imageSettings;
	// NOTE: these defaults matter. currentlyRunningSettings is displayed by "View Running Settings"
	// before any experiment has run, so every POD field must be valid (e.g. acquisitionMode must map
	// to a real HamamatsuRunModes value, picsPerRepetition must be non-zero).
	bool emGainModeIsOn = false;
	int emGainLevel = 0;
	int readMode = 0;
	int acquisitionMode = 0;          // 0 == HamamatsuRunModes::mode::Standard
	int frameTransferMode = 0;
	std::string triggerMode;
	std::string coolerMode;
	std::string fanMode;
	std::string cameraMode;
	float frameRate = 1.0f;
	bool controlCamera = true;
	bool showPicsInRealTime = false;
	//
	float kineticCycleTime = 0.0f;
	float accumulationTime = 0.0f;
	int accumulationNumber = 1;
	std::vector<double> exposureTimes;
	//
	UINT picsPerRepetition = 1;
	ULONGLONG repetitionsPerVariation = 1;
	bool repFirst = false;
	ULONGLONG totalVariations = 1;
	unsigned __int64 totalPicsInVariation();
	unsigned long long totalPicsInExperiment();
	//
	int temperatureSetting = -20;
	//
	//int fanMode;
	int VSSpeed = 0;
};
