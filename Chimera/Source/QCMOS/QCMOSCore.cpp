#include "stdafx.h"

#include "QCMOSCore.h"

QCMOSCore::QCMOSCore() : QCMOSCore(HAM_SAFEMODE) {}

QCMOSCore::QCMOSCore(bool safemodeOpt) : flume(safemodeOpt) {}

QCMOSRunSettings QCMOSCore::getSettings() const {
    return runSettings;
}

void QCMOSCore::setSettings(const QCMOSRunSettings& settingsToSet) {
    runSettings = settingsToSet;
}

void QCMOSCore::armCamera(double& minKineticCycleTime) {
    minKineticCycleTime = 0.0;

    setImageParametersToCamera();
    setExposures();
    setTemperature();
    setCameraTriggerMode();

    flume.allocateBuffer(runSettings.totalPicsInExperiment > 0 ? runSettings.totalPicsInExperiment : 1);
    flume.startAcquisition();

    currentFrameIndex = 0;
    cameraIsRunning = true;
}

std::vector<Matrix<long>> QCMOSCore::acquireImageData() {
    std::vector<Matrix<long>> images;
    if (!cameraIsRunning) {
        return images;
    }

    flume.waitForAndGetLatestFrameCount(50);

    std::vector<long> frameVec;
    flume.getOldestImage(frameVec, static_cast<int>(currentFrameIndex));

    int width = abs(runSettings.imageSettings.right - runSettings.imageSettings.left);
    int height = abs(runSettings.imageSettings.top - runSettings.imageSettings.bottom);
    if (width <= 0 || height <= 0) {
        width = 1;
        height = static_cast<int>(frameVec.size());
    }

    Matrix<long> frame(static_cast<unsigned>(height), static_cast<unsigned>(width));
    for (size_t i = 0; i < frameVec.size() && i < frame.size(); ++i) {
        frame.data[i] = frameVec[i];
    }

    images.push_back(frame);
    currentFrameIndex++;
    return images;
}

void QCMOSCore::setTemperature() {
    flume.setTemperature(runSettings.temperatureSetting);
}

void QCMOSCore::setExposures() {
    double exposureSec = runSettings.exposureTimes.empty() ? 0.001 : runSettings.exposureTimes.front();
    flume.setExposureSeconds(exposureSec);
}

void QCMOSCore::setImageParametersToCamera() {
    flume.setImage(runSettings.imageSettings);
}

void QCMOSCore::setCameraTriggerMode() {
    flume.setTriggerMode(runSettings.triggerMode);
}

void QCMOSCore::onFinish() {
    abortAcquisition();
}

bool QCMOSCore::isRunning() const {
    return cameraIsRunning;
}

void QCMOSCore::setIsRunningState(bool state) {
    cameraIsRunning = state;
}

std::string QCMOSCore::getSystemInfo() {
    return flume.getSystemInfo();
}

void QCMOSCore::abortAcquisition() {
    flume.abortAcquisition();
    flume.releaseBuffer();
    cameraIsRunning = false;
}

QCMOSFlume& QCMOSCore::getFlume() {
    return flume;
}

const QCMOSFlume& QCMOSCore::getFlume() const {
    return flume;
}
