#pragma once

#include <qobject.h>
#include <string>
#include <Hamamatsu/cameraThreadInput.h>

class HamamatsuCameraThreadWorker : public QObject {
    Q_OBJECT

    public:
        HamamatsuCameraThreadWorker(cameraThreadWorkerInput* input_);
        ~HamamatsuCameraThreadWorker ();
    private:
        cameraThreadWorkerInput * input;

    public Q_SLOTS:
        void process ();
		
	Q_SIGNALS:
		void error (QString, unsigned);
        void notify (QString, unsigned);
        void pictureTaken (int);
        void acquisitionFinished ();
};
 

