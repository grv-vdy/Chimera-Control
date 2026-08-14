// created by Mark O. Brown

#include "stdafx.h"
#include "PictureSettingsControl.h"

#include "HamamatsuCameraCore.h"
#include "Hamamatsu/HamamatsuCameraSettingsControl.h"
#include "PrimaryWindows/QtHamamatsuWindow.h"
#include "ConfigurationSystems/ConfigSystem.h"

#include "Commctrl.h"
#include <boost/lexical_cast.hpp>

namespace {
	constexpr unsigned MAX_UI_PICS = 64;
}

PictureSettingsControl::PictureSettingsControl()
{
}

void PictureSettingsControl::initialize( IChimeraQtWindow* parent ){
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto handleChange = [this, parent]() {
		try {
			auto* hamWin = parent->hamamatsuWin;
			if (!hamWin) {
				hamWin = dynamic_cast<QtHamamatsuWindow*>(parent);
			}
			if (hamWin) {
				hamWin->handlePictureSettings();
			}
		}
		catch (ChimeraError& err){
			parent->reportErr (err.qtrace());
		}
	};
	auto handlePicsPerRepLiveChange = [this, parent]() {
		try {
			handleOptionChange();
			auto* hamWin = parent->hamamatsuWin;
			if (!hamWin) {
				hamWin = dynamic_cast<QtHamamatsuWindow*>(parent);
			}
			if (hamWin) {
				hamWin->refreshPics();
			}
		}
		catch (ChimeraError& err) {
			parent->reportErr(err.qtrace());
		}
	};
	QHBoxLayout* layout1 = new QHBoxLayout();
	layout1->setContentsMargins(0, 0, 0, 0);
	auto picsPerRepLabel = new QLabel("PicsPerRep: ",parent);
	picsPerRepEdit = new QSpinBox(this);
	picsPerRepEdit->setValue(1);
	picsPerRepEdit->setEnabled(true);
	picsPerRepEdit->setRange(1, MAX_UI_PICS);
	picsPerRepEdit->setKeyboardTracking(true);
	// Debounce: rebuild the picture layout only ~250ms after the user stops changing pics-per-rep,
	// instead of on every spinbox tick (and only on valueChanged, not also textChanged). Keeps it smooth.
	picsPerRepDebounce = new QTimer(this);
	picsPerRepDebounce->setSingleShot(true);
	parent->connect(picsPerRepDebounce, &QTimer::timeout, handlePicsPerRepLiveChange);
	parent->connect(picsPerRepEdit, qOverload<int>(&QSpinBox::valueChanged),
		[this](int) { picsPerRepDebounce->start(250); });
	layout1->addWidget(picsPerRepLabel, 0);
	layout1->addWidget(picsPerRepEdit, 0);
	layout1->addStretch(1);

	settingsScrollArea = new QScrollArea(this);
	settingsScrollArea->setWidgetResizable(true);
	settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	settingsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	settingsRowsWidget = new QWidget(settingsScrollArea);
	settingsRowsLayout = new QGridLayout(settingsRowsWidget);
	settingsRowsLayout->setContentsMargins(0, 0, 0, 0);
	settingsRowsLayout->setHorizontalSpacing(6);
	settingsRowsLayout->setVerticalSpacing(2);

	pictureLabel = new QLabel("Picture #", settingsRowsWidget);
	exposureLabel = new QLabel("Exposure (ms)", settingsRowsWidget);
	thresholdLabel = new QLabel("Threshold (cts)", settingsRowsWidget);
	displayTypeLabel = new QLabel("Display?", settingsRowsWidget);
	settingsRowsLayout->addWidget(pictureLabel, 0, 0);
	settingsRowsLayout->addWidget(exposureLabel, 0, 1);
	settingsRowsLayout->addWidget(thresholdLabel, 0, 2);
	settingsRowsLayout->addWidget(displayTypeLabel, 0, 3);

	pictureNumbers.resize(MAX_UI_PICS);
	exposureEdits.resize(MAX_UI_PICS);
	thresholdEdits.resize(MAX_UI_PICS);
	displayChecks.resize(MAX_UI_PICS);

	for (auto picInc : range(MAX_UI_PICS)) {
		int row = static_cast<int>(picInc) + 1;
		pictureNumbers[picInc] = new QLabel(cstr(picInc + 1), settingsRowsWidget);
		exposureEdits[picInc] = new CQLineEdit(parent);
		thresholdEdits[picInc] = new CQLineEdit("100", parent);
		displayChecks[picInc] = new CQCheckBox(parent);
		displayChecks[picInc]->setChecked(true);

		parent->connect(exposureEdits[picInc], &QLineEdit::textEdited, handleChange);
		parent->connect(thresholdEdits[picInc], &QLineEdit::textEdited, handleChange);
		parent->connect(displayChecks[picInc], &QCheckBox::clicked, handleChange);

		settingsRowsLayout->addWidget(pictureNumbers[picInc], row, 0);
		settingsRowsLayout->addWidget(exposureEdits[picInc], row, 1);
		settingsRowsLayout->addWidget(thresholdEdits[picInc], row, 2);
		settingsRowsLayout->addWidget(displayChecks[picInc], row, 3);
	}

	settingsScrollArea->setWidget(settingsRowsWidget);
	{
		int headerHeight = pictureLabel->sizeHint().height();
		int rowHeight = exposureEdits.empty() ? 24 : exposureEdits[0]->sizeHint().height();
		int targetHeight = headerHeight + rowHeight * 5 + 20;
		settingsScrollArea->setMinimumHeight(targetHeight);
		settingsScrollArea->setMaximumHeight(targetHeight);
	}
	setUnofficialExposures(std::vector<float>(MAX_UI_PICS, 10 / 1000.0f));
	setUnofficialPicsPerRep(1);
	layout->addLayout(layout1);
	layout->addWidget(settingsScrollArea, 1);
}

std::vector<displayTypeOption> PictureSettingsControl::getDisplayTypeOptions( ){
	std::vector<displayTypeOption> options(picsPerRepEdit ? picsPerRepEdit->value() : 1);
	for (auto picInc : range(options.size())) {
		options[picInc].isDiff = false;
		options[picInc].whichPicForDif = 0;
	}
	return options;
}

std::vector<bool> PictureSettingsControl::getDisplayMask(){
	auto activePics = std::max(1, picsPerRepEdit->value());
	std::vector<bool> mask(activePics, true);
	for (auto picInc : range(mask.size())) {
		mask[picInc] = displayChecks[picInc] && displayChecks[picInc]->isChecked();
	}
	return mask;
}


std::vector<std::string> PictureSettingsControl::getThresholdStrings(){
	std::vector<std::string> res(4);
	for (unsigned thresholdInc = 0; thresholdInc < 4; thresholdInc++) {
		res[ thresholdInc ] = str(thresholdEdits[thresholdInc]->text ());
	}
	return res;
}

void PictureSettingsControl::handleSaveConfig(ConfigStream& saveFile){
	saveFile << "PICTURE_SETTINGS\n";
	//saveFile << "/*Transformation Mode:*/ " << str (transformationModeCombo->currentText ());
	saveFile << "\n/*Color Options:*/ ";
	for (unsigned colorInc = 0; colorInc < 4; colorInc++) {
		auto srcIdx = std::min<size_t>(colorInc, currentPicSettings.colors.empty() ? 0 : currentPicSettings.colors.size() - 1);
		auto color = currentPicSettings.colors.empty() ? 0 : currentPicSettings.colors[srcIdx];
		saveFile << color << " ";
	}
	saveFile << "\n/*Threshold Settings:*/ ";
	for (auto threshold : getThresholdStrings() ){
		saveFile << threshold << " ";
	}
	saveFile << "\n/*Software Accumulation (accum all / Number)*/ ";
	auto saOpts = getSoftwareAccumulationOptions();
	for (unsigned optInc = 0; optInc < 4; optInc++) {
		auto srcIdx = std::min<size_t>(optInc, saOpts.empty() ? 0 : saOpts.size() - 1);
		auto saOpt = saOpts.empty() ? softwareAccumulationOption{} : saOpts[srcIdx];
		saveFile << saOpt.accumAll << " " << saOpt.accumNum << " ";
	}
	//saveFile << "\n/*Pic Scale Factor:*/\t" << str(picScaleFactorEdit->text());
	saveFile << "\nEND_PICTURE_SETTINGS\n";
}

hamamatsuPicSettingsGroup PictureSettingsControl::getPictureSettingsFromConfig (ConfigStream& configFile ){
	hamamatsuPicSettingsGroup fileSettings;
	for (unsigned colorInc = 0; colorInc < 4; colorInc++) {
		configFile >> fileSettings.colors[colorInc];
	}
	for (unsigned thresholdInc = 0; thresholdInc < 4; thresholdInc++) {
		configFile >> fileSettings.thresholdStrs[thresholdInc];
	}
	for (unsigned optInc = 0; optInc < 4; optInc++) {
		configFile >> fileSettings.saOpts[optInc].accumAll >> fileSettings.saOpts[optInc].accumNum;
	}
	return fileSettings;
}

void PictureSettingsControl::handleOpenConfig(ConfigStream& openFile, HamamatsuCameraCore* hamamatsu){
	ConfigSystem::checkDelimiterLine(openFile, "PICTURE_SETTINGS");
	auto settings = getPictureSettingsFromConfig ( openFile );
	updateAllSettings ( settings );
	ConfigSystem::checkDelimiterLine(openFile, "END_PICTURE_SETTINGS");
}

void PictureSettingsControl::setSoftwareAccumulationOptions (const std::vector<softwareAccumulationOption>& opts ){
	UNREFERENCED_PARAMETER(opts);
}

std::vector<softwareAccumulationOption> PictureSettingsControl::getSoftwareAccumulationOptions ( ){
	std::vector<softwareAccumulationOption> opts(picsPerRepEdit ? picsPerRepEdit->value() : 1);
	for ( auto picInc : range(opts.size())){
		opts[picInc].accumAll = false;
		opts[picInc].accumNum = 1;
	}
	return opts;
}

void PictureSettingsControl::setPictureControlEnabled (int pic, bool enabled){
	if (pic < 0 || static_cast<size_t>(pic) >= exposureEdits.size()) {
		return;
	}
	if (!exposureEdits[pic] || !thresholdEdits[pic] || !displayChecks[pic]) {
		return;
	}
	exposureEdits[pic]->setEnabled(enabled);
	thresholdEdits[pic]->setEnabled (enabled);
	displayChecks[pic]->setEnabled(enabled);
}

unsigned PictureSettingsControl::getPicsPerRepetition(){
	return getPicsPerRepetitionNonContinuous();
}

unsigned PictureSettingsControl::getPicsPerRepetitionNonContinuous()
{
	int picNum = picsPerRepEdit->value();
	if (picNum <= 0) {
		thrower("ERROR: failed to get pics per repetition?!?");
	}
	return static_cast<unsigned>(picNum);
}

void PictureSettingsControl::setUnofficialPicsPerRep( unsigned picNum ){
	if (picNum < 1) {
		thrower("Tried to set bad number of pics per rep: " + str(picNum));
	}
	unsigned clampedPics = std::min<unsigned>(picNum, MAX_UI_PICS);
	if (picsPerRepEdit->value() != static_cast<int>(clampedPics)) {
		picsPerRepEdit->setValue(static_cast<int>(clampedPics));
	}
	unofficialPicsPerRep = clampedPics;

	for (auto picInc : range(exposureEdits.size())) {
		bool activeRow = picInc < clampedPics;
		if (pictureNumbers[picInc]) {
			pictureNumbers[picInc]->setVisible(activeRow);
		}
		if (exposureEdits[picInc]) {
			exposureEdits[picInc]->setVisible(activeRow);
		}
		if (thresholdEdits[picInc]) {
			thresholdEdits[picInc]->setVisible(activeRow);
		}
		if (displayChecks[picInc]) {
			displayChecks[picInc]->setVisible(activeRow);
		}
		setPictureControlEnabled(static_cast<int>(picInc), activeRow);
	}
}


void PictureSettingsControl::handleOptionChange( ){
	picsPerRepEdit->setEnabled(true);
	bool ok = false;
	int typedPics = picsPerRepEdit->text().toInt(&ok);
	setUnofficialPicsPerRep(static_cast<unsigned>(std::max(1, ok ? typedPics : picsPerRepEdit->value())));
}


std::vector<float> PictureSettingsControl::getExposureTimes ( ){
	std::vector<float> times(picsPerRepEdit ? picsPerRepEdit->value() : 1, 10e-3f);
	for ( auto ctrlNum : range(times.size()) ){
		auto& ctrl = exposureEdits[ ctrlNum ];
		if (!ctrl) {
			return times;
		}
		try	{
			times[ctrlNum] = ctrl->text ().toDouble ()*1e-3;
			//times[ ctrlNum ] = boost::lexical_cast<double>( str(ctrl->text ()) ) * 1e-3;
		}
		catch ( boost::bad_lexical_cast& ){
			thrower ( "Failed to convert exposure time to a float!" );
		}
	}
	return times;
}

std::vector<float> PictureSettingsControl::getUsedExposureTimes() {
	updateSettings( );
	auto allTimes = getExposureTimes();
	std::vector<float> usedTimes(std::begin(allTimes), std::end(allTimes));
	auto targetCount = getPicsPerRepetition();
	if (targetCount <= usedTimes.size()) {
		usedTimes.resize(targetCount);
	}
	else {
		float fillExposure = usedTimes.empty() ? 0.01f : usedTimes.back();
		usedTimes.resize(targetCount, fillExposure);
	}
	return usedTimes;
}

void PictureSettingsControl::setThresholds(const std::vector<std::string>& newThresholds){
	if (newThresholds.empty()) {
		return;
	}
	for (auto thresholdInc : range(thresholdEdits.size())) {
		auto srcIdx = std::min<size_t>(thresholdInc, newThresholds.size() - 1);
		thresholdEdits[thresholdInc]->setText(newThresholds[srcIdx].c_str());
	}
}

std::vector<int> PictureSettingsControl::getPictureColors(){
	return std::vector<int>(std::max(1, picsPerRepEdit->value()), 0);
}

void PictureSettingsControl::updateColormaps (const std::vector<int>& colorIndexes ){
	UNREFERENCED_PARAMETER(colorIndexes);
}

void PictureSettingsControl::setUnofficialExposures ( std::vector<float> times ){
	unsigned count = 0;
	for ( auto ti : times ){
		if (count >= exposureEdits.size()) {
			break;
		}
		exposureEdits[ count++ ]->setText ( cstr ( ti*1e3,5) );
	}
}

void PictureSettingsControl::updateAllSettings ( hamamatsuPicSettingsGroup inputSettings ) {
	updateColormaps ( inputSettings.colors );
	setThresholds ( inputSettings.thresholdStrs );
	setSoftwareAccumulationOptions ( inputSettings.saOpts );
}

std::vector<std::vector<int>> PictureSettingsControl::getThresholds ( ){
	updateSettings ( );
	return currentPicSettings.thresholds;
}

void PictureSettingsControl::updateSettings( ){
	auto activePics = std::max(1, picsPerRepEdit->value());
	currentPicSettings.thresholds.resize(activePics);
	currentPicSettings.colors.assign(activePics, 0);

	for (auto thresholdInc : range(static_cast<size_t>(activePics)) ){
		if (!thresholdEdits[thresholdInc]) {
			return;
		}
		auto& picThresholds = currentPicSettings.thresholds[ thresholdInc ];
		picThresholds.resize ( 1 );
		int threshold;
		
		try{
			QString txt = thresholdEdits[thresholdInc]->text ();						
			threshold = txt.toInt ();
			picThresholds[0] = threshold;
		}
		catch ( boost::bad_lexical_cast& ){
			picThresholds.clear ( );
			// assume it's a file location.
			std::ifstream thresholdFile;
			thresholdFile.open ( str(thresholdEdits[thresholdInc]->text ()).c_str() );
			if ( !thresholdFile.is_open ( ) ){
				thrower  ( "ERROR: failed to convert threshold number " + str ( thresholdInc + 1 ) + " to an integer, "
						 "and it wasn't the address of a threshold-file." );  
			}
			while ( true ){
				double indv_file_threshold;
				thresholdFile >> indv_file_threshold;
				if ( thresholdFile.eof ( ) ) { break; }
				picThresholds.push_back ( indv_file_threshold );
			}
		}
	}
}

void PictureSettingsControl::setEnabledStatus (bool viewRunningSettings) {
	if (viewRunningSettings) {
		picsPerRepEdit->setEnabled(false);
		for (auto num : range(exposureEdits.size())) {
			exposureEdits[num]->setEnabled (false);
			thresholdEdits[num]->setEnabled (false);
			displayChecks[num]->setEnabled(false);
		}
	}
	else {
		picsPerRepEdit->setEnabled(true);
		setUnofficialPicsPerRep (unofficialPicsPerRep);
	}
}

void PictureSettingsControl::toggleExposureTimeEditGui(bool enable)
{
	if (enable) {
		for (auto* eEdit : exposureEdits) {
			eEdit->setEnabled(true);
			eEdit->setStyleSheet("QLineEdit { background: rgb(255, 255, 255); }");
		}
	}
	else {
		for (auto* eEdit : exposureEdits) {
			eEdit->setEnabled(false);
			eEdit->setStyleSheet("QLineEdit { background: rgb(204, 204, 204); }");
		}
	}
}