#include "stdafx.h"
#include "Andor/AndorCameraThreadImageGrabber.h"
#include <Andor/AndorCameraCore.h>

AndorCameraThreadImageGrabber::AndorCameraThreadImageGrabber(cameraThreadImageGrabberInput* input_)
	:input(input_) {};

AndorCameraThreadImageGrabber::~AndorCameraThreadImageGrabber() {}

void AndorCameraThreadImageGrabber::prepareCruncherExit()
{
	input->Andor->threadExpectingAcquisition = false;
	*input->cruncherThreadActive = false;
	// this will invoke cruncherThread and it will then detect cruncherThreadActive=false and therefore quit (-1 is not being recongnized)
	input->imageQueue.push({ {-1ULL, 0, 0}, Matrix<long>() });
}



//CHNAGED HERE KEPT LOOPING 
void AndorCameraThreadImageGrabber::process()
{
	unsigned long long pictureNumber = 0;
	std::unique_lock<std::mutex> lock(input->runMutex);

	while (!input->Andor->cameraThreadExitIndicator) {

		// In safemode, do not sit in the camera acquisition loop forever.
		if (input->Andor->safemode) {
			prepareCruncherExit();
			input->Andor->cameraThreadExitIndicator = true;
			break;
		}

		// Wait until/unless camera is ready to take images.
		// Also wake/exit cleanly if the camera thread is asked to exit.
		while (!input->Andor->threadExpectingAcquisition &&
			!input->Andor->cameraThreadExitIndicator) {

			input->signaler.wait(lock);

			input->imageQueue.clear();
			pictureNumber = 0;
		}

		// If shutdown was requested while waiting, leave cleanly.
		if (input->Andor->cameraThreadExitIndicator) {
			break;
		}

		auto popPictureNumber = input->picBufferQueue->pop();

		if (!input->Andor->cameraIsRunning) {
			// Aborted by user and re-woken by worker thread.
			prepareCruncherExit();
			continue;
		}

		if (popPictureNumber != pictureNumber) {
			thrower(
				"AndorCameraThreadImageGrabber error: \r\n The pop-ed pictureNumber from WorkerThread is: "
				+ str(popPictureNumber)
				+ ", but the expected pictureNumber is: "
				+ str(pictureNumber)
				+ ". This is a low-level error as no other thread should be able to pop the picBufferQueue and the order should be FIFO.",
				0
			);
		}

		input->Andor->updatePictureNumber(pictureNumber);

		try {
			auto images = input->Andor->acquireImageData();
			auto repVar = input->Andor->getCurrentRepVarNumber(pictureNumber);
			AndorRunSettings curSettings = input->Andor->getAndorRunSettings();

			size_t currentActivePicNum = curSettings.continuousMode
				? 0
				: pictureNumber % curSettings.picsPerRepetition;

			// Push to imageQueue, which will invoke the atomCruncher thread.
			input->imageQueue.push({
				{ pictureNumber, repVar.first, repVar.second },
				images[currentActivePicNum]
				});

			emit pictureGrabbed({
				{ pictureNumber, repVar.first, repVar.second },
				images[currentActivePicNum]
				});
		}
		catch (ChimeraError& err) {
			emit error(err.qtrace(), 0);
			emit pauseExperiment();
			prepareCruncherExit();
			continue;
		}

		pictureNumber++;

		if (pictureNumber == input->Andor->runSettings.totalPicsInExperiment()) {
			// Got the last picture; acquisition is over.
			prepareCruncherExit();
		}
	}
}