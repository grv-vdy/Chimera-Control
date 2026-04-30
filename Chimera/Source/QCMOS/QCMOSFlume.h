#pragma once

#include <string>
#include <vector>

#include <GeneralImaging/imageParameters.h>

#include "dcamapi4.h"
#include "dcamprop.h"

class QCMOSFlume {
public:
    QCMOSFlume() = delete;
    explicit QCMOSFlume(bool safemodeOpt);
    QCMOSFlume(const QCMOSFlume&) = delete;
    QCMOSFlume& operator=(const QCMOSFlume&) = delete;
    ~QCMOSFlume();

    void initialize();
    void shutdown();

    std::string getSystemInfo();

    void setExposureSeconds(double exposureSec);
    void setTriggerMode(const std::string& triggerMode);
    void setImage(const imageParameters& imageSettings);
    void setTemperature(int temperatureC);

    void allocateBuffer(int frameCount);
    void releaseBuffer();
    void startAcquisition();
    void abortAcquisition();

    long waitForAndGetLatestFrameCount(int timeoutMs);
    void getOldestImage(std::vector<long>& dataArray, int frameIndex);

private:
    const bool safemode;
    bool initialized = false;

    HDCAM hdcam = nullptr;
    HDCAMWAIT hwait = nullptr;
    DCAMAPI_INIT apiinit = {};
    DCAMDEV_OPEN devopen = {};
    DCAMWAIT_OPEN waitopen = {};
};
