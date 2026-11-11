#pragma once
#include <qobject.h>
#include <Hamamatsu/cameraThreadInput.h>

class HamamatsuCameraThreadImageGrabber : public QObject
{
	Q_OBJECT
public:
	HamamatsuCameraThreadImageGrabber(cameraThreadImageGrabberInput* input_);
	~HamamatsuCameraThreadImageGrabber();

private:
	cameraThreadImageGrabberInput* input;
	void prepareCruncherExit();
public slots:
	void process();

signals:
	void pictureGrabbed(NormalImage);
	void pauseExperiment();
	void error(QString, unsigned);
	void notify(QString, unsigned);

};
