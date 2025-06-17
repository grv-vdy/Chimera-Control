#include "stdafx.h"
#include "ExperimentSeqPlotter.h"
#include <ExperimentThread/ExpThreadWorker.h>
#include <PrimaryWindows/IChimeraQtWindow.h>
#include <PrimaryWindows/QtAuxiliaryWindow.h>
#include <Plotting/QCustomPlotCtrl.h>
#include <AnalogOutput/AoStructures.h>
#include <DigitalOutput/DoStructures.h>


ExperimentSeqPlotter::ExperimentSeqPlotter(IChimeraQtWindow* parent)
	: QObject(parent) {}

void ExperimentSeqPlotter::initialize(IChimeraQtWindow* parent)
{
	int maxh = 1000 / (NUM_DAC_PLTS + NUM_TTL_PLTS);
	aoPlots.resize(NUM_DAC_PLTS);
	for (auto dacPltCount : range(aoPlots.size())) {
		std::string titleTxt;
		switch (dacPltCount) {
		case 0:
			titleTxt = "DAC 0";
			break;
		case 1:
			titleTxt = "DAC 1";
			break;
		}
		aoPlots[dacPltCount] = new QCustomPlotCtrl();
		aoPlots[dacPltCount]->setStyle(plotStyle::DacPlot);
		aoPlots[dacPltCount]->init(parent, qstr(titleTxt), size_t(AOGrid::total) / NUM_DAC_PLTS);
		aoPlots[dacPltCount]->plot->setMaximumHeight(maxh);
		aoPlots[dacPltCount]->plot->setMinimumSize(500, maxh / 2);
	}

	// ttl plots are similar to aoSys.
	ttlPlots.resize(NUM_TTL_PLTS);
	for (auto ttlPltCount : range(ttlPlots.size())) {
		// currently assuming 4 ttl plots...
		std::string titleTxt;
		switch (ttlPltCount) {
		case 0:
			titleTxt = "Ttls: 1-2";
			break;
		case 1:
			titleTxt = "Ttls: 3-4";
			break;
		case 2:
			titleTxt = "Ttls: 5-6";
			break;
		case 3:
			titleTxt = "Ttls: 7-8";
			break;
		}
		ttlPlots[ttlPltCount] = new QCustomPlotCtrl();
		ttlPlots[ttlPltCount]->setStyle(plotStyle::TtlPlot);
		ttlPlots[ttlPltCount]->init(parent, qstr(titleTxt), size_t(DOGrid::total) / NUM_TTL_PLTS);
		ttlPlots[ttlPltCount]->plot->setMaximumHeight(maxh);
		ttlPlots[ttlPltCount]->plot->setMinimumSize(500, maxh / 2);
	}
}

void ExperimentSeqPlotter::handleDoAoOlPlotData(
	const std::vector<std::vector<plotDataVec>>& doData,
	const std::vector<std::vector<plotDataVec>>& aoData)
{
	auto win = dynamic_cast<IChimeraQtWindow*>(parent());
	auto dacN = win->auxWin->getDacNames();
	auto ttlN = win->auxWin->getTtlNames();
	
	unsigned numTrace = size_t(DOGrid::total) / NUM_TTL_PLTS;
	for (auto ttlPlotNum : range(ttlPlots.size())) {
		ttlPlots[ttlPlotNum]->setData(doData[ttlPlotNum],
			std::vector<std::string>(ttlN.begin() + ttlPlotNum * numTrace,
				ttlN.begin() + (ttlPlotNum + 1) * numTrace));
	}
	numTrace = size_t(AOGrid::total) / NUM_DAC_PLTS;
	for (auto aoPlotNum : range(aoPlots.size())) {
		aoPlots[aoPlotNum]->setData(aoData[aoPlotNum],
			std::vector<std::string>(dacN.begin() + aoPlotNum * numTrace,
				dacN.begin() + (aoPlotNum + 1) * numTrace));
	}
}

