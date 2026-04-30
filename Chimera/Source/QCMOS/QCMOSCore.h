#pragma once

#include <vector>
#include <string>

#include <GeneralObjects/Matrix.h>

#include "QCMOSFlume.h"
#include "QCMOSRunSettings.h"

class QCMOSCore {
public:
    QCMOSCore();
    explicit QCMOSCore(bool safemodeOpt);

    QCMOSRunSettings getSettings() const;
    void setSettings(const QCMOSRunSettings& settingsToSet);

    void armCamera(double& minKineticCycleTime);
    std::vector<Matrix<long>> acquireImageData();

    void setTemperature();
    void setExposures();
    void setImageParametersToCamera();
    void setCameraTriggerMode();

    void onFinish();
    bool isRunning() const;
    void setIsRunningState(bool state);

    std::string getSystemInfo();
    void abortAcquisition();

    QCMOSFlume& getFlume();
    const QCMOSFlume& getFlume() const;

private:
    QCMOSRunSettings runSettings;
    QCMOSFlume flume;

    bool cameraIsRunning = false;
    long currentFrameIndex = 0;
};
