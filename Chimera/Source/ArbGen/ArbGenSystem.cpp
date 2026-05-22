// created by Mark O. Brown
#include "stdafx.h"

#include "ArbGenSystem.h"
#include "ParameterSystem/ParameterSystem.h"
#include "ConfigurationSystems/ConfigSystem.h"
#include "boost/cast.hpp"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <filesystem>
#include "GeneralUtilityFunctions/range.h"
#include <PrimaryWindows/QtMainWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <LowLevel/constants.h>
#include "boost/lexical_cast.hpp"
#include <qdir.h>
#include <qfile.h>
#include <qgroupbox.h>
#include <qregularexpression.h>
#include <qstringlist.h>
#include <qtextstream.h>
#include <qbuttongroup.h>
#include <qlayout.h>
#include <qcoreapplication.h>
#include <qeventloop.h>
#include <QtConcurrent/qtconcurrentrun.h>
#include <cctype>
#include <cmath>

ArbGenSystem::ArbGenSystem( const arbGenSettings& settings, ArbGenType type, IChimeraQtWindow* parent )
	: IChimeraSystem(parent)
	, initSettings(settings)
	, arbGenScript(parent)
	, arbType(type)
{
	switch (arbType) {
	case ArbGenType::Agilent:
		pCore = new AgilentCore(settings);
		break;
	case ArbGenType::Siglent:
		pCore = new SiglentCore(settings);
		break;
	}
}

ArbGenSystem::~ArbGenSystem()
{
	if (csvUploadWatcher && csvUploadWatcher->isRunning()) {
		csvUploadWatcher->waitForFinished();
	}
	delete pCore;
}

void ArbGenSystem::programArbGenNow(std::vector<parameterType> constants){
	currentGuiInfo = getOutputInfo();
	pCore->convertInputToFinalSettings (0, currentGuiInfo, constants);
	pCore->convertInputToFinalSettings (1, currentGuiInfo, constants);
	pCore->setArbGen (0, constants, currentGuiInfo, nullptr);
	pCore->setRunSettings(currentGuiInfo); // This is meant to let the core save the gui setting

}

std::string ArbGenSystem::getDeviceIdentity (){
	return pCore->getDeviceIdentity ();
}

std::string ArbGenSystem::getConfigDelim (){
	return pCore->configDelim;
}

bool ArbGenSystem::getSavedStatus (){
	return true;
}

void ArbGenSystem::updateSavedStatus (bool isSaved){
	(void)isSaved;
}

void ArbGenSystem::initialize(std::string headerText, IChimeraQtWindow* win)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);
	layout->setAlignment(Qt::AlignTop);
	try {
		pCore->initialize ();
	}
	catch (ChimeraError& err) {
		// Device not connected - continue with "Disconnected" status
		// User can click Reconnect button later to retry
	}
	header = new QLabel (cstr (headerText), win);
	auto deviceInfo = pCore->getDeviceInfo ();
	if (deviceInfo.size () > 1) {// deal with trailing newline
		deviceInfo.erase (deviceInfo.size ()-1, 1);
	}
	deviceInfoDisplay = new QLabel (qstr (deviceInfo), win);
	deviceInfoDisplay->setStyleSheet ("QLabel { font: 8pt }; ");
	deviceInfoDisplay->setVisible(false);
	layout->addWidget(header, 0);


	polarityButton = new CQCheckBox("Polarity Invert?", win);
	polarityButton->setChecked(false);

	uploadCsvNow = new CQPushButton("Upload and Program", win);

	QGroupBox* siglentGroup = new QGroupBox("Siglent FM Workflow", win);
	QGridLayout* siglentLayout = new QGridLayout(siglentGroup);

	siglentFmCtrlButton = new CQCheckBox("Ctrl?", win);
	siglentFmCtrlButton->setChecked(false);
	siglentLayout->addWidget(siglentFmCtrlButton, 0, 0, 1, 1);

	clockExternalButton = new CQCheckBox("External Clock", win);
	clockExternalButton->setChecked(true);
	siglentLayout->addWidget(clockExternalButton, 0, 1, 1, 1);
	connect(clockExternalButton, &QCheckBox::toggled, this, [this, win](bool checked) {
		try {
			auto* siglent = dynamic_cast<SiglentCore*>(pCore);
			if (!siglent) {
				return;
			}
			siglent->setClockSourceLikePyvisa(checked);
			if (win) {
				win->reportStatus(QString("Siglent clock source set to ") + (checked ? "EXTERNAL" : "INTERNAL") + "\r\n");
			}
		}
		catch (ChimeraError& err) {
			if (win) {
				win->reportErr("Failed to set clock source: " + err.qtrace());
			}
		}
		catch (std::exception& err) {
			if (win) {
				win->reportErr("Failed to set clock source (std::exception): " + qstr(err.what()));
			}
		}
		catch (...) {
			if (win) {
				win->reportErr("Failed to set clock source: unknown exception.");
			}
		}
	});

	CQPushButton* reconnectButton = new CQPushButton("Reconnect", win);
	connect(reconnectButton, &QPushButton::released, this, [this, win]() {
		try {
			if (csvUploadInProgress) {
				thrower("Cannot reconnect while an upload is in progress.");
			}
			pCore->reconnectFlume();
			if (win) {
				win->reportStatus("Reconnected to Siglent AWG.\r\n");
			}
		}
		catch (ChimeraError& err) {
			if (win) {
				win->reportErr("Reconnect failed: " + err.qtrace());
			}
		}
		});
	siglentLayout->addWidget(reconnectButton, 1, 0, 1, 2);

	siglentLayout->addWidget(new QLabel("Binary Waveforms", win), 2, 0);
	QHBoxLayout* csvLayout = new QHBoxLayout();
	generatedWaveformCombo = new CQComboBox(win);
	csvLayout->addWidget(generatedWaveformCombo, 1);
	CQPushButton* refreshBinButton = new CQPushButton("Refresh", win);
	refreshBinButton->setMaximumWidth(80);
	connect(refreshBinButton, &QPushButton::released, this, [this]() {
		refreshGeneratedWaveforms();
	});
	connect(generatedWaveformCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
		this, [this](int) {
			updateCalculatedSampleRateDisplay();
		});
	csvLayout->addWidget(refreshBinButton, 0);
	siglentLayout->addLayout(csvLayout, 2, 1);

	CQPushButton* programSettingsButton = new CQPushButton("Program", win);
	QHBoxLayout* actionButtonsLayout = new QHBoxLayout();
	actionButtonsLayout->addWidget(uploadCsvNow, 1);
	actionButtonsLayout->addWidget(programSettingsButton, 1);
	siglentLayout->addLayout(actionButtonsLayout, 3, 0, 1, 2);

	connect(uploadCsvNow, &QPushButton::released, this, [this, win]() {
		QString previousSampleRateLabel = ch1SampleRateLabel ? ch1SampleRateLabel->text() : QString("(Calculated on Send)");
		uploadCsvNow->setEnabled(false);
		uploadCsvNow->setText("Uploading...");
		if (ch1SampleRateLabel) {
			ch1SampleRateLabel->setText("Uploading...");
		}
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		try {
			handleUploadCsvPressed(win);
			if (win) {
				win->reportStatus("Upload and program workflow complete.\r\n");
			}
		}
		catch (ChimeraError& err) {
			if (ch1SampleRateLabel) {
				ch1SampleRateLabel->setText(previousSampleRateLabel);
			}
			if (win) {
				QString msg = "Upload and program failed: " + err.qtrace();
				win->reportErr(msg);
				win->reportStatus(msg + "\r\n");
			}
		}
		catch (std::exception& err) {
			if (ch1SampleRateLabel) {
				ch1SampleRateLabel->setText(previousSampleRateLabel);
			}
			if (win) {
				QString msg = "Upload and program failed (std::exception): " + qstr(err.what());
				win->reportErr(msg);
				win->reportStatus(msg + "\r\n");
			}
		}
		catch (...) {
			if (ch1SampleRateLabel) {
				ch1SampleRateLabel->setText(previousSampleRateLabel);
			}
			if (win) {
				QString msg = "Upload and program failed: unknown exception.";
				win->reportErr(msg);
				win->reportStatus(msg + "\r\n");
			}
		}
		uploadCsvNow->setText("Upload and Program");
		uploadCsvNow->setEnabled(true);
		});

	siglentLayout->addWidget(new QLabel("Pulse Duration (ms)", win), 4, 0);
	ch1PulseDurationMsEdit = new QLineEdit("1.0", win);
	connect(ch1PulseDurationMsEdit, &QLineEdit::textChanged, this, [this](const QString&) {
		updateCalculatedSampleRateDisplay();
	});
	siglentLayout->addWidget(ch1PulseDurationMsEdit, 4, 1);

	siglentLayout->addWidget(new QLabel("Sample Rate (Sa/s)", win), 5, 0);
	ch1SampleRateLabel = new QLabel("-", win);
	ch1SampleRateLabel->setText("(Calculated on Send)");
	siglentLayout->addWidget(ch1SampleRateLabel, 5, 1);

	QHBoxLayout* channelColumns = new QHBoxLayout();

	QGroupBox* ch2Group = new QGroupBox("CH2 / Carrier", win);
	QGridLayout* ch2Layout = new QGridLayout(ch2Group);
	ch2Layout->addWidget(new QLabel("Frequency (MHz)", win), 0, 0);
	ch2FrequencyMHzEdit = new QLineEdit("100.0", win);
	ch2FrequencyMHzEdit->setPlaceholderText("e.g. 100.0 or fm_freq_var");
	ch2Layout->addWidget(ch2FrequencyMHzEdit, 0, 1);
	ch2Layout->addWidget(new QLabel("Amplitude (Vpp)", win), 1, 0);
	ch2AmplitudeEdit = new QLineEdit("2.0", win);
	ch2AmplitudeEdit->setPlaceholderText("e.g. 2.0 or fm_amp_var");
	ch2Layout->addWidget(ch2AmplitudeEdit, 1, 1);
	ch2Layout->addWidget(new QLabel("Phase (deg)", win), 2, 0);
	ch2PhaseEdit = new QLineEdit("0", win);
	ch2PhaseEdit->setPlaceholderText("e.g. 0 or fm_phase_var");
	ch2Layout->addWidget(ch2PhaseEdit, 2, 1);
	ch2Layout->addWidget(new QLabel("FM Deviation (MHz)", win), 3, 0);
	ch2FrequencyDeviationMHzEdit = new QLineEdit("0.05", win);
	ch2FrequencyDeviationMHzEdit->setPlaceholderText("e.g. 0.05 or fm_dev_var");
	ch2Layout->addWidget(ch2FrequencyDeviationMHzEdit, 3, 1);

	QGroupBox* ch1Group = new QGroupBox("CH1 / Modulator", win);
	QGridLayout* ch1Layout = new QGridLayout(ch1Group);
	ch1Layout->addWidget(new QLabel("Amplitude (Vpp)", win), 0, 0);
	ch1AmplitudeEdit = new QLineEdit("1.0", win);
	ch1AmplitudeEdit->setPlaceholderText("e.g. 1.0 or fm_ch1_amp_var");
	ch1Layout->addWidget(ch1AmplitudeEdit, 0, 1);
	ch1Layout->addWidget(new QLabel("Offset (V)", win), 1, 0);
	ch1OffsetEdit = new QLineEdit("0.0", win);
	ch1OffsetEdit->setPlaceholderText("e.g. 0.0 or fm_ch1_offset_var");
	ch1Layout->addWidget(ch1OffsetEdit, 1, 1);
	ch1Layout->addWidget(new QLabel("Start Phase (deg)", win), 2, 0);
	ch1StartPhaseEdit = new QLineEdit("0", win);
	ch1StartPhaseEdit->setPlaceholderText("e.g. 0 or fm_ch1_phase_var");
	ch1Layout->addWidget(ch1StartPhaseEdit, 2, 1);
	ch1Layout->addWidget(new QLabel("Burst Cycles (NCYC)", win), 3, 0);
	ch1BurstCyclesEdit = new QLineEdit("1", win);
	ch1BurstCyclesEdit->setPlaceholderText("e.g. 1 or burst_cycles_var");
	ch1Layout->addWidget(ch1BurstCyclesEdit, 3, 1);

	channelColumns->addWidget(ch2Group, 1);
	channelColumns->addWidget(ch1Group, 1);
	siglentLayout->addLayout(channelColumns, 6, 0, 1, 2);

	connect(programSettingsButton, &QPushButton::released, this, [this, win, programSettingsButton]() {
		programSettingsButton->setEnabled(false);
		programSettingsButton->setText("Programming...");
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		try {
			handleProgramSettingsPressed(win);
			if (win) {
				win->reportStatus("Program workflow complete.\r\n");
			}
		}
		catch (ChimeraError& err) {
			if (win) {
				QString msg = "Program workflow failed: " + err.qtrace();
				win->reportErr(msg);
				win->reportStatus(msg + "\r\n");
			}
		}
		catch (std::exception& err) {
			if (win) {
				QString msg = "Program workflow failed (std::exception): " + qstr(err.what());
				win->reportErr(msg);
				win->reportStatus(msg + "\r\n");
			}
		}
		catch (...) {
			if (win) {
				QString msg = "Program workflow failed: unknown exception.";
				win->reportErr(msg);
				win->reportStatus(msg + "\r\n");
			}
		}
		programSettingsButton->setText("Program");
		programSettingsButton->setEnabled(true);
		});

	layout->addWidget(siglentGroup, 0);

	refreshGeneratedWaveforms();
	updateCalculatedSampleRateDisplay();

	(void)win;
}

void ArbGenSystem::refreshGeneratedWaveforms() {
	if (!generatedWaveformCombo) {
		return;
	}
	QString selectedPath;
	if (generatedWaveformCombo->currentIndex() >= 0) {
		selectedPath = generatedWaveformCombo->currentData().toString();
	}
	QString waveformDir = qstr(str(CODE_ROOT) + "\\Chimera\\generated waveforms");
	QDir dir(waveformDir);
	if (!dir.exists()) {
		dir.mkpath(".");
	}
	QStringList binFilters;
	binFilters << "*.bin";
	QFileInfoList files = dir.entryInfoList(binFilters, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

	generatedWaveformCombo->blockSignals(true);
	generatedWaveformCombo->clear();
	for (const auto& fileInfo : files) {
		generatedWaveformCombo->addItem(fileInfo.fileName(), fileInfo.absoluteFilePath());
	}
	if (!selectedPath.isEmpty()) {
		int selectedIndex = generatedWaveformCombo->findData(selectedPath);
		if (selectedIndex >= 0) {
			generatedWaveformCombo->setCurrentIndex(selectedIndex);
		}
	}
	generatedWaveformCombo->blockSignals(false);
	updateCalculatedSampleRateDisplay();
}

int ArbGenSystem::getSelectedCsvPointCount() const {
	if (!generatedWaveformCombo || generatedWaveformCombo->currentIndex() < 0) {
		return 0;
	}
	QString path = generatedWaveformCombo->currentData().toString();
	QFile binFile(path);
	if (!binFile.open(QIODevice::ReadOnly)) {
		return 0;
	}
	// Binary format is int16, so 2 bytes per sample
	qint64 fileSize = binFile.size();
	binFile.close();
	if (fileSize <= 0 || fileSize % 2 != 0) {
		return 0;  // Invalid file size for int16 samples
	}
	return static_cast<int>(fileSize / 2);
}

double ArbGenSystem::getPulseDurationMs(IChimeraQtWindow* win) const {
	if (!ch1PulseDurationMsEdit) {
		return 0;
	}
	bool ok = false;
	double durationMs = ch1PulseDurationMsEdit->text().toDouble(&ok);
	if (ok && durationMs > 0) {
		return durationMs;
	}
	// Expression evaluation requires access to currently defined variables.
	if (!win || !win->auxWin) {
		return 0;
	}
	Expression durationExpr = str(ch1PulseDurationMsEdit->text());
	std::vector<parameterType> constants = win->auxWin->getAllParams();
	for (auto& param : constants) {
		if (!param.constant && !param.ranges.empty()) {
			param.constant = true;
			param.constantValue = param.ranges[0].initialValue;
		}
	}
	ScanRangeInfo constantRange;
	constantRange.defaultInit();
	ParameterSystem::generateKey(constants, false, constantRange);
	durationExpr.assertValid(constants, GLOBAL_PARAMETER_SCOPE);
	durationExpr.internalEvaluate(constants, 1);
	durationMs = durationExpr.getValue(0);
	if (!std::isfinite(durationMs) || durationMs <= 0) {
		thrower("Pulse duration expression must evaluate to a positive finite value in ms.");
	}
	return durationMs;
}

void ArbGenSystem::updateCalculatedSampleRateDisplay() {
	if (!ch1SampleRateLabel) {
		return;
	}
	int points = getSelectedCsvPointCount();
	if (points <= 0) {
		ch1SampleRateLabel->setText("-");
		return;
	}
	const double maxSampleRate = 300e6;
	double minDurationMs = (points / maxSampleRate) * 1e3;
	double durationMs = 0;
	try {
		durationMs = getPulseDurationMs(parentWin);
	}
	catch (ChimeraError&) {
		// While the user is typing, expressions may be temporarily invalid.
		ch1SampleRateLabel->setText("Enter valid duration");
		return;
	}
	if (durationMs <= 0) {
		ch1SampleRateLabel->setText("Min duration: " + QString::number(minDurationMs, 'f', 3) + " ms");
		return;
	}
	double sampleRate = points / (durationMs * 1e-3);
	if (sampleRate > maxSampleRate) {
		ch1SampleRateLabel->setText("Min duration: " + QString::number(minDurationMs, 'f', 3) + " ms");
		return;
	}
	ch1SampleRateLabel->setText(QString::number(sampleRate, 'f', 0));
}

void ArbGenSystem::handleUploadCsvPressed(IChimeraQtWindow* win) {
	auto* siglent = dynamic_cast<SiglentCore*>(pCore);
	if (!siglent) {
		thrower("CSV upload is only implemented for Siglent AWGs.");
	}
	if (!generatedWaveformCombo || generatedWaveformCombo->currentIndex() < 0) {
		thrower("No binary waveform selected. Put .bin files in Chimera/generated waveforms and select one.");
	}
	double durationMs = getPulseDurationMs(win);
	if (durationMs <= 0) {
		int points = getSelectedCsvPointCount();
		if (points > 0 && ch1SampleRateLabel) {
			double minDurationMs = (points / 300e6) * 1e3;
			ch1SampleRateLabel->setText("Min duration: " + QString::number(minDurationMs, 'f', 3) + " ms");
		}
		thrower("Pulse duration is invalid. Use the displayed minimum duration.");
	}
	unsigned guiSampleRateSaS = 0;
	{
		int points = getSelectedCsvPointCount();
		if (points <= 0) {
			thrower("Could not determine selected waveform point count for CH1 sample-rate calculation.");
		}
		const double maxSampleRate = 300e6;
		double requestedSampleRate = points / (durationMs * 1e-3);
		if (requestedSampleRate > maxSampleRate) {
			double minDurationMs = (points / maxSampleRate) * 1e3;
			if (ch1SampleRateLabel) {
				ch1SampleRateLabel->setText("Min duration: " + QString::number(minDurationMs, 'f', 3) + " ms");
			}
			thrower("Pulse duration is too short. Use the displayed minimum duration.");
		}
		guiSampleRateSaS = static_cast<unsigned>(std::llround(requestedSampleRate));
	}
	QString csvPath = generatedWaveformCombo->currentData().toString();
	const std::string csvPathStd = csvPath.toStdString();
	
	// Verify file exists before attempting upload
	if (!std::filesystem::exists(csvPathStd)) {
		thrower("Binary waveform file not found: " + csvPathStd);
	}
	
	std::string waveName = generatedWaveformCombo->currentText().toStdString();
	// Remove .bin extension if present
	if (waveName.size() > 4 && waveName.substr(waveName.size() - 4) == ".bin") {
		waveName = waveName.substr(0, waveName.size() - 4);
	}
	// Clean up filename to be a valid waveform name
	for (char& c : waveName) {
		if (!std::isalnum(static_cast<unsigned char>(c))) {
			c = '_';
		}
	}
	if (win) {
		win->reportStatus("Running upload and program workflow...\r\n");
	}

	std::vector<parameterType> constants;
	if (win && win->auxWin) {
		constants = win->auxWin->getAllParams();
		for (auto& param : constants) {
			if (!param.constant && !param.ranges.empty()) {
				param.constant = true;
				param.constantValue = param.ranges[0].initialValue;
			}
		}
		ScanRangeInfo constantRange;
		constantRange.defaultInit();
		ParameterSystem::generateKey(constants, false, constantRange);
	}
	deviceOutputInfo tempSettings = getOutputInfo();
	tempSettings.siglentFm.control = true;
	pCore->convertInputToFinalSettings(0, tempSettings, constants);
	auto cyclesVal = tempSettings.siglentFm.ch1BurstCycles.getValue(0);
	auto roundedCycles = std::llround(cyclesVal);
	if (roundedCycles <= 0 || std::fabs(cyclesVal - roundedCycles) > 1e-6) {
		thrower("CH1 burst cycles must evaluate to a positive integer. Value: " + str(cyclesVal));
	}

	// Exact order requested: reset, program CH2, program CH1, then upload CH1.
	siglent->resetAwgLikePyvisa();
	siglent->setupCh2LikePyvisa(
		tempSettings.siglentFm.ch2FrequencyMHz.getValue(0),
		tempSettings.siglentFm.ch2AmplitudeVpp.getValue(0),
		tempSettings.siglentFm.ch2PhaseDeg.getValue(0),
		tempSettings.siglentFm.ch2FrequencyDeviationMHz.getValue(0));
	siglent->programCh1TrueArbLikePyvisa(tempSettings.siglentFm.ch1AmplitudeVpp.getValue(0),
		tempSettings.siglentFm.ch1OffsetV.getValue(0),
		tempSettings.siglentFm.ch1StartPhaseDeg.getValue(0),
		guiSampleRateSaS, static_cast<unsigned>(roundedCycles), "wave");
	auto sampleRate = siglent->uploadBinWaveformToChannel1(csvPathStd, durationMs, waveName,
		tempSettings.siglentFm.ch1AmplitudeVpp.getValue(0),
		tempSettings.siglentFm.ch1OffsetV.getValue(0),
		tempSettings.siglentFm.ch1StartPhaseDeg.getValue(0));
	siglent->waitForOperationCompleteLikePyvisa();


	if (ch1SampleRateLabel) {
		ch1SampleRateLabel->setText(qstr(str(sampleRate)));
	}
}

void ArbGenSystem::handleProgramSettingsPressed(IChimeraQtWindow* win) {
	auto* siglent = dynamic_cast<SiglentCore*>(pCore);
	if (!siglent) {
		thrower("Direct FM settings programming is only implemented for Siglent AWGs.");
	}
	if (win) {
		win->reportStatus("Running program workflow...\r\n");
	}

	// Exact order requested: reset, program CH1, program CH2, testing keys.
	siglent->resetAwgLikePyvisa();

	std::vector<parameterType> constants;
	if (win && win->auxWin) {
		constants = win->auxWin->getAllParams();
		for (auto& param : constants) {
			if (!param.constant && !param.ranges.empty()) {
				param.constant = true;
				param.constantValue = param.ranges[0].initialValue;
			}
		}
		ScanRangeInfo constantRange;
		constantRange.defaultInit();
		ParameterSystem::generateKey(constants, false, constantRange);
	}
	deviceOutputInfo tempSettings = getOutputInfo();
	tempSettings.siglentFm.control = true;
	pCore->convertInputToFinalSettings(0, tempSettings, constants);
	double durationMs = getPulseDurationMs(win);
	if (durationMs <= 0) {
		thrower("Pulse duration is invalid. Use a positive duration to define CH1 sample rate.");
	}
	int points = getSelectedCsvPointCount();
	if (points <= 0) {
		thrower("Could not determine selected waveform point count for CH1 sample-rate calculation.");
	}
	const double maxSampleRate = 300e6;
	double requestedSampleRate = points / (durationMs * 1e-3);
	if (requestedSampleRate > maxSampleRate) {
		double minDurationMs = (points / maxSampleRate) * 1e3;
		thrower("Pulse duration is too short for the selected waveform. Minimum duration is " + str(minDurationMs) + " ms.");
	}
	unsigned guiSampleRateSaS = static_cast<unsigned>(std::llround(requestedSampleRate));
	auto cyclesVal = tempSettings.siglentFm.ch1BurstCycles.getValue(0);
	auto roundedCycles = std::llround(cyclesVal);
	if (roundedCycles <= 0 || std::fabs(cyclesVal - roundedCycles) > 1e-6) {
		thrower("CH1 burst cycles must evaluate to a positive integer. Value: " + str(cyclesVal));
	}
	siglent->setupCh2LikePyvisa(
		tempSettings.siglentFm.ch2FrequencyMHz.getValue(0),
		tempSettings.siglentFm.ch2AmplitudeVpp.getValue(0),
		tempSettings.siglentFm.ch2PhaseDeg.getValue(0),
		tempSettings.siglentFm.ch2FrequencyDeviationMHz.getValue(0));
	siglent->programCh1TrueArbLikePyvisa(tempSettings.siglentFm.ch1AmplitudeVpp.getValue(0),
		tempSettings.siglentFm.ch1OffsetV.getValue(0),
		tempSettings.siglentFm.ch1StartPhaseDeg.getValue(0),
		guiSampleRateSaS, static_cast<unsigned>(roundedCycles), "wave");
	
	siglent->selectWaveform();
	siglent->waitForOperationCompleteLikePyvisa();

	if (win) {
		win->reportStatus("Program workflow complete.\r\n");
	}
}


ArbGenCore& ArbGenSystem::getCore (){
	return *pCore;
}


void ArbGenSystem::checkSave( std::string configPath, RunInfo info ){
	(void)configPath;
	(void)info;
}


void ArbGenSystem::verifyScriptable ( ){
	// Keep this as a compatibility no-op: legacy menu actions call verifyScriptable()
	// before script operations, but the Siglent FM workflow does not expose scripting mode.
	return;
}

void ArbGenSystem::setDefault (unsigned chan){
	pCore->setDefault (chan);
}



















deviceOutputInfo ArbGenSystem::getOutputInfo(){
	deviceOutputInfo info = currentGuiInfo;
	syncSiglentFmSettingsFromGui(info);
	return info;
}

/*
This function outputs a string that contains all of the information that is set by the user for a given configuration. 
*/
void ArbGenSystem::handleSavingConfig(ConfigStream& saveFile, std::string configPath, RunInfo info,
	bool includeSectionDelimiters){	
	deviceOutputInfo outputInfo = getOutputInfo();
	(void)configPath;
	(void)info;
	if (includeSectionDelimiters) {
		saveFile << pCore->configDelim + "\n";
	}
	saveFile << "/*Synced Option:*/ " << str (outputInfo.synced);
	std::vector<std::string> channelStrings = { "\nCHANNEL_1", "\nCHANNEL_2" };
	for (auto chanInc : range (2)){
		auto& channel = outputInfo.channel[chanInc];
		saveFile << channelStrings[chanInc];
		saveFile << "\n/*Channel Mode:*/\t\t\t\t" << ArbGenChannelMode::toStr (channel.option);
		saveFile << "\n/*Polarity Invert:*/\t\t\t" << channel.polarityInvert;
		saveFile << "\n/*DC Level:*/\t\t\t\t\t" << channel.dc.dcLevel;
		saveFile << "\n/*DC Calibrated:*/\t\t\t\t" << channel.dc.useCal;
		saveFile << "\n/*Sine Amplitude:*/\t\t\t\t" << channel.sine.amplitude;
		saveFile << "\n/*Sine Freq:*/\t\t\t\t\t" << channel.sine.frequency;
		saveFile << "\n/*Sine Phase:*/\t\t\t\t\t" << channel.sine.phase;
		saveFile << "\n/*Sine Burst:*/\t\t\t\t\t" << channel.sine.burstMode;
		saveFile << "\n/*Sine Calibrated:*/\t\t\t" << channel.sine.useCal;
		saveFile << "\n/*Square Amplitude:*/\t\t\t" << channel.square.amplitude;
		saveFile << "\n/*Square Freq:*/\t\t\t\t" << channel.square.frequency;
		saveFile << "\n/*Square Offset:*/\t\t\t\t" << channel.square.offset;
		saveFile << "\n/*Square DutyCycle:*/\t\t\t" << channel.square.dutyCycle;
		saveFile << "\n/*Square Phase:*/\t\t\t\t" << channel.square.phase;
		saveFile << "\n/*Square Burst:*/\t\t\t\t" << channel.square.burstMode;
		saveFile << "\n/*Square Calibrated:*/\t\t\t" << channel.square.useCal;
		saveFile << "\n/*Preloaded Arb Address:*/\t\t" << channel.preloadedArb.address;
		saveFile << "\n/*Preloaded Arb Calibrated:*/\t" << channel.preloadedArb.useCal;
		saveFile << "\n/*Preloaded Arb Burst:*/\t" << channel.preloadedArb.burstMode;
	}
	saveFile << "\n/*Siglent FM Control:*/\t\t" << outputInfo.siglentFm.control;
	saveFile << "\n/*Siglent FM External Clock:*/\t" << outputInfo.siglentFm.useExternalClock;
	saveFile << "\n/*Siglent FM CH1 Amplitude:*/\t" << outputInfo.siglentFm.ch1AmplitudeVpp;
	saveFile << "\n/*Siglent FM CH1 Phase:*/\t\t" << outputInfo.siglentFm.ch1StartPhaseDeg;
	saveFile << "\n/*Siglent FM CH1 Burst Cycles:*/\t" << outputInfo.siglentFm.ch1BurstCycles;
	saveFile << "\n/*Siglent FM CH2 Frequency:*/\t" << outputInfo.siglentFm.ch2FrequencyMHz;
	saveFile << "\n/*Siglent FM CH2 Amplitude:*/\t" << outputInfo.siglentFm.ch2AmplitudeVpp;
	saveFile << "\n/*Siglent FM CH2 Phase:*/\t\t" << outputInfo.siglentFm.ch2PhaseDeg;
	saveFile << "\n/*Siglent FM Deviation:*/\t\t" << outputInfo.siglentFm.ch2FrequencyDeviationMHz;
	saveFile << "\n/*Siglent FM CH1 Pulse Duration:*/\t" << outputInfo.siglentFm.ch1PulseDurationMs;
	saveFile << "\n/*Siglent FM CH1 Offset:*/\t\t" << outputInfo.siglentFm.ch1OffsetV;
	if (includeSectionDelimiters) {
		saveFile << "\nEND_" + pCore->configDelim + "\n";
	}
	else {
		saveFile << "\n";
	}
}

void ArbGenSystem::setOutputSettings (deviceOutputInfo info){
	currentGuiInfo = info;
	loadSiglentFmSettingsToGui(currentGuiInfo);
}


void ArbGenSystem::handleOpenConfig( ConfigStream& file ){
	setOutputSettings (pCore->getSettingsFromConfig (file));
	loadSiglentFmSettingsToGui(currentGuiInfo);
}


bool ArbGenSystem::scriptingModeIsSelected (){
	return false; // Scripting not supported in FM-only workflow
}

void ArbGenSystem::syncSiglentFmSettingsFromGui(deviceOutputInfo& info) const
{
	if (siglentFmCtrlButton) {
		info.siglentFm.control = siglentFmCtrlButton->isChecked();
	}
	if (clockExternalButton) {
		info.siglentFm.useExternalClock = clockExternalButton->isChecked();
	}
	if (ch1PulseDurationMsEdit) {
		info.siglentFm.ch1PulseDurationMs.expressionStr = str(ch1PulseDurationMsEdit->text());
	}
	if (ch1AmplitudeEdit) {
		info.siglentFm.ch1AmplitudeVpp.expressionStr = str(ch1AmplitudeEdit->text());
	}
	if (ch1OffsetEdit) {
		info.siglentFm.ch1OffsetV.expressionStr = str(ch1OffsetEdit->text());
	}
	if (ch1StartPhaseEdit) {
		info.siglentFm.ch1StartPhaseDeg.expressionStr = str(ch1StartPhaseEdit->text());
	}
	if (ch1BurstCyclesEdit) {
		info.siglentFm.ch1BurstCycles.expressionStr = str(ch1BurstCyclesEdit->text());
	}
	if (ch2FrequencyMHzEdit) {
		info.siglentFm.ch2FrequencyMHz.expressionStr = str(ch2FrequencyMHzEdit->text());
	}
	if (ch2AmplitudeEdit) {
		info.siglentFm.ch2AmplitudeVpp.expressionStr = str(ch2AmplitudeEdit->text());
	}
	if (ch2PhaseEdit) {
		info.siglentFm.ch2PhaseDeg.expressionStr = str(ch2PhaseEdit->text());
	}
	if (ch2FrequencyDeviationMHzEdit) {
		info.siglentFm.ch2FrequencyDeviationMHz.expressionStr = str(ch2FrequencyDeviationMHzEdit->text());
	}
}

void ArbGenSystem::loadSiglentFmSettingsToGui(const deviceOutputInfo& info)
{
	if (siglentFmCtrlButton) {
		siglentFmCtrlButton->setChecked(info.siglentFm.control);
	}
	if (clockExternalButton) {
		clockExternalButton->setChecked(info.siglentFm.useExternalClock);
	}
	if (ch1PulseDurationMsEdit) {
		ch1PulseDurationMsEdit->setText(qstr(info.siglentFm.ch1PulseDurationMs.expressionStr));
	}
	if (ch1AmplitudeEdit) {
		ch1AmplitudeEdit->setText(qstr(info.siglentFm.ch1AmplitudeVpp.expressionStr));
	}
	if (ch1OffsetEdit) {
		ch1OffsetEdit->setText(qstr(info.siglentFm.ch1OffsetV.expressionStr));
	}
	if (ch1StartPhaseEdit) {
		ch1StartPhaseEdit->setText(qstr(info.siglentFm.ch1StartPhaseDeg.expressionStr));
	}
	if (ch1BurstCyclesEdit) {
		ch1BurstCyclesEdit->setText(qstr(info.siglentFm.ch1BurstCycles.expressionStr));
	}
	if (ch2FrequencyMHzEdit) {
		ch2FrequencyMHzEdit->setText(qstr(info.siglentFm.ch2FrequencyMHz.expressionStr));
	}
	if (ch2AmplitudeEdit) {
		ch2AmplitudeEdit->setText(qstr(info.siglentFm.ch2AmplitudeVpp.expressionStr));
	}
	if (ch2PhaseEdit) {
		ch2PhaseEdit->setText(qstr(info.siglentFm.ch2PhaseDeg.expressionStr));
	}
	if (ch2FrequencyDeviationMHzEdit) {
		ch2FrequencyDeviationMHzEdit->setText(qstr(info.siglentFm.ch2FrequencyDeviationMHz.expressionStr));
	}
}

void ArbGenSystem::initializeSiglentFmOnStartup(IChimeraQtWindow* win) {
	(void)win;
	// Requested behavior: do not auto-reset or auto-program on startup.
}
