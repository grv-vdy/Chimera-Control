#include "stdafx.h"
#include "QCMOSFlume.h"

#include <cstdlib>
#include <stdexcept>

QCMOSFlume::QCMOSFlume(bool safemodeOpt) : safemode(safemodeOpt) {}

QCMOSFlume::~QCMOSFlume() {
    shutdown();
}

void QCMOSFlume::initialize() {
    if (safemode || initialized) {
        initialized = true;
        return;
    }

    apiinit = {};
    apiinit.size = sizeof(apiinit);
    DCAMERR err = dcamapi_init(&apiinit);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: dcamapi_init failed.");
    }

    devopen = {};
    devopen.size = sizeof(devopen);
    devopen.index = 0;
    err = dcamdev_open(&devopen);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: dcamdev_open failed.");
    }
    hdcam = devopen.hdcam;

    waitopen = {};
    waitopen.size = sizeof(waitopen);
    waitopen.hdcam = hdcam;
    err = dcamwait_open(&waitopen);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: dcamwait_open failed.");
    }
    hwait = waitopen.hwait;

    initialized = true;
}

void QCMOSFlume::shutdown() {
    if (!initialized || safemode) {
        initialized = false;
        return;
    }

    if (hdcam != nullptr) {
        dcamcap_stop(hdcam);
        dcambuf_release(hdcam);
    }
    if (hwait != nullptr) {
        dcamwait_close(hwait);
        hwait = nullptr;
    }
    if (hdcam != nullptr) {
        dcamdev_close(hdcam);
        hdcam = nullptr;
    }
    dcamapi_uninit();
    initialized = false;
}

std::string QCMOSFlume::getSystemInfo() {
    if (safemode) {
        return "QCMOS SAFEMODE";
    }
    initialize();

    char model[256] = {};
    char cameraid[64] = {};
    DCAMDEV_STRING param = {};
    param.size = sizeof(param);

    param.text = model;
    param.textbytes = sizeof(model);
    param.iString = DCAM_IDSTR_MODEL;
    DCAMERR err = dcamdev_getstring(hdcam, &param);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to query model string.");
    }

    param.text = cameraid;
    param.textbytes = sizeof(cameraid);
    param.iString = DCAM_IDSTR_CAMERAID;
    err = dcamdev_getstring(hdcam, &param);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to query camera id string.");
    }

    return std::string("Camera Model: ") + model + "\nCamera Serial Number: " + cameraid + "\n";
}

void QCMOSFlume::setExposureSeconds(double exposureSec) {
    if (safemode) {
        return;
    }
    initialize();
    DCAMERR err = dcamprop_setvalue(hdcam, DCAM_IDPROP_EXPOSURETIME, exposureSec);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to set exposure time.");
    }
}

void QCMOSFlume::setTriggerMode(const std::string& triggerMode) {
    if (safemode) {
        return;
    }
    initialize();

    DCAMERR err;
    if (triggerMode == "Internal Trigger") {
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_TRIGGERSOURCE, DCAMPROP_TRIGGERSOURCE__INTERNAL);
    }
    else {
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_TRIGGERSOURCE, DCAMPROP_TRIGGERSOURCE__EXTERNAL);
        if (!failed(err)) {
            err = dcamprop_setvalue(hdcam, DCAM_IDPROP_TRIGGERACTIVE, DCAMPROP_TRIGGERACTIVE__EDGE);
        }
    }

    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to set trigger mode.");
    }
}

void QCMOSFlume::setImage(const imageParameters& imageSettings) {
    if (safemode) {
        return;
    }
    initialize();

    int32 hSize = static_cast<int32>(std::abs(imageSettings.right - imageSettings.left));
    int32 vSize = static_cast<int32>(std::abs(imageSettings.top - imageSettings.bottom));

    DCAMERR err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYMODE, DCAMPROP_MODE__OFF);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to disable subarray mode.");
    }

    err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYHSIZE, hSize);
    if (!failed(err)) {
        err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SUBARRAYVSIZE, vSize);
    }
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to set image dimensions.");
    }
}

void QCMOSFlume::setTemperature(int temperatureC) {
    if (safemode) {
        return;
    }
    initialize();
    DCAMERR err = dcamprop_setvalue(hdcam, DCAM_IDPROP_SENSORTEMPERATURETARGET, temperatureC);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to set temperature target.");
    }
}

void QCMOSFlume::allocateBuffer(int frameCount) {
    if (safemode) {
        return;
    }
    initialize();
    releaseBuffer();
    DCAMERR err = dcambuf_alloc(hdcam, frameCount);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to allocate camera buffer.");
    }
}

void QCMOSFlume::releaseBuffer() {
    if (safemode || !initialized) {
        return;
    }
    dcambuf_release(hdcam);
}

void QCMOSFlume::startAcquisition() {
    if (safemode) {
        return;
    }
    initialize();
    DCAMERR err = dcamcap_start(hdcam, DCAMCAP_START_SNAP);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to start acquisition.");
    }
}

void QCMOSFlume::abortAcquisition() {
    if (safemode || !initialized) {
        return;
    }
    dcamcap_stop(hdcam);
}

long QCMOSFlume::waitForAndGetLatestFrameCount(int timeoutMs) {
    if (safemode) {
        return 0;
    }
    initialize();

    DCAMWAIT_START waitstart = {};
    waitstart.size = sizeof(waitstart);
    waitstart.eventmask = DCAMWAIT_CAPEVENT_FRAMEREADY;
    waitstart.timeout = timeoutMs;
    DCAMERR err = dcamwait_start(hwait, &waitstart);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: frame wait failed.");
    }

    DCAMCAP_TRANSFERINFO info = {};
    info.size = sizeof(info);
    err = dcamcap_transferinfo(hdcam, &info);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: transfer info query failed.");
    }
    return info.nFrameCount;
}

void QCMOSFlume::getOldestImage(std::vector<long>& dataArray, int frameIndex) {
    if (safemode) {
        return;
    }
    initialize();

    DCAMBUF_FRAME frame = {};
    frame.size = sizeof(frame);
    frame.iFrame = frameIndex;
    DCAMERR err = dcambuf_lockframe(hdcam, &frame);
    if (failed(err)) {
        throw std::runtime_error("QCMOSFlume: failed to lock frame.");
    }

    auto* src = reinterpret_cast<const unsigned short*>(frame.buf);
    const int pixelCount = frame.width * frame.height;
    dataArray.resize(pixelCount);
    for (int i = 0; i < pixelCount; ++i) {
        dataArray[i] = static_cast<long>(src[i]);
    }
}
