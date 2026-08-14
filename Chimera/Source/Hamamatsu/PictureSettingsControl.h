// created by Mark O. Brown
#pragma once

#include "ConfigurationSystems/Version.h"
#include "GeneralImaging/softwareAccumulationOption.h"
#include "ConfigurationSystems/ConfigStream.h"
#include "Hamamatsu/hamamatsuPicSettingsGroup.h"
#include <array>
#include <vector>
#include "PrimaryWindows/IChimeraQtWindow.h"
#include <qlabel.h>
#include <CustomQtControls/AutoNotifyCtrls.h>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>

class HamamatsuCameraCore;
class HamamatsuCameraSettingsControl;

struct displayTypeOption{
	bool isDiff = false;
	// zero-indexed.
	unsigned whichPicForDif = 0;
};

/*
 * This class handles all of the gui objects for assigning camera settings. It works closely with the Hamamatsu class
 * because it eventually needs to communicate all of these settings to the Hamamatsu class.
 */
class PictureSettingsControl : public QWidget
{
	Q_OBJECT
	public:
		// must have parent. Enforced partially because both are singletons.
		PictureSettingsControl();
		void updateAllSettings ( hamamatsuPicSettingsGroup inputSettings );
		void handleSaveConfig(ConfigStream& saveFile);
		void handleOpenConfig(ConfigStream& openFile, HamamatsuCameraCore* hamamatsu);
		void initialize( IChimeraQtWindow* parent );
		void handleOptionChange( );
		void setPictureControlEnabled (int pic, bool enabled);
		void setUnofficialExposures ( std::vector<float> times );
		std::vector<int> getPictureColors ( );
		std::vector<float> getExposureTimes ( );
		std::vector<float> getUsedExposureTimes();
		std::vector<std::vector<int>> getThresholds();
		std::vector<displayTypeOption> getDisplayTypeOptions( );
		std::vector<bool> getDisplayMask();
		void setThresholds( const std::vector<std::string>& thresholds);
		unsigned getPicsPerRepetition();
		unsigned getPicsPerRepetitionNonContinuous();
		void updateSettings( );
		void updateColormaps ( const std::vector<int>& colorsIndexes );
		void setUnofficialPicsPerRep( unsigned picNum);
		std::vector<std::string> getThresholdStrings();
		std::vector<softwareAccumulationOption> getSoftwareAccumulationOptions ( );
		void setSoftwareAccumulationOptions ( const std::vector<softwareAccumulationOption>& opts );
		static hamamatsuPicSettingsGroup getPictureSettingsFromConfig (ConfigStream& configFile );
		void setEnabledStatus (bool viewRunningSettings);

		void toggleExposureTimeEditGui(bool enable);
	private:
		// the internal memory of the settings here is somewhat redundant with the gui objects. It'd probably be better
		// if this didn't exist and all the getters just converted straight from the gui objects, but that's a 
		// refactoring for another time.
		hamamatsuPicSettingsGroup currentPicSettings;
		unsigned unofficialPicsPerRep=1;
		/// Grid of PictureOptions
		QSpinBox* picsPerRepEdit;
		// Debounces the heavy picture-layout rebuild while the user is changing pics-per-rep.
		QTimer* picsPerRepDebounce = nullptr;
		QLabel* totalPicNumberLabel;
		QLabel* pictureLabel;
		QLabel* exposureLabel;
		QLabel* thresholdLabel;
		QLabel* displayTypeLabel;
		QScrollArea* settingsScrollArea = nullptr;
		QWidget* settingsRowsWidget = nullptr;
		QGridLayout* settingsRowsLayout = nullptr;
		std::vector<QLabel*> pictureNumbers;
		//QLabel* picScaleFactorLabel;
		//QLineEdit* picScaleFactorEdit;
		//QLabel* transfModeLabel;
		//CQComboBox* transformationModeCombo;
		// 
		std::vector<CQLineEdit*> exposureEdits;
		std::vector<CQLineEdit*> thresholdEdits;
		std::vector<CQCheckBox*> displayChecks;
};


