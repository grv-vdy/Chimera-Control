#include "stdafx.h"
#include "dcamapi4.h"
#include "dcamprop.h"
#include "HamamatsuCameraCore.h"
#include "HamamatsuRunMode.h"
#include "HamamatsuTriggerModes.h"
#include "PrimaryWindows/QtHamamatsuWindow.h"
#include <chrono>
#include <process.h>
#include <algorithm>
#include <numeric>
#include <cmath>

std::string HamamatsuCameraCore::getSystemInfo()
{
	return flume.getSystemInfo();
}

std::string HamamatsuCameraCore::getTimingSummary()
{
	return flume.getTimingSummary();
}

HamamatsuCameraCore::HamamatsuCameraCore(bool safemode_option) : flume(), safemode(safemode_option)
{
	cameraIsRunning = false;
	cameraThreadExitIndicator = false;
	plottingMutex = CreateMutex(NULL, FALSE, NULL);
	imagesMutex = CreateMutex(NULL, FALSE, NULL);
}

HamamatsuCameraCore::HamamatsuCameraCore() : HamamatsuCameraCore(HAM_SAFEMODE)
{
}

HamamatsuCameraCore::~HamamatsuCameraCore()
{
	if (plottingMutex != NULL)
	{
		CloseHandle(plottingMutex);
	}
	if (imagesMutex != NULL)
	{
		CloseHandle(imagesMutex);
	}
}

void HamamatsuCameraCore::initializeClass(chronoTimes* imageTimes)
{
	threadInput.imageTimes = imageTimes;
	threadInput.camera = this;
	threadInput.spuriousWakeupHandler = false;
	// begin the camera wait thread.
	_beginthreadex(NULL, 0, &HamamatsuCameraCore::cameraThread, &threadInput, 0, &cameraThreadID);
}

void HamamatsuCameraCore::updatePictureNumber(ULONGLONG newNumber)
{
	currentPictureNumber = newNumber;
}

void HamamatsuCameraCore::pauseThread()
{
	// Camera should not be taking images anymore at this point.
	threadInput.spuriousWakeupHandler = false;
}

void HamamatsuCameraCore::onFinish()
{
	threadInput.signaler.notify_all();
	cameraIsRunning = false;
	// Fully un-arm: stop the DCAM acquisition (dcamcap_stop) so the camera returns to idle and can be
	// cleanly re-armed for the next run. Guarded for safemode and wrapped so it never throws out of here
	// (this is called from GUI slots, the finish path, and abort).
	if (!HAM_SAFEMODE)
	{
		try { flume.abortAcquisition(); }
		catch (ChimeraError&) {}
	}
}

/*
 * this thread watches the camera for pictures and when it sees a picture lets the main thread know via a message.
 * it gets initialized at the start of the program and is basically always running.
 */
unsigned __stdcall HamamatsuCameraCore::cameraThread(void* voidPtr)
{
	cameraThreadInput* input = (cameraThreadInput*)voidPtr;
	std::unique_lock<std::mutex> lock(input->runMutex);
	int safeModeCount = 0;
	long pictureNumber = 0;
	bool armed = false;

	while (!input->camera->cameraThreadExitIndicator)
	{
		input->signaler.wait(lock, [input, &safeModeCount]() { return input->spuriousWakeupHandler; });
		if (!HAM_SAFEMODE)
		{
			try
			{
				std::cout << "Hello";

				if (armed && (input->camera->flagChecker) && input->camera->currentThreadPictureNumber == 0)
				{
					if (pictureNumber == 0)
					{
						// Acquisition finished
					}
				}
				else
				{
					if (pictureNumber % 2 == 0)
					{
						(*input->imageTimes).push_back(std::chrono::high_resolution_clock::now());
					}
					armed = true;

					if (true)
					{
						try
						{
							input->camera->flume.getAcquisitionProgress(pictureNumber);
							input->camera->initialChecker = true;
						}
						catch (ChimeraError& exception)
						{
							emit input->camera->cameraErrorOccurred(QString::fromStdString(exception.what()));
						}

						if (pictureNumber != 0)
						{
							if (input->camera->currentThreadPictureNumber == 0 && input->camera->initialChecker == true)
							{
								input->camera->currentThreadPictureNumber = 1;
								input->camera->frameIndexQueue.push(input->camera->currentThreadPictureNumber);
							}
							else if (input->camera->currentThreadPictureNumber < pictureNumber)
							{
								input->camera->currentThreadPictureNumber += 1;
								input->camera->frameIndexQueue.push(input->camera->currentThreadPictureNumber);
							}
							if (input->camera->currentThreadPictureNumber == input->camera->runSettings.totalPicsInExperiment())
							{
								pictureNumber = 0;
								input->camera->flagChecker = true;
								input->camera->initialChecker = false;
								input->camera->frameIndexQueue.push(-1);
								input->camera->currentThreadPictureNumber = 0;
							}
						}
					}
				}
			}
			catch (ChimeraError&)
			{
				// Error handling
			}
		}
		else
		{
			// simulate an actual wait.
			Sleep(100);
			if (pictureNumber % 2 == 0)
			{
				(*input->imageTimes).push_back(std::chrono::high_resolution_clock::now());
			}
			if (input->camera->cameraIsRunning && safeModeCount < input->camera->runSettings.totalPicsInExperiment())
			{
				if (input->camera->runSettings.cameraMode == "Kinetic Series Mode"
					|| input->camera->runSettings.cameraMode == "Accumulation Mode")
				{
					safeModeCount++;
					emit input->camera->cameraProgressUpdated(safeModeCount);
				}
				else
				{
					emit input->camera->cameraProgressUpdated(1);
				}
			}
			else
			{
				input->camera->cameraIsRunning = false;
				safeModeCount = 0;
				emit input->camera->cameraFinished();
				input->spuriousWakeupHandler = false;
			}
		}
	}
	return 0;
}

/*
 * Get whatever settings the camera is currently using in it's operation, assuming it's operating.
 */
HamamatsuRunSettings HamamatsuCameraCore::getSettings() const
{
	return runSettings;
}

HamamatsuRunSettings HamamatsuCameraCore::getSettingsFromConfig(ConfigStream& configFile) const
{
	HamamatsuRunSettings newSettings = runSettings;
	configFile >> newSettings.controlCamera;
	configFile >> newSettings.triggerMode;
	// Canonicalize the trigger token (maps legacy "External" -> "Edge"; defaults to Edge if unknown).
	try {
		newSettings.triggerMode = HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::fromStr(newSettings.triggerMode));
	}
	catch (ChimeraError&) {
		newSettings.triggerMode = HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Edge);
	}
	std::string acquisitionModeTxt;
	configFile >> acquisitionModeTxt;
	newSettings.acquisitionMode = static_cast<int>(HamamatsuRunModes::fromStr(acquisitionModeTxt));
	configFile >> newSettings.frameRate;
	configFile >> newSettings.temperatureSetting;

	unsigned exposureNum = 0;
	configFile >> exposureNum;
	newSettings.exposureTimes.clear();
	newSettings.exposureTimes.reserve(exposureNum);
	for (unsigned idx = 0; idx < exposureNum; ++idx) {
		double exposure = 0;
		configFile >> exposure;
		newSettings.exposureTimes.push_back(exposure);
	}

	// Continuous mode was removed; consume its (now-placeholder) token to keep the positional format.
	bool retiredContinuousMode = false;
	configFile >> retiredContinuousMode;
	configFile >> newSettings.picsPerRepetition;
	return newSettings;
}

void HamamatsuCameraCore::setSettings(HamamatsuRunSettings settingsToSet)
{
	runSettings = settingsToSet;
}

void HamamatsuCameraCore::programSettings()
{
	// Program the current run settings to the camera without starting an acquisition. Used by the
	// "Program Now" button (settings/temperature only); armCamera() also calls this before capturing.
	setImageParametersToCamera();

	// Program exposure(s) unless we're in Level trigger mode, where the exposure length is defined by
	// the external TTL high-time and the camera ignores the exposure-time property.
	if (runSettings.triggerMode != HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Level))
	{
		setExposures();
	}
	setTemperature();
	setCoolerMode();
	setFanMode();

	setCameraTriggerMode();
	setCameraMode();
}

void HamamatsuCameraCore::armCamera(QtHamamatsuWindow* camWin, double& minKineticCycleTime)
{
	/// Set a bunch of parameters.
	// Set to 1 MHz readout rate in both cases

	// deallocate any existing buffer because this can create problems
	DCAMERR err = DCAMERR_NONE;
	if (!HAM_SAFEMODE)
	{
		err = dcambuf_release(flume.hdcam);
	}

	flagChecker = false;
	currentFrameAccessedIndex = 0;

	int32 status;
	flume.queryStatus(status);

	// Push all the user settings to the camera (ROI, exposure, temperature, trigger, readout).
	programSettings();

	cameraIsRunning = true;

	// Allocate a recycling frame buffer. Rather than reserving one slot per picture for the whole run
	// (which scales unbounded with run length and is enormous over a frame grabber), size a ring just
	// big enough to absorb a brief reader stall; acquireImageData() copies each frame out promptly and
	// detects a lap if the reader ever falls too far behind.
	bufferFrameCount = computeCaptureBufferSize();
	if (!HAM_SAFEMODE)
	{
		err = dcambuf_alloc(flume.hdcam, bufferFrameCount);
		if (failed(err))
		{
			thrower("ERROR: dcambuf_alloc failed to allocate " + str(bufferFrameCount)
				+ " frames. DCAMERR code: " + str(static_cast<int>(err)));
		}
	}
	// The image-grabber thread (started by the window right after arming) is now the sole acquisition
	// loop, so the legacy internal cameraThread is intentionally left idle (not woken here).
	currentFrameIndex = 0;
	flume.startAcquisition();
}

int32 HamamatsuCameraCore::computeCaptureBufferSize()
{
	// The ring only needs to hold frames not yet copied out. Size it from a stall headroom, then clamp
	// so it never splits a repetition and never exceeds the whole (finite) run.
	const unsigned long long totalPics = runSettings.totalPicsInExperiment();
	const long long picsPerRep = runSettings.picsPerRepetition > 0
		? static_cast<long long>(runSettings.picsPerRepetition) : 1;

	// External triggers only: the incoming trigger rate is unknown, so use a repetition-based margin.
	long long ring = picsPerRep * CAPTURE_BUFFER_HEADROOM_REPS;
	if (ring < picsPerRep) { ring = picsPerRep; }                 // never split a repetition across a lap
	if (ring < CAPTURE_BUFFER_MIN_FRAMES) { ring = CAPTURE_BUFFER_MIN_FRAMES; }
	if (totalPics > 0 && ring > static_cast<long long>(totalPics)) { ring = static_cast<long long>(totalPics); }

	return static_cast<int32>(ring);
}

std::vector<std::vector<long>> HamamatsuCameraCore::acquireImageData()
{
	int size;
	int experimentPictureNumber;
	if (runSettings.showPicsInRealTime)
	{
		experimentPictureNumber = 0;
	}
	else
	{
		experimentPictureNumber = (((currentPictureNumber - 1) % runSettings.totalPicsInVariation())
			% runSettings.picsPerRepetition);
	}

	if (experimentPictureNumber == 0)
	{
		WaitForSingleObject(imagesMutex, INFINITE);
		imagesOfExperiment.clear();
		if (runSettings.showPicsInRealTime)
		{
			imagesOfExperiment.resize(1);
		}
		else
		{
			imagesOfExperiment.resize(runSettings.picsPerRepetition);
		}
		ReleaseMutex(imagesMutex);
	}

	const size_t imageWidth = static_cast<size_t>(runSettings.imageSettings.width());
	const size_t imageHeight = static_cast<size_t>(runSettings.imageSettings.height());
	const size_t imageSize = imageWidth * imageHeight;
	size = static_cast<int>(imageSize);
	std::vector<long> tempImage;
	tempImage.resize(imageSize);
	WaitForSingleObject(imagesMutex, INFINITE);
	imagesOfExperiment[experimentPictureNumber].resize(imageSize);
	ReleaseMutex(imagesMutex);
	if (!HAM_SAFEMODE)
	{
		// Lap detection: the recycling ring only holds the most recent bufferFrameCount frames. If the
		// reader has fallen so far behind that the frame we want was already overwritten, raise a clear
		// error instead of handing back a stale/torn frame.
		DCAMCAP_TRANSFERINFO xferInfo;
		memset(&xferInfo, 0, sizeof(xferInfo));
		xferInfo.size = sizeof(xferInfo);
		DCAMERR xerr = dcamcap_transferinfo(flume.hdcam, &xferInfo);
		if (!failed(xerr) && bufferFrameCount > 0)
		{
			const long long oldestStillBuffered =
				static_cast<long long>(xferInfo.nFrameCount) - static_cast<long long>(bufferFrameCount);
			if (static_cast<long long>(currentFrameIndex) < oldestStillBuffered)
			{
				thrower("ERROR: Hamamatsu frame " + str(currentFrameIndex) + " was overwritten before it "
					"could be read (reader fell behind the " + str(bufferFrameCount) + "-frame buffer). "
					"Frames were dropped - lower the frame rate or increase the buffer headroom.");
			}
		}

		DCAMBUF_FRAME bufframe;
		memset(&bufframe, 0, sizeof(bufframe));
		bufframe.size = sizeof(bufframe);
		// Absolute frame index (DCAM maps it into the ring). If this SDK build requires a ring-relative
		// index instead, change to (currentFrameIndex % bufferFrameCount).
		bufframe.iFrame = currentFrameIndex;
		currentFrameIndex += 1;

		DCAMERR err = dcambuf_lockframe(flume.hdcam, &bufframe);
		if (failed(err))
		{
			thrower("ERROR: dcambuf_lockframe failed. DCAMERR code: " + str(static_cast<int>(err)));
		}

		int32 width = bufframe.width;
		int32 height = bufframe.height;
		uint16_t* pSrc = static_cast<uint16_t*>(bufframe.buf);

		for (int row = 0; row < height; row++)
		{
			for (int col = 0; col < width; col++)
			{
				const size_t index = static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col);
				if (index < tempImage.size()) {
					tempImage[index] = static_cast<long>(*pSrc);
				}
				pSrc++;
			}
		}

		WaitForSingleObject(imagesMutex, INFINITE);
		imagesOfExperiment[experimentPictureNumber] = tempImage;
		ReleaseMutex(imagesMutex);
	}
	else
	{
		if (imageWidth == 0 || imageHeight == 0) {
			return imagesOfExperiment;
		}
		// generate a fake image.
		for (size_t imageVecInc = 0; imageVecInc < imagesOfExperiment[experimentPictureNumber].size(); imageVecInc++)
		{
			tempImage[imageVecInc] = rand() % 30 + 95;
		}
		WaitForSingleObject(imagesMutex, INFINITE);
		for (size_t imageVecInc = 0; imageVecInc < imagesOfExperiment[experimentPictureNumber].size(); imageVecInc++)
		{
			const size_t col = imageVecInc % imageWidth;
			const size_t row = imageVecInc / imageWidth;
			const size_t base = (col + 1) * imageHeight;
			if (base == 0 || row + 1 > base) {
				continue;
			}
			const size_t remapIndex = base - row - 1;
			if (remapIndex < tempImage.size()) {
				imagesOfExperiment[experimentPictureNumber][imageVecInc] = tempImage[remapIndex];
			}
		}
		ReleaseMutex(imagesMutex);
	}
	return imagesOfExperiment;
}

void HamamatsuCameraCore::setCameraTriggerMode()
{
	if (HAM_SAFEMODE)
	{
		// No hardware to program in safemode.
		return;
	}
	if (flume.hdcam == NULL)
	{
		thrower("ERROR: cannot set trigger mode; Hamamatsu camera handle is null.");
	}

	// Set a DCAM property and convert any failure into a ChimeraError (callers catch ChimeraError).
	auto setProp = [&](int32 idprop, double value, const std::string& what)
	{
		DCAMERR err = dcamprop_setvalue(flume.hdcam, idprop, value);
		if (failed(err))
		{
			thrower("ERROR: failed to set Hamamatsu trigger property (" + what
				+ "). DCAMERR code: " + str(static_cast<int>(err)));
		}
	};

	const std::string mode = runSettings.triggerMode;
	if (mode == HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Level))
	{
		// External level ("bulb"): shutter stays open while the TTL is high, so the exposure
		// length equals the TTL high-time and the exposure-time setting is ignored.
		setProp(DCAM_IDPROP_TRIGGERSOURCE, DCAMPROP_TRIGGERSOURCE__EXTERNAL, "source=external");
		setProp(DCAM_IDPROP_TRIGGERPOLARITY, DCAMPROP_TRIGGERPOLARITY__POSITIVE, "polarity=positive");
		setProp(DCAM_IDPROP_TRIGGER_MODE, DCAMPROP_TRIGGER_MODE__NORMAL, "trigger_mode=normal");
		setProp(DCAM_IDPROP_TRIGGERACTIVE, DCAMPROP_TRIGGERACTIVE__LEVEL, "active=level");
		// In Level mode the exposure is the TTL high-time, so hand exposure control to the trigger rather
		// than a fixed exposure-time value. Best-effort: not verified on this model and not essential, so
		// ignore any failure instead of throwing (it must not break Level arming).
		dcamprop_setvalue(flume.hdcam, DCAM_IDPROP_EXPOSURETIME_CONTROL, DCAMPROP_EXPOSURETIME_CONTROL__OFF);
	}
	else
	{
		// Edge (default): each rising external edge captures one frame using the configured
		// exposure time.
		setProp(DCAM_IDPROP_TRIGGERSOURCE, DCAMPROP_TRIGGERSOURCE__EXTERNAL, "source=external");
		setProp(DCAM_IDPROP_TRIGGERPOLARITY, DCAMPROP_TRIGGERPOLARITY__POSITIVE, "polarity=positive");
		setProp(DCAM_IDPROP_TRIGGER_MODE, DCAMPROP_TRIGGER_MODE__NORMAL, "trigger_mode=normal");
		setProp(DCAM_IDPROP_TRIGGERACTIVE, DCAMPROP_TRIGGERACTIVE__EDGE, "active=edge");
	}
}

void HamamatsuCameraCore::queueBuffers(ULONGLONG pictureNumber)
{
	UNREFERENCED_PARAMETER(pictureNumber);
}

void HamamatsuCameraCore::waitForAcquisition(ULONGLONG pictureNumber, unsigned timeoutMs)
{
	UNREFERENCED_PARAMETER(pictureNumber);
	UNREFERENCED_PARAMETER(timeoutMs);
	DCAMERR err = DCAMERR_NONE;
	flume.waitForAcquisition(err);
	if (err == DCAMERR_TIMEOUT) {
		thrower("AT_ERR_TIMEDOUT");
	}
	if (failed(err)) {
		thrower("ERROR: dcamwait_start failed. DCAMERR code: " + str(static_cast<int>(err)));
	}
}

void HamamatsuCameraCore::setExpRunningExposure(ULONGLONG pictureNumber)
{
	if (runSettings.exposureTimes.empty()) {
		return;
	}
	// In Level mode the exposure IS the TTL high-time; don't reprogram the exposure-time property (and
	// don't let setSingleExposure flip EXPOSURETIME_CONTROL back on, which would break Level).
	if (runSettings.triggerMode == HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Level)) {
		return;
	}
	// Cycle through the per-picture exposures within each repetition (exposureTimes has one entry per
	// picture, so pictureNumber % size == the picture index within the current repetition).
	const size_t idx = static_cast<size_t>(pictureNumber % runSettings.exposureTimes.size());
	flume.setSingleExposure(runSettings.exposureTimes[idx]);
}

int HamamatsuCameraCore::queryStatus()
{
	return flume.queryStatus();
}

void HamamatsuCameraCore::preparationChecks() const
{
	if (runSettings.picsPerRepetition == 0) {
		thrower("ERROR: pictures-per-repetition is zero.");
	}
	if (runSettings.exposureTimes.empty()) {
		thrower("ERROR: no exposure times configured.");
	}
}


void HamamatsuCameraCore::setTemperature()
{
	// Get the current temperature
	if (runSettings.temperatureSetting < -35 || runSettings.temperatureSetting > 25)
	{
		thrower("Warning: The selected temperature is outside the normal camera range (-35 through 25 C).");
	}
	// Procedure to initiate cooling
	changeTemperatureSetting(false);
}

void HamamatsuCameraCore::changeTemperatureSetting(bool turnTemperatureControlOff)
{
	char aBuffer[256];
	int minimumAllowedTemp, maximumAllowedTemp;
	// the default, in case the program is in safemode.
	minimumAllowedTemp = -35;
	maximumAllowedTemp = 25;
	// clear buffer
	wsprintf(aBuffer, "");
	// check if temp is in valid range
	if (runSettings.temperatureSetting < minimumAllowedTemp || runSettings.temperatureSetting > maximumAllowedTemp)
	{
		thrower("ERROR: Temperature is out of range\r\n");
	}
	else
	{
		// if it is in range, switch on cooler and set temp
		if (turnTemperatureControlOff == false)
		{
			flume.temperatureControlOn();
		}
		else
		{
			flume.temperatureControlOff();
		}
	}

	if (turnTemperatureControlOff == false)
	{
		try
		{
			DCAMERR err;
			err = dcamprop_setvalue(flume.hdcam, DCAM_IDPROP_SENSORTEMPERATURETARGET, runSettings.temperatureSetting);
			if (failed(err))
			{
				// Some camera states can temporarily reject setting target temperature.
				// Fall back to wrapper path without interrupting acquisition/program flow.
				flume.setTemperature(runSettings.temperatureSetting);
			}
		}
		catch (ChimeraError& err)
		{
			// Non-fatal for run startup; keep going and let periodic status display reflect actual camera temperature.
			OutputDebugStringA((std::string("Hamamatsu temperature set warning: ") + err.what() + "\n").c_str());
		}
	}
	else
	{
		thrower("Temperature Control has been turned off.\r\n");
	}
}

void HamamatsuCameraCore::setExposures()
{
	if (runSettings.exposureTimes.size() > 0 && runSettings.exposureTimes.size() <= 16)
	{
		try
		{
			flume.setSingleExposure(runSettings.exposureTimes[0]);
		}
		catch (ChimeraError& err)
		{
			errBox(std::string("ERROR: ") + err.what());
		}
	}
	else
	{
		thrower("ERROR: Invalid size for vector of exposure times, value of " + std::to_string(runSettings.exposureTimes.size()) + ".");
	}
}

void HamamatsuCameraCore::setImageParametersToCamera()
{
	// setImage(hBin, vBin, lBorder, rBorder, tBorder, bBorder): horizontal comes from left/right, vertical
	// from top/bottom. (These were previously passed scrambled.)
	flume.setImage(runSettings.imageSettings.horizontalBinning, runSettings.imageSettings.verticalBinning,
		runSettings.imageSettings.left, runSettings.imageSettings.right,
		runSettings.imageSettings.top, runSettings.imageSettings.bottom);
}

void HamamatsuCameraCore::setCameraMode()
{
	if (HAM_SAFEMODE)
	{
		return;
	}
	// Map the selected run mode to the camera's readout-speed property. Standard scan -> fastest
	// readout; Slow scan -> slowest readout (lower read noise).
	int32 readoutSpeed = (runSettings.acquisitionMode == static_cast<int>(HamamatsuRunModes::mode::Slow))
		? DCAMPROP_READOUTSPEED__SLOWEST
		: DCAMPROP_READOUTSPEED__FASTEST;
	flume.setReadoutSpeed(readoutSpeed);
}

void HamamatsuCameraCore::setCoolerMode()
{
	if (HAM_SAFEMODE)
	{
		return;
	}
	flume.setCoolerMode(runSettings.coolerMode);
}

void HamamatsuCameraCore::setFanMode()
{
	if (HAM_SAFEMODE)
	{
		return;
	}
	flume.setFanMode(runSettings.fanMode);
}

bool HamamatsuCameraCore::isRunning() const
{
	return cameraIsRunning;
}

bool HamamatsuCameraCore::isCalibrating() const
{
	return calibrating;
}

void HamamatsuCameraCore::setCalibrating(bool calibratingOption)
{
	calibrating = calibratingOption;
}

void HamamatsuCameraCore::setIsRunningState(bool state)
{
	cameraIsRunning = state;
}

HamamatsuRunSettings HamamatsuCameraCore::getHamamatsuRunSettings() const
{
	return runSettings;
}

HamamatsuTemperatureStatus HamamatsuCameraCore::getTemperature()
{
	HamamatsuTemperatureStatus status;
	status.temperature = runSettings.temperatureSetting;
	status.temperatureSetting = runSettings.temperatureSetting;
	status.msg = "Temperature read pending";
	status.colorCode = QColor(255, 215, 0); // Gold

	if (safemode || HAM_SAFEMODE)
	{
		status.msg = "Camera safemode: sensor temperature unavailable";
		status.colorCode = QColor(160, 160, 160);
		return status;
	}

	try
	{
		double sensorTemp = 0.0;
		flume.getTemperature(sensorTemp);
		status.temperature = static_cast<int>(sensorTemp);

		int delta = abs(status.temperature - status.temperatureSetting);
		if (delta <= 1)
		{
			status.msg = "Sensor temperature at target";
			status.colorCode = QColor(0, 128, 0);
		}
		else
		{
			status.msg = "Cooling to target temperature";
			status.colorCode = QColor(255, 165, 0);
		}
	}
	catch (ChimeraError& err)
	{
		status.msg = "Failed to read sensor temperature";
		status.hamamatsuRawMsg = err.what();
		status.colorCode = QColor(220, 20, 60);
	}

	return status;
}

atomGrid HamamatsuCameraCore::getMainAtomGrid()
{
	// Return the main atom grid
	atomGrid grid;
	return grid;
}

std::string HamamatsuCameraCore::getMostRecentDateString()
{
	// Return the most recent date string
	return "";
}

int HamamatsuCameraCore::getMostRecentFid()
{
	// Return the most recent frame ID
	return 0;
}

int HamamatsuCameraCore::getPicsPerRep() const
{
	return runSettings.picsPerRepetition;
}

ThreadsafeQueue<NormalImage>* HamamatsuCameraCore::getGrabberQueue()
{
	// Return pointer to the image queue used by the grabber thread
	// This should be created or managed by the camera thread
	static ThreadsafeQueue<NormalImage> grabberQueue;
	return &grabberQueue;
}

std::pair<int, int> HamamatsuCameraCore::getCurrentRepVarNumber(ULONGLONG pictureNumber) const
{
	// Calculate current repetition and variation number from picture number
	if (runSettings.picsPerRepetition == 0) {
		return { 0, 0 };
	}
	int rep = pictureNumber / runSettings.picsPerRepetition;
	int var = pictureNumber % runSettings.picsPerRepetition;
	return { rep, var };
}

