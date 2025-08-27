#include "stdafx.h"
#include "QtAnalysisWindow.h"
#include <PrimaryWindows/QtScriptWindow.h>
#include <PrimaryWindows/QtAndorWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <PrimaryWindows/QtMainWindow.h>
#include <ExperimentMonitoringAndStatus/colorbox.h>


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
	msg += "Static DDS System:\n";
	if (!STATICDDS_SAFEMODE) { // Check the first device, or loop and check all if needed
		for (size_t i = 0; i < STATICDDS_PORT.size(); ++i) {
			msg += "\tStatic DDS System " + str(i) + " is Active at port " + STATICDDS_PORT[i]
				+ ", with baudrate " + str(STATICDDS_BAUDRATE[i]) + "\n";
			/*msg += "\t" + staticDds.getDeviceInfo(i) + "\n";*/
		}
		
	}
	else {
		msg += "\tStatic DDS System is disabled! Enable in \"constants.h\"\n";
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