#include "stdafx.h"
#include "StaticDdsSystem.h"
#include <PrimaryWindows/IChimeraQtWindow.h>
#include <PrimaryWindows/QtMainWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <qpushbutton.h>
#include <qlayout.h>

StaticDdsSystem::StaticDdsSystem(IChimeraQtWindow* parent) :
	IChimeraSystem(parent),
	expActive(false),
	core(STATICDDS_SAFEMODE, STATICDDS_PORT, STATICDDS_BAUDRATE)
{
}

void StaticDdsSystem::initialize()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setMaximumWidth(600);

    QLabel* title = new QLabel("STATIC VALON DDS", this);
    layout->addWidget(title, 0);

    QHBoxLayout* layout1 = new QHBoxLayout();
    layout1->setContentsMargins(0, 0, 0, 0);

    auto programNowButton = new QPushButton("Program DDS Now", this);
    connect(programNowButton, &QPushButton::released, [this]() {
        try {
            handleProgramNowPress(parentWin->auxWin->getUsableConstants());
        }
        catch (ChimeraError& err) {
            parentWin->reportErr("Failed to program Static DDS system! \n" + err.qtrace());
        }
    });
    ctrlButton = new QCheckBox("Ctrl?", this);
    ctrlButton->setChecked(false);
    connect(ctrlButton, &QCheckBox::clicked, [this]() {
        try {
            updateCtrlEnable();
            parentWin->configUpdated();
        }
        catch (ChimeraError& err) {
            parentWin->reportErr(err.qtrace());
        }
    });

    layout1->addWidget(programNowButton, 0);
    layout1->addWidget(ctrlButton, 0);
    layout1->addStretch(1);
    layout->addLayout(layout1, 0);

    QGridLayout* layout2 = new QGridLayout();
    layout2->setContentsMargins(0, 0, 0, 0);
    layout2->addWidget(new QLabel(""), 0, 0); // Empty top-left cell for port label
    for (int ch = 0; ch < 2; ++ch) {
        QLabel* freqtitle = new QLabel("Frequency (MHz)", this);
        QLabel* leveltitle = new QLabel("Attenuation (dB)", this);
        layout2->addWidget(freqtitle, 0, ch * 2 + 1);
        layout2->addWidget(leveltitle, 0, ch * 2 + 2);
    }
    layout->addLayout(layout2);

    QGridLayout* layout3 = new QGridLayout();
    layout3->setContentsMargins(0, 0, 0, 0);

    for (auto port : range(size_t(StaticDDSGrid::total))) {
        auto strChan = qstr(core.getDeviceInfo());
        labels_port[port] = new QLabel("Port " + strChan + ":", this);
        for (auto ch : range(2)) {
            labels_channel[port][ch] = new QLabel(qstr(ch) + ":", this);
            edits_frequency[port][ch] = new QLineEdit(this);
            edits_level[port][ch] = new QLineEdit(this);
            edits_frequency[port][ch]->setText("0.0");
            edits_level[port][ch]->setText("0.0");
            connect(edits_frequency[port][ch], &QLineEdit::textChanged, [this]() { parentWin->configUpdated(); });
            connect(edits_level[port][ch], &QLineEdit::textChanged, [this]() { parentWin->configUpdated(); });
        }
    }
	
    for (auto port : range(size_t(StaticDDSGrid::total))) {
        QHBoxLayout* lay = new QHBoxLayout();
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(labels_port[port], 0);
        for (auto ch : range(2)) {
            lay->addWidget(labels_channel[port][ch], 0);
            lay->addWidget(edits_frequency[port][ch], 0);
            lay->addWidget(edits_level[port][ch], 0);
            lay->addStretch(1);
            layout3->addLayout(lay, ch / 4, ch % 4);
        }
    }
    layout->addLayout(layout3);
}

void StaticDdsSystem::handleOpenConfig(ConfigStream& configFile)
{
    auto configVals = core.getSettingsFromConfig(configFile);
    for (auto port : range(size_t(StaticDDSGrid::total))) {
        for (auto ch : range(size_t(StaticDDSGrid::total))) {
            edits_frequency[port][ch]->setText(qstr(configVals.staticDDSs[ch*port + ch][0].expressionStr));
            edits_level[port][ch]->setText(qstr(configVals.staticDDSs[ch*port + ch][1].expressionStr));
        }
    }
    ctrlButton->setChecked(configVals.ctrlDDS);
    updateCtrlEnable();
}

void StaticDdsSystem::handleSaveConfig(ConfigStream& configFile)
{
    configFile << core.configDelim;
    for (auto port : range(size_t(StaticDDSGrid::total))) {
        for (auto ch : range(2)) {
            configFile << "\n/* DDS-" + str(ch*port+ch) + " Frequency:*/\t\t" << Expression(str(edits_frequency[port][ch]->text()));
            configFile << "\n/* DDS-" + str(ch*port+ch) + " Level:*/\t\t" << Expression(str(edits_level[port][ch]->text()));
        }
    }
    configFile << "\n/*Control?*/\t\t\t" << ctrlButton->isChecked()
        << "\nEND_" + core.configDelim << "\n";
}

void StaticDdsSystem::updateCtrlEnable()
{
	auto ctrl = ctrlButton->isChecked();
	for (auto& e : edits_frequency) {
		e[0]->setEnabled(!ctrl);
        e[1]->setEnabled(!ctrl);
	}
    for (auto& e : edits_level) {
		e[0]->setEnabled(!ctrl);
        e[1]->setEnabled(!ctrl);
	}
}

void StaticDdsSystem::handleProgramNowPress(std::vector<parameterType> constants)
{
    StaticDDSSettings tmpSetting;
    for (auto port : range(size_t(StaticDDSGrid::total))) {
        for (auto ch : range(2)) {
            tmpSetting.staticDDSs[port * ch + ch][0].expressionStr = str(edits_frequency[port][ch]->text());
            tmpSetting.staticDDSs[port * ch + ch][1].expressionStr = str(edits_level[port][ch]->text());
        }
        tmpSetting.ctrlDDS = true;

        core.setStaticDDSExpSetting(tmpSetting);
        core.calculateVariations(constants, nullptr);
        core.programVariation(0, constants, nullptr);

        emit notification("Finished programming Static DDS system!\n", 0);
    }
}

std::string StaticDdsSystem::getDeviceInfo()
{
    return core.getDeviceInfo();
}

void StaticDdsSystem::setDdsEditFrequencyValue(std::string ddsfreq, unsigned channel, unsigned port)
{
    if (port >= size_t(StaticDDSGrid::total)) {
        thrower("Port " + str(port) + " outside range of static DDS " + str(size_t(StaticDDSGrid::total)));
    }
	if (channel >= size_t(2)) {
		thrower("Channel " + str(channel) + " outside range of static DDS " + str(size_t(StaticDDSGrid::total)));
    }
	edits_frequency[port][channel]->setText(qstr(ddsfreq));
}

void StaticDdsSystem::setDdsEditLevelValue(std::string ddsfreq, unsigned channel, unsigned port)
{
    if (port >= size_t(StaticDDSGrid::total)) {
        thrower("Port " + str(port) + " outside range of static DDS " + str(size_t(StaticDDSGrid::total)));
    }
	if (channel >= size_t(2)) {
		thrower("Channel " + str(channel) + " outside range of static DDS " + str(size_t(StaticDDSGrid::total)));
    }
	edits_level[port][channel]->setText(qstr(ddsfreq));
}