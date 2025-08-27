#pragma once
#include <GeneralObjects/IDeviceCore.h>
#include <ParameterSystem/ParameterSystemStructures.h>
#include "StaticDirectDigitalSynthesis/StaticDdsStructures.h" // Include the definition of StaticDDSSettings
#include "StaticDirectDigitalSynthesis/StaticDdsFlume.h" // Include the definition of StaticDDSFlume



class ConfigStream;
class ExpThreadWorker;
class DataLogger;
class StaticDdsCore : public IDeviceCore
{
public:
    // THIS CLASS IS NOT COPYABLE.
    StaticDdsCore(const StaticDdsCore&) = delete;
    StaticDdsCore& operator=(const StaticDdsCore&) = delete;

    StaticDdsCore(
    const bool safemode,
    const std::array<std::string, STATICDDS_NUMBER>& port,
    const std::array<unsigned int, STATICDDS_NUMBER>& baudrate
    );

    virtual void loadExpSettings(ConfigStream& stream) override;
    virtual void logSettings(DataLogger& logger, ExpThreadWorker* threadworker) override;
    virtual void calculateVariations(std::vector<parameterType>& params, ExpThreadWorker* threadworker) override;
    virtual void programVariation(unsigned variation, std::vector<parameterType>& params,
        ExpThreadWorker* threadworker) override;
    virtual void normalFinish() override {};
    virtual void errorFinish() override {};
    virtual std::string getDelim() override { return configDelim; };

    StaticDDSSettings getSettingsFromConfig(ConfigStream& file);

    std::string getDeviceInfo(unsigned int port);
    void setStaticDDSExpSetting(StaticDDSSettings tmpSetting); // used only for ProgramNow in StaticAOSystem

    const std::string configDelim = "STATIC_DDS_SYSTEM";
    const bool safemode;
private:
    std::string getDDSCommand(double ddsfreqVal);
    void writeDDSs(std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> outputs_frequency,
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> outputs_level);
    bool checkBoundFreq(double ddsfreqVal);
    bool checkBoundLevel(double ddslevelVal);

public:
    static constexpr double ddsResolutionInst = 1e-6; // 1 Hz
    const int numFreqDigits = static_cast<int>(abs(round(log10(ddsResolutionInst) - 0.49)));
    const double minFreqVal = 20;
    const double maxFreqVal = 6400;
    const double minLevelVal = 0;
    const double maxLevelVal = 63;


private:
    std::array<StaticDDSFlume, STATICDDS_NUMBER> sddsFlume;
    StaticDDSSettings expSettings;    
};

