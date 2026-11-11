#pragma once
#include "GeneralImaging/softwareAccumulationOption.h"
#include <array>
#include <vector>

struct hamamatsuPicSettingsGroup{
	std::vector<int> colors = std::vector<int>(4, 0);
	//std::vector<float> exposureTimesUnofficial;
	std::vector<std::string> thresholdStrs = std::vector<std::string>(4, "100");
	std::vector<std::vector<int>> thresholds = std::vector<std::vector<int>>(4, std::vector<int>{100});
	std::vector<softwareAccumulationOption> saOpts = std::vector<softwareAccumulationOption>(4);
	std::string tMode;
	int picScaleFactor = 50;
};


