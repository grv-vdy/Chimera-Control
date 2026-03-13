#pragma once
#include <string>
#include <vector>
#include <ParameterSystem/Expression.h>

enum class WieserlabsDDSType {
	WieserlabsDDS
};

namespace WieserlabsDDSEnum {
	constexpr WieserlabsDDSType allWls[] = { WieserlabsDDSType::WieserlabsDDS };
}

struct wieserlabsDdsChannel {
	bool on = false;
	double frequency = 0.0; // Hz
	Expression frequencyExpression = "0";
	double amplitude = 0.0; // 0-1 (normalized)
	double phase = 0.0; // degrees
};

struct wieserlabsDdsSettings {
	bool safemode;
	std::string ipAddress;
	int ipPort;
	std::string deviceName;
	std::pair<unsigned, unsigned> triggerLineCh0; // BNC_IN_A for Channel 0
	std::pair<unsigned, unsigned> triggerLineCh1; // BNC_IN_A for Channel 1
	std::string configurationFileDelimiter;
};

struct scriptedWieserlabsDDSInfo {
	std::vector<wieserlabsDdsChannel> channels;
	std::string scriptName;
	std::string scriptAddress;
	bool isScript;
};