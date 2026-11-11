#include "stdafx.h"
#include "dcamapi4.h"
#include "dcamprop.h"
#include "HamamatsuFlume.h"
#include "PrimaryWindows/QtHamamatsuWindow.h"
#include <chrono>
#include <algorithm>
#include <numeric>

HamamatsuFlume::HamamatsuFlume()
{
	std::string errorMessage;
	if (HAM_SAFEMODE)
	{
		errorMessage = "Hamamatsu Camera is in SAFEMODE: Initialization Not attempted.";
	}

	try
	{
		memset(&apiinit, 0, sizeof(apiinit));
		apiinit.size = sizeof(apiinit);

		DCAMERR err;
		err = dcamapi_init(&apiinit);
	}
	catch (ChimeraError& err)
	{
		errBox(err.trace());
	}

	try
	{
		DCAMERR err;
		memset(&devopen, 0, sizeof(devopen));
		devopen.size = sizeof(devopen);
		devopen.index = 0;
		err = dcamdev_open(&devopen);
		hdcam = devopen.hdcam;

		memset(&waitopen, 0, sizeof(waitopen));
		waitopen.size = sizeof(waitopen);
		waitopen.hdcam = hdcam;
		err = dcamwait_open(&waitopen);
		hwait = waitopen.hwait;

		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_CAPTUREMODE, DCAMPROP_CAPTUREMODE__NORMAL);
		std::cout << "Hamamatsu Camera Initialized";
	}
	catch (ChimeraError& error)
	{
		thrower("ERROR: Hamamatsu camera initialization failed: " + error.trace());
	}
}

HamamatsuFlume::~HamamatsuFlume()
{
}

void HamamatsuFlume::startAcquisition()
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		err = dcamcap_start(hdcam, DCAMCAP_START_SEQUENCE);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
		}
	}
}

void HamamatsuFlume::abortAcquisition()
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		err = dcamcap_stop(hdcam);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
		}
	}
}

void HamamatsuFlume::waitForAcquisition(DCAMERR& err)
{
	if (!HAM_SAFEMODE)
	{
		DCAMWAIT_START waitstart;
		memset(&waitstart, 0, sizeof(waitstart));
		waitstart.size = sizeof(waitstart);
		waitstart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
		waitstart.timeout = 1000;
		err = dcamwait_start(hwait, &waitstart);
	}
}

void HamamatsuFlume::getAcquisitionProgress(long& seriesNumber)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		DCAMCAP_TRANSFERINFO captransferinfo;
		memset(&captransferinfo, 0, sizeof(captransferinfo));
		captransferinfo.size = sizeof(captransferinfo);

		err = dcamcap_transferinfo(hdcam, &captransferinfo);
		if (!failed(err))
		{
			seriesNumber = captransferinfo.nFrameCount;
		}
	}
}

void HamamatsuFlume::getAcquisitionProgress(long& accumulationNumber, long& seriesNumber)
{
	if (!HAM_SAFEMODE)
	{
		getAcquisitionProgress(seriesNumber);
		accumulationNumber = seriesNumber;
	}
}

int HamamatsuFlume::queryStatus()
{
	int32 status;
	queryStatus(status);
	return status;
}

void HamamatsuFlume::queryStatus(int32& status)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		err = dcamcap_status(hdcam, &status);
		if (!failed(err))
		{
			return;
		}
	}
}

void HamamatsuFlume::getAcquisitionTimes(float& exposure, float& accumulation, float& kinetic)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		double expTime;
		err = dcamprop_getvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, &expTime);
		exposure = static_cast<float>(expTime);

		accumulation = 0.0f;
		kinetic = 0.0f;
	}
}

void HamamatsuFlume::setRingExposureTimes(int sizeOfTimesArray, double* arrayOfTimes)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME_CONTROL, DCAMPROP_MODE__ON);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
		}
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, arrayOfTimes[0]);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
		}
	}
}

void HamamatsuFlume::setSingleExposure(double exposureTime)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		try
		{
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME_CONTROL, DCAMPROP_MODE__ON);
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}

		try
		{
			double internalFrameRate;
			double minTrigger;
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, exposureTime);
			err = dcamprop_getvalue(hdcam, DCAM_IDPROP_INTERNALFRAMERATE, &internalFrameRate);
			err = dcamprop_getvalue(hdcam, DCAM_IDPROP_TIMING_MINTRIGGERINTERVAL, &minTrigger);
			if (err == 1)
			{
				std::cout << "ERROR";
			}
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}
	}
}

void HamamatsuFlume::setImage(int hBin, int vBin, int lBorder, int rBorder, int tBorder, int bBorder)
{
	// The ROI edges are 1-indexed and inclusive (imageParameters convention: width = right-left+1).
	// DCAM wants a 0-indexed start POSITION and a pixel COUNT (size); on Hamamatsu both must be multiples
	// of 4. Use the smaller edge as the start (so top/bottom order doesn't matter) and an inclusive count
	// so the subarray size equals imageParameters.width()/height() and the displayed image lines up.
	int32 hStart = (lBorder < rBorder) ? lBorder : rBorder;
	int32 vStart = (tBorder < bBorder) ? tBorder : bBorder;
	int32 lB = hStart - 1;                       // 0-indexed left position
	int32 vB = vStart - 1;                       // 0-indexed top position
	int32 hSize = abs(rBorder - lBorder) + 1;    // width  (== imageParameters.width())
	int32 vSize = abs(bBorder - tBorder) + 1;    // height (== imageParameters.height())

	if (!HAM_SAFEMODE)
	{
		DCAMERR err;

		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_BINNING_INDEPENDENT, DCAMPROP_MODE__ON);

		try
		{
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF);
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}

		try
		{
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF);
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}

		// Hamamatsu subarrays require position AND size to be multiples of 4 and within the sensor.
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYHSIZE, hSize);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu rejected subarray WIDTH (H size) = " + str(hSize)
				+ " (must be a multiple of 4). DCAMERR code: " + str(static_cast<int>(err)));
		}
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYHPOS, lB);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu rejected subarray H POSITION (left) = " + str(lB)
				+ " (must be a multiple of 4). DCAMERR code: " + str(static_cast<int>(err)));
		}
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYVSIZE, vSize);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu rejected subarray HEIGHT (V size) = " + str(vSize)
				+ " (must be a multiple of 4). DCAMERR code: " + str(static_cast<int>(err)));
		}
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYVPOS, vB);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu rejected subarray V POSITION (top) = " + str(vB)
				+ " (must be a multiple of 4). DCAMERR code: " + str(static_cast<int>(err)));
		}
		err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__ON);
		if (failed(err))
		{
			thrower("ERROR: Hamamatsu rejected enabling subarray mode. DCAMERR code: "
				+ str(static_cast<int>(err)));
		}
	}
}

void HamamatsuFlume::setTemperature(int temp)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		try
		{
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORTEMPERATURETARGET, temp);
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}
	}
}

void HamamatsuFlume::temperatureControlOn()
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		try
		{
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLER, DCAMPROP_SENSORCOOLER__ON);
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}
	}
}

void HamamatsuFlume::temperatureControlOff()
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		try
		{
			err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLER, DCAMPROP_SENSORCOOLER__OFF);
		}
		catch (ChimeraError& err)
		{
			errBox(err.trace());
		}
	}
}

void HamamatsuFlume::getTemperature(double& temp)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		err = dcamprop_getvalue(hdcam, DCAM_IDPROP_SENSORTEMPERATURE, &temp);
		if (failed(err))
		{
			thrower("ERROR: Failed to read Hamamatsu sensor temperature.");
		}
	}
}

void HamamatsuFlume::getTemperatureRange(int& min, int& max)
{
	min = -35;
	max = 25;
}

int HamamatsuFlume::getTemperatureCode()
{
	double temp;
	getTemperature(temp);
	return static_cast<int>(temp);
}

std::string HamamatsuFlume::getHeadModel()
{
	DCAMERR err;

	char model[256] = {};
	DCAMDEV_STRING param;
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.text = model;
	param.textbytes = sizeof(model);
	param.iString = DCAM_IDSTR_MODEL;
	err = dcamdev_getstring(hdcam, &param);
	if (failed(err))
	{
		thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
	}

	std::string info;
	info = "Camera Model: " + std::string(model) + "\n";
	return info;
}

std::string HamamatsuFlume::getSystemInfo()
{
	DCAMERR err;

	char model[256] = {};
	char cameraid[64] = {};
	DCAMDEV_STRING param;
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.text = model;
	param.textbytes = sizeof(model);
	param.iString = DCAM_IDSTR_MODEL;
	err = dcamdev_getstring(hdcam, &param);
	if (failed(err))
	{
		thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
	}

	param.text = cameraid;
	param.textbytes = sizeof(cameraid);
	param.iString = DCAM_IDSTR_CAMERAID;
	err = dcamdev_getstring(hdcam, &param);
	if (failed(err))
	{
		thrower("ERROR: Hamamatsu DCAM call failed. DCAMERR code: " + str(static_cast<int>(err)));
	}
	std::string info;
	info += "Camera Model: " + std::string(model) + "\n";
	info += "Camera Serial Number: " + std::string(cameraid) + "\n";
	return info;
}

std::string HamamatsuFlume::getTimingSummary()
{
	if (HAM_SAFEMODE)
	{
		return "timing unavailable (safemode)";
	}
	// All are read-only, in seconds. They reflect the CURRENT settings (exposure, ROI, readout, trigger),
	// so query them after programming settings for meaningful numbers.
	double readout = 0.0, minTrigInterval = 0.0, invalidExpPeriod = 0.0;
	dcamprop_getvalue(hdcam, DCAM_IDPROP_TIMING_READOUTTIME, &readout);
	dcamprop_getvalue(hdcam, DCAM_IDPROP_TIMING_MINTRIGGERINTERVAL, &minTrigInterval);
	dcamprop_getvalue(hdcam, DCAM_IDPROP_TIMING_INVALIDEXPOSUREPERIOD, &invalidExpPeriod);
	return "readout = " + str(readout * 1000.0, 4) + " ms, min trigger interval = "
		+ str(minTrigInterval * 1000.0, 4) + " ms, exposure-change lockout = "
		+ str(invalidExpPeriod * 1000.0, 4) + " ms";
}

void HamamatsuFlume::setReadoutSpeed(int32 readoutSpeedValue)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err = dcamprop_setvalue(hdcam, DCAM_IDPROP_READOUTSPEED, readoutSpeedValue);
		if (failed(err))
		{
			thrower("ERROR: failed to set Hamamatsu readout speed (DCAM_IDPROP_READOUTSPEED = "
				+ str(static_cast<int>(readoutSpeedValue)) + "). DCAMERR code: " + str(static_cast<int>(err)));
		}
	}
}

void HamamatsuFlume::setFanMode(const std::string& mode)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		try
		{
			if (mode == "Fan On")
			{
				err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLERFAN, DCAMPROP_MODE__ON);
			}
			else
			{
				err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLERFAN, DCAMPROP_MODE__OFF);
				std::cout << "Fan Off";
			}
		}
		catch (ChimeraError& err)
		{
			errBox(err.what());
		}
	}
}

void HamamatsuFlume::setCoolerMode(const std::string& mode)
{
	if (!HAM_SAFEMODE)
	{
		DCAMERR err;
		try
		{
			if (mode == "Sensor Cooler On")
			{
				err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLER, DCAMPROP_SENSORCOOLER__ON);
			}
			else if (mode == "Sensor Cooler Off")
			{
				err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLER, DCAMPROP_SENSORCOOLER__OFF);
			}
			else if (mode == "Sensor Cooler Max")
			{
				err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORCOOLER, DCAMPROP_SENSORCOOLER__MAX);
			}
		}
		catch (ChimeraError& err)
		{
			errBox(err.what());
		}
	}
}
