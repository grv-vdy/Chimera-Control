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
	if (!core.connected()) {
		QMessageBox::warning(this, "Wieserlabs DDS", "DDS not connected!");
		return;
	}

	try {
		bool on = onButtons[chan]->isChecked();
		double freq = freqEdits[chan]->text().toDouble() * 1e6; // Convert MHz to Hz
		double amp = ampEdits[chan]->text().toDouble();
		double phase = phaseEdits[chan]->text().toDouble();

		if (on) {
			core.programSingleToneNow(chan, freq, amp, phase);
		}
	}
	catch (ChimeraError& err) {
		QMessageBox::warning(this, "Wieserlabs DDS Error", QString::fromStdString(err.trace()));
	}
}

void WieserlabsDDSSystem::handleModeCombo()
{
	bool scripting = (modeCombo->currentIndex() == 0);
	wieserlabsDdsScript->setEnabled(scripting, false);
}

void WieserlabsDDSSystem::readGuiSettings()
{
	for (int chan = 0; chan < 2; chan++) {
		readGuiSettings(chan);
	}
}

void WieserlabsDDSSystem::readGuiSettings(int chan)
{
	// Read GUI settings for channel
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
		freqEdits[chan]->setText(QString::number(settings.snapshot[chan].frequency / 1e6)); // Convert Hz to MHz
		ampEdits[chan]->setText(QString::number(settings.snapshot[chan].amplitude));
		phaseEdits[chan]->setText(QString::number(settings.snapshot[chan].phase));
	}
}

void WieserlabsDDSSystem::updateSettingsDisplay(int chan, std::string configPath, RunInfo currentRunInfo)
{
	// Update display for specific channel
	chan -= 1; // zero-indexed
	if (chan >= 0 && chan < 2) {
		auto& channelInfo = currentGuiInfo.snapshot[chan];
		onButtons[chan]->setChecked(channelInfo.on);
		freqEdits[chan]->setText(QString::number(channelInfo.frequency / 1e6));
		ampEdits[chan]->setText(QString::number(channelInfo.amplitude));
		phaseEdits[chan]->setText(QString::number(channelInfo.phase));
	}
}

void WieserlabsDDSSystem::updateSettingsDisplay(std::string configPath, RunInfo currentRunInfo)
{
	for (int chan = 1; chan <= 2; chan++) {
		updateSettingsDisplay(chan, configPath, currentRunInfo);
	}
}

deviceOutputInfo WieserlabsDDSSystem::getOutputInfo()
{
	deviceOutputInfo info;
	info.snapshot = std::vector<wieserlabsDdsChannel>(2);

	for (int chan = 0; chan < 2; chan++) {
		info.snapshot[chan].on = onButtons[chan]->isChecked();
		info.snapshot[chan].frequency = freqEdits[chan]->text().toDouble() * 1e6;
		info.snapshot[chan].amplitude = ampEdits[chan]->text().toDouble();
		info.snapshot[chan].phase = phaseEdits[chan]->text().toDouble();
	}

	return info;
}

WieserlabsDDSCore& WieserlabsDDSSystem::getCore()
{
	return core;
}

void WieserlabsDDSSystem::setOutputSettings(deviceOutputInfo info)
{
	currentGuiInfo = info;
	// Immediately reflect loaded settings in the UI so the DDS window shows config values.
	updateSettingsDisplay("", RunInfo());
}