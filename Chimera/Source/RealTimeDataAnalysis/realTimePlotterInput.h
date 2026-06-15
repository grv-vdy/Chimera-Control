// created by Mark O. Brown
#pragma once
#include "atomGrid.h"
#include "GeneralImaging/imageParameters.h"
#include "Plotting/tinyPlotInfo.h"
#include <GeneralObjects/Queues.h>
#include <Plotting/dataPoint.h>
#include <atomic>
#include <vector>
#include <mutex>

class IChimeraQtWindow;
class AnalysisThreadWorker;

struct realTimePlotterInput{
	realTimePlotterInput ( ) { }
	IChimeraQtWindow* plotParentWindow;

	std::vector<tinyPlotInfo> plotInfo;
	std::vector<atomGrid> grids;
	imageParameters imageShape;

	unsigned picsPerVariation;
	unsigned picsPerRep;
	unsigned variations;

	unsigned alertThreshold;
	bool wantAtomAlerts;

	bool needsCounts;
};


