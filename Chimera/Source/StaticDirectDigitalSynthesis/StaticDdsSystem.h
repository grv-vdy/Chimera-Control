#pragma once
#include <GeneralObjects/IChimeraSystem.h>
#include <ParameterSystem/ParameterSystem.h>
#include "StaticDirectDigitalSynthesis/StaticDdsCore.h" // Ensure StaticDdsCore is included


class IChimeraQtWindow;
class StaticDdsSystem : public IChimeraSystem
{
	Q_OBJECT
public:
	// THIS CLASS IS NOT COPYABLE.
	StaticDdsSystem& operator=(const StaticDdsSystem&) = delete;
	StaticDdsSystem(const StaticDdsSystem&) = delete;
	StaticDdsSystem(IChimeraQtWindow* parent);

	void initialize();
	void handleOpenConfig(ConfigStream& configFile);
	void handleSaveConfig(ConfigStream& configFile);
	void updateCtrlEnable();
	void handleProgramNowPress(std::vector<parameterType> constants);
	std::string getConfigDelim() { return core.getDelim(); };
	StaticDdsCore& getCore() { return core; };
	std::string getDeviceInfo(unsigned int port);

	void setDdsEditFrequencyValue(std::string ddsfreq, unsigned channel, unsigned port); // should only be used in CommandModulator
	void setDdsEditLevelValue(std::string ddsfreq, unsigned channel, unsigned port); // should only be used in CommandModulator
private:
	bool expActive;
	StaticDdsCore core;
	QCheckBox* ctrlButton;
	// Per-channel sweep buttons and parameters
	std::array<std::array<QPushButton*, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> sweepButtons;
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> sweepStartFreqMHz;
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> sweepStopFreqMHz;
	std::array<std::array<double, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> sweepTimeSec;
	std::array<QLabel*, size_t(StaticDDSGrid::total)> labels_port;
	std::array<std::array<QLabel*, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> labels_channel;
	std::array<std::array<QLineEdit*, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> edits_frequency;
	std::array<std::array<QLineEdit*, size_t(StaticDDSGrid::numPERunit)>, size_t(StaticDDSGrid::numOFunit)> edits_level;


};


