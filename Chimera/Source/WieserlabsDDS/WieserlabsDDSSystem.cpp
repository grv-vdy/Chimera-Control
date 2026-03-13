#define _WINSOCKAPI_
#include "stdafx.h"
#include <WinSock2.h>
#include "WieserlabsDDSSystem.h"
#include "WieserlabsDDSStructures.h"
#include "ConfigurationSystems/ConfigSystem.h"
#include "Scripts/ScriptStream.h"
#include "ParameterSystem/Expression.h"
#include "ParameterSystem/ParameterSystem.h"
#include "ParameterSystem/ParameterSystemStructures.h"
#include "PrimaryWindows/IChimeraQtWindow.h"
#include "PrimaryWindows/QtAuxiliaryWindow.h"
#include "ExcessDialogs/saveWithExplorer.h"
#include "ExcessDialogs/openWithExplorer.h"
#include <qdebug.h>
#include <QMessageBox>

WieserlabsDDSSystem::WieserlabsDDSSystem(const wieserlabsDdsSettings& settings, IChimeraQtWindow* parent)
	: IChimeraSystem(parent), initSettings(settings), core(settings)
{
}

WieserlabsDDSSystem::~WieserlabsDDSSystem()
{
}

void WieserlabsDDSSystem::initialize(std::string headerText, IChimeraQtWindow* win)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	header = new QLabel(QString(headerText.c_str()), this);
	header->setStyleSheet("QLabel { font-size: 12pt; font-weight: bold; }");
	layout->addWidget(header);

	// Mode combo
	modeCombo = new CQComboBox(win);
	modeCombo->addItem("Script");
	modeCombo->addItem("No Script");
	modeCombo->setCurrentIndex(0);
	layout->addWidget(modeCombo);

	ctrlButton = new CQCheckBox("Control", win);
	ctrlButton->setChecked(false);
	layout->addWidget(ctrlButton);

	// Channel controls
	for (int chan = 0; chan < 2; chan++) {
		QHBoxLayout* chanLayout = new QHBoxLayout();

		// Channel label
		channelLabels.push_back(new QLabel(QString("Ch %1:").arg(chan), this));
		chanLayout->addWidget(channelLabels.back());

		// On button
		onButtons.push_back(new CQCheckBox("", win));
		chanLayout->addWidget(onButtons.back());

		// Frequency
		chanLayout->addWidget(new QLabel("Freq (MHz):", this));
		freqEdits.push_back(new CQLineEdit("", win));
		chanLayout->addWidget(freqEdits.back());

		// Amplitude
		chanLayout->addWidget(new QLabel("Amp (0-1):", this));
		ampEdits.push_back(new CQLineEdit("", win));
		chanLayout->addWidget(ampEdits.back());

		// Phase
		chanLayout->addWidget(new QLabel("Phase (deg):", this));
		phaseEdits.push_back(new CQLineEdit("", win));
		chanLayout->addWidget(phaseEdits.back());

		// Channel button
		channelButtons.push_back(new QPushButton("Set", this));
		chanLayout->addWidget(channelButtons.back());

		layout->addLayout(chanLayout);
	}

	// Script control
	wieserlabsDdsScript = new Script(win);
	wieserlabsDdsScript->initialize(win, "Wieserlabs DDS", "DDS");
	layout->addWidget(wieserlabsDdsScript);
	refreshScriptedWaveform();

	connect(modeCombo, qOverload<int>(&CQComboBox::currentIndexChanged), this, &WieserlabsDDSSystem::handleModeCombo);

	for (int chan = 0; chan < 2; chan++) {
		connect(channelButtons[chan], &QPushButton::clicked, [this, chan]() {
			handleChannelPress(chan, "", RunInfo());
		});
	}
}

void WieserlabsDDSSystem::updateButtonDisplay(int chan)
{
	// Update button display if needed
}

void WieserlabsDDSSystem::checkSave(std::string configPath, RunInfo info)
{
	// Check if settings need saving
}

void WieserlabsDDSSystem::handleChannelPress(int chan, std::string configPath, RunInfo currentRunInfo)
{
	try {
		bool on = onButtons[chan]->isChecked();

		currentGuiInfo = getOutputInfo();
		core.setRunSettings(currentGuiInfo);

		bool ok = false;
		double freq = freqEdits[chan]->text().toDouble(&ok); // Already in MHz
		bool canProgramNow = ok;
		if (!ok) {
			try {
				Expression expr(str(freqEdits[chan]->text()));
				if (parentWin && parentWin->auxWin) {
					try {
						auto constants = parentWin->auxWin->getUsableConstants();
						expr.assertValid(constants, GLOBAL_PARAMETER_SCOPE);
						freq = expr.evaluate(constants, 0);
						canProgramNow = true;
					}
					catch (ChimeraError&) {
						canProgramNow = false;
					}
				}
			}
			catch (...) {
				canProgramNow = false;
			}
		}
		double amp = ampEdits[chan]->text().toDouble();
		double phase = phaseEdits[chan]->text().toDouble();

		if (on) {
			if (canProgramNow) {
				if (core.connected()) {
					core.programSingleToneNow(chan, freq, amp, phase);
					qDebug() << "Programmed DDS ch" << chan << ":" << freq << "MHz," << amp << "amp," << phase << "deg";
				}
				else {
					QMessageBox::warning(this, "Wieserlabs DDS", "DDS not connected. Settings were saved but not programmed.");
				}
			}
		}
	}
	catch (ChimeraError& err) {
		QMessageBox::warning(this, "Wieserlabs DDS Error", QString::fromStdString(err.trace()));
	}
}

void WieserlabsDDSSystem::handleModeCombo()
{
	bool scripting = (modeCombo->currentIndex() == 0);
	lastScriptingMode = scripting;
	wieserlabsDdsScript->setEnabled(scripting, false);
	refreshScriptedWaveform();
}

void WieserlabsDDSSystem::readGuiSettings()
{
	currentGuiInfo = getOutputInfo();
	core.setRunSettings(currentGuiInfo);
	refreshScriptedWaveform();
}

void WieserlabsDDSSystem::readGuiSettings(int chan)
{
	if (chan < 0 || chan >= 2) {
		return;
	}
	currentGuiInfo = getOutputInfo();
	core.setRunSettings(currentGuiInfo);
}

bool WieserlabsDDSSystem::scriptingModeIsSelected()
{
	return modeCombo->currentIndex() == 0;
}

bool WieserlabsDDSSystem::getSavedStatus()
{
	return saved;
}

void WieserlabsDDSSystem::updateSavedStatus(bool isSaved)
{
	saved = isSaved;
}

void WieserlabsDDSSystem::handleSavingConfig(ConfigStream& saveFile, std::string configPath, RunInfo info)
{
	// Save configuration
	saveFile << core.getDelim() + "\n";
	for (int chan = 0; chan < 2; chan++) {
		saveFile << "\nchannel_" << (chan + 1); // lower to align with ConfigStream lowercasing
		saveFile << "\n/*On:*/\t\t\t\t\t" << onButtons[chan]->isChecked();
		saveFile << "\n/*Frequency:*/\t\t\t\t" << freqEdits[chan]->text().toStdString();
		saveFile << "\n/*Amplitude:*/\t\t\t\t" << ampEdits[chan]->text().toStdString();
		saveFile << "\n/*Phase:*/\t\t\t\t\t" << phaseEdits[chan]->text().toStdString();
	}
	saveFile << "\n/*Control:*/\t\t\t\t" << ctrlButton->isChecked();
	// Save script address for auto-loading on next config open
	saveFile << "\n/*Script Address:*/\t\t\t" << wieserlabsDdsScript->getScriptPathAndName();
	saveFile << "\nEND_" + core.getDelim() << "\n"; // ensure newline so next section starts cleanly
}

std::string WieserlabsDDSSystem::getDeviceIdentity()
{
	return core.getDeviceIdentity();
}

void WieserlabsDDSSystem::handleOpenConfig(deviceOutputInfo settings)
{
	// Set UI from settings
	for (int chan = 0; chan < 2; chan++) {
		onButtons[chan]->setChecked(settings.snapshot[chan].on);
		if (!settings.snapshot[chan].frequencyExpression.expressionStr.empty()) {
			freqEdits[chan]->setText(qstr(settings.snapshot[chan].frequencyExpression.expressionStr));
		}
		else {
			freqEdits[chan]->setText(QString::number(settings.snapshot[chan].frequency)); // Config stores MHz
		}
		ampEdits[chan]->setText(QString::number(settings.snapshot[chan].amplitude));
		phaseEdits[chan]->setText(QString::number(settings.snapshot[chan].phase));
	}
	ctrlButton->setChecked(settings.wieserlabsControl);
}

void WieserlabsDDSSystem::updateSettingsDisplay(int chan, std::string configPath, RunInfo currentRunInfo)
{
	// Update display for specific channel
	chan -= 1; // zero-indexed
	if (chan >= 0 && chan < 2) {
		auto& channelInfo = currentGuiInfo.snapshot[chan];
		onButtons[chan]->setChecked(channelInfo.on);
		if (!channelInfo.frequencyExpression.expressionStr.empty()) {
			freqEdits[chan]->setText(qstr(channelInfo.frequencyExpression.expressionStr));
		}
		else {
			freqEdits[chan]->setText(QString::number(channelInfo.frequency)); // Config stores MHz
		}
		ampEdits[chan]->setText(QString::number(channelInfo.amplitude));
		phaseEdits[chan]->setText(QString::number(channelInfo.phase));
	}
	ctrlButton->setChecked(currentGuiInfo.wieserlabsControl);
}

void WieserlabsDDSSystem::updateSettingsDisplay(std::string configPath, RunInfo currentRunInfo)
{
	for (int chan = 1; chan <= 2; chan++) {
		updateSettingsDisplay(chan, configPath, currentRunInfo);
	}
}

void WieserlabsDDSSystem::refreshScriptedWaveform()
{
	if (!wieserlabsDdsScript) {
		qDebug() << "WieserlabsDDS: No script object exists";
		return;
	}

	if (!scriptingModeIsSelected()) {
		qDebug() << "WieserlabsDDS: Not in scripting mode, clearing waveform";
		scriptedWaveform = ScriptedWieserlabsDDSWaveform();
		core.setScriptedWaveform(scriptedWaveform);
		return;
	}

	std::string scriptText = wieserlabsDdsScript->getScriptText();
	qDebug() << "WieserlabsDDS: Script text length:" << scriptText.length();
	qDebug() << "WieserlabsDDS: Script text:" << QString::fromStdString(scriptText);

	ScriptStream stream(scriptText);
	ScriptedWieserlabsDDSWaveform parsedWaveform;
	std::vector<parameterType> params;
	if (parentWin && parentWin->auxWin) {
		try {
			params = parentWin->auxWin->getAllParams();
			if (!params.empty()) {
				auto rangeInfo = parentWin->auxWin->getConfigs().getRangeInfo();
				ParameterSystem::generateKey(params, false, rangeInfo);
			}
		}
		catch (ChimeraError&) {
			params.clear();
			qDebug() << "WieserlabsDDS: parameters unavailable while refreshing script waveform; parsing without parameters";
		}
	}
	std::string warnings;

	while (stream.peek() != EOF) {
		try {
			if (!parsedWaveform.analyzeWieserlabsDDSScriptCommand(stream, params, warnings)) {
				break;
			}
		}
		catch (ChimeraError& err) {
			warnings += "Exception while parsing DDS script: " + err.trace() + "\n";
			break;
		}
		catch (std::exception& err) {
			warnings += "Exception while parsing DDS script: " + std::string(err.what()) + "\n";
			break;
		}
		catch (...) {
			warnings += "Unknown exception while parsing DDS script\n";
			break;
		}
	}

	if (!warnings.empty()) {
		qDebug() << "Wieserlabs DDS script warnings:\n" << QString::fromStdString(warnings);
	}

	qDebug() << "WieserlabsDDS: Parsed" << parsedWaveform.getCommandList().size() << "commands";

	scriptedWaveform = parsedWaveform;
	core.setScriptedWaveform(scriptedWaveform);
}

deviceOutputInfo WieserlabsDDSSystem::getOutputInfo()
{
	deviceOutputInfo info;
	info.snapshot = std::vector<wieserlabsDdsChannel>(2);

	for (int chan = 0; chan < 2; chan++) {
		info.snapshot[chan].on = onButtons[chan]->isChecked();
		info.snapshot[chan].frequencyExpression.expressionStr = str(freqEdits[chan]->text());
		bool ok = false;
		info.snapshot[chan].frequency = freqEdits[chan]->text().toDouble(&ok);
		if (!ok) {
			if (chan < currentGuiInfo.snapshot.size()) {
				info.snapshot[chan].frequency = currentGuiInfo.snapshot[chan].frequency;
			}
		}
		info.snapshot[chan].amplitude = ampEdits[chan]->text().toDouble();
		info.snapshot[chan].phase = phaseEdits[chan]->text().toDouble();
	}
	info.wieserlabsControl = ctrlButton->isChecked();

	return info;
}

WieserlabsDDSCore& WieserlabsDDSSystem::getCore()
{
	return core;
}

void WieserlabsDDSSystem::setOutputSettings(deviceOutputInfo info)
{
	currentGuiInfo = info;
	core.setRunSettings(currentGuiInfo);
	// Immediately reflect loaded settings in the UI so the DDS window shows config values.
	updateSettingsDisplay("", RunInfo());
}