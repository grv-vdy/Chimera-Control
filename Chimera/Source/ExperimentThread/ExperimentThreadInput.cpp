#include "stdafx.h"
#include "ExperimentThread/ExperimentThreadInput.h"
#include "PrimaryWindows/QtAuxiliaryWindow.h"
#include "PrimaryWindows/QtMainWindow.h"
#include "PrimaryWindows/QtScriptWindow.h"
#include "PrimaryWindows/IChimeraQtWindow.h"

ExperimentThreadInput::ExperimentThreadInput(IChimeraQtWindow* win) :
	ttlSys(win->auxWin->getTtlSystem()),
	ttls(win->auxWin->getTtlCore()),
	aoSys(win->auxWin->getAoSys()),
	ao(win->auxWin->getAoSys().getCore()),
	logger(win->mainWin->getLogger())
{
	devices = win->mainWin->getDevices ();
};

