#include "stdafx.h"
#include "QtScriptWindow.h"
#include <qdesktopwidget.h>
#include <qlayout.h>
#include <qcombobox.h>
#include <qstackedwidget.h>
#include <PrimaryWindows/QtScriptWindow.h>
#include <PrimaryWindows/QtAndorWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <PrimaryWindows/QtMakoWindow.h>
#include <PrimaryWindows/QtMainWindow.h>
#include <ExcessDialogs/saveWithExplorer.h>
#include <ExcessDialogs/openWithExplorer.h>
#include "WieserlabsDDS/WieserlabsDDSSystem.h"
#include "WieserlabsDDS/WieserlabsDDSSettings.h"

QtScriptWindow::QtScriptWindow(QWidget* parent) : IChimeraQtWindow(parent)
	, masterScript(this)
	, arbGens{ {ArbGenSystem(UWAVE_SIGLENT_SETTINGS, ArbGenType::Siglent, this),
	            ArbGenSystem(UWAVE_SIGLENT_SETTINGS_2, ArbGenType::Siglent, this) } }
	, gigaMoog(this)
	, wieserlabsDds(WIESERLABS_DDS_SETTINGS, this)
{
	setWindowTitle ("Script Window");
}

QtScriptWindow::~QtScriptWindow (){
}

void QtScriptWindow::initializeWidgets (){
	statBox = new ColorBox(this, mainWin->getDevices());
	QWidget* centralWidget = new QWidget();
	setCentralWidget(centralWidget);
	QHBoxLayout* layout = new QHBoxLayout(centralWidget);
	//centralWidget->setStyleSheet("border: 2px solid  black; ");
	for (auto name : ArbGenEnum::allAgs) {
		arbGens[(int)name].initialize(arbGens[(int)name].initSettings.deviceName, this);
	}

	wieserlabsDds.initialize("Wieserlabs DDS", this);

	masterScript.initialize(this, "Master", "Master Script");
	gigaMoog.initialize(this);
	//profileDisplay.initialize (this);
	QVBoxLayout* layout1 = new QVBoxLayout(this);
	layout1->setContentsMargins(0, 0, 0, 0);
	arbSelector = new QComboBox(this);
	arbStack = new QStackedWidget(this);
	for (auto name : ArbGenEnum::allAgs) {
		auto idx = static_cast<int>(name);
		arbSelector->addItem(qstr(arbGens[idx].initSettings.deviceName));
		arbStack->addWidget(&arbGens[idx]);
	}
	if (numArbGen <= 1) {
		arbSelector->setVisible(false);
	}
	connect(arbSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (arbStack && index >= 0 && index < arbStack->count()) {
			arbStack->setCurrentIndex(index);
		}
		});
	layout1->addWidget(arbSelector, 0);
	layout1->addWidget(arbStack, 1);
	layout1->addWidget(&wieserlabsDds, 1);
	layout->addLayout(layout1, 1);
	layout->addWidget(&gigaMoog, 1);
	layout->addWidget(&masterScript, 1);
	
	// Requested behavior: on startup only connect; do not force ArbGen channels to DC defaults.


	updateDoAoDdsNames ();
	updateVarNames ();
}

void QtScriptWindow::updateVarNames() {
	auto params = auxWin->getAllParams ();
	masterScript.highlighter->setGlobalParams(auxWin->getGlobalParams());
	masterScript.highlighter->setOtherParams (auxWin->getConfigParams());
	masterScript.highlighter->setLocalParams (masterScript.getLocalParams ());
	masterScript.highlighter->rehighlight();

	wieserlabsDds.wieserlabsDdsScript->highlighter->setOtherParams(params);
	wieserlabsDds.wieserlabsDdsScript->highlighter->setLocalParams(wieserlabsDds.wieserlabsDdsScript->getLocalParams());
	wieserlabsDds.wieserlabsDdsScript->highlighter->rehighlight();
}

void QtScriptWindow::updateDoAoDdsNames () {
	auto doNamesArr = auxWin->getTtlNames ();
	auto doNames = std::vector<std::string>(doNamesArr.begin(), doNamesArr.end());
	auto aoNamesArr = auxWin->getDacNames();
	auto aoNames = std::vector<std::string>(aoNamesArr.begin(), aoNamesArr.end());
	auto calNames = auxWin->getCalNames();

	
	masterScript.highlighter->setTtlNames(doNames);
	masterScript.highlighter->setDacNames(aoNames);
	masterScript.highlighter->setCalNames(calNames);

	wieserlabsDds.wieserlabsDdsScript->highlighter->setTtlNames(doNames);
	wieserlabsDds.wieserlabsDdsScript->highlighter->setDacNames(aoNames);
	wieserlabsDds.wieserlabsDdsScript->highlighter->setCalNames(calNames);
	wieserlabsDds.wieserlabsDdsScript->highlighter->rehighlight();
}


void QtScriptWindow::handleMasterFunctionChange (){
	try{
		masterScript.functionChangeHandler (mainWin->getProfileSettings ().configLocation);
		masterScript.updateSavedStatus (true);
	}
	catch (ChimeraError& err){
		errBox (err.trace ());
	}
}

void QtScriptWindow::checkScriptSaves (){
	masterScript.checkSave (getProfile ().configLocation, mainWin->getRunInfo());
	gigaMoog.gmoogScript.checkSave(getProfile().configLocation, mainWin->getRunInfo());
	wieserlabsDds.checkSave(getProfile().configLocation, mainWin->getRunInfo());
}

std::string QtScriptWindow::getSystemStatusString (){
	//std::string status = "Intensity Agilent:\n\t" + intensityAgilent.getDeviceIdentity();
	std::string status;
	for (auto name : ArbGenEnum::allAgs) {
		status += arbGens[(int)name].initSettings.deviceName + ":\n\t" + arbGens[(int)name].getDeviceIdentity();
		status += "\t";
		status += "Attached trigger line is \n\t\t";
		{
			status += "(" + str(arbGens[(int)name].initSettings.triggerRow) + "," + str(arbGens[(int)name].initSettings.triggerNumber) + ") ";
		}
		status += "\n";
	}
	status += "GIGAMOOG:\n\t";
	if (!GIGAMOOG_SAFEMODE) {
		status += str("GIGAMOOG System is Active at " + GIGAMOOG_IPADDRESS + " and port," + str(GIGAMOOG_IPPORT) + "\n\t");
		status += "Attached trigger line is \n\t\t";
		for (const auto& gmtrig : GM_TRIGGER_LINE) {
			status += "(" + str(gmtrig.first) + "," + str(gmtrig.second) + ") ";
		}
		status += "\n";
	}
	else {
		status += "\tGIGAMOOG System is disabled! Enable in \"constants.h\" \n";
	}
	status += "WIESERLABS DDS:\n\t";
	if (!WIESERLABS_SAFEMODE) {
		status += str("WIESERLABS DDS System is Active at " + WIESERLABS_IPADDRESS + " and port," + str(WIESERLABS_IPPORT) + "\n\t");
		std::string slotStr = "Connected: " + std::string(wieserlabsDds.getCore().connected() ? "Yes" : "No") + "\n";
		status += slotStr;
	}
	else {
		status += "\tWIESERLABS DDS System is disabled! Enable in \"constants.h\" \n";
	}
	return status;
}

/* 
  This function retuns the names (just the names) of currently active scripts.
*/
scriptInfo<std::string> QtScriptWindow::getScriptNames (){
	scriptInfo<std::string> names;
	names.master = masterScript.getScriptName ();
	names.gmoog = gigaMoog.gmoogScript.getScriptName();
	names.wieserlabsDds = wieserlabsDds.wieserlabsDdsScript->getScriptName();
	return names;
}

/*
  This function returns indicators of whether a given script has been saved or not.
*/
scriptInfo<bool> QtScriptWindow::getScriptSavedStatuses (){
	scriptInfo<bool> status;
	status.master = masterScript.savedStatus ();
	status.gmoog = gigaMoog.gmoogScript.savedStatus();
	status.wieserlabsDds = wieserlabsDds.wieserlabsDdsScript->savedStatus();
	return status;
}

/*
  This function returns the current addresses of all files in all scripts.
*/
scriptInfo<std::string> QtScriptWindow::getScriptAddresses (){
	scriptInfo<std::string> addresses;
	addresses.master = masterScript.getScriptPathAndName ();
	addresses.gmoog = gigaMoog.gmoogScript.getScriptPathAndName();
	addresses.wieserlabsDds = wieserlabsDds.wieserlabsDdsScript->getScriptPathAndName();
	return addresses;
}

void QtScriptWindow::setIntensityDefault() 
{
	try {
		for (auto name : ArbGenEnum::allAgs) {
			arbGens[(int)name].setDefault(1);
			arbGens[(int)name].setDefault(2);
		}
	}
	catch (ChimeraError& err) {
		reportErr(err.qtrace());
	}
}

/// Commonly Called Functions
/*
	The following set of functions, mostly revolving around saving etc. of the script files, are called by all of the
	window objects because they are associated with the menu at the top of each screen
*/

void QtScriptWindow::updateArbGen(ArbGenEnum::name name) {
	(void)name;
}


void QtScriptWindow::newArbGenScript(ArbGenEnum::name name) 
{
	(void)name;
	reportErr("ArbGen scripting is no longer supported.");
}

void QtScriptWindow::openArbGenScript(ArbGenEnum::name name, IChimeraQtWindow* parent)
{
	(void)name;
	(void)parent;
	reportErr("ArbGen scripting is no longer supported.");
}

void QtScriptWindow::saveArbGenScript(ArbGenEnum::name name) {
	(void)name;
	reportErr("ArbGen scripting is no longer supported.");
}

void QtScriptWindow::saveArbGenScriptAs(ArbGenEnum::name name, IChimeraQtWindow* parent) {
	(void)name;
	(void)parent;
	reportErr("ArbGen scripting is no longer supported.");
}




// just a quick shortcut.
profileSettings QtScriptWindow::getProfile (){
	return mainWin->getProfileSettings ();
}

void QtScriptWindow::windowOpenConfig (ConfigStream& configFile){
	qDebug() << "QtScriptWindow::windowOpenConfig CALLED";
	try{
		ConfigSystem::initializeAtDelim (configFile, "SCRIPTS");
	}
	catch (ChimeraError&){
		reportErr ("Failed to initialize configuration file at scripting window entry point \"SCRIPTS\".");
		return;
	}
	try{
		auto getlineFunc = ConfigSystem::getGetlineFunc (configFile.ver);
		std::string masterName/*, gmoogName*/;
		// order should match the windowsaveconfig
		getlineFunc (configFile, masterName);
		//getlineFunc(configFile, gmoogName);
		ConfigSystem::checkDelimiterLine (configFile, "END_SCRIPTS");
		try {
			openMasterScript(masterName);
		}
		catch (ChimeraError& err) {
			auto answer = QMessageBox::question(this, "Open Failed", "ERROR: Failed to open master script file: "
				+ qstr(masterName) + ", with error \r\n" + err.qtrace() + "\r\nAttempt to find file yourself?");
			if (answer == QMessageBox::Yes) {
				openMasterScript(openWithExplorer(nullptr, "mScript", CONFIGURATION_PATH));
			}
		}

		ConfigSystem::standardOpenConfig(configFile, gigaMoog.getDelim(), &gigaMoog);
		try {
			openGMoogScript(gigaMoog.scriptAddress);
		}
		catch (ChimeraError& err) {
			auto answer = QMessageBox::question(this, "Open Failed", "ERROR: Failed to open master script file: "
				+ qstr(gigaMoog.scriptAddress) + ", with error \r\n" + err.qtrace() + "\r\nAttempt to find file yourself?");
			if (answer == QMessageBox::Yes) {
				openGMoogScript(openWithExplorer(nullptr, "gScript", CONFIGURATION_PATH));
			}
		}
		try {
			// New format: one SIGLENT_AWG section with AWG_1, AWG_2, ... entries.
			ConfigSystem::initializeAtDelim(configFile, "SIGLENT_AWG", Version("1.0"));
			std::streampos posAfterDelim = configFile.tellg();
			std::string firstToken;
			configFile >> firstToken;
			configFile.clear();
			configFile.seekg(posAfterDelim);

			auto lowerToken = [](std::string value) {
				std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
					return static_cast<char>(std::tolower(character));
				});
				return value;
			};

			bool parsedUnified = false;
			if (lowerToken(firstToken).rfind("awg_", 0) == 0) {
				parsedUnified = true;
				for (auto name : ArbGenEnum::allAgs) {
					ConfigSystem::checkDelimiterLine(configFile, "AWG_" + str((int)name + 1));
					auto info = arbGens[(int)name].getCore().getSettingsFromConfig(configFile);
					arbGens[(int)name].setOutputSettings(info);
				}
				ConfigSystem::checkDelimiterLine(configFile, "END_SIGLENT_AWG");
			}

			if (!parsedUnified) {
				// Legacy format compatibility.
				auto info0 = arbGens[(int)ArbGenEnum::name::Siglent0].getCore().getSettingsFromConfig(configFile);
				arbGens[(int)ArbGenEnum::name::Siglent0].setOutputSettings(info0);

				// Legacy variant A: single SIGLENT_AWG block with one AWG payload.
				// Legacy variant B: single SIGLENT_AWG block with two consecutive AWG payloads.
				std::streampos posBeforeEndCheck = configFile.tellg();
				bool parsedEndOfBlock = false;
				try {
					ConfigSystem::checkDelimiterLine(configFile, "END_SIGLENT_AWG");
					parsedEndOfBlock = true;
				}
				catch (ChimeraError&) {
					configFile.clear();
					configFile.seekg(posBeforeEndCheck);
				}

				if (!parsedEndOfBlock) {
					try {
						auto info1InSameBlock = arbGens[(int)ArbGenEnum::name::Siglent1].getCore().getSettingsFromConfig(configFile);
						arbGens[(int)ArbGenEnum::name::Siglent1].setOutputSettings(info1InSameBlock);
						ConfigSystem::checkDelimiterLine(configFile, "END_SIGLENT_AWG");
						parsedEndOfBlock = true;
					}
					catch (ChimeraError&) {
						configFile.clear();
						configFile.seekg(posBeforeEndCheck);
					}
				}

				if (parsedEndOfBlock) {
					// Parsed fully from the current SIGLENT_AWG block.
				}
				else {

					try {
						ConfigSystem::initializeAtDelim(configFile, "SIGLENT_AWG", Version("1.0"));
						auto info1 = arbGens[(int)ArbGenEnum::name::Siglent1].getCore().getSettingsFromConfig(configFile);
						arbGens[(int)ArbGenEnum::name::Siglent1].setOutputSettings(info1);
						ConfigSystem::checkDelimiterLine(configFile, "END_SIGLENT_AWG");
					}
					catch (ChimeraError&) {
						try {
							ConfigSystem::initializeAtDelim(configFile, "SIGLENT_AWG_2", Version("1.0"));
							auto info1 = arbGens[(int)ArbGenEnum::name::Siglent1].getCore().getSettingsFromConfig(configFile);
							arbGens[(int)ArbGenEnum::name::Siglent1].setOutputSettings(info1);
							std::streampos posBeforeEndAwg2 = configFile.tellg();
							try {
								ConfigSystem::checkDelimiterLine(configFile, "END_SIGLENT_AWG_2");
							}
							catch (ChimeraError&) {
								// Legacy variant: a second payload may appear before END_SIGLENT_AWG_2.
								configFile.clear();
								configFile.seekg(posBeforeEndAwg2);

								std::string nextTokenAwg2;
								configFile >> nextTokenAwg2;
								configFile.clear();
								configFile.seekg(posBeforeEndAwg2);
								std::string loweredAwg2 = lowerToken(nextTokenAwg2);
								bool looksLikeAnotherPayload =
									(loweredAwg2 == "0" || loweredAwg2 == "1" || loweredAwg2 == "true" || loweredAwg2 == "false"
										|| loweredAwg2.rfind("awg_", 0) == 0);

								if (looksLikeAnotherPayload) {
									auto info1Extra = arbGens[(int)ArbGenEnum::name::Siglent1].getCore().getSettingsFromConfig(configFile);
									arbGens[(int)ArbGenEnum::name::Siglent1].setOutputSettings(info1Extra);
									ConfigSystem::checkDelimiterLine(configFile, "END_SIGLENT_AWG_2");
								}
								// If it doesn't look like payload start (e.g. next section token),
								// treat missing END_SIGLENT_AWG_2 as legacy/non-fatal and continue.
							}
						}
						catch (ChimeraError& err) {
							qDebug() << "QtScriptWindow: skipping missing ArbGen config block for"
								<< qstr(arbGens[(int)ArbGenEnum::name::Siglent1].initSettings.deviceName) << ":" << qstr(err.qtrace());
						}
					}
				}
			}
		}
		catch (ChimeraError& err) {
			qDebug() << "QtScriptWindow: skipping ArbGen config load:" << qstr(err.qtrace());
		}

		deviceOutputInfo ddsInfo;
		try {
			qDebug() << "QtScriptWindow: Loading DDS config...";
			// Load DDS channel settings (stdGetFromConfig handles everything including END delimiter)
			ConfigSystem::stdGetFromConfig(configFile, wieserlabsDds.getCore(), ddsInfo, Version("1.0"));
			wieserlabsDds.setOutputSettings(ddsInfo);
			wieserlabsDds.updateSettingsDisplay(getProfileSettings().configLocation, mainWin->getRunInfo());
			qDebug() << "QtScriptWindow: DDS settings loaded successfully";
			
			// Auto-open DDS script if one was saved
			std::string scriptAddr = wieserlabsDds.getCore().getLoadedScriptAddress();
			qDebug() << "DDS Script Address from config:" << qstr(scriptAddr);
			// Simple check: if not empty and not the special empty marker, try to load
			if (!scriptAddr.empty() && scriptAddr != "!#EMPTY_STRING#!") {
				try {
					qDebug() << "Attempting to open DDS script:" << qstr(scriptAddr);
					openWieserlabsDDSScript(scriptAddr);
					qDebug() << "DDS script opened successfully";
				}
				catch (ChimeraError& err) {
					qDebug() << "Failed to open DDS script:" << err.qtrace();
					// Script address exists but file not found - just report, don't force dialog
					reportErr(qstr("DDS script from config not found: " + err.trace()));
				}
			}
			else {
				qDebug() << "No DDS script address in config - user can open manually if needed";
			}
		}
		catch (ChimeraError& err) {
			// DDS config not present in file, skip loading
			qDebug() << "DDS config load failed:" << err.qtrace();
			reportErr("DDS config load skipped: " + err.qtrace());
		}

		considerScriptLocations();
	}
	catch (ChimeraError& err)	{
		reportErr ("Scripting Window failed to read parameters from the configuration file.\n\n" + err.qtrace ());
	}

	// Requested behavior: do not auto-program Siglent AWGs after config load.
}

void QtScriptWindow::newMasterScript (){
	try {
		masterScript.checkSave (getProfile ().configLocation, mainWin->getRunInfo());
		masterScript.newScript ();
		updateConfigurationSavedStatus (false);
		masterScript.updateScriptNameText (getProfile ().configLocation);
	}
	catch (ChimeraError & err) {
		reportErr (err.qtrace ());
	}
}

void QtScriptWindow::openMasterScript (IChimeraQtWindow* parent){
	try	{
		masterScript.checkSave (getProfile ().configLocation, mainWin->getRunInfo());
		std::string openName = openWithExplorer (parent, Script::MASTER_SCRIPT_EXTENSION, CONFIGURATION_PATH);
		masterScript.openParentScript (openName, getProfile ().configLocation, mainWin->getRunInfo());
		updateConfigurationSavedStatus (false);
		masterScript.updateScriptNameText (getProfile ().configLocation);
	}
	catch (ChimeraError& err){
		reportErr ("Open Master Script Failed: " + err.qtrace () + "\r\n");
	}
}

void QtScriptWindow::openMasterScript(std::string name, bool askMove) {
	masterScript.openParentScript(name, getProfile().configLocation, mainWin->getRunInfo(), askMove);
}

void QtScriptWindow::saveMasterScript (){
	if (masterScript.isFunction ())	{
		masterScript.saveAsFunction ();
		return;
	}
	masterScript.saveScript (getProfile ().configLocation, mainWin->getRunInfo());
	masterScript.updateScriptNameText (getProfile ().configLocation);
}

void QtScriptWindow::saveMasterScriptAs (IChimeraQtWindow* parent){
	std::string extensionNoPeriod = masterScript.getExtension ();
	if (extensionNoPeriod.size () == 0)	{
		return;
	}
	extensionNoPeriod = extensionNoPeriod.substr (1, extensionNoPeriod.size ());
	std::string newScriptAddress = saveWithExplorer (parent, extensionNoPeriod, getProfileSettings ());
	masterScript.saveScriptAs (newScriptAddress, mainWin->getRunInfo());
	updateConfigurationSavedStatus (false);
	masterScript.updateScriptNameText (getProfile ().configLocation);
}

void QtScriptWindow::newMasterFunction (){
	try{
		masterScript.newFunction ();
	}
	catch (ChimeraError& exception){
		reportErr ("New Master function Failed: " + exception.qtrace () + "\r\n");
	}
}

void QtScriptWindow::reloadMasterFunction()
{
	try {
		masterScript.loadFunctions();
	}
	catch (ChimeraError& exception) {
		reportErr("New Master function Failed: " + exception.qtrace() + "\r\n");
	}
}

void QtScriptWindow::saveMasterFunction (){
	try{
		masterScript.saveAsFunction ();
	}
	catch (ChimeraError& exception){
		reportErr ("Save Master Script Function Failed: " + exception.qtrace () + "\r\n");
	}
}

void QtScriptWindow::deleteMasterFunction (){
	// todo. Right now you can just delete the file itself...
}

void QtScriptWindow::newGMoogScript()
{
	try {
		gigaMoog.gmoogScript.checkSave(getProfile().configLocation, mainWin->getRunInfo());
		gigaMoog.gmoogScript.newScript();
		updateConfigurationSavedStatus(false);
		gigaMoog.gmoogScript.updateScriptNameText(getProfile().configLocation);
	}
	catch (ChimeraError& err) {
		reportErr(err.qtrace());
	}
}

void QtScriptWindow::openGMoogScript(IChimeraQtWindow* parent)
{
	try {
		gigaMoog.gmoogScript.checkSave(getProfile().configLocation, mainWin->getRunInfo());
		std::string openName = openWithExplorer(parent, Script::GMOOG_SCRIPT_EXTENSION, CONFIGURATION_PATH);
		gigaMoog.gmoogScript.openParentScript(openName, getProfile().configLocation, mainWin->getRunInfo());
		updateConfigurationSavedStatus(false);
		gigaMoog.gmoogScript.updateScriptNameText(getProfile().configLocation);
	}
	catch (ChimeraError& err) {
		reportErr("Open GigaMoog Script Failed: " + err.qtrace() + "\r\n");
	}
}

void QtScriptWindow::openGMoogScript(std::string name)
{
	gigaMoog.gmoogScript.openParentScript(name, getProfile().configLocation, mainWin->getRunInfo());
}

void QtScriptWindow::saveGMoogScript()
{
	gigaMoog.gmoogScript.saveScript(getProfile().configLocation, mainWin->getRunInfo());
	gigaMoog.gmoogScript.updateScriptNameText(getProfile().configLocation);
}

void QtScriptWindow::saveGMoogScriptAs(IChimeraQtWindow* parent)
{
	std::string extensionNoPeriod = gigaMoog.gmoogScript.getExtension();
	if (extensionNoPeriod.size() == 0) {
		return;
	}
	extensionNoPeriod = extensionNoPeriod.substr(1, extensionNoPeriod.size());
	std::string newScriptAddress = saveWithExplorer(parent, extensionNoPeriod, getProfileSettings());
	gigaMoog.gmoogScript.saveScriptAs(newScriptAddress, mainWin->getRunInfo());
	updateConfigurationSavedStatus(false);
	gigaMoog.gmoogScript.updateScriptNameText(getProfile().configLocation);
}

void QtScriptWindow::newWieserlabsDDSScript()
{
	try {
		wieserlabsDds.wieserlabsDdsScript->checkSave(getProfile().configLocation, mainWin->getRunInfo());
		wieserlabsDds.wieserlabsDdsScript->newScript();
		updateConfigurationSavedStatus(false);
		wieserlabsDds.wieserlabsDdsScript->updateScriptNameText(getProfile().configLocation);
	}
	catch (ChimeraError& err) {
		reportErr(err.qtrace());
	}
}

void QtScriptWindow::openWieserlabsDDSScript(IChimeraQtWindow* parent)
{
	try {
		wieserlabsDds.wieserlabsDdsScript->checkSave(getProfile().configLocation, mainWin->getRunInfo());
		std::string openName = openWithExplorer(parent, Script::DDS_SCRIPT_EXTENSION, CONFIGURATION_PATH);
		wieserlabsDds.wieserlabsDdsScript->openParentScript(openName, getProfile().configLocation, mainWin->getRunInfo());
		updateConfigurationSavedStatus(false);
		wieserlabsDds.wieserlabsDdsScript->updateScriptNameText(getProfile().configLocation);
		wieserlabsDds.refreshScriptedWaveform(); // Parse the script into waveform
	}
	catch (ChimeraError& err) {
		reportErr("Open Wieserlabs DDS Script Failed: " + err.qtrace() + "\r\n");
	}
}

void QtScriptWindow::openWieserlabsDDSScript(std::string name)
{
	try {
		qDebug() << "openWieserlabsDDSScript: Opening script at path:" << qstr(name);
		wieserlabsDds.wieserlabsDdsScript->openParentScript(name, getProfile().configLocation, mainWin->getRunInfo());
		qDebug() << "openWieserlabsDDSScript: Script opened, now refreshing waveform";
		wieserlabsDds.refreshScriptedWaveform(); // Parse the script into waveform
		qDebug() << "openWieserlabsDDSScript: Waveform refreshed";
	}
	catch (ChimeraError& err) {
		reportErr("ERROR opening DDS script: " + err.qtrace());
	}
}

void QtScriptWindow::saveWieserlabsDDSScript()
{
	wieserlabsDds.wieserlabsDdsScript->saveScript(getProfile().configLocation, mainWin->getRunInfo());
	wieserlabsDds.wieserlabsDdsScript->updateScriptNameText(getProfile().configLocation);
}

void QtScriptWindow::saveWieserlabsDDSScriptAs(IChimeraQtWindow* parent)
{
	std::string extensionNoPeriod = wieserlabsDds.wieserlabsDdsScript->getExtension();
	if (extensionNoPeriod.size() == 0) {
		return;
	}
	extensionNoPeriod = extensionNoPeriod.substr(1, extensionNoPeriod.size());
	std::string newScriptAddress = saveWithExplorer(parent, extensionNoPeriod, getProfileSettings());
	wieserlabsDds.wieserlabsDdsScript->saveScriptAs(newScriptAddress, mainWin->getRunInfo());
	updateConfigurationSavedStatus(false);
	wieserlabsDds.wieserlabsDdsScript->updateScriptNameText(getProfile().configLocation);
}

void QtScriptWindow::saveAllScript()
{
	saveMasterScript();
	saveGMoogScript();
	saveWieserlabsDDSScript();
}

void QtScriptWindow::windowSaveConfig (ConfigStream& saveFile){
	scriptInfo<std::string> addresses = getScriptAddresses ();
	// order matters!
	saveFile << "SCRIPTS\n";
	saveFile << "/*Master Script Address:*/ " << addresses.master << "\n";
	//saveFile << "/*GigaMoog Script Address:*/ " << addresses.gmoog << "\n";
	saveFile << "END_SCRIPTS\n";
	gigaMoog.handleSaveConfig(saveFile);
	saveFile << "SIGLENT_AWG\n";
	for (auto name : ArbGenEnum::allAgs) {
		saveFile << "AWG_" + str((int)name + 1) + "\n";
		arbGens[(int)name].handleSavingConfig(saveFile, getProfileSettings().configLocation,
			mainWin->getRunInfo(), false);
	}
	saveFile << "END_SIGLENT_AWG\n";
	wieserlabsDds.handleSavingConfig(saveFile, getProfileSettings().configLocation, mainWin->getRunInfo());
}

void QtScriptWindow::checkMasterSave (){
	masterScript.checkSave (getProfile ().configLocation, mainWin->getRunInfo());
	gigaMoog.gmoogScript.checkSave(getProfile().configLocation, mainWin->getRunInfo());
	wieserlabsDds.wieserlabsDdsScript->checkSave(getProfile().configLocation, mainWin->getRunInfo());
}

void QtScriptWindow::considerScriptLocations() {
	masterScript.considerCurrentLocation(getProfile().configLocation, mainWin->getRunInfo());
	gigaMoog.gmoogScript.considerCurrentLocation(getProfile().configLocation, mainWin->getRunInfo());
}

//void QtScriptWindow::updateProfile (std::string text){
//	//profileDisplay.update (text);
//}

profileSettings QtScriptWindow::getProfileSettings (){
	return mainWin->getProfileSettings ();
}

void QtScriptWindow::updateConfigurationSavedStatus (bool status){
	mainWin->updateConfigurationSavedStatus (status);
}

void QtScriptWindow::fillExpDeviceList (DeviceList& list) {
	for (auto name : ArbGenEnum::allAgs) {
		list.list.push_back(arbGens[(int)name].getCore());
	}
	list.list.push_back(wieserlabsDds.getCore());
	list.list.push_back(gigaMoog.getCore());
}

void QtScriptWindow::fillMasterThreadInput(ExperimentThreadInput* input) {
	// Push current Wieserlabs GUI state (including static expressions/control)
	// and refresh scripted waveform before the experiment starts.
	qDebug() << "QtScriptWindow::fillMasterThreadInput: Updating Wieserlabs DDS run settings";
	wieserlabsDds.readGuiSettings();
	// Note: Other devices like arbGens and gigaMoog don't need pre-experiment refresh
	// as their scripts are parsed differently
}

std::vector<std::reference_wrapper<ArbGenSystem>> QtScriptWindow::getArbGenSystem()
{
	std::vector<std::reference_wrapper<ArbGenSystem>> ags;
	for (ArbGenSystem& ag : arbGens) {
		ags.push_back(ag);
	}
	return ags;
}

std::vector<std::reference_wrapper<ArbGenCore>> QtScriptWindow::getArbGenCore()
{
	std::vector<std::reference_wrapper<ArbGenCore>> agCores;
	for (ArbGenSystem& ag : arbGens) {
		agCores.push_back(ag.getCore());
	}
	return agCores;
}