// created by Mark O. Brown
#pragma once

#include "ConfigurationSystems/Version.h"
#include "PictureSettingsControl.h"
#include "CameraImageDimensions.h"
#include "HamamatsuTemperatureStatus.h"
#include "HamamatsuCameraCore.h"
#include "ConfigurationSystems/Version.h"
#include "GeneralImaging/softwareAccumulationOption.h"
#include "PrimaryWindows/IChimeraQtWindow.h"
#include <qlabel.h>
#include <qcheckbox>
#include <qcombobox.h>
#include <qlineedit.h>

struct cameraPositions;

struct HamamatsuCameraSettings {
	HamamatsuRunSettings hamamatsu;
	std::vector<std::vector<int>> thresholds = std::vector<std::vector<int>>(4, std::vector<int>{100});
	std::vector<int> palleteNumbers = { 0, 1, 2, 3 };
};

/*
 * This large class maintains all of the settings & user interactions for said settings of the Hamamatsu camera. It more or
 * less contains the PictureSettingsControl Class, as this is meant to be the parent of such an object. It is distinct
 * but highly related to the Hamamatsu class, where the Hamamatsu class is the class that actually manages communications with
 * the camera and some base settings that the user does not change. Because of the close contact between this and the
 * hamamatsu class, this object is initialized with a pointer to the hamamatsu object.
 ***********************************************************************************************************************/
class HamamatsuCameraSettingsControl : public QWidget
{
	Q_OBJECT
	public:
		HamamatsuCameraSettingsControl();
		void setVariationNumber(unsigned varNumber);
		void setRepsPerVariation(unsigned repsPerVar);
		void updateRunSettingsFromPicSettings( );
		void initialize( IChimeraQtWindow* parent, std::vector<std::string> vertSpeeds, 
						 std::vector<std::string> horSpeeds );
		void updateSettings( );
		//void updateMinKineticCycleTime( double time );
		//void setEmGain( bool currentlyOn, int currentEmGainLevel );
		void updateWindowEnabledStatus ();
		void handlePictureSettings();
		void updateTriggerMode( );
		void handleSetTemperaturePress();
		void changeTemperatureDisplay( HamamatsuTemperatureStatus stat );
		void checkIfReady();
		void cameraIsOn( bool state );
		void updateCameraMode( );
		void updateGainMode();
		//void updateBinningMode();
		HamamatsuCameraSettings getConfigSettings();
		void setImageParameters(imageParameters newSettings);
		void setRunSettings(HamamatsuRunSettings inputSettings);
		//void updateImageDimSettings ( imageParameters settings );
		void updatePicSettings ( hamamatsuPicSettingsGroup settings );
		void updateDisplays ();
		static hamamatsuPicSettingsGroup getPictureSettingsFromConfig (ConfigStream& configFile);
		void handleSaveConfig(ConfigStream& configFile);
		void handelSaveMasterConfig(std::stringstream& configFile);
		void handleOpenMasterConfig(ConfigStream& configFile, QtHamamatsuWindow* camWin);
		std::vector<Matrix<long>> getImagesToDraw( const std::vector<Matrix<long>>& rawData  );
		const imageParameters fullResolution = { 1, 2048, 2048, 1, 1, 1 };
		std::vector<softwareAccumulationOption> getSoftwareAccumulationOptions ( );
		std::vector<bool> getDisplayMask();
		void setConfigSettings (HamamatsuRunSettings inputSettings);
		HamamatsuRunSettings getRunningSettings ();
		unsigned getHsSpeed ();
		unsigned getVsSpeed ();
		unsigned getFrameTransferMode();
		bool getAutoCal();
		bool getUseCal();
	private:

		HamamatsuRunSettings currentlyRunningSettings;
		bool currentlyUneditable = false;
		//double getKineticCycleTime( );
		//double getAccumulationCycleTime( );
		//unsigned getAccumulationNumber( );
		imageParameters readImageParameters( );
		QLabel* header;
		QPushButton* programNow;
		QCheckBox* viewRunningSettings;
		CQCheckBox* controlHamamatsuCameraCheck;
		// Hardware Accumulation Parameters
		//QLabel* accumulationCycleTimeLabel;
		//CQLineEdit* accumulationCycleTimeEdit = nullptr;
		//QLabel* accumulationNumberLabel;
		//CQLineEdit* accumulationNumberEdit = nullptr;
		// 
		CQComboBox* cameraModeCombo;

		//CQComboBox* frameTransferModeCombo = nullptr;
		//CQComboBox* verticalShiftSpeedCombo;
		//CQComboBox* horizontalShiftSpeedCombo;

		//QLabel* emGainLabel;
		//CQLineEdit* emGainEdit = nullptr;
		//CQPushButton* emGainBtn = nullptr;
		//QLabel* emGainDisplay;
		CQComboBox* triggerCombo = nullptr;
		CQComboBox* coolerCombo = nullptr;
		CQComboBox* fanCombo = nullptr;
		CQComboBox* gainCombo = nullptr;
		//CQComboBox* binningCombo = nullptr;
		// Temperature
		CQPushButton* setTemperatureButton = nullptr;
		//CQPushButton* temperatureOffButton = nullptr;
		CQLineEdit* temperatureEdit = nullptr;
		QLabel* temperatureDisplay;
		QLabel* temperatureMsg;

		// Kinetic Cycle Time
		//CQLineEdit* kineticCycleTimeEdit = nullptr;
		//QLabel* kineticCycleTimeLabel = nullptr;
		//QLabel* minKineticCycleTimeDisp = nullptr;
		//QLabel* minKineticCycleTimeLabel = nullptr;
		// two subclassed groups.
		ImageDimsControl imageDimensionsObj;
		PictureSettingsControl picSettingsObj;

		// the currently selected settings, not necessarily those being used to run the current
		// experiment.
		HamamatsuCameraSettings configSettings;
};

