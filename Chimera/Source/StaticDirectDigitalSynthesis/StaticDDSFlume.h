#pragma once
#include <string>
#include <GeneralFlumes/BoostAsyncSerial.h>

class StaticDDSFlume {
public:
	StaticDDSFlume(std::string portAddress, unsigned baudrate, bool safemode);
	std::string query(std::string msg);
	void write(std::string msg);
	std::string read();
	void resetConnection();
	const bool SAFEMODE;
	std::string getSerialNumberOnly();
	void setFrequency(double frequencyMHz, int ch);
	void setOutputLevel(double leveldBm, int ch);
	void setReferenceFrequency(double refMHz);
	// Sweep control (Valon native commands)
	void startSweep(double startMHz, double stopMHz, double stepMHz, unsigned rateMs, int ch);
	void stopSweep(int ch);
private:
	void readCallback(int byte);
	void errorCallback(std::string error);
	BoostAsyncSerial boostFlume;
	std::atomic<bool> readComplete;
	std::vector<unsigned char> readRegister;
	std::string errorMsg;

};