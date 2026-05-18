#pragma once
#include "ArbGenSettings.h"

enum class ArbGenType {
	Agilent, Siglent
};


/*this name is only for assisting the coding, not related to script at all*/
struct ArbGenEnum {
	enum class name {
		Siglent0, Siglent1
	};
	static const std::array<name, numArbGen> allAgs;
	static std::string toStr (name m_) {
		switch (m_) {
		case name::Siglent0:
			return UWAVE_SIGLENT_SETTINGS.deviceName;
		case name::Siglent1:
			return UWAVE_SIGLENT_SETTINGS_2.deviceName;
		}
		return "";
	}
	static name fromStr (std::string txt) {
		if (txt == "Cryo Siglent") {
			return name::Siglent0;
		}
		if (txt == "siglent_AWG" || txt == "siglent_AWG1" || txt == "siglent_AWG_1") {
			return name::Siglent0;
		}
		if (txt == "Cryo Siglent 2" || txt == "Cryo Siglent_2") {
			return name::Siglent1;
		}
		if (txt == UWAVE_SIGLENT_SETTINGS.configurationFileDelimiter) {
			return name::Siglent0;
		}
		if (txt == UWAVE_SIGLENT_SETTINGS_2.configurationFileDelimiter) {
			return name::Siglent1;
		}
		for (auto opt : allAgs) {
			if (toStr (opt) == txt) {
				return opt;
			}
		}
		thrower ("Failed to convert string to Which ArbGen option!");
		return name::Siglent0;
	}
};
