#include "stdafx.h"
#include "Hamamatsu/HamamatsuCameraThreadImageGrabber.h"
#include "HamamatsuCameraCore.h"
#include <QThread>
#include <chrono>

HamamatsuCameraThreadImageGrabber::HamamatsuCameraThreadImageGrabber(cameraThreadImageGrabberInput* input_)
	:input(input_) {}

HamamatsuCameraThreadImageGrabber::~HamamatsuCameraThreadImageGrabber()
{
	// This worker owns the input struct it was handed by the window.
	delete input;
}

void HamamatsuCameraThreadImageGrabber::prepareCruncherExit()
{
	if (input == nullptr) { return; }
	if (input->Hamamatsu != nullptr) {
		input->Hamamatsu->threadExpectingAcquisition = false;
	}
	if (input->cruncherThreadActive != nullptr) {
		*input->cruncherThreadActive = false;
	}
	// Wake the atom-cruncher with a sentinel image so it notices the exit flag and returns.
	NormalImage exitImage;
	exitImage.picStat = { 18446744073709551615ULL, 0, 0 };
	exitImage.image = Matrix<long>();
	if (input->Hamamatsu != nullptr) {
		input->Hamamatsu->getGrabberQueue()->push(exitImage);
	}
}

// Single per-run acquisition loop. For each expected picture: wait for a frame (external trigger in
// real mode, simulated in safe mode), copy it out of the camera buffer, then hand it to the window
// display/save slot (via pictureGrabbed) and the atom-cruncher queue. Runs on its own QThread and
// returns when the run finishes or is aborted.
void HamamatsuCameraThreadImageGrabber::process()
{
	HamamatsuCameraCore* cam = (input != nullptr) ? input->Hamamatsu : nullptr;
	if (cam == nullptr) {
		QThread::currentThread()->quit();
		return;
	}

	const unsigned long long totalPics = cam->getHamamatsuRunSettings().totalPicsInExperiment();
	unsigned long long pictureNumber = 0;

	while (pictureNumber < totalPics && cam->isRunning())
	{
		// Program THIS frame's exposure before its trigger arrives, so Edge mode uses the per-picture
		// exposure ring (pic 0's exposure, pic 1's, ...). No-op in Level mode (TTL sets exposure).
		cam->setExpRunningExposure(pictureNumber);

		if (!cam->safemode)
		{
			// Wait for the next frame. dcamwait times out roughly every second; keep re-waiting while
			// the run is active (external triggers can arrive slowly). Bail on stop or a genuine error.
			bool gotFrame = false;
			while (cam->isRunning())
			{
				try {
					cam->waitForAcquisition(pictureNumber, 1000);
					gotFrame = true;
					break;
				}
				catch (ChimeraError& e) {
					if (e.whatBare() == "AT_ERR_TIMEDOUT") {
						continue; // no trigger yet; keep waiting while the camera is running
					}
					emit error(e.qtrace(), 0);
					emit pauseExperiment();
					break;
				}
			}
			if (!gotFrame) { break; }
		}
		else
		{
			// Safe mode: simulate frames arriving at a modest rate so the display/save path can be
			// exercised without hardware.
			Sleep(200);
			if (!cam->isRunning()) { break; }
		}

		// acquireImageData() uses (currentPictureNumber - 1) for its internal per-repetition indexing,
		// so hand it a 1-based number.
		cam->updatePictureNumber(pictureNumber + 1);
		try
		{
			std::vector<std::vector<long>> images = cam->acquireImageData();
			HamamatsuRunSettings curSettings = cam->getHamamatsuRunSettings();
			const unsigned ppr = (curSettings.picsPerRepetition > 0) ? curSettings.picsPerRepetition : 1;
			const size_t activePic = static_cast<size_t>(pictureNumber % ppr);
			if (images.empty() || activePic >= images.size() || images[activePic].empty()) {
				// Nothing usable this frame (e.g. zero-size image parameters); skip it.
				pictureNumber++;
				continue;
			}
			std::pair<int, int> repVar = cam->getCurrentRepVarNumber(pictureNumber);

			NormalImage grabbed;
			grabbed.picStat = { pictureNumber, static_cast<unsigned>(repVar.first),
								static_cast<unsigned>(repVar.second) };
			grabbed.image = Matrix<long>(curSettings.imageSettings.height(),
										 curSettings.imageSettings.width(), images[activePic]);

			// Feed the atom-cruncher consumer and (via queued signal) the window display + file-save slot.
			cam->getGrabberQueue()->push(grabbed);
			emit pictureGrabbed(grabbed);

			if (input->imageTimes != nullptr) {
				input->imageTimes->push_back(std::chrono::high_resolution_clock::now());
			}
		}
		catch (ChimeraError& err)
		{
			emit error(err.qtrace(), 0);
			emit pauseExperiment();
			break;
		}
		pictureNumber++;
	}

	prepareCruncherExit();
	QThread::currentThread()->quit();
}
