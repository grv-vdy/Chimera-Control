#define _WINSOCKAPI_
#include "stdafx.h"
#include <WinSock2.h>
#include "WieserlabsDDSSystem.h"
#include "WieserlabsDDSStructures.h"
#include "ConfigurationSystems/ConfigSystem.h"
#include "PrimaryWindows/IChimeraQtWindow.h"
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
		chanLayout->addWidget(new QLabel("Freq (Hz):", this));
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

	connect(modeCombo, qOverload<int>(&CQComboBox::currentIndexChanged), this, &WeiserlabsDDSSystem::handleModeCombo);

	for (int chan = 0; chan < 2; chan++) {
		connect(channelButtons[chan], &QPushButton::clicked, [this, chan]() {
			handleChannelPress(chan, "", RunInfo());
		});
	}
}

void WeiserlabsDDSSystem::updateButtonDisplay(int chan)
{
	// Update button display if needed
}

void WeiserlabsDDSSystem::checkSave(std::string configPath, RunInfo info)
{
	// Check if settings need saving
}

void WeiserlabsDDSSystem::handleChannelPress(int chan, std::string configPath, RunInfo currentRunInfo)
{
	if (!core.connected()) {
		QMessageBox::warning(this, "Weiserlabs DDS", "DDS not connected!");
		return;
	}

	try {
		bool on = onButtons[chan]->isChecked();
		double freq = freqEdits[chan]->text().toDouble();
		double amp = ampEdits[chan]->text().toDouble();
		double phase = phaseEdits[chan]->text().toDouble();

		if (on) {
			core.programSingleToneNow(chan, freq, amp, phase);
		}
	}
	catch (ChimeraError& err) {
		QMessageBox::warning(this, "Weiserlabs DDS Error", QString::fromStdString(err.trace()));
	}
}

void WeiserlabsDDSSystem::handleModeCombo()
{
	bool scripting = (modeCombo->currentIndex() == 0);
	wieserlabsDdsScript->setEnabled(scripting, false);
}

void WeiserlabsDDSSystem::readGuiSettings()
{
	for (int chan = 0; chan < 2; chan++) {
		readGuiSettings(chan);
	}
}

void WeiserlabsDDSSystem::readGuiSettings(int chan)
{
	// Read GUI settings for channel
}

bool WeiserlabsDDSSystem::scriptingModeIsSelected()
{
	return modeCombo->currentIndex() == 0;
}

bool WeiserlabsDDSSystem::getSavedStatus()
{
	return saved;
}

void WeiserlabsDDSSystem::updateSavedStatus(bool isSaved)
{
	saved = isSaved;
}

void WeiserlabsDDSSystem::handleSavingConfig(ConfigStream& saveFile, std::string configPath, RunInfo info)
{
	saveFile << core.getDelim() << "\n";
	
	// Save channel settings - format must match getSettingsFromConfig
	auto channelInfo = getOutputInfo();
	for (unsigned chan = 0; chan < channelInfo.snapshot.size(); chan++) {
		saveFile << "CHANNEL_" << (chan + 1);
		saveFile << "\n/*On:*/\t\t\t\t\t\t" << channelInfo.snapshot[chan].on;
		saveFile << "\n/*Frequency (MHz):*/\t\t\t" << channelInfo.snapshot[chan].frequency;
		saveFile << "\n/*Amplitude (0-1):*/\t\t\t" << channelInfo.snapshot[chan].amplitude;
		saveFile << "\n/*Phase (deg):*/\t\t\t\t" << channelInfo.snapshot[chan].phase;
	}
	
	// Save script address for future auto-loading
	saveFile << "\n/*Script Address:*/\t\t\t" << wieserlabsDdsScript->getScriptPathAndName(configPath, info);
	
	saveFile << "\nEND_" << core.getDelim() << "\n";
}

std::string WeiserlabsDDSSystem::getDeviceIdentity()
{
	return core.getDeviceIdentity();
}

void WeiserlabsDDSSystem::handleOpenConfig(deviceOutputInfo settings)
{
	// Settings are applied via setOutputSettings in QtScriptWindow
	// This method can be used for additional processing if needed
}

void WeiserlabsDDSSystem::updateSettingsDisplay(int chan, std::string configPath, RunInfo currentRunInfo)
{
	// Update display for specific channel
	chan -= 1; // zero-indexed
	if (chan >= 0 && chan < 2) {
		auto& channelInfo = currentGuiInfo.snapshot[chan];
		onButtons[chan]->setChecked(channelInfo.on);
		freqEdits[chan]->setText(QString::number(channelInfo.frequency));
		ampEdits[chan]->setText(QString::number(channelInfo.amplitude));
		phaseEdits[chan]->setText(QString::number(channelInfo.phase));
	}
}

void WeiserlabsDDSSystem::updateSettingsDisplay(std::string configPath, RunInfo currentRunInfo)
{
	for (int chan = 1; chan <= 2; chan++) {
		updateSettingsDisplay(chan, configPath, currentRunInfo);
	}
}

deviceOutputInfo WeiserlabsDDSSystem::getOutputInfo()
{
	deviceOutputInfo info;
	info.snapshot = std::vector<weiserlabsDdsChannel>(2);

	for (int chan = 0; chan < 2; chan++) {
		info.snapshot[chan].on = onButtons[chan]->isChecked();
		info.snapshot[chan].frequency = freqEdits[chan]->text().toDouble();
		info.snapshot[chan].amplitude = ampEdits[chan]->text().toDouble();
		info.snapshot[chan].phase = phaseEdits[chan]->text().toDouble();
	}

	return info;
}

WeiserlabsDDSCore& WeiserlabsDDSSystem::getCore()
{
	return core;
}

void WeiserlabsDDSSystem::setOutputSettings(deviceOutputInfo info)
{
	currentGuiInfo = info;
}