#pragma once
#include <Plotting/PlotInfo.h>
#include <qobject.h>
class IChimeraQtWindow;
class QCustomPlotCtrl;

class ExperimentSeqPlotter : public QObject
{
	Q_OBJECT
	// A small wrapper of the sequence plot
public:
	ExperimentSeqPlotter(IChimeraQtWindow* parent);
	ExperimentSeqPlotter(const ExperimentSeqPlotter&) = delete;
	void initialize(IChimeraQtWindow* parent);
	void handleDoAoOlPlotData(
		const std::vector<std::vector<plotDataVec>>& doData, 
		const std::vector<std::vector<plotDataVec>>& aoData);
	static const unsigned NUM_DAC_PLTS = 2;
	static const unsigned NUM_TTL_PLTS = 4;
private:

public:
	std::vector<QCustomPlotCtrl*> aoPlots;
	std::vector<QCustomPlotCtrl*> ttlPlots;

public slots:


};

