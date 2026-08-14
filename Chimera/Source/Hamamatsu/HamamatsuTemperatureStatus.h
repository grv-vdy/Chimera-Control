#pragma once
#include <string>
#include <qcolor.h>

struct HamamatsuTemperatureStatus
{
	int temperature;
	int temperatureSetting;
	std::string hamamatsuRawMsg;
	std::string msg;
	QColor colorCode;
};
