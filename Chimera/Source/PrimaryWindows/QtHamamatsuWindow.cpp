#include "stdafx.h"
#include "QtHamamatsuWindow.h"
#include <qdesktopwidget.h>
#include <PrimaryWindows/QtScriptWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <PrimaryWindows/QtMakoWindow.h>
#include <PrimaryWindows/QtMainWindow.h>
#include <RealTimeDataAnalysis/AnalysisThreadWorker.h>
#include <Rearrangement/AtomCruncherWorker.h>
#include <Hamamatsu/HamamatsuCameraThreadImageGrabber.h>
#include <Hamamatsu/cameraThreadInput.h>
#include <ExperimentThread/ExpThreadWorker.h>
#include <QThread.h>
#include <QScrollArea>
#include <qelapsedtimer.h>
#include <qdebug.h>


QtHamamatsuWindow::QtHamamatsuWindow (QWidget* parent) : IChimeraQtWindow (parent),
	hamamatsuSettingsCtrl (),
	dataHandler (DATA_SAVE_LOCATION, this),
	hamamatsu (HAM_SAFEMODE),
	pics (false, "HAMAMATSU_PICTURE_MANAGER", false, Qt::SmoothTransformation),
	analysisHandler (this)
{
	
	setWindowTitle ("HAMAMATSU Window");
}

QtHamamatsuWindow::~QtHamamatsuWindow (){}

int QtHamamatsuWindow::getDataCalNum () {
	return dataHandler.getCalibrationFileIndex ();
}

void QtHamamatsuWindow::initializeWidgets (){
	statBox = new ColorBox(this, mainWin->getDevices());
	
	QWidget* centralWidget = new QWidget();
	setCentralWidget(centralWidget);
	QHBoxLayout* layout = new QHBoxLayout(centralWidget);
	layout->setContentsMargins(2, 2, 2, 2);

	QVBoxLayout* layout1 = new QVBoxLayout();
	layout1->setContentsMargins(0, 0, 0, 0);
	hamamatsu.initializeClass(&imageTimes);
	alerts.alertMainThread (0);
	alerts.initialize (this);
	analysisHandler.initialize (this);
	hamamatsuSettingsCtrl.initialize ( this, std::vector<std::string>()/*hamamatsu.getVertShiftSpeeds()*/, std::vector<std::string>()/*hamamatsu.getHorShiftSpeeds()*/);
	alerts.setMaximumWidth(450);
	analysisHandler.setMaximumSize(450, 300);
	hamamatsuSettingsCtrl.setMaximumWidth(450);
	layout1->addWidget(&alerts);
	layout1->addWidget(&analysisHandler);
	layout1->addWidget(&hamamatsuSettingsCtrl);
	layout1->addStretch(0);

	QVBoxLayout* layout2 = new QVBoxLayout();
	layout2->setContentsMargins(0, 0, 0, 0);
	stats.initialize (this);
	layout2->addWidget(&stats);
	for (auto pltInc : range (6)){
		mainAnalysisPlots.push_back (new QCustomPlotCtrl(1, plotStyle::BinomialDataPlot, { 0,0,0,0 }, false, false));
		mainAnalysisPlots.back()->init(this, "INACTIVE");
		mainAnalysisPlots.back()->plot->setMinimumSize(350, 90);
		mainAnalysisPlots.back()->plot->setMaximumSize(450, 120);
		mainAnalysisPlots.back()->plot->hide();
	}
	layout2->addStretch(1);

	QVBoxLayout* layout3 = new QVBoxLayout();
	layout3->setContentsMargins(0, 0, 0, 0);
	timer.initialize (this);
	timer.setMinimumWidth(750);
	pics.initialize (this);
	QScrollArea* pictureScrollArea = new QScrollArea(this);
	pictureScrollArea->setWidgetResizable(true);
	pictureScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	pictureScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	pictureScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pictureScrollArea->setMinimumSize(820, 620);
	pictureScrollArea->setWidget(&pics);
	// end of literal initialization calls
	//pics.setSinglePicture (andorSettingsCtrl.getConfigSettings ().andor.imageSettings);
	timer.setMaximumHeight(45);
	layout3->addWidget(&timer);
	layout3->addWidget(pictureScrollArea, 1);
	layout3->addStretch();



	hamamatsu.setSettings (hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu);
	layout->addLayout(layout1, 0);
	layout->addLayout(layout2, 0);
	layout->addLayout(layout3, 1);
	layout->setStretch(0, 0);
	layout->setStretch(1, 0);
	layout->setStretch(2, 1);
	
	//pics.setMultiplePictures(imageParameters(), 4);
	//pics.setSinglePicture(imageParameters());


	QTimer* timer = new QTimer (this);
	connect (timer, &QTimer::timeout, [this]() {
		auto temp = hamamatsu.getTemperature ();
		hamamatsuSettingsCtrl.changeTemperatureDisplay (temp); 
		});
	
	timer->start (2000);
}

void QtHamamatsuWindow::manualArmCamera () {
	try {
		double time;
		hamamatsu.armCamera (this, time);
		// Start pulling frames out only after the camera is armed and running.
		startImageGrabberThread ();
	}
	catch (ChimeraError & err) {
		reportErr (qstr (err.trace ()));
	}
}

void QtHamamatsuWindow::startImageGrabberThread () {
	// Fresh image queue for this run so a previous run's leftovers don't linger.
	hamamatsu.getGrabberQueue ()->clear ();

	auto* grabberInput = new cameraThreadImageGrabberInput;
	grabberInput->Hamamatsu = &hamamatsu;
	grabberInput->cruncherThreadActive = &atomCrunchThreadActive;
	grabberInput->imageTimes = &imageGrabTimes;
	grabberInput->picBufferQueue = nullptr; // single-thread grabber; no separate worker queue

	imageGrabberWorker = new HamamatsuCameraThreadImageGrabber (grabberInput);
	QThread* thread = new QThread;
	imageGrabberWorker->moveToThread (thread);
	connect (thread, &QThread::started, imageGrabberWorker, &HamamatsuCameraThreadImageGrabber::process);
	// Each grabbed frame is delivered to the GUI thread for display + file save.
	connect (imageGrabberWorker, &HamamatsuCameraThreadImageGrabber::pictureGrabbed,
			 this, &QtHamamatsuWindow::onCameraProgress);
	connect (imageGrabberWorker, &HamamatsuCameraThreadImageGrabber::error, this,
			 [this](QString msg, unsigned) { reportErr (msg); });
	connect (imageGrabberWorker, &HamamatsuCameraThreadImageGrabber::pauseExperiment, this,
			 [this]() {
				 try { if (!mainWin->experimentIsPaused ()) { mainWin->pauseExperiment (); } }
				 catch (ChimeraError&) {}
			 });
	// process() calls quit() when the run ends; clean up the worker and thread then.
	connect (thread, &QThread::finished, imageGrabberWorker, &QObject::deleteLater);
	connect (thread, &QThread::finished, thread, &QObject::deleteLater);
	thread->start ();
}

void QtHamamatsuWindow::handlePrepareForAcq (HamamatsuRunSettings* lparam, analysisSettings aSettings){
	try {
		reportStatus ("Preparing Hamamatsu Window for Acquisition...\n");
		currentPictureNum = 0;
		currentRawPictures.clear();
		HamamatsuRunSettings* settings = (HamamatsuRunSettings*)lparam;
		analysisHandler.setRunningSettings (aSettings);
		armCameraWindow (settings);
		completeCruncherStart ();
		completePlotterStart ();
	}
	catch (ChimeraError & err) {
		reportErr (qstr (err.trace ()));
	}
}

void QtHamamatsuWindow::handlePlotPop (unsigned id){
	for (auto& plt : mainAnalysisPlots)	{
	}
}

void QtHamamatsuWindow::refreshPics()
{
	auto settings = hamamatsuSettingsCtrl.getConfigSettings().hamamatsu;
	auto displayMask = hamamatsuSettingsCtrl.getDisplayMask();
	stats.setVisiblePictureCount(std::min<unsigned>(settings.picsPerRepetition, PictureManager::MAX_PICTURES));
	pics.setDisplayMask(displayMask);
	if (settings.picsPerRepetition <= 1) {
		pics.setSinglePicture(settings.imageSettings);
	}
	else {
		pics.setMultiplePictures(settings.imageSettings,
			std::min<unsigned>(settings.picsPerRepetition, PictureManager::MAX_PICTURES));
	}
	pics.setNumberPicturesActive(std::min<unsigned>(settings.picsPerRepetition, PictureManager::MAX_PICTURES));
}

void QtHamamatsuWindow::displayAnalysisGrid(atomGrid grids)
{
	try {
		for (auto& pic : pics.pictures) {
			pic.drawAnalysisMarkers(grids);
		}
	}
	catch (ChimeraError& err) {
		reportErr(qstr(err.trace()));
	}

}

void QtHamamatsuWindow::removeAnalysisGrid()
{
	for (auto& pic : pics.pictures) {
		pic.removeAnalysisMarkers();
	}
}

bool QtHamamatsuWindow::wasJustCalibrated (){
	return justCalibrated;
}

bool QtHamamatsuWindow::wantsAutoCal (){
	return hamamatsuSettingsCtrl.getAutoCal ();
}

void QtHamamatsuWindow::writeVolts (unsigned currentVoltNumber, std::vector<float64> data){
	try	{
		dataHandler.writeVolts (currentVoltNumber, data);
	}
	catch (ChimeraError& err){
		reportErr (qstr (err.trace ()));
	}
}

void QtHamamatsuWindow::handleImageDimsEdit (){
	try {
		pics.setParameters (hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu.imageSettings);
		QPainter painter (this);
		pics.redrawPictures (selectedPixel, analysisHandler.getRunningSettings().grids, true, mostRecentPicNum, painter); 
	}
	catch (ChimeraError& err){
		reportErr (qstr (err.trace ()));
	}
}

//void QtAndorWindow::handleEmGainChange (){
//	try {
//		auto runSettings = andor.getAndorRunSettings ();
//		andorSettingsCtrl.setEmGain (runSettings.emGainModeIsOn, runSettings.emGainLevel);
//		auto settings = andorSettingsCtrl.getConfigSettings ();
//		runSettings.emGainModeIsOn = settings.andor.emGainModeIsOn;
//		runSettings.emGainLevel = settings.andor.emGainLevel;
//		andor.setSettings (runSettings);
//		// and immediately change the EM gain mode.
//		try	{
//			andor.setGainMode ();
//		}
//		catch (ChimeraError& err){
//			// this can happen e.g. if the camera is aquiring.
//			reportErr (qstr (err.trace ()));
//		}
//	}
//	catch (ChimeraError err){
//		reportErr (qstr (err.trace ()));
//	}
//}


std::string QtHamamatsuWindow::getSystemStatusString (){
	std::string statusStr;
	statusStr = "\nHamamatsu Camera:\n";
	if (!HAM_SAFEMODE){
		statusStr += "\tCode System is Active!\n";
		statusStr += "\t" + hamamatsu.getSystemInfo ();
		statusStr += "\n";
	}
	else{
		statusStr += "\tCode System is disabled! Enable in \"constants.h\"\n";
	}
	return statusStr;
}

void QtHamamatsuWindow::windowSaveConfig (ConfigStream& saveFile){
	hamamatsuSettingsCtrl.handleSaveConfig (saveFile);
	pics.handleSaveConfig (saveFile);
	analysisHandler.handleSaveConfig (saveFile);
}

void QtHamamatsuWindow::windowOpenConfig (ConfigStream& configFile){
	HamamatsuRunSettings camSettings;
	try	{
		ConfigSystem::stdGetFromConfig (configFile, hamamatsu, camSettings);
		hamamatsuSettingsCtrl.setConfigSettings (camSettings);
		hamamatsuSettingsCtrl.setImageParameters (camSettings.imageSettings);
		hamamatsuSettingsCtrl.updateRunSettingsFromPicSettings ();
	}
	catch (ChimeraError& err){
		reportErr (qstr("Failed to get Hamamatsu Camera Run settings from file! " + err.trace ()));
	}
	try	{
		auto picSettings = ConfigSystem::stdConfigGetter (configFile, "PICTURE_SETTINGS",
			HamamatsuCameraSettingsControl::getPictureSettingsFromConfig);
		hamamatsuSettingsCtrl.updatePicSettings (picSettings);
	}
	catch (ChimeraError& err)	{
		reportErr (qstr ("Failed to get Hamamatsu Camera Picture settings from file! " + err.trace ()));
	}
	try	{
		ConfigSystem::standardOpenConfig (configFile, pics.configDelim, &pics);
	}
	catch (ChimeraError&)	{
		reportErr ("Failed to load picture settings from config!");
	}
	try	{
		ConfigSystem::standardOpenConfig (configFile, "DATA_ANALYSIS", &analysisHandler);
	}
	catch (ChimeraError&){
		reportErr ("Failed to load Data Analysis settings from config!");
	}
	try	{
		pics.resetPictureStorage ();
		auto nums = hamamatsuSettingsCtrl.getConfigSettings ().palleteNumbers;
		pics.setPalletes (nums);
	}
	catch (ChimeraError& e){
		reportErr (qstr ("Hamamatsu Camera Window failed to read parameters from the configuration file.\n\n" + e.trace ()));
	}
	analysisHandler.updateUnofficialPicsPerRep(hamamatsuSettingsCtrl.getConfigSettings().hamamatsu.picsPerRepetition,
		false);
}

void QtHamamatsuWindow::abortCameraRun (bool askDelete){
	int status = hamamatsu.queryStatus ();
	if (HAM_SAFEMODE)	{
		// simulate as if you needed to abort.
		status = DCAMCAP_STATUS_BUSY;
	}
	if (true/*status == DRV_ACQUIRING*/){
		//hamamatsu.abortAcquisition ();
		// since the abortion can happen when the threadworker is waitForAcquisition, need to queue a buffer for it to get out of the wait
		// if has a trigger for hamamatsu, also need to attach a trigger for it
		//hamamatsu.updatePictureNumber(currentPictureNum + 1);
		//hamamatsu.setIsRunningState(false);
		//hamamatsu.queueBuffers();
		//Sleep(20);
		////auxWin->getTtlCore().FPGAForcePulse(auxWin->getTtlSystem().getCurrentStatus(), HAMAMATSU_TRIGGER_LINE, 0.5);
		//Sleep(600); // just for  4fps during test, when run exp, probably not need this long
		hamamatsu.onFinish();
		qDebug() << "QtHamamatsuWindow::abortCameraRun: Hamamatsu camera acquisition aborted, does WaitForAcquisition automatically release the hold? Tested and the answer is NO!";
		timer.setTimerDisplay ("Aborted");
		hamamatsu.setIsRunningState (false);
		// double set the cruncher thread flag just to be sure, this should be set in grabber already
		atomCrunchThreadActive = false;
		// camera is no longer running.
		try	{
			dataHandler.normalCloseFile ();
		}
		catch (ChimeraError& err)	{
			reportErr (qstr (err.trace ()));
		}

		if (askDelete/*hamamatsu.getHamamatsuRunSettings ().acquisitionMode != HamamatsuRunModes::mode::Video*/){
			auto answer = QMessageBox::question(this, qstr("Delete Data?"), qstr("Acquisition Aborted. Delete Data "
				"file (data_" + str (dataHandler.getDataFileNumber ()) + ".h5) for this run?"));
			if (answer == QMessageBox::Yes){
				try	{
					dataHandler.deleteFile ();
				}
				catch (ChimeraError& err) {
					reportErr (qstr (err.trace ()));
				}
			}
		}
	}
	else if (status == DCAMCAP_STATUS_READY || status == DCAMCAP_STATUS_STABLE) {
		hamamatsu.setIsRunningState (false);
	}
}

bool QtHamamatsuWindow::cameraIsRunning (){
	return hamamatsu.isRunning ();
}

void QtHamamatsuWindow::onCameraProgress(NormalImage picGrabbed){
	auto timerE = QElapsedTimer();
	timerE.start();
	unsigned long long picNumReported = picGrabbed.picStat.picNum;
	unsigned picNum = currentPictureNum;
	currentPictureNum++;
	if (picNum % 2 == 1){
		mainThreadStartTimes.push_back (std::chrono::high_resolution_clock::now ());
	}
	HamamatsuRunSettings curSettings = hamamatsu.getHamamatsuRunSettings ();
	if (picNumReported != picNum){
		reportErr("WARNING: picture number reported by hamamatsu isn't matching the"
			"camera window record?!?!?!?!?");
	}
	if (picNum % curSettings.picsPerRepetition == 0) {
		currentRawPictures.clear();
		currentRawPictures.reserve(curSettings.picsPerRepetition);
	}
	currentRawPictures.push_back(picGrabbed.image);
	auto& rawPicData = currentRawPictures;

	std::vector<Matrix<long>> calPicData (rawPicData.size ());
	if (hamamatsuSettingsCtrl.getUseCal () && avgBackground.size () == rawPicData.front ().size ()){
		for (auto picInc : range (rawPicData.size ())){
			calPicData[picInc] = Matrix<long> (rawPicData[picInc].getRows (), rawPicData[picInc].getCols (), 0);
			for (auto pixInc : range (rawPicData[picInc].size ()))
			{
				calPicData[picInc].data[pixInc] = (rawPicData[picInc].data[pixInc] - avgBackground.data[pixInc]);
			}
		}
	}
	else { calPicData = rawPicData; }

	if (picNum % 2 == 1){
		imageGrabTimes.push_back (std::chrono::high_resolution_clock::now ());
	}
	size_t currentActivePicNum = picNum % curSettings.picsPerRepetition;
	size_t maxDisplayPics = std::min<size_t>(curSettings.picsPerRepetition, PictureManager::MAX_PICTURES);
	unsigned currentDisplayPicNum = static_cast<unsigned>(picNum % maxDisplayPics);
	//emit newImage({ {picNum, repVar.first, repVar.second}, calPicData[currentActivePicNum] });

	/// send picture data to plotter
	qDebug() << "send Image data for drawing for image " << picNum << " at time " << timerE.elapsed() << " ms";
	auto picsToDraw = hamamatsuSettingsCtrl.getImagesToDraw (calPicData);
	try
	{
		std::pair<int, int> minMax;
		// draw the most recent pic.
		minMax = stats.update (picsToDraw.back (), currentDisplayPicNum, selectedPixel,
			picNum / curSettings.picsPerRepetition,
			curSettings.totalPicsInExperiment () / curSettings.picsPerRepetition);
		QPainter painter (this);
		pics.drawBitmap (picsToDraw.back (), minMax, currentDisplayPicNum,
			analysisHandler.getRunningSettings ().grids, picNum, 
			analysisHandler.getRunningSettings ().displayGridOption, painter);
			
		timer.update(picNum / curSettings.picsPerRepetition, curSettings.repetitionsPerVariation,
			curSettings.totalVariations, curSettings.picsPerRepetition, curSettings.repFirst);
	}
	catch (ChimeraError& err){
		reportErr (qstr (err.trace ()));
		try {
			mainWin->pauseExperiment ();
		}
		catch (ChimeraError & err) {
			reportErr (qstr (err.trace ()));
		}
	}
	/// write the data to the file — only during an actual experiment. A manual "Program Now" arm has no
	/// data file open, so writing would throw; skip it and just display the (e.g. safe-mode) images.
	qDebug() << "write image to file for image " << picNum << " at time " << timerE.elapsed() << " ms";
	if (mainWin->masterIsRunning () && mainWin->getLogger ().isFileOpen ()){
		try	{
			// important! write the original raw data, not the pic-to-draw, which can be a difference pic, or the calibrated
			// pictures, which can have the background subtracted. Save into the experiment's own data file
			// (mainWin's logger, opened for this run) under a /Hamamatsu group, not a separate file.
			mainWin->getLogger ().writeHamamatsuPic ( rawPicData[currentActivePicNum],
									    curSettings.imageSettings );
		}
		catch (ChimeraError& err){
			reportErr (err.qtrace ());
			try {
				if (!mainWin->experimentIsPaused()) {
					mainWin->pauseExperiment();
				}
			}
			catch (ChimeraError & err2) {
				reportErr (err2.qtrace ());
			}
		}
	}
	mostRecentPicNum = picNum;
	qDebug() << "finish write image to file for image " << picNum << " at time " << timerE.elapsed() << " ms";
	if (picNum == curSettings.totalPicsInExperiment() - 1) {
		hamamatsu.onFinish();
	}
}

void QtHamamatsuWindow::wakeRearranger (){
	std::unique_lock<std::mutex> lock (rearrangerLock);
	rearrangerConditionVariable.notify_all ();
}

LRESULT QtHamamatsuWindow::onCameraCalFinish (WPARAM wParam, LPARAM lParam){
	// notify the hamamatsu object that it is done.
	hamamatsu.onFinish ();
	hamamatsu.pauseThread ();
	hamamatsu.setCalibrating (false);
	justCalibrated = true;
	hamamatsuSettingsCtrl.cameraIsOn (false);
	// normalize.
	for (auto& p : avgBackground){
		p /= 100.0;
	}
	// if auto cal is selected, always assume that the user was trying to start with F5.
	if (hamamatsuSettingsCtrl.getAutoCal ()){
		//PostMessageA (WM_COMMAND, MAKEWPARAM (ID_ACCELERATOR_F5, 0));
	}
	return 0;
}

dataPoint QtHamamatsuWindow::getMainAnalysisResult (){
	return mostRecentAnalysisResult;
}

void QtHamamatsuWindow::cleanUpAfterExp (){
	atomCrunchThreadActive = false;
	// Stop the acquisition grabber (sets isRunning() false so its loop exits) so it doesn't sit waiting
	// for more triggers after the experiment ends.
	hamamatsu.onFinish ();
	try {
		dataHandler.normalCloseFile ();
	}
	catch (ChimeraError&) {
		// no data file was open (e.g. Stage-1 display-only run) — nothing to close.
	}
}

int QtHamamatsuWindow::getMostRecentFid (){
	return dataHandler.getDataFileNumber ();
}

int QtHamamatsuWindow::getPicsPerRep (){
	return hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu.picsPerRepetition;
}

std::string QtHamamatsuWindow::getMostRecentDateString (){
	return dataHandler.getMostRecentDateString ();
}

bool QtHamamatsuWindow::wantsThresholdAnalysis (){
	return analysisHandler.getRunningSettings ().autoThresholdAnalysisOption;
}

atomGrid QtHamamatsuWindow::getMainAtomGrid (){
	return analysisHandler.getRunningSettings ().grids[0];
}


void QtHamamatsuWindow::armCameraWindow (HamamatsuRunSettings* settings){
	// Sync the camera core to the settings we're about to run with. Without this the core keeps whatever
	// settings it last had, so totalPicsInExperiment() (which the grabber loops over) can be wrong.
	hamamatsu.setSettings (*settings);
	unsigned displayPicCount = std::min<unsigned>(settings->picsPerRepetition, PictureManager::MAX_PICTURES);
	auto displayMask = hamamatsuSettingsCtrl.getDisplayMask();
	stats.setVisiblePictureCount(displayPicCount);
	pics.setDisplayMask(displayMask);
	pics.setNumberPicturesActive(displayPicCount);
	if (displayPicCount == 1) {
		pics.setSinglePicture(settings->imageSettings);
	}
	else {
		pics.setMultiplePictures(settings->imageSettings, displayPicCount);
	}
	pics.setNumberPicturesActive(displayPicCount);
	pics.resetPictureStorage ();
	pics.setParameters (settings->imageSettings);
	redrawPictures (false);
	hamamatsuSettingsCtrl.setRunSettings (*settings);
	hamamatsuSettingsCtrl.setRepsPerVariation (settings->repetitionsPerVariation);
	hamamatsuSettingsCtrl.setVariationNumber (settings->totalVariations);
	pics.setSoftwareAccumulationOptions (hamamatsuSettingsCtrl.getSoftwareAccumulationOptions ());
	try {
		hamamatsu.preparationChecks ();
	}
	catch (ChimeraError & err) {
		reportErr (err.qtrace ());
	}
	// turn some buttons off.
	hamamatsuSettingsCtrl.cameraIsOn (true);
	stats.reset ();
	analysisHandler.updateDataSetNumberEdit (dataHandler.getNextFileNumber () - 1);
}

bool QtHamamatsuWindow::getCameraStatus (){
	return hamamatsu.isRunning ();
}

void QtHamamatsuWindow::stopSound (){
	alerts.stopSound ();
}

void QtHamamatsuWindow::passSetTemperaturePress (){
	try{
		if (hamamatsu.isRunning ()){
			thrower ("ERROR: the camera (thinks that it?) is running. You can't change temperature settings during camera "
				"operation.");
		}
		hamamatsuSettingsCtrl.handleSetTemperaturePress ();
		auto settings = hamamatsuSettingsCtrl.getConfigSettings ();
		hamamatsu.setSettings (settings.hamamatsu);
		hamamatsu.setTemperature ();
		// The temperature button also applies the cooler + fan (all part of temperature control).
		hamamatsu.setCoolerMode ();
		hamamatsu.setFanMode ();
	}
	catch (ChimeraError& err){
		reportErr (qstr (err.trace ()));
	}
	mainWin->updateConfigurationSavedStatus (false);
}

void QtHamamatsuWindow::assertDataFileClosed () {
	dataHandler.assertClosed ();
}

void QtHamamatsuWindow::handlePictureSettings (){
	selectedPixel = { 0,0 };
	hamamatsuSettingsCtrl.handlePictureSettings ();
	const auto settings = hamamatsuSettingsCtrl.getConfigSettings().hamamatsu;
	const unsigned picsPerRep = settings.picsPerRepetition;
	const imageParameters imageSettings = settings.imageSettings;

	const unsigned displayPicCount = std::min<unsigned>(picsPerRep, PictureManager::MAX_PICTURES);
	const auto displayMask = hamamatsuSettingsCtrl.getDisplayMask();
	stats.setVisiblePictureCount(displayPicCount);
	pics.setDisplayMask(displayMask);
	if (displayPicCount == 1) {
		pics.setSinglePicture(imageSettings);
	}
	else {
		pics.setMultiplePictures(imageSettings, displayPicCount);
	}
	pics.setNumberPicturesActive(displayPicCount);
	
	pics.resetPictureStorage ();
	auto nums = hamamatsuSettingsCtrl.getConfigSettings ().palleteNumbers;
	pics.setPalletes (nums);
	analysisHandler.updateUnofficialPicsPerRep (picsPerRep, false);
}

/*
Check that the camera is idle, or not aquiring pictures. Also checks that the data analysis handler isn't active.
*/
void QtHamamatsuWindow::checkCameraIdle (){
	if (hamamatsu.isRunning ()){
		thrower ("Camera is already running! Please Abort to restart.\r\n");
	}
	// make sure it's idle.
	try{
		hamamatsu.queryStatus ();
		if (HAM_SAFEMODE){
			thrower ("DRV_IDLE");
		}
	}
	catch (ChimeraError& exception){
		if (exception.whatBare () != "DRV_IDLE"){
			throwNested (" while querying hamamatsu status to check if idle.");
		}
	}
}

void QtHamamatsuWindow::handleMasterConfigSave (std::stringstream& configStream){
	hamamatsuSettingsCtrl.handelSaveMasterConfig (configStream);
}

void QtHamamatsuWindow::handleMasterConfigOpen (ConfigStream& configStream){
	mainWin->updateConfigurationSavedStatus (false);
	selectedPixel = { 0,0 };
	hamamatsuSettingsCtrl.handleOpenMasterConfig (configStream, this);
	pics.setParameters (hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu.imageSettings);
	redrawPictures (true);
}

DataLogger& QtHamamatsuWindow::getLogger (){
	return dataHandler;
}

void QtHamamatsuWindow::loadCameraCalSettings (AllExperimentInput& input){
	redrawPictures (false);
	try{
		checkCameraIdle ();
	}
	catch (ChimeraError& err){
		reportErr (qstr (err.trace ()));
	}
	// I used to mandate use of a button to change image parameters. Now I don't have the button and just always 
	// update at this point.
	readImageParameters ();
	pics.setNumberPicturesActive (1);
	// biggest check here, camera settings includes a lot of things.
	hamamatsuSettingsCtrl.checkIfReady ();
	// reset the image which is about to be calibrated.
	avgBackground = Matrix<long> (0, 0);
	/// start the camera.
	hamamatsu.setCalibrating (true);
}

HamamatsuCameraCore& QtHamamatsuWindow::getCamera (){
	return hamamatsu;
}


void QtHamamatsuWindow::prepareAtomCruncher (AllExperimentInput& input){
	input.cruncherInput = new atomCruncherInput;
	//input.cruncherInput->plotterActive = plotThreadActive;
	input.cruncherInput->imageDims = hamamatsuSettingsCtrl.getRunningSettings().imageSettings;
	atomCrunchThreadActive = true;
	//input.cruncherInput->plotterNeedsImages = input.masterInput->plotterInput->needsCounts;
	input.cruncherInput->cruncherThreadActive = &atomCrunchThreadActive;
	skipNext = false;
	input.cruncherInput->skipNext = &skipNext;
	//input.cruncherInput->imQueue = &imQueue;
	// options
	if (input.masterInput){
		input.cruncherInput->rearrangerActive = false;
	}
	else{
		input.cruncherInput->rearrangerActive = false;
	}
	input.cruncherInput->grids = analysisHandler.getRunningSettings ().grids;
	input.cruncherInput->thresholds = hamamatsuSettingsCtrl.getConfigSettings ().thresholds;
	input.cruncherInput->picsPerRep = hamamatsuSettingsCtrl.getRunningSettings ().picsPerRepetition;
	input.cruncherInput->catchPicTime = &crunchSeesTimes;
	input.cruncherInput->finTime = &crunchFinTimes;
	input.cruncherInput->atomThresholdForSkip = mainWin->getMainOptions ().atomSkipThreshold;
	input.cruncherInput->rearrangerConditionWatcher = &rearrangerConditionVariable;
}

bool QtHamamatsuWindow::wantsAutoPause (){
	return alerts.wantsAutoPause ();
}

void QtHamamatsuWindow::completeCruncherStart () {
	if ((mainWin->getExpThread() == nullptr) || (mainWin->getExpThread()->isFinished())) {
		// then this is called from ProgramNow in HamamatsuWindow
		return;
	}
	auto cruncherInput = std::make_unique<atomCruncherInput>();
	cruncherInput->imageQueue = hamamatsu.getGrabberQueue();
	cruncherInput->imageDims = hamamatsuSettingsCtrl.getRunningSettings().imageSettings;
	atomCrunchThreadActive = true;
	cruncherInput->cruncherThreadActive = &atomCrunchThreadActive;
	skipNext = false;
	cruncherInput->skipNext = &skipNext;
	cruncherInput->rearrangerActive = false;
	cruncherInput->grids = analysisHandler.getRunningSettings ().grids;
	cruncherInput->thresholds = hamamatsuSettingsCtrl.getConfigSettings ().thresholds;
	cruncherInput->picsPerRep = hamamatsuSettingsCtrl.getRunningSettings ().picsPerRepetition;
	cruncherInput->catchPicTime = &crunchSeesTimes;
	cruncherInput->finTime = &crunchFinTimes;
	cruncherInput->atomThresholdForSkip = mainWin->getMainOptions ().atomSkipThreshold;
	cruncherInput->rearrangerConditionWatcher = &rearrangerConditionVariable;

	atomCruncherWorker = new CruncherThreadWorker(std::move(cruncherInput));
	QThread* thread = new QThread;
	atomCruncherWorker->moveToThread(thread);
	connect(thread, &QThread::started, atomCruncherWorker, &CruncherThreadWorker::init);
	connect(mainWin->getExpThread(), &QThread::finished, thread, &QThread::quit);
	connect(thread, &QThread::finished, atomCruncherWorker, &CruncherThreadWorker::deleteLater);
	connect(atomCruncherWorker, &QThread::destroyed, thread, &CruncherThreadWorker::deleteLater);
	//connect(this, &QtAndorWindow::newImage, atomCruncherWorker, &CruncherThreadWorker::handleImage);
	thread->start();
}

void QtHamamatsuWindow::completePlotterStart () {
	/// start the plotting thread.
	auto pltInput = std::make_unique<realTimePlotterInput>();
	pltInput->plotParentWindow = this;
	
	auto camSettings = hamamatsuSettingsCtrl.getRunningSettings ();
	pltInput->variations = camSettings.totalVariations;
	pltInput->picsPerVariation = camSettings.totalPicsInVariation();

	pltInput->imageShape = camSettings.imageSettings;
	pltInput->picsPerRep = camSettings.picsPerRepetition;
	
	pltInput->alertThreshold = alerts.getAlertThreshold ();
	pltInput->wantAtomAlerts = alerts.wantsAtomAlerts ();
	analysisHandler.fillPlotThreadInput (pltInput.get());
	// remove old plots that aren't trying to sustain.
	unsigned mainPlotInc = 0;
	for (auto plotParams : pltInput->plotInfo) {
		plotStyle style = plotParams.isHist ? plotStyle::HistPlot : plotStyle::BinomialDataPlot;
		if (mainPlotInc < 6) {
			mainAnalysisPlots[mainPlotInc]->setStyle (style);
			mainAnalysisPlots[mainPlotInc]->setThresholds (hamamatsuSettingsCtrl.getConfigSettings ().thresholds[0]);
			mainAnalysisPlots[mainPlotInc]->setTitle (plotParams.name);
			mainPlotInc++;
		}
	}

	bool gridHasBeenSet = false;
	for (auto gridInfo : pltInput->grids) {
		if (!(gridInfo.gridOrigin == coordinate(0, 0)) || gridInfo.useFile) {
			gridHasBeenSet = true;
			break;
		}
	}
	if ((!gridHasBeenSet) || pltInput->plotInfo.size () == 0) {
		//plotThreadActive = false;
	}
	else {
		// start the plotting thread
		analysisThreadWorker = new AnalysisThreadWorker (std::move(pltInput));
		QThread* thread = new QThread;
		analysisThreadWorker->moveToThread (thread);
		connect (thread, &QThread::started, analysisThreadWorker, &AnalysisThreadWorker::init);
		connect(mainWin->getExpThread(), &QThread::finished, thread, &QThread::quit);
		connect(thread, &QThread::finished, analysisThreadWorker, &AnalysisThreadWorker::deleteLater);
		connect(analysisThreadWorker, &AnalysisThreadWorker::destroyed, thread, &QThread::deleteLater);

		connect (mainWin->getExpThreadWorker(), &ExpThreadWorker::plot_Xvals_determined,
				 analysisThreadWorker, &AnalysisThreadWorker::setXpts);
		connect (analysisThreadWorker, &AnalysisThreadWorker::newPlotData, this,
			[this](std::vector<std::vector<dataPoint>> data, int plotNum) {mainAnalysisPlots[plotNum]->setData (data); });
		if (atomCruncherWorker) {
			connect (atomCruncherWorker, &CruncherThreadWorker::atomArray,
				analysisThreadWorker, &AnalysisThreadWorker::handleNewPic);
			connect (atomCruncherWorker, &CruncherThreadWorker::pixArray,
				analysisThreadWorker, &AnalysisThreadWorker::handleNewPix);
		}
		thread->start ();
	}
}

bool QtHamamatsuWindow::wantsNoMotAlert (){
	if (cameraIsRunning ()){
		return alerts.wantsMotAlerts ();
	}
	else{
		return false;
	}
}

unsigned QtHamamatsuWindow::getNoMotThreshold (){
	return alerts.getAlertThreshold ();
}

std::string QtHamamatsuWindow::getStartMessage (){
	// get selected plots
	auto hamSttngs = hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu;
	std::vector<std::string> plots = analysisHandler.getActivePlotList ();
	imageParameters currentImageParameters = hamSttngs.imageSettings;
	bool errCheck = false;
	for (unsigned plotInc = 0; plotInc < plots.size (); plotInc++){
		PlottingInfo tempInfoCheck (PLOT_FILES_SAVE_LOCATION + "\\" + plots[plotInc] + ".plot");
		if (tempInfoCheck.getPicNumber() != hamSttngs.picsPerRepetition) {
			thrower (": one of the plots selected, " + plots[plotInc] + ", is not built for the currently "
					 "selected number of pictures per experiment. (" + str(hamSttngs.picsPerRepetition) 
					 + ") Please revise either the current setting or the plot file.");
		}
	}
	std::string dialogMsg;
	dialogMsg = "Camera Parameters:\r\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\r\n";
	dialogMsg += "Current Camera Temperature Setting:\r\n\t" + str (
		hamSttngs.temperatureSetting) + "\r\n";
	dialogMsg += "Exposure Times: ";
	for (auto& time : hamSttngs.exposureTimes){
		dialogMsg += str (time * 1000) + ", ";
	}
	dialogMsg += "\r\n";
	dialogMsg += "Image Settings:\r\n\t" + str (currentImageParameters.left) + " - " + str (currentImageParameters.right) + ", "
		+ str (currentImageParameters.bottom) + " - " + str (currentImageParameters.top) + "\r\n";
	dialogMsg += "\r\n";
	dialogMsg += "FrameRate:\r\n\t" + str (hamSttngs.frameRate) + "\r\n";
	//dialogMsg += "Kintetic Cycle Time:\r\n\t" + str (hamSttngs.kineticCycleTime) + "\r\n";
	dialogMsg += "Pictures per Repetition:\r\n\t" + str (hamSttngs.picsPerRepetition) + "\r\n";
	dialogMsg += "Repetitions per Variation:\r\n\t" + str (hamSttngs.totalPicsInVariation ()) + "\r\n";
	dialogMsg += "Variations per Experiment:\r\n\t" + str (hamSttngs.totalVariations) + "\r\n";
	dialogMsg += "Total Pictures per Experiment:\r\n\t" + str (hamSttngs.totalPicsInExperiment ()) + "\r\n";

	dialogMsg += "Real-Time Atom Detection Thresholds:\r\n\t";
	unsigned count = 0;
	for (auto& picThresholds : hamamatsuSettingsCtrl.getConfigSettings ().thresholds){
		dialogMsg += "Pic " + str (count) + " thresholds: ";
		for (auto thresh : picThresholds){
			dialogMsg += str (thresh) + ", ";
		}
		dialogMsg += "\r\n";
		count++;
	}
	dialogMsg += "\r\nReal-Time Plots:\r\n";
	for (unsigned plotInc = 0; plotInc < plots.size (); plotInc++){
		dialogMsg += "\t" + plots[plotInc] + "\r\n";
	}
	return dialogMsg;
}

void QtHamamatsuWindow::fillMasterThreadInput (ExperimentThreadInput* input){
	// starting a not-calibration, so reset this.
	justCalibrated = false;
	input->rearrangerLock = &rearrangerLock;
	input->andorsImageTimes = &imageTimes;
	input->grabTimes = &imageGrabTimes;
	input->conditionVariableForRerng = &rearrangerConditionVariable;
}

void QtHamamatsuWindow::setTimerText (std::string timerText){
	timer.setTimerDisplay (timerText);
}

void QtHamamatsuWindow::setDataType (std::string dataType){
	stats.updateType (dataType);
}

void QtHamamatsuWindow::redrawPictures (bool andGrid){
	try	{
		if (andGrid){
			QPainter painter (this);
			pics.drawGrids (painter);
		}
		// ??? should there be handling here???
	}
	catch (ChimeraError& err){
		reportErr (err.qtrace ());
	}
	// currently don't attempt to redraw previous picture data.
}

std::atomic<bool>* QtHamamatsuWindow::getSkipNextAtomic (){
	return &skipNext;
}

// this is typically a little redundant to call, but can use to make sure things are set to off.
void QtHamamatsuWindow::assertOff (){
	hamamatsuSettingsCtrl.cameraIsOn (false);
	atomCrunchThreadActive = false;
}

void QtHamamatsuWindow::readImageParameters (){
	selectedPixel = { 0,0 };
	try	{
		redrawPictures (false);
		imageParameters parameters = hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu.imageSettings;
		pics.setParameters (parameters);
	}
	catch (ChimeraError& exception){
		reportErr (exception.qtrace () + "\r\n");
	}
	QPainter painter (this);
	pics.drawGrids (painter);
}

void QtHamamatsuWindow::fillExpDeviceList (DeviceList& list){
	UNREFERENCED_PARAMETER(list);
}

void QtHamamatsuWindow::handleNormalFinish (profileSettings finishedProfile) {
	wakeRearranger ();
	cleanUpAfterExp ();
	handleBumpAnalysis (finishedProfile);
}

void QtHamamatsuWindow::handleBumpAnalysis (profileSettings finishedProfile) {
	std::ifstream configFileRaw (finishedProfile.configFilePath ());
	// check if opened correctly.
	if (!configFileRaw.is_open ()) {
		errBox ("Opening of Configuration File for bump analysis Failed!");
		return;
	}
	ConfigStream cStream (configFileRaw);
	cStream.setCase (false);
	configFileRaw.close ();
	ConfigSystem::getVersionFromFile (cStream);
	ConfigSystem::jumpToDelimiter (cStream, "DATA_ANALYSIS");
	auto settings = analysisHandler.getAnalysisSettingsFromFile (cStream);
	// get the options from the config file, not from the current config settings. this is important especially for 
	// handling this in the calibration. 
	if (settings.autoBumpOption) {
		auto grid = hamamatsu.getMainAtomGrid ();
		auto dateStr = hamamatsu.getMostRecentDateString ();
		auto fid = hamamatsu.getMostRecentFid ();
		auto ppr = hamamatsu.getPicsPerRep ();
		try {
			auto res = pythonHandler.runCarrierAnalysis (dateStr, fid, grid, this);
			auto name =	settings.bumpParam;
			// zero is the default.
			if (name != "" && res != 0) {
				auxWin->getGlobals ().adjustVariableValue (str (name, 13, false, true), res);
				// TODO adapt this for cryo. ZZP 06/26/2021
			}
			reportStatus ( qstr("Successfully completed auto bump analysis and set variable \"" + name + "\" to value " 
						   + str (res) + "\n"));
		}
		catch (ChimeraError & err) {
			reportErr ("Bump Analysis Failed! " + err.qtrace ());
		}
	}
}

NewPythonHandler* QtHamamatsuWindow::getPython() {
	return &pythonHandler;
}

void QtHamamatsuWindow::prepareForExperiment(unsigned repetitions, unsigned variations)
{
	// Runs on the GUI thread via a BlockingQueuedConnection from the experiment thread, so it MUST NOT
	// throw or hang — otherwise it stalls/aborts the whole experiment. Arms the camera and starts the
	// grabber so the camera is ready before the sequence begins triggering.
	try {
		auto settings = hamamatsuSettingsCtrl.getConfigSettings().hamamatsu;
		if (!settings.controlCamera) {
			// The user isn't controlling the Hamamatsu this run; do nothing.
			return;
		}
		// Match the camera's frame count to the experiment so the grabber loops the right number of
		// frames (picsPerRep x repetitions x variations).
		settings.repetitionsPerVariation = repetitions;
		settings.totalVariations = variations;
		handlePrepareForAcq(&settings, analysisHandler.getRunningSettings());
		// Arm directly (NOT via manualArmCamera, which swallows its errors) so an arm failure is reported
		// here and the "armed" message only prints on real success.
		double armTime = 0.0;
		hamamatsu.armCamera(this, armTime);
		startImageGrabberThread();
		// Frames captured this run are saved into the experiment's data file (opened by the exp thread),
		// under a /Hamamatsu group. Reset the counter so this run's pictures start at Picture_0.
		if (mainWin->getLogger().isFileOpen()) {
			mainWin->getLogger().currentHamamatsuPicNumber = 0;
			reportStatus("Hamamatsu: saving frames to data_" + qstr(mainWin->getLogger().getDataFileNumber())
				+ ".h5 (/Hamamatsu group).\n");
		}
		else {
			reportStatus("Hamamatsu: no data file open for this run — display only (not saving).\n");
		}
		reportStatus("Hamamatsu camera armed for experiment (waiting for triggers).\n");
	}
	catch (ChimeraError& err) {
		reportErr("Failed to prepare Hamamatsu camera for experiment: " + err.qtrace());
	}
	catch (...) {
		reportErr("Failed to prepare Hamamatsu camera for experiment (unknown error).");
	}
}

void QtHamamatsuWindow::manualProgramCameraSetting()
{
	// "Program Now": push the current GUI settings to the camera (temperature, exposure, trigger, ROI,
	// readout speed) WITHOUT starting an acquisition. Actual image capture only happens on an experiment
	// run. This lets the camera start cooling to the set temperature, etc.
	try {
		hamamatsu.setSettings (hamamatsuSettingsCtrl.getConfigSettings ().hamamatsu);
		hamamatsu.programSettings ();
		reportStatus ("Programmed Hamamatsu camera settings.\n");
		// Report the camera's own timing for these settings — tells you how fast you can trigger and the
		// window after a trigger during which the exposure can't be changed (relevant for per-image exposure).
		reportStatus ("Hamamatsu timing: " + qstr (hamamatsu.getTimingSummary ()) + "\n");
	}
	catch (ChimeraError& err) {
		reportErr (qstr (err.trace ()));
	}
}

