// created by Mark O. Brown
#pragma once
#include "WieserlabsDDSCore.h"
#include "Scripts/Script.h"
#include "GeneralObjects/IChimeraSystem.h"
#include "ConfigurationSystems/ConfigSystem.h"
#include "WieserlabsDDSStructures.h"
#include "ScriptedWieserlabsDDSWaveform.h"
#include <vector>
#include <array>
#include <qlabel.h>
#include <CustomQtControls/AutoNotifyCtrls.h>

class IChimeraQtWindow;

// A class for programming Wieserlabs DDS.
class WieserlabsDDSSystem : public IChimeraSystem
{
public:
	// THIS CLASS IS NOT COPYABLE.
	WieserlabsDDSSystem& operator=(const WieserlabsDDSSystem&) = delete;
	WieserlabsDDSSystem(const WieserlabsDDSSystem&) = delete;

	WieserlabsDDSSystem(const wieserlabsDdsSettings& settings, IChimeraQtWindow* parent);
	~WieserlabsDDSSystem();
	void initialize(std::string headerText, IChimeraQtWindow* win);
	void updateButtonDisplay(int chan);
	void checkSave(std::string configPath, RunInfo info);
	void handleChannelPress(int chan, std::string configPath, RunInfo currentRunInfo);
	void handleModeCombo();

	void readGuiSettings();
	void readGuiSettings(int chan);
	bool scriptingModeIsSelected();
	bool getSavedStatus();
	void updateSavedStatus(bool isSaved);
	void handleSavingConfig(ConfigStream& saveFile, std::string configPath, RunInfo info);
	std::string getDeviceIdentity();
	void handleOpenConfig(deviceOutputInfo settings);
	void updateSettingsDisplay(int chan, std::string configPath, RunInfo currentRunInfo);
	void updateSettingsDisplay(std::string configPath, RunInfo currentRunInfo);
	deviceOutputInfo getOutputInfo();
	WieserlabsDDSCore& getCore();
	void setOutputSettings(deviceOutputInfo info);
	void refreshScriptedWaveform();

	// making the script public greatly simplifies opening, saving, etc. files from this script.
	Script* wieserlabsDdsScript = nullptr;
	ScriptedWieserlabsDDSWaveform scriptedWaveform;

private:
	const wieserlabsDdsSettings initSettings;
	WieserlabsDDSCore core;
	std::vector<CQCheckBox*> onButtons;
	std::vector<CQLineEdit*> freqEdits;
	std::vector<CQLineEdit*> ampEdits;
	std::vector<CQLineEdit*> phaseEdits;
	std::vector<QLabel*> channelLabels;
	std::vector<QPushButton*> channelButtons;
	CQCheckBox* ctrlButton;
	CQComboBox* modeCombo;
	QLabel* header;
	deviceOutputInfo currentGuiInfo;
	bool lastScriptingMode = true;
	bool saved = true;
};