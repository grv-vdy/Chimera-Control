#pragma once
#include <PrimaryWindows/QtMainWindow.h>
#include <QTimer>
#include "ConfigurationSystems/ProfileIndicator.h"
#include "ConfigurationSystems/profileSettings.h"
#include "ExperimentThread/ExperimentThreadInput.h"
#include "IChimeraQtWindow.h"
#include "../Hamamatsu/HamamatsuCameraSettingsControl.h"
#include "Hamamatsu/HamamatsuRunSettings.h"
#include "ExperimentMonitoringAndStatus/ColorBox.h"
#include "GeneralImaging/PictureStats.h"
#include "GeneralImaging/PictureManager.h"
#include "ExperimentMonitoringAndStatus/AlertSystem.h"
#include "RealTimeDataAnalysis/DataAnalysisControl.h"
#include "ExperimentMonitoringAndStatus/ExperimentTimer.h"
#include "DataLogging/DataLogger.h"
#include "GeneralUtilityFunctions/commonFunctions.h"
#include "ExternalController/CommandModulator.h"
#include "Rearrangement/atomCruncherInput.h"
#include "GeneralObjects/commonTypes.h"
#include "GeneralObjects/Queues.h"
#include <Python/NewPythonHandler.h>
#include <bitset>
 
class AnalysisThreadWorker;
class CruncherThreadWorker;
class HamamatsuCameraThreadImageGrabber;
 
namespace Ui {
    class QtHamamatsuWindow;
}
 
class QtHamamatsuWindow : public IChimeraQtWindow{
    Q_OBJECT
 
    public:
        explicit QtHamamatsuWindow (QWidget* parent= nullptr);
        ~QtHamamatsuWindow ();
 
        void initializeWidgets ();
 
        void handleBumpAnalysis (profileSettings finishedProfile);
        void wakeRearranger ();
        void readImageParameters ();
        void passSetTemperaturePress ();
 
        void setDataType (std::string dataType);
        dataPoint getMainAnalysisResult ();
        void checkCameraIdle ();
        //void handleEmGainChange ();
        void fillMasterThreadInput (ExperimentThreadInput* input) override;
        DataLogger& getLogger ();
        std::string getSystemStatusString ();
        void windowSaveConfig (ConfigStream& saveFile);
        void windowOpenConfig (ConfigStream& configFile);
 
        void handleMasterConfigSave (std::stringstream& configStream);
        void handleMasterConfigOpen (ConfigStream& configStream);
        void handleNormalFinish (profileSettings finishedProfile);
        void redrawPictures (bool andGrid);
        bool getCameraStatus ();
        void setTimerText (std::string timerText);
        void armCameraWindow (HamamatsuRunSettings* settings);
        int getDataCalNum ();
        std::string getStartMessage ();
        void handlePictureSettings ();
        bool cameraIsRunning ();
        void abortCameraRun (bool askDelete = true);
        void assertOff ();
        void assertDataFileClosed ();
        void prepareAtomCruncher (AllExperimentInput& input);
        void writeVolts (unsigned currentVoltNumber, std::vector<float64> data);
        bool wantsAutoPause ();
        std::atomic<bool>* getSkipNextAtomic ();
        void stopSound ();
        void handleImageDimsEdit ();
        void loadCameraCalSettings (AllExperimentInput& input);
        bool wasJustCalibrated ();
        bool wantsAutoCal ();
        bool wantsNoMotAlert ();
        unsigned getNoMotThreshold ();
        atomGrid getMainAtomGrid ();
        std::string getMostRecentDateString ();
        int getMostRecentFid ();
        int getPicsPerRep ();
        bool wantsThresholdAnalysis ();
        HamamatsuCameraCore& getCamera ();
        void cleanUpAfterExp ();
        void handlePlotPop (unsigned id);
        // purely for getting rid of the bugs of resizing the plotter, only need it once after andor window is activated in QtMainWindow
        void refreshPics();
        void displayAnalysisGrid(atomGrid grids);
        void removeAnalysisGrid();
 
        void fillExpDeviceList (DeviceList& list);
        CruncherThreadWorker* atomCruncherWorker = nullptr;
        AnalysisThreadWorker* analysisThreadWorker = nullptr;
        HamamatsuCameraThreadImageGrabber* imageGrabberWorker = nullptr;
        void manualArmCamera ();
        // Starts the per-run image-grabber thread that pulls frames out of the camera and drives
        // onCameraProgress (display + save). Call right after the camera is armed.
        void startImageGrabberThread ();
        NewPythonHandler* getPython ();
 
        // for programming the camera setting before the experiment run so that the settings are the settings that read back from andor
        void manualProgramCameraSetting();
 
        friend void commonFunctions::handleCommonMessage(int msgID, IChimeraQtWindow* win);
        friend class CommandModulator;

        

    private:
        Ui::QtHamamatsuWindow* ui;
 
        bool justCalibrated = false;

        HamamatsuCameraSettingsControl hamamatsuSettingsCtrl;
        PictureManager pics;
        PictureStats stats;
        AlertSystem alerts;
        ExperimentTimer timer;
        HamamatsuCameraCore hamamatsu;
        DataAnalysisControl analysisHandler;
        NewPythonHandler pythonHandler;
        DataLogger dataHandler;
        std::vector<QCustomPlotCtrl*> mainAnalysisPlots;
        coordinate selectedPixel = { 0,0 };
        
        // rearrange stuff;
        std::condition_variable rearrangerConditionVariable;
        //std::mutex plotLock;
        std::mutex rearrangerLock;
 
        dataPoint mostRecentAnalysisResult;
        
        std::atomic<bool> atomCrunchThreadActive;
        std::atomic<bool> skipNext = false;
        chronoTimes imageTimes, imageGrabTimes, mainThreadStartTimes, crunchSeesTimes, crunchFinTimes;
        unsigned mostRecentPicNum = 0;
        unsigned currentPictureNum = 0;
        std::vector<Matrix<long>> currentRawPictures; // store pictures within one experiment cycle
        Matrix<long> avgBackground;
    Q_SIGNALS:
        //void newImage (NormalImage);
 
    public Q_SLOTS:
        void onCameraProgress(NormalImage);
        LRESULT onCameraCalFinish (WPARAM wParam, LPARAM lParam);
        // Called (blocking) by the experiment thread at run start: arm the camera + start the grabber so
        // it's ready before the sequence triggers. Never throws/hangs into the experiment thread.
        void prepareForExperiment (unsigned repetitions, unsigned variations);
        void handlePrepareForAcq (HamamatsuRunSettings* lparam, analysisSettings aSettings);
        void completePlotterStart ();
        void completeCruncherStart ();
 
};
 
