#pragma once
#include "CameraImageDimensions.h"
#include "HamamatsuRunSettings.h"
#include "HamamatsuFlume.h"
#include "HamamatsuTemperatureStatus.h"
#include "RealTimeDataAnalysis/atomGrid.h"
#include "GeneralObjects/Queues.h"
#include "GeneralObjects/commonTypes.h"
#include <QObject>
#include <process.h>
#include <mutex>
#include "dcamapi4.h"
#include "dcamprop.h"
#include "GeneralObjects/ThreadsafeQueue.h"
#include "ConfigurationSystems/ConfigStream.h"
#include <string>


/// /////////////////////////////////////////////////////
/// 
///			The Hamamatsu Camera Core Class
///
/// /////////////////////////////////////////////////////

// This class is designed to facilitate interaction with the Hamamatsu camera and
// is based around the DCAM API. It is largely based on what was previously written for the ANDOR camera.

class HamamatsuCameraCore;
class QtHamamatsuWindow;

struct cameraThreadInput
{
	bool spuriousWakeupHandler;
	std::mutex runMutex;
	std::condition_variable signaler;
	HamamatsuCameraCore* camera;
	std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>>* imageTimes;
};

/// the all-important camera class.
class HamamatsuCameraCore : public QObject
{
	Q_OBJECT
public:

	DCAMAPI_INIT apiinit;
	DCAMDEV_OPEN devopen;
	DCAMWAIT_OPEN waitopen;
	HDCAMWAIT hwait;
	DCAMWAIT_START waitstart;


	// THIS CLASS IS NOT COPYABLE.
	HamamatsuCameraCore& operator=(const HamamatsuCameraCore&) = delete;
	HamamatsuCameraCore(const HamamatsuCameraCore&) = delete;

	explicit HamamatsuCameraCore(bool safemode_option);
	HamamatsuCameraCore();
	~HamamatsuCameraCore();

	// Define initial, flag and current frame accessed index
	int currentFrameAccessedIndex;
	bool flagChecker;
	bool initialChecker = false;
	ThreadsafeQueue<int> frameIndexQueue;
	int currentThreadPictureNumber = 0;

	int currentFrameIndex;
	// Number of frames in the recycling capture ring allocated for the current run.
	int32 bufferFrameCount = 0;

	// Main public methods
	HamamatsuRunSettings getSettings() const;
	HamamatsuRunSettings getSettingsFromConfig(ConfigStream& configFile) const;
	std::string getDelim() { return "CAMERA_SETTINGS"; }
	void setSettings(HamamatsuRunSettings settingsToSet);
	void pauseThread();
	// Program the current settings to the camera without starting an acquisition (temperature, ROI,
	// exposure, trigger, readout). "Program Now" uses this; armCamera also calls it before capturing.
	void programSettings();
	void armCamera(QtHamamatsuWindow* camWin, double& minKineticCycleTime);
	std::vector<std::vector<long>> acquireImageData();
	void setTemperature();
	void setExposures();
	void setExpRunningExposure(ULONGLONG pictureNumber);
	void setImageParametersToCamera();
	void setCameraMode();
	// Program the sensor cooler (On/Off/Max) and cooler fan (On/Off) from the current run settings.
	void setCoolerMode();
	void setFanMode();
	void setCameraTriggerMode();
	// Compute the recycling capture-ring size (frames) from the current run settings.
	int32 computeCaptureBufferSize();
	void queueBuffers(ULONGLONG pictureNumber);
	void waitForAcquisition(ULONGLONG pictureNumber, unsigned timeoutMs);
	int queryStatus();
	void preparationChecks() const;
	void onFinish();
	bool isRunning() const;
	bool isCalibrating() const;
	void setCalibrating(bool calibratingOption);
	void setIsRunningState(bool state);
	void updatePictureNumber(ULONGLONG newNumber);
	void changeTemperatureSetting(bool temperatureControlOff);

	static UINT __stdcall cameraThread(void* voidPtr);

	void initializeClass(chronoTimes* imageTimes);
	std::string getSystemInfo();
	// Camera-reported timing for the current settings (readout, min trigger interval, exposure lockout).
	std::string getTimingSummary();
	HamamatsuRunSettings getHamamatsuRunSettings() const;
	HamamatsuTemperatureStatus getTemperature();
	atomGrid getMainAtomGrid();
	std::string getMostRecentDateString();
	int getMostRecentFid();
	int getPicsPerRep() const;
	ThreadsafeQueue<NormalImage>* getGrabberQueue();
	std::pair<int, int> getCurrentRepVarNumber(ULONGLONG pictureNumber) const;

signals:
	void cameraProgressUpdated(int count);
	void cameraErrorOccurred(QString errorMsg);
	void cameraFinished();

public:
	std::array<int, 4> picturesToDraw = { 0, 1, 2, 3 };

	/// These are official settings and are the final say on what the camera does.
	/// Some unofficial settings are stored in smaller classes.
	HamamatsuRunSettings runSettings;
	bool safemode = false;
	bool cameraIsRunning;
	bool threadExpectingAcquisition = false;
	bool cameraThreadExitIndicator = false;
	bool calibrating = false;
private:
	// Capture-ring sizing (see computeCaptureBufferSize). These are tuning values, not hard caps:
	// the ring only has to cover a brief reader stall; the software image queue is the elastic buffer.
	static constexpr int    CAPTURE_BUFFER_HEADROOM_REPS = 4;      // Repetitions of headroom.
	static constexpr int32  CAPTURE_BUFFER_MIN_FRAMES = 4;         // Absolute floor.

	imageParameters readImageParameters;
	imageParameters runningImageParameters;
	
	
	bool plotThreadExitIndicator;
	

	ULONGLONG currentPictureNumber;
	ULONGLONG currentRepetitionNumber;

	HANDLE plottingMutex;
	HANDLE imagesMutex;
	
	std::vector<std::vector<long>> imagesOfExperiment;
	std::vector<std::vector<long>> imageVecQueue;
	UINT cameraThreadID = 0;

	cameraThreadInput threadInput;

	// The low-level SDK wrapper
	HamamatsuFlume flume;
};
