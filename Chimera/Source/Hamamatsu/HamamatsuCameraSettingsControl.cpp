// created by Mark O. Brown
#include "stdafx.h"
#include "HamamatsuRunMode.h"
#include "HamamatsuTriggerModes.h"
#include "HamamatsuCameraSettingsControl.h"

#include "../PrimaryWindows/QtHamamatsuWindow.h"
#include "GeneralUtilityFunctions/miscCommonFunctions.h"
#include "ConfigurationSystems/ConfigSystem.h"

#include <boost/lexical_cast.hpp>

HamamatsuCameraSettingsControl::HamamatsuCameraSettingsControl() : imageDimensionsObj("hamamatsu"){
	HamamatsuRunSettings& hamamatsuSettings = configSettings.hamamatsu;
	hamamatsuSettings.temperatureSetting = -20;
}

void HamamatsuCameraSettingsControl::initialize ( IChimeraQtWindow* parent, std::vector<std::string> vertSpeeds,
											  std::vector<std::string> horSpeeds )
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	header = new QLabel ("HAMAMATSU CAMERA SETTINGS", parent);
	programNow = new QPushButton ("Program Now", parent);
	parent->connect (programNow, &QPushButton::released, parent->hamamatsuWin, [parent]() {
			// Program Now only applies settings to the camera (temperature, exposure, trigger, ROI,
			// readout). It does NOT start an acquisition; images are captured during an experiment run.
			parent->hamamatsuWin->manualProgramCameraSetting ();
		});
	layout->addWidget(header, 1);
	layout->addWidget(programNow, 1);

	QHBoxLayout* layout1 = new QHBoxLayout();
	layout1->setContentsMargins(0, 0, 0, 0);
	controlHamamatsuCameraCheck = new CQCheckBox ("Control Hamamatsu Camera?", parent);
	controlHamamatsuCameraCheck->setChecked (true);
	viewRunningSettings = new QCheckBox ("View Running Settings?", parent);
	parent->connect (viewRunningSettings, &QCheckBox::stateChanged, [this]() {
		// Guard the whole handler: an exception escaping a Qt slot terminates the app.
		try {
			if (viewRunningSettings->isChecked ()) {
				// just changed to checked, so the settings should indicate the config settings still.
				updateSettings ();
				updateWindowEnabledStatus ();
				updateDisplays ();
				currentlyUneditable = true;
			}
			else {
				updateWindowEnabledStatus ();
				updateDisplays ();
				currentlyUneditable = false;
			}
		}
		catch (ChimeraError& e) {
			currentlyUneditable = false;
			errBox ("Failed to toggle View Running Settings: " + e.trace ());
		}
		});
	layout1->addWidget(controlHamamatsuCameraCheck, 0);
	layout1->addWidget(viewRunningSettings, 0);

	QHBoxLayout* layout2 = new QHBoxLayout();
	layout2->setContentsMargins(0, 0, 0, 0);
	cameraModeCombo = new CQComboBox (parent);
	for (auto mode : HamamatsuRunModes::allModes) {
		cameraModeCombo->addItem(qstr(HamamatsuRunModes::toStr(mode)));
	}
	parent->connect (cameraModeCombo, qOverload<int> (&QComboBox::activated),
		[this, parent]() {
			if (!viewRunningSettings->isChecked ()) {
				updateCameraMode ();
				updateWindowEnabledStatus ();
			}
		});
	cameraModeCombo->setCurrentIndex(0);
	configSettings.hamamatsu.acquisitionMode = static_cast<int>(HamamatsuRunModes::mode::Standard);
	
	triggerCombo = new CQComboBox (parent);
	for (auto mode : HamamatsuTriggerMode::allModes) {
		triggerCombo->addItem(qstr(HamamatsuTriggerMode::toStr(mode)));
	}
	parent->connect (triggerCombo, qOverload<int> (&QComboBox::activated),
		[this, parent]() {
			if (!viewRunningSettings->isChecked ()) {
				updateTriggerMode ();
				updateWindowEnabledStatus ();
				auto& hamamatsuCore = parent->hamamatsuWin->getCamera();
				updateSettings();
				hamamatsuCore.setSettings(configSettings.hamamatsu);
				const bool isLevel = configSettings.hamamatsu.triggerMode
					== HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Level);
				// Exposure time is ignored in Level (the TTL high-time sets it); editable otherwise.
				picSettingsObj.toggleExposureTimeEditGui(!isLevel);
				try {
					hamamatsuCore.setCameraTriggerMode();
				}
				catch (ChimeraError& e) {
					parent->reportErr(e.qtrace());
				}
			}
		});
	triggerCombo->setCurrentIndex(0);
	configSettings.hamamatsu.triggerMode = HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Edge);

	gainCombo = new CQComboBox(parent);
	//for (auto mode : HamamatsuGainMode::allModes) {
	//	gainCombo->addItem(qstr(HamamatsuGainMode::toStr(mode)));
	//}
	//parent->connect(gainCombo, qOverload<int>(&QComboBox::activated),
	//	[this, parent]() {
	//		if (!viewRunningSettings->isChecked()) {
	//			updateGainMode();
	//			updateWindowEnabledStatus();
	//			auto& hamamatsuCore = parent->hamamatsuWin->getCamera();
	//			updateSettings();
	//			hamamatsuCore.setSettings(configSettings.hamamatsu);
	//			try {
	//				hamamatsuCore.setImageParametersToCamera();
	//				hamamatsuCore.setCameraGainMode();
	//			}
	//			catch (ChimeraError& e) {
	//				parent->reportErr(e.qtrace());
	//			}
	//			currentlyRunningSettings.frameRate = configSettings.hamamatsu.frameRate;
	//			updateMaxFrameRate(hamamatsuCore.getMaxFrameRate());
	//		}
	//	});
	//gainCombo->setCurrentIndex(0);
	//configSettings.hamamatsu.gainMode = HamamatsuGainMode::mode::FastestFrameRate;

	//binningCombo = new CQComboBox(parent);
	//for (auto mode : HamamatsuBinningMode::allModes) {
	//	binningCombo->addItem(qstr(HamamatsuBinningMode::toStr(mode)));
	//}
	//binningCombo->setCurrentIndex(0);
	//parent->connect(binningCombo, qOverload<int>(&QComboBox::activated),
	//	[this, parent]() {
	//		if (!viewRunningSettings->isChecked()) {
	//			updateBinningMode();
	//			updateWindowEnabledStatus();
	//			auto& hamamatsuCore = parent->hamamatsuWin->getCamera();
	//			updateSettings();
	//			hamamatsuCore.setSettings(configSettings.hamamatsu);
	//			try {
	//				hamamatsuCore.setCameraBinningMode();
	//			}
	//			catch (ChimeraError& e) {
	//				parent->reportErr(e.qtrace());
	//			}
	//		}
	//	});
	//configSettings.hamamatsu.binningMode = HamamatsuBinningMode::mode::oneByOne;

	cameraModeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	triggerCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	layout2->addWidget(cameraModeCombo, 1);
	layout2->addWidget(triggerCombo, 1);
	//layout2->addWidget(gainCombo, 0);

	//QHBoxLayout* layout3 = new QHBoxLayout();
	//layout3->setContentsMargins(0, 0, 0, 0);
	//frameTransferModeCombo = new CQComboBox(parent);
	//frameTransferModeCombo->setToolTip("Frame Transfer Mode OFF:\n"
	//	"Slower than when on. Cleans between images. Mechanical shutter may not be necessary.\n"
	//	"Frame Transfer Mode ON:\n"
	//	"Faster than when off. Does not clean between images, giving better background.\n"
	//	"Mechanical Shutter probably necessary unless imaging continuously.\n"
	//	"See iXonEM + hardware guide pg 42.\n");
	//frameTransferModeCombo->addItems({ "FTM: OFF!", "FTM: ON!" });
	//frameTransferModeCombo->setCurrentIndex(0);
	//configSettings.hamamatsu.frameTransferMode = 0;

	//verticalShiftSpeedCombo = new CQComboBox (parent);
	//for (auto speed : vertSpeeds){ 
	//	verticalShiftSpeedCombo->addItem ("VS: " + qstr(speed));
	//}
	//verticalShiftSpeedCombo->setCurrentIndex (1);
	//configSettings.hamamatsu.vertShiftSpeedSetting = 0;

	//horizontalShiftSpeedCombo = new CQComboBox (parent);
	//for (auto speed : horSpeeds){
	//	horizontalShiftSpeedCombo->addItem ("HS: " + qstr (speed));
	//}
	//horizontalShiftSpeedCombo->setCurrentIndex (0);
	//configSettings.hamamatsu.horShiftSpeedSetting = 0;

	//layout3->addWidget(frameTransferModeCombo, 0);
	//layout3->addWidget(verticalShiftSpeedCombo, 0);
	//layout3->addWidget(horizontalShiftSpeedCombo, 0);

	//QHBoxLayout* layout4 = new QHBoxLayout();
	//layout4->setContentsMargins(0, 0, 0, 0);
	//emGainBtn = new CQPushButton ("Set EM Gain (-1=OFF)", parent);
	//parent->connect (emGainBtn, &QPushButton::released, [parent]() {
	//		parent->hamamatsuWin->handleEmGainChange ();
	//	});
	//emGainEdit = new CQLineEdit ("-1", parent);
	//emGainEdit->setToolTip( "Set the state & gain of the EM gain of the camera. Enter a negative number to turn EM Gain"
	//					   " mode off. The program will immediately change the state of the camera after changing this"
	//					   " edit." );
	////
	//emGainDisplay = new QLabel ("OFF", parent);
	//// initialize settings.
	//configSettings.hamamatsu.emGainLevel = 0;
	//configSettings.hamamatsu.emGainModeIsOn = false;
	//layout4->addWidget(emGainBtn, 0);
	//layout4->addWidget(emGainEdit, 0);
	//layout4->addWidget(emGainDisplay, 0);

	QHBoxLayout* layout5 = new QHBoxLayout();
	layout5->setContentsMargins(0, 0, 0, 0);
	setTemperatureButton = new CQPushButton ("Set Camera Temperature (C)", parent);
	parent->connect (setTemperatureButton, &QPushButton::released, 
		[parent]() {
			parent->hamamatsuWin->passSetTemperaturePress ();
		});
	temperatureEdit = new CQLineEdit ("-20", parent);
	temperatureDisplay = new QLabel ("", parent);
	//temperatureOffButton = new CQPushButton("OFF", parent);
	temperatureEdit->setMaximumWidth(70);
	layout5->addWidget(setTemperatureButton, 0);
	layout5->addWidget(temperatureEdit, 0);
	layout5->addWidget(temperatureDisplay, 1);
	//layout5->addWidget(temperatureOffButton, 0);
	
	temperatureMsg = new QLabel("Temperature control is disabled", parent);

	// Sensor cooler (thermoelectric) + cooler-fan controls.
	QHBoxLayout* coolerFanLayout = new QHBoxLayout();
	coolerFanLayout->setContentsMargins(0, 0, 0, 0);
	coolerCombo = new CQComboBox(parent);
	coolerCombo->addItem("Sensor Cooler On");
	coolerCombo->addItem("Sensor Cooler Off");
	coolerCombo->addItem("Sensor Cooler Max");
	coolerCombo->setToolTip("Sensor thermoelectric cooler: On (normal), Off, or Max (strongest cooling).");
	fanCombo = new CQComboBox(parent);
	fanCombo->addItem("Fan On");
	fanCombo->addItem("Fan Off");
	fanCombo->setToolTip("Sensor cooler fan (helps the thermoelectric cooler dump heat).");
	parent->connect(coolerCombo, qOverload<int>(&QComboBox::activated), [this, parent]() {
		if (!viewRunningSettings->isChecked()) {
			updateSettings();
			auto& core = parent->hamamatsuWin->getCamera();
			core.setSettings(configSettings.hamamatsu);
			try { core.setCoolerMode(); }
			catch (ChimeraError& e) { parent->reportErr(e.qtrace()); }
		}
	});
	parent->connect(fanCombo, qOverload<int>(&QComboBox::activated), [this, parent]() {
		if (!viewRunningSettings->isChecked()) {
			updateSettings();
			auto& core = parent->hamamatsuWin->getCamera();
			core.setSettings(configSettings.hamamatsu);
			try { core.setFanMode(); }
			catch (ChimeraError& e) { parent->reportErr(e.qtrace()); }
		}
	});
	coolerCombo->setCurrentIndex(0);
	fanCombo->setCurrentIndex(0);
	configSettings.hamamatsu.coolerMode = str(coolerCombo->currentText());
	configSettings.hamamatsu.fanMode = str(fanCombo->currentText());
	coolerCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	fanCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	coolerFanLayout->addWidget(new QLabel("Cooler:", parent), 0);
	coolerFanLayout->addWidget(coolerCombo, 1);
	coolerFanLayout->addWidget(new QLabel("Fan:", parent), 0);
	coolerFanLayout->addWidget(fanCombo, 1);

	picSettingsObj.initialize( parent );
	imageDimensionsObj.initialize(parent);

	//// Accumulation Time
	//accumulationCycleTimeLabel = new QLabel ("Accumulation Cycle Time", parent);
	//accumulationCycleTimeEdit = new CQLineEdit ("0.1", parent);
	//// Accumulation Number
	//accumulationNumberLabel = new QLabel ("Accumulation #", parent);
	//accumulationNumberEdit = new CQLineEdit ("1", parent);
	//// minimum kinetic cycle time (determined by camera)
	//minKineticCycleTimeLabel = new QLabel ("Minimum Kinetic Cycle Time (s)", parent);
	//minKineticCycleTimeDisp = new QLabel("---", parent);
	///// Kinetic Cycle Time
	//kineticCycleTimeLabel = new QLabel ("Kinetic Cycle Time (s)", parent);
	//kineticCycleTimeEdit = new CQLineEdit("0.1", parent);	
	//layout6->addWidget(accumulationCycleTimeLabel, 0, 0);
	//layout6->addWidget(accumulationCycleTimeEdit, 0, 1);
	//layout6->addWidget(accumulationNumberLabel, 0, 2);
	//layout6->addWidget(accumulationNumberEdit, 0, 3);
	//layout6->addWidget(minKineticCycleTimeLabel, 1, 0);
	//layout6->addWidget(minKineticCycleTimeDisp, 1, 1);
	//layout6->addWidget(kineticCycleTimeLabel, 1, 2);
	//layout6->addWidget(kineticCycleTimeEdit, 1, 3);

	layout->addLayout(layout1);
	layout->addLayout(layout2);
	layout->addLayout(layout5);
	layout->addLayout(coolerFanLayout);
	layout->addWidget(temperatureMsg);
	layout->addWidget(&imageDimensionsObj);
	layout->addWidget(&picSettingsObj);
	updateWindowEnabledStatus ();

	emit cameraModeCombo->activated(0);
	emit triggerCombo->activated(0);
	//emit gainCombo->activated(0);
}


// note that this object doesn't actually store the camera state, it just uses it in passing to figure out whether 
// buttons should be on or off.
void HamamatsuCameraSettingsControl::cameraIsOn(bool state){
	// Can't change em gain mode or camera settings once started.
	//emGainEdit->setEnabled( !state );
	setTemperatureButton->setEnabled ( !state );
	//temperatureOffButton->setEnabled ( !state );
}

void HamamatsuCameraSettingsControl::setConfigSettings (HamamatsuRunSettings inputSettings) {
	configSettings.hamamatsu = inputSettings;
	updateDisplays ();
}

void HamamatsuCameraSettingsControl::updateDisplays () {
	auto optionsIn = viewRunningSettings->isChecked () ? currentlyRunningSettings : configSettings.hamamatsu;
	controlHamamatsuCameraCheck->setChecked (optionsIn.controlCamera);
	//kineticCycleTimeEdit->setText (qstr (optionsIn.kineticCycleTime));
	//accumulationCycleTimeEdit->setText (qstr (optionsIn.accumulationTime));
	int ind = cameraModeCombo->findText (qstr(HamamatsuRunModes::toStr(
		static_cast<HamamatsuRunModes::mode>(optionsIn.acquisitionMode))));
	if (ind != -1) {
		cameraModeCombo->setCurrentIndex (ind);
	}
	ind = triggerCombo->findText (qstr(optionsIn.triggerMode));
	if (ind != -1) {
		triggerCombo->setCurrentIndex (ind);
	}
	if (coolerCombo) {
		int ci = coolerCombo->findText (qstr(optionsIn.coolerMode));
		if (ci != -1) { coolerCombo->setCurrentIndex (ci); }
	}
	if (fanCombo) {
		int fi = fanCombo->findText (qstr(optionsIn.fanMode));
		if (fi != -1) { fanCombo->setCurrentIndex (fi); }
	}
	//ind = gainCombo->findText(HamamatsuGainMode::toStr(optionsIn.gainMode).c_str());
	//if (ind != -1) {
	//	gainCombo->setCurrentIndex(ind);
	//}
	//ind = binningCombo->findText(HamamatsuBinningMode::toStr(optionsIn.binningMode).c_str());
	//if (ind != -1) {
	//	binningCombo->setCurrentIndex(ind);
	//}
	//kineticCycleTimeEdit->setText (qstr (optionsIn.kineticCycleTime));
	//accumulationCycleTimeEdit->setText (qstr (optionsIn.accumulationTime * 1000.0));
	//accumulationNumberEdit->setText (qstr (optionsIn.accumulationNumber));
	temperatureEdit->setText (qstr (optionsIn.temperatureSetting));
	imageDimensionsObj.setImageParametersFromInput (optionsIn.imageSettings);

	//verticalShiftSpeedCombo->setCurrentIndex(optionsIn.vertShiftSpeedSetting);
	//horizontalShiftSpeedCombo->setCurrentIndex(optionsIn.horShiftSpeedSetting);
	//frameTransferModeCombo->setCurrentIndex(optionsIn.frameTransferMode);

	picSettingsObj.setUnofficialExposures(std::vector<float>(optionsIn.exposureTimes.begin(), optionsIn.exposureTimes.end()));
	picSettingsObj.setUnofficialPicsPerRep (optionsIn.picsPerRepetition);

}


void HamamatsuCameraSettingsControl::setRunSettings(HamamatsuRunSettings inputSettings){
	currentlyRunningSettings = inputSettings;
	picSettingsObj.setUnofficialExposures(std::vector<float>(inputSettings.exposureTimes.begin(), inputSettings.exposureTimes.end()));
	picSettingsObj.setUnofficialPicsPerRep ( inputSettings.picsPerRepetition );
	///
	updateDisplays ();
}


void HamamatsuCameraSettingsControl::handleSetTemperaturePress(){
	try{
		configSettings.hamamatsu.temperatureSetting = boost::lexical_cast<int>(str(temperatureEdit->text ()));
	}
	catch ( boost::bad_lexical_cast&){
		throwNested("Error: Couldn't convert temperature input to a double! Check for unusual characters.");
	}
}

void HamamatsuCameraSettingsControl::updateTriggerMode( ){
	if (currentlyUneditable) {
		return;
	}
	int itemIndex = triggerCombo->currentIndex( );
	if ( itemIndex == -1 ){
		return;
	}
	configSettings.hamamatsu.triggerMode = str(triggerCombo->currentText());
}

unsigned HamamatsuCameraSettingsControl::getHsSpeed () {
	//return horizontalShiftSpeedCombo->currentIndex ();
	return -1;
}

unsigned HamamatsuCameraSettingsControl::getVsSpeed () {
	//return verticalShiftSpeedCombo->currentIndex ();
	return -1;
}

unsigned HamamatsuCameraSettingsControl::getFrameTransferMode() {
	//return frameTransferModeCombo->currentIndex();
	return -1;
}

bool HamamatsuCameraSettingsControl::getAutoCal()
{
	// Return whether automatic calibration is enabled
	return configSettings.hamamatsu.controlCamera;
}

bool HamamatsuCameraSettingsControl::getUseCal()
{
	// Return whether to use calibration
	return configSettings.hamamatsu.controlCamera;
}

void HamamatsuCameraSettingsControl::updateSettings(){
	if (currentlyUneditable) {
		return;
	}
	// update all settings with current values from controls
	configSettings.hamamatsu.controlCamera =		controlHamamatsuCameraCheck->isChecked ();
	if (coolerCombo) { configSettings.hamamatsu.coolerMode = str(coolerCombo->currentText()); }
	if (fanCombo) { configSettings.hamamatsu.fanMode = str(fanCombo->currentText()); }
	{
		auto exposureTimes = picSettingsObj.getUsedExposureTimes();
		configSettings.hamamatsu.exposureTimes = std::vector<double>(exposureTimes.begin(), exposureTimes.end());
	}
	configSettings.thresholds =				picSettingsObj.getThresholds( );
	configSettings.palleteNumbers =			picSettingsObj.getPictureColors( );
	configSettings.hamamatsu.picsPerRepetition =	picSettingsObj.getPicsPerRepetition( );
	
	configSettings.hamamatsu.imageSettings = readImageParameters( );
	//configSettings.hamamatsu.kineticCycleTime = getKineticCycleTime( );
	//configSettings.hamamatsu.accumulationTime = getAccumulationCycleTime( );
	//configSettings.hamamatsu.accumulationNumber = getAccumulationNumber( );

	updateCameraMode( );
	updateTriggerMode( );
	//updateGainMode();
	//updateBinningMode();

	//configSettings.hamamatsu.horShiftSpeedSetting = getHsSpeed ();
	//configSettings.hamamatsu.vertShiftSpeedSetting = getVsSpeed ();
	//configSettings.hamamatsu.frameTransferMode = getFrameTransferMode();
}

std::vector<softwareAccumulationOption> HamamatsuCameraSettingsControl::getSoftwareAccumulationOptions ( ){
	return picSettingsObj.getSoftwareAccumulationOptions();
}

std::vector<bool> HamamatsuCameraSettingsControl::getDisplayMask(){
	return picSettingsObj.getDisplayMask();
}

HamamatsuCameraSettings HamamatsuCameraSettingsControl::getConfigSettings(){
	updateSettings ();
	return configSettings;
}

HamamatsuRunSettings HamamatsuCameraSettingsControl::getRunningSettings () {
	return currentlyRunningSettings;
}



//void HamamatsuCameraSettingsControl::setEmGain( bool emGainCurrentlyOn, int currentEmGainLevel ){
//	auto emGainText = emGainEdit->text();
//	if ( emGainText == "" ){
//		// set to off.
//		emGainText = "-1";
//	}
//	int emGain;
//	try{
//		emGain = boost::lexical_cast<int>(str(emGainText));
//	}
//	catch ( boost::bad_lexical_cast&){
//		throwNested("ERROR: Couldn't convert EM Gain text to integer! Aborting!");
//	}
//	// < 0 corresponds to NOT USING EM GAIN (using conventional gain).
//	if (emGain < 0){
//		configSettings.hamamatsu.emGainModeIsOn = false;
//		configSettings.hamamatsu.emGainLevel = 0;
//		emGainDisplay->setText("OFF");
//	}
//	else{
//		configSettings.hamamatsu.emGainModeIsOn = true;
//		configSettings.hamamatsu.emGainLevel = emGain;
//		emGainDisplay->setText(cstr("Gain: X" + str(configSettings.hamamatsu.emGainLevel)));
//	}
//	// Change the hamamatsu settings.
//	std::string promptMsg = "";
//	if ( emGainCurrentlyOn != configSettings.hamamatsu.emGainModeIsOn ){
//		promptMsg += "Set Hamamatsu EM Gain State to " + str(configSettings.hamamatsu.emGainModeIsOn ? "ON" : "OFF");
//	}
//	if ( currentEmGainLevel != configSettings.hamamatsu.emGainLevel ){
//		if ( promptMsg != "" ){
//			promptMsg += ", ";
//		}
//		promptMsg += "Set Hamamatsu EM Gain Level to " + str(configSettings.hamamatsu.emGainLevel);
//	}
//	if ( promptMsg != "" ){
//		promptMsg += "?";
//		auto result = QMessageBox::question (nullptr, "Hamamatsu Settings", qstr(promptMsg));
//		if ( result == QMessageBox::No ){
//			thrower ( "Aborting camera settings update at EM Gain update!" );
//		}
//	}
//}

void HamamatsuCameraSettingsControl::setVariationNumber(unsigned varNumber){
	HamamatsuRunSettings& hamamatsuSettings = configSettings.hamamatsu;
	hamamatsuSettings.totalVariations = varNumber;
	if ( hamamatsuSettings.totalPicsInExperiment() > INT_MAX){
		thrower ( "ERROR: Trying to take too many pictures! Maximum picture number is " + str( INT_MAX ) );
	}
}

void HamamatsuCameraSettingsControl::setRepsPerVariation(unsigned repsPerVar){
	HamamatsuRunSettings& hamamatsuSettings = configSettings.hamamatsu;
	hamamatsuSettings.repetitionsPerVariation = repsPerVar;
	if ( hamamatsuSettings.totalPicsInExperiment() > INT_MAX){
		thrower ( "ERROR: Trying to take too many pictures! Maximum picture number is " + str( INT_MAX ) );
	}
}

void HamamatsuCameraSettingsControl::changeTemperatureDisplay( HamamatsuTemperatureStatus stat ){
	temperatureDisplay->setText (qstr(stat.temperature) + " C (set " + qstr(stat.temperatureSetting) + " C)");
	temperatureMsg->setText (  qstr( stat.msg ) );
	QString colorcode = QVariant(stat.colorCode).toString();
	for (auto l : { temperatureDisplay ,temperatureMsg }) {
		l->setStyleSheet("QLabel { background-color :" + colorcode + " ; }");
	}
}

void HamamatsuCameraSettingsControl::updateRunSettingsFromPicSettings( ){
	auto exposureTimes = picSettingsObj.getUsedExposureTimes();
	configSettings.hamamatsu.exposureTimes = std::vector<double>(exposureTimes.begin(), exposureTimes.end());
	configSettings.hamamatsu.picsPerRepetition = picSettingsObj.getPicsPerRepetition( );
	if ( configSettings.hamamatsu.totalPicsInExperiment ( ) > INT_MAX ){
		thrower ( "ERROR: Trying to take too many pictures! Maximum picture number is " + str( INT_MAX ) );
	}
}

void HamamatsuCameraSettingsControl::handlePictureSettings(){
	picSettingsObj.handleOptionChange();
	updateRunSettingsFromPicSettings( );
}

//double HamamatsuCameraSettingsControl::getKineticCycleTime( ){
//	if (!kineticCycleTimeEdit) {
//		return 0;
//	}
//	try{
//		configSettings.hamamatsu.kineticCycleTime = boost::lexical_cast<float>( str(kineticCycleTimeEdit->text ()) );
//		kineticCycleTimeEdit->setText( cstr( configSettings.hamamatsu.kineticCycleTime ) );
//	}
//	catch ( boost::bad_lexical_cast& ){
//		configSettings.hamamatsu.kineticCycleTime = 0.1f;
//		kineticCycleTimeEdit->setText ( cstr( configSettings.hamamatsu.kineticCycleTime ) );
//		throwNested( "Please enter a valid float for the kinetic cycle time." );
//	}
//	return configSettings.hamamatsu.kineticCycleTime;
//}
//
//double HamamatsuCameraSettingsControl::getAccumulationCycleTime( ){
//	if (!accumulationCycleTimeEdit){
//		return 0;
//	}
//	try	{
//		configSettings.hamamatsu.accumulationTime = boost::lexical_cast<float>( str(accumulationCycleTimeEdit->text ()) );
//		accumulationCycleTimeEdit->setText( cstr( configSettings.hamamatsu.accumulationTime ) );
//	}
//	catch ( boost::bad_lexical_cast& ){
//		configSettings.hamamatsu.accumulationTime = 0.1f;
//		accumulationCycleTimeEdit->setText( cstr( configSettings.hamamatsu.accumulationTime ) );
//		throwNested( "Please enter a valid float for the accumulation cycle time." );
//	}
//	return configSettings.hamamatsu.accumulationTime;
//}
//
//unsigned HamamatsuCameraSettingsControl::getAccumulationNumber( ){
//	if (!accumulationNumberEdit){
//		return 0;
//	}
//	try	{
//		configSettings.hamamatsu.accumulationNumber = boost::lexical_cast<long>( str(accumulationNumberEdit->text ()) );
//		accumulationNumberEdit->setText( cstr( configSettings.hamamatsu.accumulationNumber ) );
//	}
//	catch ( boost::bad_lexical_cast& ){
//		configSettings.hamamatsu.accumulationNumber = 1;
//		accumulationNumberEdit->setText( cstr( configSettings.hamamatsu.accumulationNumber ) );
//		throwNested( "Please enter a valid float for the Accumulation number." );
//	}
//	return configSettings.hamamatsu.accumulationNumber;
//}

void HamamatsuCameraSettingsControl::updatePicSettings ( hamamatsuPicSettingsGroup settings ){
	picSettingsObj.updateAllSettings ( settings );
}

//void HamamatsuCameraSettingsControl::updateImageDimSettings( imageParameters settings ){
//	imageDimensionsObj.setImageParametersFromInput ( settings );
//}

hamamatsuPicSettingsGroup HamamatsuCameraSettingsControl::getPictureSettingsFromConfig (ConfigStream& configFile ){
	return PictureSettingsControl::getPictureSettingsFromConfig ( configFile );
}

void HamamatsuCameraSettingsControl::handleSaveConfig(ConfigStream& saveFile){
	updateSettings ();
	saveFile << "CAMERA_SETTINGS\n";
	saveFile << "/*Control Hamamatsu:*/\t\t\t" << configSettings.hamamatsu.controlCamera << "\n";
	saveFile << "/*Trigger Mode:*/\t\t\t" << configSettings.hamamatsu.triggerMode << "\n";
	//saveFile << "/*EM-Gain Is On:*/\t\t\t" << configSettings.hamamatsu.emGainModeIsOn << "\n";
	//saveFile << "/*EM-Gain Level:*/\t\t\t" << configSettings.hamamatsu.emGainLevel << "\n";
	saveFile << "/*Acquisition Mode:*/\t\t" << HamamatsuRunModes::toStr(
		static_cast<HamamatsuRunModes::mode>(configSettings.hamamatsu.acquisitionMode)) << "\n";
	//saveFile << "/*Gain Mode:*/\t\t\t\t" << HamamatsuGainMode::toStr(configSettings.hamamatsu.gainMode) << "\n";
	saveFile << "/*Frame Rate:*/\t\t\t\t" << configSettings.hamamatsu.frameRate << "\n";
	//saveFile << "/*Kinetic Cycle Time:*/\t\t" << configSettings.hamamatsu.kineticCycleTime << "\n";
	//saveFile << "/*Accumulation Time:*/\t\t" << configSettings.hamamatsu.accumulationTime << "\n";
	//saveFile << "/*Accumulation Number:*/\t" << configSettings.hamamatsu.accumulationNumber << "\n";
	saveFile << "/*Camera Temperature:*/\t\t" << configSettings.hamamatsu.temperatureSetting << "\n";
	saveFile << "/*Number of Exposures:*/\t" << configSettings.hamamatsu.exposureTimes.size ( ) 
			 << "\n/*Exposure Times:*/\t\t\t";
	for ( auto exposure : configSettings.hamamatsu.exposureTimes ){
		saveFile << exposure << " ";
	}
	// Continuous mode was removed; keep a 0 placeholder so the positional config format is unchanged.
	saveFile << "\n/*Hamamatsu Continuous Mode (retired):*/\t" << 0;
	saveFile << "\n/*Hamamatsu Pics Per Rep:*/\t\t" << configSettings.hamamatsu.picsPerRepetition;
	//saveFile << "\n/*Horizontal Shift Speed*/\t" << configSettings.hamamatsu.horShiftSpeedSetting;
	//saveFile << "\n/*Vertical Shift Speed*/\t" << configSettings.hamamatsu.vertShiftSpeedSetting;
	saveFile << "\nEND_CAMERA_SETTINGS\n";
	picSettingsObj.handleSaveConfig(saveFile);
	imageDimensionsObj.handleSave (saveFile);
}


void HamamatsuCameraSettingsControl::updateCameraMode( ){
	/* updates settings.hamamatsu.cameraMode based on combo selection, then updates 
		settings.hamamatsu.acquisitionMode and other settings depending on the mode.
	*/
	if (currentlyUneditable) {
		return;
	}
	int sel = cameraModeCombo->currentIndex( );
	if ( sel == -1 ){
		return;
	}
	//std::string txt (str(cameraModeCombo->currentText()));
	configSettings.hamamatsu.acquisitionMode = static_cast<int>(
		HamamatsuRunModes::fromStr(str(cameraModeCombo->currentText())));
	//if ( txt == HamamatsuRunModes::toStr (HamamatsuRunModes::mode::Single)){
	//	configSettings.hamamatsu.acquisitionMode = HamamatsuRunModes::mode::Single;
	//	//configSettings.hamamatsu.repetitionsPerVariation = INT_MAX;
	//}
	//else if ( txt == HamamatsuRunModes::toStr ( HamamatsuRunModes::mode::Kinetic )){
	//	configSettings.hamamatsu.acquisitionMode = HamamatsuRunModes::mode::Kinetic;
	//}
	//else if ( txt == HamamatsuRunModes::toStr ( HamamatsuRunModes::mode::Accumulate )){
	//	configSettings.hamamatsu.acquisitionMode = HamamatsuRunModes::mode::Accumulate;
	//}
	//else{
	//	thrower  ( "ERROR: unrecognized combo for hamamatsu run mode text???" );
	//}
}

//void HamamatsuCameraSettingsControl::updateGainMode()
//{
//	if (currentlyUneditable) {
//		return;
//	}
//	int sel = gainCombo->currentIndex();
//	if (sel == -1) {
//		return;
//	}
//	configSettings.hamamatsu.gainMode = HamamatsuGainMode::fromStr(str(gainCombo->currentText()));
//}

//void HamamatsuCameraSettingsControl::updateBinningMode()
//{
//	if (currentlyUneditable) {
//		return;
//	}
//	int sel = binningCombo->currentIndex();
//	if (sel == -1) {
//		return;
//	}
//	configSettings.hamamatsu.binningMode = HamamatsuBinningMode::fromStr(str(binningCombo->currentText()));
//	//imageDimensionsObj.setBinningMode(configSettings.hamamatsu.binningMode);
//}

void HamamatsuCameraSettingsControl::updateWindowEnabledStatus (){
	controlHamamatsuCameraCheck->setEnabled (!viewRunningSettings->isChecked ());
	cameraModeCombo->setEnabled (!viewRunningSettings->isChecked ());
	//emGainEdit->setEnabled (!viewRunningSettings->isChecked ());
	//emGainBtn->setEnabled (!viewRunningSettings->isChecked ());
	triggerCombo->setEnabled (!viewRunningSettings->isChecked ());
	if (coolerCombo) { coolerCombo->setEnabled (!viewRunningSettings->isChecked ()); }
	if (fanCombo) { fanCombo->setEnabled (!viewRunningSettings->isChecked ()); }
	//gainCombo->setEnabled(!viewRunningSettings->isChecked());
	//binningCombo->setEnabled(!viewRunningSettings->isChecked());

	auto settings = getConfigSettings ();
	// Exposure edit is meaningful except in Level trigger (where the TTL high-time is the exposure).
	picSettingsObj.toggleExposureTimeEditGui(
		settings.hamamatsu.triggerMode != HamamatsuTriggerMode::toStr(HamamatsuTriggerMode::mode::Level)
		&& !viewRunningSettings->isChecked());
	//accumulationCycleTimeEdit->setEnabled(settings.hamamatsu.acquisitionMode == HamamatsuRunModes::mode::Accumulate 
	//	&& !viewRunningSettings->isChecked ());
	//accumulationNumberEdit->setEnabled (settings.hamamatsu.acquisitionMode == HamamatsuRunModes::mode::Accumulate 
	//	&& !viewRunningSettings->isChecked ());
	//kineticCycleTimeEdit->setEnabled (settings.hamamatsu.acquisitionMode == HamamatsuRunModes::mode::Kinetic 
	//	&& !viewRunningSettings->isChecked ());
	
	imageDimensionsObj.updateEnabledStatus (viewRunningSettings->isChecked ());
	picSettingsObj.setEnabledStatus (viewRunningSettings->isChecked ());

}

//void HamamatsuCameraSettingsControl::updateMinKineticCycleTime( double time ){
//	minKineticCycleTimeDisp->setText( cstr( time ) );
//}

imageParameters HamamatsuCameraSettingsControl::readImageParameters(){
	return imageDimensionsObj.readImageParameters( );
}

void HamamatsuCameraSettingsControl::setImageParameters(imageParameters newSettings){
	imageDimensionsObj.setImageParametersFromInput(newSettings);
}

void HamamatsuCameraSettingsControl::checkIfReady(){
	if ( picSettingsObj.getUsedExposureTimes().size() == 0 ){
		thrower ("Please Set at least one exposure time.");
	}
	if ( !imageDimensionsObj.checkReady() ){
		thrower ("Please set the image parameters.");
	}
	if ( configSettings.hamamatsu.picsPerRepetition <= 0 ){
		thrower ("ERROR: Please set the number of pictures per repetition to a positive non-zero value.");
	}
	if (configSettings.hamamatsu.acquisitionMode == static_cast<int>(HamamatsuRunModes::mode::Slow)) {
		if ( configSettings.hamamatsu.repetitionsPerVariation <= 0 ){
			thrower ("ERROR: Please set the \"Repetitions Per Variation\" variable to a positive non-zero value.");
		}
		if ( configSettings.hamamatsu.totalVariations <= 0 ){
			thrower ("ERROR: Please set the number of variations to a positive non-zero value.");
		}
	}
	//if ( configSettings.hamamatsu.acquisitionMode == HamamatsuRunModes::mode::Accumulate ){
	//	if ( configSettings.hamamatsu.accumulationNumber <= 0 ){
	//		thrower ("ERROR: Please set the current Accumulation Number to a positive non-zero value.");
	//	}
	//	if ( configSettings.hamamatsu.accumulationTime <= 0 ){
	//		thrower ("ERROR: Please set the current Accumulation Time to a positive non-zero value.");
	//	}
	//}
}

void HamamatsuCameraSettingsControl::handelSaveMasterConfig ( std::stringstream& configFile ){
	imageParameters settings = getConfigSettings ( ).hamamatsu.imageSettings;
	configFile << settings.left << " " << settings.right << " " << settings.horizontalBinning << " ";
	configFile << settings.bottom << " " << settings.top << " " << settings.verticalBinning << "\n";
	// introduced in version 2.2
	configFile << getAutoCal ( ) << " " << getUseCal ( ) << "\n";
}

void HamamatsuCameraSettingsControl::handleOpenMasterConfig ( ConfigStream& configStream, QtHamamatsuWindow* camWin ){
	imageParameters settings = getConfigSettings ( ).hamamatsu.imageSettings;
	std::string tempStr;
	try	{
		configStream >> tempStr;
		settings.left = boost::lexical_cast<long> ( tempStr );
		configStream >> tempStr;
		settings.right = boost::lexical_cast<long> ( tempStr );
		configStream >> tempStr;
		settings.horizontalBinning = boost::lexical_cast<long> ( tempStr );
		configStream >> tempStr;
		settings.bottom = boost::lexical_cast<long> ( tempStr );
		configStream >> tempStr;
		settings.top = boost::lexical_cast<long> ( tempStr );
		configStream >> tempStr;
		settings.verticalBinning = boost::lexical_cast<long> ( tempStr );
		setImageParameters ( settings );
	}
	catch ( boost::bad_lexical_cast& ){
		throwNested ( "ERROR: Bad value (i.e. failed to convert to long) seen in master configueration file while attempting "
				  "to load camera dimensions!" );
	}
}


std::vector<Matrix<long>> HamamatsuCameraSettingsControl::getImagesToDraw ( const std::vector<Matrix<long>>& rawData ){
	std::vector<Matrix<long>> imagesToDraw ( rawData.size ( ) );
	auto options = picSettingsObj.getDisplayTypeOptions ( );
	for ( auto picNum : range ( rawData.size ( ) ) ){
		auto option = picNum < options.size() ? options[picNum] : displayTypeOption{};
		if ( !option.isDiff ){
			imagesToDraw[ picNum ] = rawData[ picNum ];
		}
		else{
			// the whichPic variable is 1-indexed.
			if ( option.whichPicForDif >= rawData.size ( ) ){
				imagesToDraw[ picNum ] = rawData[ picNum ];
			}
			else{
				imagesToDraw[ picNum ] = Matrix<long>(rawData[picNum].getRows(), rawData[picNum].getCols(), 0);
				for ( auto i : range ( rawData[ picNum ].size ( ) ) ){
					imagesToDraw[ picNum ].data[ i ] = rawData[ picNum ].data[ i ] - rawData[ option.whichPicForDif - 1 ].data[ i ];
				}
			}
		}
	}
	return imagesToDraw;
}


