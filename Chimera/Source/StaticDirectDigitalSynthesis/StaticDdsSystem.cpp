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
    for (auto idx : range(size_t(StaticDDSGrid::total))) {
        channelNames[idx] = "dds" + str(idx);
    }
}

void StaticDdsSystem::initialize()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setMaximumWidth(760);

    QLabel* title = new QLabel("STATIC VALON PLL", this);
    layout->addWidget(title, 0);

    QHBoxLayout* layout1 = new QHBoxLayout();
    layout1->setContentsMargins(0, 0, 0, 0);

    auto programNowButton = new QPushButton("Program PLL Now", this);
    connect(programNowButton, &QPushButton::released, [this]() {
        try 
        {
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
        QLabel* leveltitle = new QLabel("Level (dBm)", this);
        layout2->addWidget(freqtitle, 0, ch * 2 + 1);
        layout2->addWidget(leveltitle, 0, ch * 2 + 2);
    }
    layout->addLayout(layout2);

    QGridLayout* layout3 = new QGridLayout();
    layout3->setSpacing(10); // Add vertical spacing between port rows
    layout3->setContentsMargins(0, 0, 0, 0);

    for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
        auto strChan = qstr(core.getDeviceInfo(port));
        labels_port[port] = new QLabel("Port " + strChan + ":", this);
        for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
            labels_channel[port][ch] = new QLabel("", this);
            edits_frequency[port][ch] = new QLineEdit(this);
            edits_level[port][ch] = new QLineEdit(this);
            edits_frequency[port][ch]->setText("0.0");
            edits_level[port][ch]->setText("0.0");
            connect(edits_frequency[port][ch], &QLineEdit::textChanged, [this]() { parentWin->configUpdated(); });
            connect(edits_level[port][ch], &QLineEdit::textChanged, [this]() { parentWin->configUpdated(); });
        }
    }

    for (size_t port = 0; port < STATICDDS_NUMBER; ++port) {
        QHBoxLayout* lay = new QHBoxLayout();
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(labels_port[port], 0);
        for (int ch = 0; ch < size_t(StaticDDSGrid::numPERunit); ++ch) {
            lay->addWidget(labels_channel[port][ch], 0);
            lay->addWidget(edits_frequency[port][ch], 0);
            lay->addWidget(edits_level[port][ch], 0);
        }
        lay->addStretch(1);
        layout3->addLayout(lay, port, 0);
    }
    refreshChannelLabels();
    layout->addLayout(layout3);
}

void StaticDdsSystem::handleOpenConfig(ConfigStream& configFile)
{
    auto configVals = core.getSettingsFromConfig(configFile);
    channelNames = configVals.channelNames;
    refreshChannelLabels();
    for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
        for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
            edits_frequency[port][ch]->setText(qstr(configVals.staticDDSs[port*size_t(StaticDDSGrid::numPERunit) + ch][0].expressionStr));
            edits_level[port][ch]->setText(qstr(configVals.staticDDSs[port*size_t(StaticDDSGrid::numPERunit) + ch][1].expressionStr));
        }
    }
    ctrlButton->setChecked(configVals.ctrlDDS);
    updateCtrlEnable();
}

void StaticDdsSystem::handleSaveConfig(ConfigStream& configFile)
{
    configFile << core.configDelim;
    configFile << "\n/* DDS Name:*/ ";
    for (auto idx : range(size_t(StaticDDSGrid::total))) {
        configFile << channelNames[idx] << " ";
    }
    for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
        for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
            configFile << "\n/* DDS-" + str(port*size_t(StaticDDSGrid::numPERunit)+ch) + " Frequency:*/\t\t" << Expression(str(edits_frequency[port][ch]->text()));
            configFile << "\n/* DDS-" + str(port*size_t(StaticDDSGrid::numPERunit)+ch) + " Level:*/\t\t" << Expression(str(edits_level[port][ch]->text()));
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
    for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
        for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
            tmpSetting.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][0].expressionStr = str(edits_frequency[port][ch]->text());
            tmpSetting.staticDDSs[port*size_t(StaticDDSGrid::numPERunit)+ch][1].expressionStr = str(edits_level[port][ch]->text());
        }
        tmpSetting.ctrlDDS = true;

    }
    core.setStaticDDSExpSetting(tmpSetting);
    core.calculateVariations(constants, nullptr);
    core.programVariation(0, constants, nullptr);
    emit notification("Finished programming Static PLL system!\n", 0);
}

std::string StaticDdsSystem::getDeviceInfo(unsigned int port)
{
    return core.getDeviceInfo(port);
}

void StaticDdsSystem::setChannelNames(const std::array<std::string, size_t(StaticDDSGrid::total)>& namesIn)
{
    channelNames = namesIn;
    refreshChannelLabels();
}

void StaticDdsSystem::refreshChannelLabels()
{
    for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
        for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
            auto idx = port * size_t(StaticDDSGrid::numPERunit) + ch;
            if (labels_channel[port][ch] != nullptr) {
                labels_channel[port][ch]->setText(qstr(ch) + " (" + qstr(channelNames[idx]) + "):");
            }
        }
    }
}

void StaticDdsSystem::setDdsEditFrequencyValue(std::string ddsfreq, unsigned channel, unsigned port)
{
    if (port >= size_t(StaticDDSGrid::numOFunit)) {
        thrower("Port " + str(port) + " outside range of static PLL " + str(size_t(StaticDDSGrid::numOFunit)));
    }
	if (channel >= size_t(StaticDDSGrid::numPERunit)) {
		thrower("Channel " + str(channel) + " outside range of static PLL " + str(size_t(StaticDDSGrid::numPERunit)));
    }
	edits_frequency[port][channel]->setText(qstr(ddsfreq));
}

void StaticDdsSystem::setDdsEditLevelValue(std::string ddsfreq, unsigned channel, unsigned port)
{
    if (port >= size_t(StaticDDSGrid::numOFunit)) {
        thrower("Port " + str(port) + " outside range of static PLL " + str(size_t(StaticDDSGrid::numOFunit)));
    }
	if (channel >= size_t(StaticDDSGrid::numPERunit)) {
		thrower("Channel " + str(channel) + " outside range of static PLL " + str(size_t(StaticDDSGrid::numPERunit)));
    }
	edits_level[port][channel]->setText(qstr(ddsfreq));
}