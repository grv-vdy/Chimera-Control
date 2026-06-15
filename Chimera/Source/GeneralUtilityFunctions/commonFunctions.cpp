// created by Mark O. Brown
#include "stdafx.h"
#include "GeneralUtilityFunctions/commonFunctions.h"
#include "ExcessDialogs/openWithExplorer.h"
#include "ExcessDialogs/saveWithExplorer.h"
#include "ExperimentThread/ExperimentThreadInput.h"
#include "PrimaryWindows/QtMainWindow.h"
#include "PrimaryWindows/QtAuxiliaryWindow.h"
#include "PrimaryWindows/QtScriptWindow.h"
#include "PrimaryWindows/IChimeraQtWindow.h"
#include <ExperimentThread/ExpThreadWorker.h>
#include "ExperimentThread/ExperimentType.h"
#include "ExperimentThread/autoCalConfigInfo.h"
#include <QDateTime.h>

// Functions called by all windows to do the same thing, mostly things that happen on menu presses or hotkeys
namespace commonFunctions{
	// this function handles messages that all windows can recieve, e.g. accelerator keys and menu messages. It 
	// redirects everything to all of the other functions below, for the most part.
	void handleCommonMessage( int msgID, IChimeraQtWindow* win ){
		auto* mainWin = win->mainWin; 
		auto* scriptWin = win->scriptWin;
		auto* auxWin = win->auxWin;
		auto* makoWin1 = win->makoWin1;
		auto* makoWin2 = win->makoWin2;
		try {
			switch (msgID) {
			case ID_ACCELERATOR_F5: {
				AllExperimentInput input;
				try {
					if (mainWin->masterIsRunning ()) {
						auto response = QMessageBox::question (mainWin, "Auto-F5?",
							"The Master system is already running. Would you like to run the "
							"current configuration when master finishes? This effectively "
							"auto-presses F5 when complete and skips confirmation.");
						if (response == QMessageBox::Yes) {
							mainWin->autoF5_AfterFinish = true;
						}
						break;
					}
					// automatically save; this is important to handle changes like the auto servo and auto carrier
					commonFunctions::handleCommonMessage (ID_FILE_SAVEALL, win);
					prepareMasterThread (msgID, win, input, true, false, true, true);
					input.masterInput->expType = ExperimentType::Normal;
					if (!mainWin->autoF5_AfterFinish) {
						commonFunctions::getPermissionToStart (win, true, input);
					}
					mainWin->autoF5_AfterFinish = false;
					logStandard (input, input.masterInput->logger);
					startExperimentThread (mainWin, input);
				}
				catch (ChimeraError & err) {
					if (err.whatBare () == "CANCEL") {
						mainWin->reportStatus ("Canceled camera initialization.\r\n");
						break;
					}
					mainWin->reportErr ("EXITED WITH ERROR!\n " + err.qtrace ());
					mainWin->reportStatus ("EXITED WITH ERROR!\r\nInitialized Default Waveform\r\n");
					break;
				}
				break;
			}
			case ID_ACCELERATOR_ESC: {
				std::string status;
				bool masterAborted = false, baslerAborted = false;
				try {
					if (mainWin->expIsRunning ()) {
						status = "MASTER";
						commonFunctions::abortMaster (win);
						masterAborted = true;
					}
				}
				catch (ChimeraError & err) {
					mainWin->reportErr ("Abort Master thread exited with Error! Error Message: "
						+ err.qtrace ());
					mainWin->reportStatus ("Abort Master thread exited with Error!\r\n");
				}
				//
				if (!masterAborted && !baslerAborted) {
					for (auto& dev : mainWin->getDevices ().list) {
						mainWin->handleColorboxUpdate ("Black", qstr (dev.get ().getDelim ()));
					}
					mainWin->reportErr ("Master and Basler camera were not running. "
						"Can't Abort.\r\n");
				}
				break;
			}

			/// File Management 
			case ID_FILE_SAVEALL: {
				try {
					scriptWin->saveAllScript();
					mainWin->profile.saveConfiguration (win);
					mainWin->masterConfig.save (mainWin, auxWin);
				}
				catch (ChimeraError & err) {
					mainWin->reportErr (err.qtrace ());
				}
				break;
			}
			case ID_FILE_MY_EXIT: {
				try {
					commonFunctions::exitProgram (win);
				}
				catch (ChimeraError & err) {
					mainWin->reportErr ("ERROR! " + err.qtrace ());
				}
				break;
			}
			case ID_RUNMENU_ABORTCAMERA: {
				mainWin->reportStatus ("Camera-specific abort is disabled in the current build.\r\n");
				break;
			}
			case ID_ACCELERATOR_F1: {
				try {
					AllExperimentInput input;
					input.masterInput = new ExperimentThreadInput (win);
					input.masterInput->updatePlotterXVals = false;
					auxWin->fillMasterThreadInput (input.masterInput);
					mainWin->fillMotInput (input.masterInput);
					input.masterInput->expType = ExperimentType::LoadMot;
					mainWin->startExperimentThread (input.masterInput);
				}
				catch (ChimeraError & err) {
					mainWin->reportErr (err.qtrace ());
				}
				break;
			}
			case ID_ACCELERATOR_F11: {
				if (mainWin->experimentIsRunning) {
					return;
				}
				if (mainWin->getMainOptions ().delayAutoCal) {
					mainWin->handleNotification ("Delaying Auto-Calibration!\n");
					return;
				}
				if (QDateTime::currentDateTime ().time ().hour () < 4) {
					// This should set up the calibration to run at 4AM. 
					return;
				};
				// F11 is the set of calibrations.
				AllExperimentInput input;
				input.masterInput = new ExperimentThreadInput (win);
				input.masterInput->quiet = true;
				try {
					auxWin->fillMasterThreadInput (input.masterInput);
					auto calNum = -1;
					if (calNum == -1) {
						return;
					}
					// automatically save; this is important to handle changes like the auto servo and auto carrier
					commonFunctions::handleCommonMessage (ID_FILE_SAVEALL, win);
					auto& calInfo = AUTO_CAL_LIST[calNum];
					mainWin->reportStatus (qstr (calInfo.infoStr));
					input.masterInput->profile = calInfo.prof;
					input.masterInput->expType = ExperimentType::AutoCal;
					logStandard (input, input.masterInput->logger, calInfo.fileName, false);
					startExperimentThread (mainWin, input);
				}
				catch (ChimeraError & err) {
					mainWin->reportErr ("Failed to start auto calibration experiment: " + err.qtrace ());
				}
				break;
			}
								   // the rest of these are all one-liners. 			
			case ID_PROFILE_SAVE_PROFILE: { mainWin->profile.saveConfiguration (win); break; }
			//case ID_PLOTTING_STOPPLOTTER: { andorWin->stopPlotter (); break; }
			case ID_ACCELERATOR_F2: case ID_RUNMENU_PAUSE: { mainWin->handlePauseToggle (); break; }
			case ID_CONFIGURATION_RENAME_CURRENT_CONFIGURATION: { mainWin->profile.renameConfiguration (); break; }
			case ID_CONFIGURATION_DELETE_CURRENT_CONFIGURATION: { mainWin->profile.deleteConfiguration (); break; }
			case ID_CONFIGURATION_SAVE_CONFIGURATION_AS: { mainWin->profile.saveConfigurationAs (win); break; }
			case ID_CONFIGURATION_SAVECONFIGURATIONSETTINGS: { mainWin->profile.saveConfiguration (win); break; }
			case ID_MASTERSCRIPT_NEW: { scriptWin->newMasterScript (); break; }
			case ID_MASTERSCRIPT_SAVE: { scriptWin->saveMasterScript (); break; }
			case ID_MASTERSCRIPT_SAVEAS: { scriptWin->saveMasterScriptAs (win); break; }
			case ID_MASTERSCRIPT_OPENSCRIPT: { scriptWin->openMasterScript (win); break; }
			case ID_MASTERSCRIPT_NEWFUNCTION: { scriptWin->newMasterFunction ();	break; }
			case ID_MASTERSCRIPT_SAVEFUNCTION: { scriptWin->saveMasterFunction (); break; }
			case ID_MASTERCONFIG_SAVEMASTERCONFIGURATION: { mainWin->masterConfig.save (mainWin, auxWin); break; }
			case ID_MASTERCONFIGURATION_RELOAD_MASTER_CONFIG: { mainWin->masterConfig.load (mainWin, auxWin); break; }


			default:
				errBox ("Common message passed but not handled! The feature you're trying to use"\
					" feature likely needs re-implementation / new handling.");
			}
		}
		catch (ChimeraError & err) {
			mainWin->reportErr (err.qtrace ());
		}
	}

	void calibrateCameraBackground(IChimeraQtWindow* win){
		try {
			AllExperimentInput input;
			input.masterInput = new ExperimentThreadInput ( win );
			input.masterInput->profile = win->mainWin->getProfileSettings();
			win->auxWin->fillMasterThreadInput (input.masterInput);
			win->mainWin->loadCameraCalSettings( input.masterInput );
			win->mainWin->startExperimentThread( input.masterInput );
		}
		catch ( ChimeraError& err ){
			errBox( err.trace( ) );
		}
	}

	void prepareMasterThread( int msgID, IChimeraQtWindow* win, AllExperimentInput& input,
							  bool runMaster, bool runCamera, bool runMako, bool updatePlotXVals )	{
		win->mainWin->checkProfileSave();
		win->scriptWin->checkScriptSaves( );
		// Set the thread structure.
		input.masterInput = new ExperimentThreadInput ( win );
		input.masterInput->updatePlotterXVals = updatePlotXVals;
		input.masterInput->skipNext = nullptr;
		input.masterInput->numVariations = win->auxWin->getTotalVariationNumber ( );
		input.masterInput->sleepTime = win->mainWin->getDebuggingOptions ().sleepTime;
		input.masterInput->profile = win->mainWin->getProfileSettings ();
		// Start the programming thread. order is important.
		win->scriptWin->fillMasterThreadInput( input.masterInput );
		win->auxWin->fillMasterThreadInput( input.masterInput );
		win->mainWin->fillMasterThreadInput( input.masterInput );
	}

	void startExperimentThread(IChimeraQtWindow* win, AllExperimentInput& input){
		win->mainWin->addTimebar( "main" );
		win->mainWin->addTimebar( "error" );
		win->mainWin->startExperimentThread( input.masterInput );
	}

	void abortMaster( IChimeraQtWindow* win ){
		win->mainWin->abortMasterThread();
		win->auxWin->handleAbort();
	}

	void forceExit (IChimeraQtWindow* win){
		//PostQuitMessage ( 1 );
	}


	void exitProgram(IChimeraQtWindow* win)	{
		if (win->mainWin->masterIsRunning()){
			thrower ( "The Master system (ttls & aoSys) is currently running. Please stop the system before exiting so "
					  "that devices can stop normally." );
		}
		win->scriptWin->checkScriptSaves( );
		win->mainWin->checkProfileSave();
		std::string exitQuestion = "Are you sure you want to exit?\n\nThis will stop all output of the NI arbitrary "
			"waveform generator.";
		auto areYouSure = QMessageBox::question (win, "Exit?", qstr(exitQuestion));
		if (areYouSure == QMessageBox::Yes){
			forceExit ( win );
		}
	}

	void logStandard( AllExperimentInput input, DataLogger& logger, 
					  std::string specialName, bool needsCal ){
		logger.initializeDataFiles( specialName, needsCal );
		logger.logMasterInput( input.masterInput );
		logger.logMiscellaneousStart();
	}

	bool getPermissionToStart( IChimeraQtWindow* win, bool runMaster, AllExperimentInput& input ){
		std::string startMsg = "Current Settings:\r\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~\r\n\r\n";
		startMsg += "\r\n\r\nBegin Experiment with these Settings?";
		//StartDialog dlg( startMsg, IDD_BEGINNING_SETTINGS );
		//bool areYouSure = dlg.DoModal( );
		//if ( !areYouSure )
		//{
		//	thrower ( "CANCEL!" );
		//}
		return true;
	}
};
