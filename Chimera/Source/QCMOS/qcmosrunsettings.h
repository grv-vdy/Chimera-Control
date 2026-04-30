#pragma once

#include <string>
#include <vector>

#include <GeneralImaging/imageParameters.h>

struct QCMOSRunSettings {
    imageParameters imageSettings;

    int acquisitionMode = 3;
    std::string triggerMode = "External Trigger";
    std::string cameraMode = "Standard Scan";

    std::vector<double> exposureTimes = {0.001};

    unsigned picsPerRepetition = 1;
    unsigned long long repetitionsPerVariation = 1;
    unsigned long long totalVariations = 1;
    unsigned long long totalPicsInVariation = 1;
    int totalPicsInExperiment = 1;

    int temperatureSetting = 0;
};

using qcmosRunSettings = QCMOSRunSettings;