// created by Mark O. Brown
#pragma once
#include "ConfigurationSystems/Version.h"
#include <string>

class QtMainWindow;
class QtAuxiliaryWindow;

// This configuration system is different in style from the other configuration file system. This is designed to do more auto-saving and 
// auto-load at the beginning of the experiment. There is only supposed to be one such configuration file
class MasterConfiguration{
	public:
		MasterConfiguration(std::string address);
		void save(QtMainWindow* mainWin, QtAuxiliaryWindow* auxWin);
		void load(QtMainWindow* mainWin, QtAuxiliaryWindow* auxWin);
	private:
		const std::string configurationFileAddress;
		const Version version = Version("1.0");
};

