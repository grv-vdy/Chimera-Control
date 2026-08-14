#pragma once
#include "dcamapi4.h"
#include "dcamprop.h"
#include <string>

/// /////////////////////////////////////////////////////
/// 
///			The Hamamatsu Flume (SDK Wrapper) Class
///
/// /////////////////////////////////////////////////////

// This class is a wrapper around the DCAM-API (Hamamatsu SDK).
// It provides a cleaner interface for low-level camera operations.

class HamamatsuFlume
{
public:

	DCAMAPI_INIT apiinit;
	DCAMDEV_OPEN devopen;
	DCAMWAIT_OPEN waitopen;
	HDCAMWAIT hwait;
	DCAMWAIT_START waitstart;

	// THIS CLASS IS NOT COPYABLE.
	HamamatsuFlume& operator=(const HamamatsuFlume&) = delete;
	HamamatsuFlume(const HamamatsuFlume&) = delete;

	HamamatsuFlume();
	~HamamatsuFlume();

	// Acquisition control
	void startAcquisition();
	void abortAcquisition();
	void waitForAcquisition(DCAMERR& err);

	// Image operations
	void getAcquisitionProgress(long& seriesNumber);
	void getAcquisitionProgress(long& accumulationNumber, long& seriesNumber);
	int queryStatus();
	void queryStatus(int32& status);

	// Exposure and timing
	void getAcquisitionTimes(float& exposure, float& accumulation, float& kinetic);
	void setRingExposureTimes(int sizeOfTimesArray, double* arrayOfTimes);
	void setSingleExposure(double exposureTime);

	// Image parameters
	void setImage(int hBin, int vBin, int lBorder, int rBorder, int tBorder, int bBorder);

	// Temperature control
	void setTemperature(int temp);
	void temperatureControlOn();
	void temperatureControlOff();
	void getTemperature(double& temp);
	void getTemperatureRange(int& min, int& max);
	int getTemperatureCode();

	// Camera information
	std::string getHeadModel();
	std::string getSystemInfo();
	// Camera-reported timing (readout time, min trigger interval, exposure-change lockout window).
	std::string getTimingSummary();

	// Camera mode operations
	void setReadoutSpeed(int32 readoutSpeedValue);
	void setFanMode(const std::string& mode);
	void setCoolerMode(const std::string& mode);

	// SDK handles (accessible by core class)
	HDCAM hdcam;

private:
	
};
