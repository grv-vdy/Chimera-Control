#pragma once
#include "InfluxTypes.h"
#include "GeneralObjects/commonTypes.h"
#include <GeneralObjects/IDeviceCore.h>
#include <GeneralObjects/IChimeraSystem.h>
#include <qlabel.h>
#include <qmutex.h>
#include <InfluxDB.h>
#include <InfluxDBFactory.h>
#include <atomic>
#include "LowLevel/constants.h"

class IChimeraQtWindow;
class InfluxBroker;
class TemperatureMonitorCore;


class InfluxBroker
{
public:
	InfluxBroker(std::string identifier, std::string syntax, InfluxDataType::mode mode, InfluxDataUnitType::mode unit, bool safemode);
	InfluxBroker(const InfluxBroker&) = delete;
	InfluxBroker& operator=(const InfluxBroker&) = delete;
	//InfluxBroker(InfluxBroker&&) noexcept = default;
	//InfluxBroker& operator=(InfluxBroker&&) noexcept = default;
	//~InfluxBroker() noexcept {}

	std::pair<std::vector<long long>, std::vector<double>> getData();
	std::pair<std::vector<long long>, std::vector<double>> getDataExp();
	void experimentPrep();
	void experimentStart() { experimentOngoing=true; };
	void experimentEnd() { experimentOngoing = false; };;
	std::pair<long long,double> queryDataPoint();
	void clearNonExpData();


	// InfluxDB 2.x configuration (Flux)
	const std::string influx2Url = INFLUX2_URL;
	const std::string influx2Org = INFLUX2_ORG;
	const std::string influx2Bucket = INFLUX2_BUCKET;
	const std::string influx2Token = INFLUX2_TOKEN;
	const std::string influx2TagKey = INFLUX2_TAG_KEY;
	const std::string syntax;
	const std::string identifier;
	const InfluxDataType::mode dataMode;
	const InfluxDataUnitType::mode unitMode;

private:
	const bool safemode;
	std::unique_ptr<influxdb::InfluxDB> influxPtr;
	std::atomic<bool> experimentOngoing; 
	QMutex lock; // not movable!

	std::vector<long long> timeStamp;
	std::vector<double> data;
	// for storeing data during experiment, avoiding overnight dump and cleannig of std::vector<long long>timeStamp and std::vector<double>data
	std::vector<long long> timeStampExp;
	std::vector<double> dataExp;

};

class TemperatureMonitorCore : public IDeviceCore
{
	Q_OBJECT
public:
	TemperatureMonitorCore(IChimeraQtWindow* parent, bool safemode);
	void loadExpSettings(ConfigStream& stream) override;
	void logSettings(DataLogger& logger, ExpThreadWorker* threadworker) override {};
	void calculateVariations(std::vector<parameterType>& params, ExpThreadWorker* threadworker) override {};
	void programVariation(unsigned variation, std::vector<parameterType>& params,
		ExpThreadWorker* threadworker) override {};
	void normalFinish() override;
	void errorFinish() override;
	std::string getDelim() override { return "TEMPMON"; };
	

	void createDataFolder();
	void dumpDataToFile();
	
	std::array<InfluxBroker, TEMPMON_NUMBER> dataBroker;

private:
	//std::vector<InfluxBroker> dataBroker;
	std::string todayFoler;
	std::string fullPath;

};

class TemperatureMonitor : public IChimeraSystem
{
public:
	// THIS CLASS IS NOT COPYABLE.
	TemperatureMonitor& operator=(const TemperatureMonitor&) = delete;
	TemperatureMonitor(const TemperatureMonitor&) = delete;
	TemperatureMonitor(IChimeraQtWindow* parent_in, bool safemode);
	void initialize(IChimeraQtWindow* parent);
	TemperatureMonitorCore& getCore() { return core; };

private:
	std::array<QLabel*, TEMPMON_NUMBER>  name;
	std::array<QLabel*, TEMPMON_NUMBER>  reading;
	TemperatureMonitorCore core;

};