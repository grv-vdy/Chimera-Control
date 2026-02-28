#include "stdafx.h"
#include "QtAnalysisWindow.h"
#include <PrimaryWindows/QtScriptWindow.h>
#include <PrimaryWindows/QtAndorWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <PrimaryWindows/QtMainWindow.h>
#include <ExperimentMonitoringAndStatus/colorbox.h>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>


QtAnalysisWindow::QtAnalysisWindow(QWidget* parent) 
	: IChimeraQtWindow(parent)
	, MOTAnalySys(this)
	, SeqPlotter(this)
	, staticDds(this)
{
	setWindowTitle("Analysis Window");
}


std::string QtAnalysisWindow::getSystemStatusString()
{
	std::string msg;
	msg += "Static PLL System:\n";
	if (!STATICDDS_SAFEMODE) { // Check the first device, or loop and check all if needed
		for (size_t i = 0; i < STATICDDS_PORT.size(); ++i) {
			msg += "\tStatic PLL System " + str(i) + " is Active at port " + STATICDDS_PORT[i]
				+ ", with baudrate " + str(STATICDDS_BAUDRATE[i]) + "\n";
			/*msg += "\t" + staticDds.getDeviceInfo(i) + "\n";*/
		}
		
	}
	else {
		msg += "\tStatic PLL System is disabled! Enable in \"constants.h\"\n";
	}
	return msg;
}

void QtAnalysisWindow::windowOpenConfig(ConfigStream& configFile)
{
	try {
		ConfigSystem::standardOpenConfig(configFile, staticDds.getConfigDelim(), &staticDds);
	}
	catch (ChimeraError&) {
		throwNested("Analysis Window failed to read parameters from the configuration file.");
	}
}

void QtAnalysisWindow::windowSaveConfig(ConfigStream& configFile)
{
	staticDds.handleSaveConfig(configFile);
}

void QtAnalysisWindow::fillExpDeviceList(DeviceList& list)
{
	list.list.push_back(staticDds.getCore());
}

void QtAnalysisWindow::initializeWidgets()
{
	statBox = new ColorBox(this, mainWin->getDevices());
	QWidget* centralWidget = new QWidget(this);
	setCentralWidget(centralWidget);

	QHBoxLayout* layout = new QHBoxLayout(centralWidget);
	QVBoxLayout* layoutMOT = new QVBoxLayout(this);
	layoutMOT->setContentsMargins(0, 0, 0, 0);
	MOTAnalySys.initialize();
	for (auto& p : MOTAnalySys.MOTCalcCtrl) {
		layoutMOT->addWidget(&p, 0);
	}
	layoutMOT->addStretch(1);
	layout->addLayout(layoutMOT);

	SeqPlotter.initialize(this);
	QVBoxLayout* layoutSeq = new QVBoxLayout(this);
	layoutSeq->setContentsMargins(0, 0, 0, 0);
	for (auto* p : SeqPlotter.aoPlots) {
		layoutSeq->addWidget(p->plot);
	}
	for (auto* p : SeqPlotter.ttlPlots) {
		layoutSeq->addWidget(p->plot);
	}
	layout->addLayout(layoutSeq);

	QVBoxLayout* layoutAux = new QVBoxLayout(this);
	layoutAux->setContentsMargins(0, 0, 0, 0);
	staticDds.initialize();
	layoutAux->addWidget(&staticDds);

	layoutAux->addStretch(1);

	layout->addLayout(layoutAux);
}

void QtAnalysisWindow::prepareCalcForAcq()
{
	MOTAnalySys.prepareMOTAnalysis();
}

void QtAnalysisWindow::ViewOrChangeStaticDdsNames()
{
	mainWin->updateConfigurationSavedStatus(false);

	auto* dialog = new QDialog(this);
	dialog->setModal(false);
	dialog->setWindowTitle("Static Valon Channel Names");
	dialog->setStyleSheet(chimeraStyleSheets::stdStyleSheet());

	auto names = staticDds.getChannelNames();
	std::array<QLineEdit*, size_t(StaticDDSGrid::total)> edits;

	QVBoxLayout* outerLayout = new QVBoxLayout(dialog);
	QGridLayout* grid = new QGridLayout();
	for (auto port : range(size_t(StaticDDSGrid::numOFunit))) {
		grid->addWidget(new QLabel("PLL " + qstr(str(port))), int(port), 0);
		for (auto ch : range(size_t(StaticDDSGrid::numPERunit))) {
			auto idx = port * size_t(StaticDDSGrid::numPERunit) + ch;
			grid->addWidget(new QLabel("Ch " + qstr(str(ch))), int(port), int(ch * 2 + 1));
			edits[idx] = new QLineEdit(qstr(names[idx]));
			grid->addWidget(edits[idx], int(port), int(ch * 2 + 2));
		}
	}
	outerLayout->addLayout(grid);

	QHBoxLayout* buttons = new QHBoxLayout();
	auto* okBtn = new QPushButton("OK", dialog);
	auto* cancelBtn = new QPushButton("Cancel", dialog);
	buttons->addStretch(1);
	buttons->addWidget(okBtn);
	buttons->addWidget(cancelBtn);
	outerLayout->addLayout(buttons);

	connect(okBtn, &QPushButton::released, [this, dialog, edits]() {
		std::array<std::string, size_t(StaticDDSGrid::total)> newNames;
		for (auto idx : range(size_t(StaticDDSGrid::total))) {
			auto txt = edits[idx]->text().trimmed();
			if (txt.isEmpty()) {
				txt = "dds" + qstr(str(idx));
			}
			if (txt[0].isDigit()) {
				errBox("ERROR: " + str(txt) + " is an invalid name; names cannot start with numbers.");
				return;
			}
			newNames[idx] = str(txt);
		}
		staticDds.setChannelNames(newNames);
		dialog->close();
		dialog->deleteLater();
	});

	connect(cancelBtn, &QPushButton::released, [dialog]() {
		dialog->close();
		dialog->deleteLater();
	});

	dialog->show();
}