#pragma once
#include "IChimeraQtWindow.h"
#include "ExperimentThread/ExperimentThreadInput.h"
#include <RealTimeMOTAnalysis/MOTAnalysisSystem.h>
#include <ExperimentMonitoringAndStatus/ExperimentSeqPlotter.h>
#include "StaticDirectDigitalSynthesis/StaticDdsSystem.h" // Ensure the header file is included



namespace Ui {
	class QtAnalysisWindow;
}

class QtAnalysisWindow : public IChimeraQtWindow
{
	Q_OBJECT
public:
	explicit QtAnalysisWindow(QWidget* parent = nullptr);
	~QtAnalysisWindow() {};
	std::string getSystemStatusString();
	void windowOpenConfig(ConfigStream& configFile) override;
	void windowSaveConfig(ConfigStream& configFile) override;
	void fillExpDeviceList(DeviceList& list) override;
	void initializeWidgets() override;
	void ViewOrChangeStaticDdsNames();
	void fillMasterThreadInput(ExperimentThreadInput* input) override {};
	//MOTAnalysisControl
	StaticDdsSystem& getStaticDds() { return staticDds; };

public slots:
	void prepareCalcForAcq();

private:



public:
	MOTAnalysisSystem MOTAnalySys;
	ExperimentSeqPlotter SeqPlotter;
	StaticDdsSystem staticDds;

private:
	Ui::QtAnalysisWindow* ui;


};

