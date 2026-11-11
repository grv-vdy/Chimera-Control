#include "stdafx.h"
#include "Hamamatsu/HamamatsuCameraThreadWorker.h"
#include "HamamatsuCameraCore.h"
#include <qdebug.h>

HamamatsuCameraThreadWorker::HamamatsuCameraThreadWorker (cameraThreadWorkerInput* input_){
	input = input_;
}

HamamatsuCameraThreadWorker::~HamamatsuCameraThreadWorker () {
}

void HamamatsuCameraThreadWorker::process (){
	//... I'm not sure what this lock is doing here... why not inside while loop?
	int safeModeCount = 0;
	unsigned long long pictureNumber = 0;
	bool armed = false;
	std::unique_lock<std::timed_mutex> lock (input->runMutex, std::chrono::milliseconds (1000));
	if (!lock.owns_lock ()) {
		errBox ("ERROR: ANDOR IMAGING THREAD FAILED TO LOCK THE RUN MUTEX! IMAGING THREAD CLOSING!");
	}
	while (!input->Hamamatsu->cameraThreadExitIndicator){
		/*
		 * wait until unlocked. this happens when data is started.
		 * the first argument is the lock.  The when the lock is locked, this function just sits and doesn't use cpu,
		 * unlike a while(gGlobalCheck){} loop that waits for gGlobalCheck to be set. The second argument here is a
		 * lambda, more or less a quick inline function that doesn't in this case have a name. This handles something
		 * called spurious wakeups, which are weird and appear to relate to some optimization things from the quick
		 * search I did. Regardless, I don't fully understand why spurious wakeups occur, but this protects against
		 * them.
		 */
		 // Also, anytime this gets locked, the count should be reset.
		 // input->signaler.wait( lock, [input]() { return input->expectingAcquisition; } ); // equivalent to code below, check first before lock
		while (!input->Hamamatsu->threadExpectingAcquisition) {
			input->signaler.wait (lock);
			pictureNumber = 0;
			armed = false;
		}
		if (!input->Hamamatsu->safemode){
			try	{
				if (pictureNumber == input->Hamamatsu->runSettings.totalPicsInExperiment() && armed) {
					// get the last picture. acquisition is over 					
					// make sure the thread waits when it hits the condition variable.
					input->Hamamatsu->threadExpectingAcquisition = false;
					continue;
				}
				else{
					while (true) {
						try {
							input->Hamamatsu->waitForAcquisition(pictureNumber, 1000);
						}
						catch (ChimeraError& e) {
							if (e.whatBare() == "AT_ERR_TIMEDOUT") {
								if (input->Hamamatsu->cameraIsRunning) {
									qDebug() << "input->Hamamatsu->waitForAcquisition time out for 1000ms, camera still running, will re-wait for acquisition";
									continue;
								}
								else {
									qDebug() << "input->Hamamatsu->waitForAcquisition time out for 1000ms, camera NOT running, will abort and reset pic counter";
									input->Hamamatsu->threadExpectingAcquisition = false;
								}
							}
							else {
								emit error("HamamatsuThreadWorker error: \r\n input->Hamamatsu->waitForAcquisition error \r\n" + e.qtrace(), 0);
							}
						}
						break;
					}
					if (pictureNumber % 2 == 0) {
						(*input->imageTimes).push_back (std::chrono::high_resolution_clock::now ());
					}
					qDebug() << "From Worker thread: get image number" << pictureNumber << " at " << std::chrono::high_resolution_clock::now().time_since_epoch().count()/*(*input->imageTimes).back().time_since_epoch().count()*/ - (*input->imageTimes)[0].time_since_epoch().count();
					armed = true;
					if (!input->Hamamatsu->cameraIsRunning) {
						// aborted by user
						input->Hamamatsu->threadExpectingAcquisition = false;
						input->picBufferQueue.push(18446744073709551615ULL); // Wake grabber thread with sentinel value and grabber will reset its counter
						qDebug() << "CameraThreadWorker aborted from user by awakening it again from the waitForAcquisition";
					}
					else {
						input->picBufferQueue.push(pictureNumber); // this will wake Grabber thread to grab image from memory with buffer identified by pictureNumber
						input->Hamamatsu->queueBuffers(pictureNumber + 1); // immediately requeue buffer for next image reading
						input->Hamamatsu->setExpRunningExposure(pictureNumber + 1); // set exposure, probably should only use external exposure so do not need to worry about time delay in this function
						//emit pictureTaken (pictureNumber);
						pictureNumber++;
					}
				}
			}
			catch (ChimeraError&) {
				//...? When does this happen? not sure why this is here...
			}
		}
		else { // safemode
			// simulate an actual wait.
			Sleep (500);
			qDebug() << "Hamamatsu safemode debug: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			if (pictureNumber % 2 == 0) {
				(*input->imageTimes).push_back (std::chrono::high_resolution_clock::now ());
			}
			if (input->Hamamatsu->cameraIsRunning && safeModeCount < input->Hamamatsu->runSettings.totalPicsInExperiment ()) {
				if (true/*input->Hamamatsu->runSettings.acquisitionMode == HamamatsuRunModes::mode::Kinetic*/) {
					if (input->Hamamatsu->isCalibrating ()) {
						//input->comm->sendCameraCalProgress (safeModeCount);
					}
					else {
						emit pictureTaken (safeModeCount);
						//input->comm->sendCameraProgress (safeModeCount);
					}
					safeModeCount++;
				}
				else {
					if (input->Hamamatsu->isCalibrating ()) {
						//input->comm->sendCameraCalProgress (1);
					}
					else {
						emit pictureTaken (1);
						//input->comm->sendCameraProgress (1);
					}
				}
			}
			else{
				input->Hamamatsu->cameraIsRunning = false;
				safeModeCount = 0;
				if (input->Hamamatsu->isCalibrating ()) {
					//input->comm->sendCameraCalFin ();
				}
				else {
					emit acquisitionFinished ();
					//input->comm->sendCameraFin ();
				}
				input->Hamamatsu->threadExpectingAcquisition = false;
			}
		}
	}
}


