#include "stdafx.h"
#include "StaticDdsSystem.h"
#include <PrimaryWindows/IChimeraQtWindow.h>
#include <PrimaryWindows/QtMainWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>

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
    // Per-channel sweep buttons are created below next to each channel's edits

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
            // create per-channel sweep toggle button (press to configure/start, press again to stop)
            sweepButtons[port][ch] = new QPushButton("Sweep", this);
            // Ensure the button behaves as a persistent toggle and is visibly different when checked
            sweepButtons[port][ch]->setCheckable(true);
            sweepButtons[port][ch]->setChecked(false);
            sweepButtons[port][ch]->setAutoDefault(false);
            sweepButtons[port][ch]->setDefault(false);
            sweepButtons[port][ch]->setStyleSheet("QPushButton:checked { background-color: #d9534f; color: white; }");
            edits_frequency[port][ch]->setText("0.0");
            edits_level[port][ch]->setText("0.0");
            connect(edits_frequency[port][ch], &QLineEdit::textChanged, [this]() { parentWin->configUpdated(); });
            connect(edits_level[port][ch], &QLineEdit::textChanged, [this]() { parentWin->configUpdated(); });

            // connect sweep button: toggled ON -> open dialog and start native sweep; toggled OFF -> stop sweep
            connect(sweepButtons[port][ch], &QPushButton::toggled, [this, port, ch](bool checked) {
                if (!checked) {
                    // stop sweep on this channel
                    try {
                        core.stopFrequencySweep(port, ch);
                    }
                    catch (...) {}
                    sweepButtons[port][ch]->setText("Sweep");
                    return;
                }

                // when toggled on, open dialog to configure and start
                QDialog dlg(this);
                dlg.setWindowTitle("Frequency sweep parameters (port " + QString::number(port) + ", ch " + QString::number(ch) + ")");
                QFormLayout* form = new QFormLayout(&dlg);

                QLineEdit* startEdit = new QLineEdit(&dlg);
                QLineEdit* stopEdit = new QLineEdit(&dlg);
                QLineEdit* timeEdit = new QLineEdit(&dlg);

                // populate with previously set values if any
                startEdit->setText(QString::number(sweepStartFreqMHz[port][ch]));
                stopEdit->setText(QString::number(sweepStopFreqMHz[port][ch]));
                timeEdit->setText(QString::number(sweepTimeSec[port][ch] > 0 ? sweepTimeSec[port][ch] : 1.0));

                form->addRow("Start frequency (MHz):", startEdit);
                form->addRow("Stop frequency (MHz):", stopEdit);
                form->addRow("Sweep time (s):", timeEdit);

                QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
                form->addRow(buttonBox);

                connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
                connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

                if (dlg.exec() == QDialog::Accepted) {
                    bool ok1 = false, ok2 = false, ok3 = false;
                    double s = startEdit->text().toDouble(&ok1);
                    double e = stopEdit->text().toDouble(&ok2);
                    double t = timeEdit->text().toDouble(&ok3);
                    if (ok1 && ok2 && ok3 && t > 0.0) {
                        sweepStartFreqMHz[port][ch] = s;
                        sweepStopFreqMHz[port][ch] = e;
                        sweepTimeSec[port][ch] = t;
                        parentWin->configUpdated();
                        // start native sweep now
                        try {
                            core.startFrequencySweep(port, ch, s, e, t, 0);
                            sweepButtons[port][ch]->setText("Stop");
                        }
                        catch (ChimeraError& err) {
                            parentWin->reportErr("Failed to start sweep: " + err.qtrace());
                            // revert toggle
                            sweepButtons[port][ch]->setChecked(false);
                            sweepButtons[port][ch]->setText("Sweep");
                        }
                    }
                    else {
                        // invalid input: clear stored values and revert toggle
                        sweepStartFreqMHz[port][ch] = 0.0;
                        sweepStopFreqMHz[port][ch] = 0.0;
                        sweepTimeSec[port][ch] = 0.0;
                        sweepButtons[port][ch]->setChecked(false);
                        sweepButtons[port][ch]->setText("Sweep");
                    }
                }
                else {
                    // user cancelled: revert toggle state
                    sweepButtons[port][ch]->setChecked(false);
                    sweepButtons[port][ch]->setText("Sweep");
                }
            });
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
            lay->addWidget(sweepButtons[port][ch], 0);
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
    // Start device-native sweeps for any channel that has sweep parameters set
    for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
        for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
            double s = sweepStartFreqMHz[port][ch];
            double e = sweepStopFreqMHz[port][ch];
            double t = sweepTimeSec[port][ch];
            if (t > 0.0 && fabs(e - s) > 1e-12) {
                try {
                    // ensure any previous sweep on this channel is halted first
                    core.stopFrequencySweep(port, ch);
                }
                catch (...) {}
                try {
                    core.startFrequencySweep(port, ch, s, e, t, 0);
                }
                catch (ChimeraError& err) {
                    parentWin->reportErr("Failed to start sweep on port " + qstr(str(port)) + " ch " + qstr(str(ch)) + ": " + err.qtrace());
                }
            }
        }
    }
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