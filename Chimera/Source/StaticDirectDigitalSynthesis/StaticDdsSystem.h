#pragma once
#include <GeneralObjects/IChimeraSystem.h>
#include <ParameterSystem/ParameterSystem.h>
#include <StaticDirectDigitalSynthesis/StaticDdsCore.h>

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
	std::string getDeviceInfo();

	void setDdsEditFrequencyValue(std::string ddsfreq, unsigned channel, unsigned port); // should only be used in CommandModulator
	void setDdsEditLevelValue(std::string ddsfreq, unsigned channel, unsigned port); // should only be used in CommandModulator
private:
	bool expActive;
	StaticDdsCore core;
	QCheckBox* ctrlButton;
	std::array<QLabel*, size_t(StaticDDSGrid::total)> labels_port;
	std::array<std::array<QLabel*, 2>, size_t(StaticDDSGrid::total)> labels_channel;
	std::array<std::array<QLineEdit*, 2>, size_t(StaticDDSGrid::total)> edits_frequency;
	std::array<std::array<QLineEdit*, 2>, size_t(StaticDDSGrid::total)> edits_level;


};


