#include "stdafx.h"
#include "TemperatureMonitor.h"
#include <InfluxDBException.h>
#include <qlayout.h>
#include <qtimer.h>
#include <qdatetime.h>
#include <filesystem>
#include <algorithm>
#include <DataLogging/DataLogger.h>
#include <ParameterSystem/Expression.h>
#include <PrimaryWindows/IChimeraQtWindow.h>
#include <PrimaryWindows/QtAndorWindow.h>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>


TemperatureMonitor::TemperatureMonitor(IChimeraQtWindow* parent_in, bool safemode)
	: IChimeraSystem(parent_in)
	, core(parent_in, safemode)
	, name{ new QLabel(this), new QLabel(this) }
	, reading{ new QLabel(this), new QLabel(this) }
{
}

void TemperatureMonitor::initialize(IChimeraQtWindow* parent)
{
	for (auto id : range(TEMPMON_NUMBER)) {
		std::string identifier(core.dataBroker[id].identifier);
		std::replace(identifier.begin(), identifier.end(), '_', ' ');
		name[id]->setText(qstr(identifier)+": ");
		name[id]->setStyleSheet("QLabel {font: bold 16pt;}");
		reading[id]->setStyleSheet("QLabel {font: bold 16pt;}");
	}
	QVBoxLayout* layout = new QVBoxLayout(this);
	for (auto id : range(TEMPMON_NUMBER)) {
		auto row = new QHBoxLayout();
		row->setContentsMargins(0, 0, 0, 0);
		row->addWidget(name[id]);
		row->addWidget(reading[id], 0);
		layout->addLayout(row);
	}
	QTimer* timer = new QTimer(this);
	QObject::connect(timer, &QTimer::timeout, [this]() {
		std::pair<long long, double> timedata{0,-1.0};
		for (auto idx : range(TEMPMON_NUMBER)) {
			try {
				timedata = core.dataBroker[idx].queryDataPoint();
			}
			catch (ChimeraError& e) {
				emit warning("Temperature sensor can not read properly. Please check \n" + e.qtrace());
			}
			catch (influxdb::ConnectionError& e) {
				emit warning("Temperature sensor can not read properly. Please check \n" + qstr(e.what()));
			}
			switch (core.dataBroker[idx].dataMode)
			{
			case InfluxDataType::mode::Temperature:
				switch (core.dataBroker[idx].unitMode)
				{
				case InfluxDataUnitType::mode::K:
					reading[idx]->setText(qstr(timedata.second, 2) + " K");
					break;
				case InfluxDataUnitType::mode::C:
					reading[idx]->setText(qstr(timedata.second, 2) + " C");
					break;
				}
				break;
			case InfluxDataType::mode::Pressure:
				switch (core.dataBroker[idx].unitMode)
				{
				case InfluxDataUnitType::mode::mBar:
					reading[idx]->setText(qstr(timedata.second, 2, false, false, false, true) + " mBar");
					break;
				case InfluxDataUnitType::mode::Torr:
					reading[idx]->setText(qstr(timedata.second, 2, false, false, false, true) + " Torr");
					break;
				}
				break;
			default:
				break;
			}

		}});
	timer->start(60000/*1min*60*1e3*/); // every 1min to query a data that updates every 2min to avoid some rounding issue in time

}

TemperatureMonitorCore::TemperatureMonitorCore(IChimeraQtWindow* parent, bool safemode)
	: dataBroker{ 
	InfluxBroker(TEMPMON_ID[0],TEMPMON_SYNTAX[0], InfluxDataType::mode::Temperature, InfluxDataUnitType::mode::C, safemode),
	InfluxBroker(TEMPMON_ID[1],TEMPMON_SYNTAX[1], InfluxDataType::mode::Temperature, InfluxDataUnitType::mode::C, safemode) }
{
	this->setParent(parent);
	//dataBroker.reserve(TEMPMON_NUMBER);
	//for (unsigned idx = 0; idx < TEMPMON_NUMBER; idx++) {
	//	dataBroker.emplace_back(TEMPMON_ID[idx],TEMPMON_SYNTAX[idx]);
	//}
	auto shit = InfluxBroker(TEMPMON_ID[1], TEMPMON_SYNTAX[1], InfluxDataType::mode::Temperature, InfluxDataUnitType::mode::K, safemode);// this is in-place construction, no copy nor move in c++17

	QTime midnight = QTime(23, 59, 59, 999);
	//QTime midnightCreateFolder = QTime(23, 50, 0, 0); // give ten minute ahead to create folder 
	QTime now = QTime::currentTime();
	if (now.msecsTo(midnight) - 60000 < 0) {
		thrower("Hardworker at midnight!!!! The temperature data is tired and wouldn't want to log between 23:59-24:00"
			"Maybe try after 24:00, i.e a minite later?");
	}
	QTimer::singleShot(now.msecsTo(midnight)-60000/*60sec ahead*/, [this, parent]() {
		try {
			createDataFolder();
			dumpDataToFile();
		}
		catch (ChimeraError& e) {
			parent->reportErr(e.qtrace());
		}
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, [this, parent]() {
			try {
				createDataFolder();
				dumpDataToFile();
			}
			catch (ChimeraError& e) {
				parent->reportErr(e.qtrace());
			} });
		timer->start(24*60*60*1000);
		});

	experimentActive = !safemode;
	
}

// use as notifying the broker to start collect temp data for exp
void TemperatureMonitorCore::loadExpSettings(ConfigStream& stream)
{
	for (auto& broker : dataBroker) {
		broker.experimentPrep();
		broker.experimentStart();
	}
}

void TemperatureMonitorCore::normalFinish()
{
	auto win = static_cast<IChimeraQtWindow*>(parent());
	DataLogger& logger = win->andorWin->getLogger();
	for (auto& broker : dataBroker) {
		broker.experimentEnd();
		auto timedata = broker.getDataExp();
		switch (broker.dataMode)
		{
		case InfluxDataType::mode::Temperature:
			try {
				logger.writeTemperature(timedata, broker.identifier);
			}
			catch (ChimeraError& e) {
				win->reportErr(qstr("Temperature data abandoned. Due to ") + e.qtrace());
			}
			break;
		case InfluxDataType::mode::Pressure:
			try {
				logger.writePressure(timedata, broker.identifier, broker.unitMode);
			}
			catch (ChimeraError& e) {
				win->reportErr(qstr("Pressure data abandoned. Due to ") + e.qtrace());
			}
			break;
		default:
			win->reportErr(qstr("Influx data mode is neither Temperature nor Pressure? A low level bug!"));
			break;
		}
	}
}

void TemperatureMonitorCore::errorFinish()
{
	// Don't try to write temperature/pressure data during error/abort
	// The file is being closed and HDF5 operations will deadlock or fail
	for (auto& broker : dataBroker) {
		broker.experimentEnd();
	}
}

void TemperatureMonitorCore::createDataFolder()
{
	DataLogger::getDataLocation(DATA_SAVE_LOCATION, todayFoler, fullPath);
	struct stat info;
	fullPath += "TemperatureData"; // with "//" will cause ERROR_ALREADY_EXIT, just go without appending "//"
	int result = 1;
	int resultStat = stat(cstr(DATA_SAVE_LOCATION + fullPath), &info);
	if (resultStat != 0) {
		result = std::filesystem::create_directories((DATA_SAVE_LOCATION + fullPath).c_str());
	}
	if (!result) {
		thrower("ERROR: Failed to create save location for data at location " + DATA_SAVE_LOCATION  + fullPath +
			". Make sure you have access to it or change the save location. Error: " + str(GetLastError())
			+ "\r\n");
	}
}

void TemperatureMonitorCore::dumpDataToFile()
{
	for (auto idx : range(TEMPMON_NUMBER)) {
		char buff[128];
		auto timedata = std::move(dataBroker[idx].getData());
		std::ofstream ofs(DATA_SAVE_LOCATION + fullPath + "//" + TEMPMON_ID[idx] + ".csv", std::ofstream::out);
		switch (dataBroker[idx].dataMode)
		{
		case InfluxDataType::mode::Temperature:
			ofs << "epochTime(ms)" << ',' << "Temperature(K)" << '\n';
			for (auto idd : range(timedata.first.size())) {
				sprintf_s(buff, "%Id64,%f\n", timedata.first[idd], timedata.second[idd]);
				ofs << buff;
			}
			break;
		case InfluxDataType::mode::Pressure:
			switch (dataBroker[idx].unitMode)
			{
				case InfluxDataUnitType::mode::mBar:
					ofs << "epochTime(ms)" << ',' << "Pressure (mBar)" << '\n';
					for (auto idd : range(timedata.first.size())) {
						sprintf_s(buff, "%Id64,%e\n", timedata.first[idd], timedata.second[idd]);
						ofs << buff;
					}
				break;
				case InfluxDataUnitType::mode::Torr:
					ofs << "epochTime(ms)" << ',' << "Pressure (Torr)" << '\n';
					for (auto idd : range(timedata.first.size())) {
						sprintf_s(buff, "%Id64,%e\n", timedata.first[idd], timedata.second[idd]);
						ofs << buff;
					}
				break;
			}
			break;
		}
		ofs.close();
		dataBroker[idx].clearNonExpData();
	}

}

InfluxBroker::InfluxBroker(std::string identifier, std::string syntax, InfluxDataType::mode mode, InfluxDataUnitType::mode unit, bool safemode) :
	safemode(safemode),
	dataMode(mode),
	identifier(identifier),
	syntax(syntax), 
	unitMode(unit),
	experimentOngoing(false)
{
	// We no longer use the legacy InfluxDB v1 client for Flux queries
	// influxPtr is kept but unused when querying Influx v2
	influxPtr = nullptr;
}

std::pair<std::vector<long long>, std::vector<double>> InfluxBroker::getData()
{
	QMutexLocker locker(&lock);
	return std::make_pair(timeStamp, data);
}

std::pair<std::vector<long long>, std::vector<double>> InfluxBroker::getDataExp()
{
	QMutexLocker locker(&lock);
	return std::make_pair(timeStampExp, dataExp);
}

void InfluxBroker::experimentPrep()
{
	QMutexLocker locker(&lock);
	timeStampExp.clear();
	dataExp.clear();
}

std::pair<long long, double> InfluxBroker::queryDataPoint()
{
	if (safemode) {
		data.push_back(-1.0);
		timeStamp.push_back(1);

		if (experimentOngoing) {
			dataExp.push_back(data.back());
			timeStampExp.push_back(timeStamp.back());
		}
		return std::make_pair(timeStamp.back(), data.back());
	}

	// Build Flux query to get latest temperature for this identifier
	// Use -2h to ensure we get data even if sensor reports infrequently (every 30min)
	std::string flux =
		"from(bucket: \"" + influx2Bucket + "\")\n"
		"  |> range(start: -2h)\n"
		"  |> filter(fn: (r) => r._measurement == \"ubibot\")\n"
		"  |> filter(fn: (r) => r._field == \"temperature\")\n"
		"  |> filter(fn: (r) => r[\"" + influx2TagKey + "\"] == \"" + identifier + "\")\n"
		"  |> last()\n";

	QNetworkAccessManager mgr;
	QNetworkRequest req(QUrl(QString::fromStdString(influx2Url + "/api/v2/query?org=" + influx2Org)));
	req.setRawHeader("Authorization", QByteArray("Token ") + QByteArray::fromStdString(influx2Token));
	req.setRawHeader("Accept", "text/csv");
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/vnd.flux");

	QEventLoop loop;
	QNetworkReply* reply = mgr.post(req, QByteArray::fromStdString(flux));
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (reply->error() != QNetworkReply::NoError) {
		std::string err = reply->errorString().toStdString();
		QByteArray errBody = reply->readAll();
		std::string errBodyStr = QString::fromUtf8(errBody).toStdString();
		reply->deleteLater();
		thrower("InfluxDB v2 query failed: " + err + " | Response: " + errBodyStr);
	}

	QByteArray body = reply->readAll();
	reply->deleteLater();
	QString text = QString::fromUtf8(body);
	QStringList lines = text.split('\n', Qt::SkipEmptyParts);
	int headerIdx = -1;
	for (int i = 0; i < lines.size(); ++i) {
		if (!lines[i].startsWith('#')) { headerIdx = i; break; }
	}
	if (headerIdx < 0 || headerIdx + 1 >= lines.size()) {
		thrower("InfluxDB v2 CSV parse error: no data rows");
	}
	QStringList headers = lines[headerIdx].split(',');
	int timeCol = headers.indexOf("_time");
	int valueCol = headers.indexOf("_value");
	if (timeCol < 0 || valueCol < 0) {
		thrower("InfluxDB v2 CSV parse error: missing _time/_value columns");
	}
	// first data row after header
	QStringList cols = lines[headerIdx + 1].split(',');
	if (cols.size() <= std::max(timeCol, valueCol)) {
		thrower("InfluxDB v2 CSV parse error: insufficient columns");
	}
	QString timeStr = cols[timeCol];
	QString valueStr = cols[valueCol];

	bool ok = false;
	double val = valueStr.toDouble(&ok);
	if (!ok) {
		thrower("InfluxDB v2 CSV parse error: invalid numeric value");
	}
	QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODateWithMs);
	if (!dt.isValid()) { dt = QDateTime::fromString(timeStr, Qt::ISODate); }
	if (!dt.isValid()) {
		thrower("InfluxDB v2 CSV parse error: invalid time format");
	}
	long long secs = dt.toSecsSinceEpoch();

	QMutexLocker locker(&lock);
	if (!timeStamp.empty() && timeStamp.back() == secs) {
		return std::make_pair(timeStamp.back(), data.back());
	}
	data.push_back(val);
	timeStamp.push_back(secs);
	if (experimentOngoing) {
		dataExp.push_back(data.back());
		timeStampExp.push_back(timeStamp.back());
	}
	return std::make_pair(timeStamp.back(), data.back());
}

void InfluxBroker::clearNonExpData()
{
	QMutexLocker locker(&lock);
	timeStamp.clear();
	data.clear();
}







