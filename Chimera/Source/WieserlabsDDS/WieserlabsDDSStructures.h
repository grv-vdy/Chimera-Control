#pragma once
#include <string>
#include <vector>

enum class WieserlabsDDSType {
	WieserlabsDDS
};

namespace WieserlabsDDSEnum {
	constexpr WieserlabsDDSType allWls[] = { WieserlabsDDSType::WieserlabsDDS };
}

struct wieserlabsDdsChannel {
	bool on = false;
	double frequency = 0.0; // Hz
	double amplitude = 0.0; // 0-1 (normalized)
	double phase = 0.0; // degrees
};

struct wieserlabsDdsSettings {
	bool safemode;
	std::string ipAddress;
	int ipPort;
	std::string deviceName;
	std::pair<unsigned, unsigned> triggerLine;
	std::string configurationFileDelimiter;
};

struct scriptedWieserlabsDDSInfo {
	std::vector<wieserlabsDdsChannel> channels;
	std::string scriptName;
	std::string scriptAddress;
	bool isScript;
};