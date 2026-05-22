// created by Mark O. Brown
#pragma once
#include "ArbGenCore.h"
#include "AgilentCore.h"
#include "SiglentCore.h"
//#include "Scripts/ScriptStream.h"
//#include "ConfigurationSystems/ConfigStream.h"
#include "GeneralFlumes/VisaFlume.h"
#include "ArbGenStructures.h"
#include "whichAg.h"
#include "Scripts/Script.h"
//#include "DigitalOutput/DoRows.h"
#include <vector>
#include <array>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qfuturewatcher.h>
#include <CustomQtControls/AutoNotifyCtrls.h>

class IChimeraQtWindow;

// A class for programming agilent arbitrary waveform generators.
// in essense this includes a wrapper around agilent's implementation of the VISA protocol. 
class ArbGenSystem : public IChimeraSystem 
{
	public:
		// THIS CLASS IS NOT COPYABLE.
		ArbGenSystem& operator=(const ArbGenSystem&) = delete;
		ArbGenSystem(const ArbGenSystem&) = delete;

		ArbGenSystem( const arbGenSettings& settings, ArbGenType type, IChimeraQtWindow* parent );
		~ArbGenSystem();
		void initialize(std::string headerText, IChimeraQtWindow* win);
		void checkSave( std::string configPath, RunInfo info );


		bool scriptingModeIsSelected( );
		bool getSavedStatus ();
		void updateSavedStatus (bool isSaved);
		void handleSavingConfig( ConfigStream& saveFile, std::string configPath, RunInfo info,
			bool includeSectionDelimiters = true );
		std::string getDeviceIdentity();
		void handleOpenConfig(ConfigStream& file);

		deviceOutputInfo getOutputInfo();
/*		void handleScriptVariation( unsigned variation, scriptedArbInfo& scriptInfo, unsigned channel, 
									std::vector<parameterType>& variables );*/
		// making the script public greatly simplifies opening, saving, etc. files from this script.
		
		//std::pair<unsigned, unsigned> getTriggerLine( );
		std::string getConfigDelim ();
		void programArbGenNow (std::vector<parameterType> constants);
		
		void setOutputSettings (deviceOutputInfo info);
		void verifyScriptable ( );
		ArbGenCore& getCore ();
		void setDefault (unsigned chan);
		void refreshGeneratedWaveforms();
		void initializeSiglentFmOnStartup(IChimeraQtWindow* win);

	public:
		Script arbGenScript;
		const arbGenSettings initSettings;
	private:
		const ArbGenType arbType;
		ArbGenCore* pCore;
		std::vector<minMaxDoublet> ranges;
		deviceOutputInfo currentGuiInfo;
		// GUI ELEMENTS
		
		QLabel* header;
		QLabel* deviceInfoDisplay;
		CQCheckBox* polarityButton;

		// Dedicated Siglent FM controls.
		CQPushButton* uploadCsvNow;
		CQComboBox* generatedWaveformCombo;
		QLineEdit* ch1PulseDurationMsEdit;
		QLabel* ch1SampleRateLabel;
		CQCheckBox* siglentFmCtrlButton;
		CQCheckBox* clockExternalButton;
		QLineEdit* ch1AmplitudeEdit;
		QLineEdit* ch1OffsetEdit;
		QLineEdit* ch1StartPhaseEdit;
		QLineEdit* ch1BurstCyclesEdit;
		QLineEdit* ch2FrequencyMHzEdit;
		QLineEdit* ch2AmplitudeEdit;
		QLineEdit* ch2PhaseEdit;
		QLineEdit* ch2FrequencyDeviationMHzEdit;
		QFutureWatcher<unsigned>* csvUploadWatcher = nullptr;
		bool csvUploadInProgress = false;

		int getSelectedCsvPointCount() const;
		double getPulseDurationMs(IChimeraQtWindow* win = nullptr) const;
		void updateCalculatedSampleRateDisplay();
		void handleUploadCsvPressed(IChimeraQtWindow* win);
		void handleProgramSettingsPressed(IChimeraQtWindow* win);
		void syncSiglentFmSettingsFromGui(deviceOutputInfo& info) const;
		void loadSiglentFmSettingsToGui(const deviceOutputInfo& info);
};


