#include "stdafx.h"
#include "MOTAnalysisThreadWoker.h"
//#include <RealTimeMOTAnalysis/MOTAnalysisControl.h>
#include <CMOSCamera/GaussianFit.h>
#include <qtconcurrentrun.h>
#include <qdebug.h>

MOTAnalysisThreadWoker::MOTAnalysisThreadWoker(MOTThreadInput input_)
	: input(input_)
{

}

void MOTAnalysisThreadWoker::init()
{
	for (auto type : MOTAnalysisType::allTypes) {
		if (type != MOTAnalysisType::type::density2d) {
			result2d.insert({ type,std::vector<std::vector<double>>() });
			result2d[type].resize(input.camSet.variations);
			for (auto& vec : result2d[type]) {
				vec.reserve(input.camSet.repsPerVar);
			}
		}
	}
	density2d = std::vector<std::vector<double>>(input.camSet.variations, std::vector<double>());
	for (auto vec : density2d) {
		vec.reserve(input.camSet.dims.size());
	}

	// init background tracking structures per variation
	backgroundImage = std::vector<QVector<double>>(input.camSet.variations);
	hasBackground = std::vector<bool>(input.camSet.variations, false);
	imagesSeenThisRep = std::vector<unsigned>(input.camSet.variations, 0);
	lastRepForVar = std::vector<size_t>(input.camSet.variations, SIZE_MAX);
}

void MOTAnalysisThreadWoker::handleNewImg(QVector<double> img, int width, int height, size_t rep, size_t var)
{
	qDebug() << "MOTAnalysisThreadWoker::handleNewImg -> Receive experiment pictures for rep/var: " << rep << var;
	if (var >= lastRepForVar.size()) {
		emit error("Received image for invalid variation index");
		return;
	}
	if (rep != lastRepForVar[var]) {
		// new repetition for this variation
		imagesSeenThisRep[var] = 0;
		lastRepForVar[var] = rep;
		hasBackground[var] = false;
		backgroundImage[var].clear();
	}
	unsigned curIdxInRep = imagesSeenThisRep[var];
	imagesSeenThisRep[var]++;

	unsigned picsPerRep = input.camSet.picsPerRep;
	unsigned analyzeIdx = input.analyzeImageIndex;

	// select behavior depending on picsPerRep
	if (picsPerRep <= 1) {
		// single-image-per-rep: analyze raw
		analyzeAndStore(img, nullptr, width, height, rep, var);
	}
	else if (picsPerRep == 2) {
		// image0 = background, image1 = signal -> subtract and analyze
		if (curIdxInRep == 0) {
			backgroundImage[var] = img;
			hasBackground[var] = true;
		}
		else if (curIdxInRep == 1) {
			if (!hasBackground[var]) {
				emit error("Missing background image for subtraction");
				return;
			}
			analyzeAndStore(img, &backgroundImage[var], width, height, rep, var);
		}
	}
	else { // picsPerRep > 2
		if (analyzeIdx == 0) {
			// analyze first image raw when it arrives
			if (curIdxInRep == 0) {
				analyzeAndStore(img, nullptr, width, height, rep, var);
			}
			else {
				// ignore other images for analysis
			}
		}
		else {
			// use image0 as background, analyze image at analyzeIdx (after subtraction)
			if (curIdxInRep == 0) {
				backgroundImage[var] = img;
				hasBackground[var] = true;
			}
			else if (curIdxInRep == analyzeIdx) {
				if (!hasBackground[var]) {
					emit error("Missing background image for subtraction");
					return;
				}
				analyzeAndStore(img, &backgroundImage[var], width, height, rep, var);
			}
		}
	}
}

std::vector<double> MOTAnalysisThreadWoker::fit1dGaussian(std::vector<double> Crx)
{
	int width = (int)Crx.size();
	std::vector<double> CrxKey = std::vector<double>(width, 0.0);
	double n = 0.0;
	for (int i = 0; i < width; ++i) CrxKey[i] = n++;
	auto xmin_it = std::min_element(Crx.begin(), Crx.end());
	auto xmax_it = std::max_element(Crx.begin(), Crx.end());
	double a0x = *xmax_it - *xmin_it;
	double b0x = CrxKey.at((size_t)(xmax_it - Crx.begin()));
	double c0x = 0.5 * width;
	double d0x = *xmin_it;
	/* model function: a * exp( -1/2 * [ (t - b) / c ]^2 ) + d */
	Gaussian1DFit fit(width, CrxKey.data(), Crx.data(), a0x, b0x, c0x, d0x);
	fit.solve_system();
	QVector<double> fitParax = fit.fittedPara();
	QVector<double> confi95x = fit.confidence95Interval();
	return std::vector<double>({ fitParax[0],confi95x[0],fitParax[1],confi95x[1],fitParax[2],confi95x[2] });
}

void MOTAnalysisThreadWoker::analyzeAndStore(const QVector<double>& rawImg, const QVector<double>* background, int width, int height, size_t rep, size_t var)
{
	// Compute the final image for analysis
	QVector<double> finalImg = rawImg;
	if (background) {
		finalImg = QVector<double>(rawImg.size());
		for (int i = 0; i < rawImg.size(); ++i) {
			finalImg[i] = rawImg[i] - (*background)[i];
		}
	}

	// Use finalImg for all analysis
	auto it = std::minmax_element(finalImg.begin(), finalImg.end());
	result2d[MOTAnalysisType::type::min][var].push_back(*(it.first));
	result2d[MOTAnalysisType::type::max][var].push_back(*(it.second));

	std::vector<double> CrxX = std::vector<double>(width, 0.0);
	for (size_t idx = 0; idx < (size_t)width; idx++) {
		double tmp = 0.0;
		for (size_t j = 0; j < (size_t)height; j++) {
			tmp += finalImg[idx + j * width]; // sum over Y
		}
		CrxX[idx] = tmp;
	}
	std::vector<double> CrxY = std::vector<double>(height, 0.0);
	for (size_t idx = 0; idx < (size_t)height; idx++) {
		CrxY[idx] = std::accumulate(finalImg.begin() + idx * width, finalImg.begin() + (idx + 1) * width, 0.0); // sum over X
	}

	QFuture<std::vector<double>> futurex = QtConcurrent::run(this, &MOTAnalysisThreadWoker::fit1dGaussian, CrxX);
	QFuture<std::vector<double>> futurey = QtConcurrent::run(this, &MOTAnalysisThreadWoker::fit1dGaussian, CrxY);
	std::vector<double> fitx = futurex.result();
	std::vector<double> fity = futurey.result();
	result2d[MOTAnalysisType::type::meanx][var].push_back(fitx[2]);
	result2d[MOTAnalysisType::type::sigmax][var].push_back(std::abs(fitx[4]));
	result2d[MOTAnalysisType::type::meany][var].push_back(fity[2]);
	result2d[MOTAnalysisType::type::sigmay][var].push_back(std::abs(fity[4]));
	result2d[MOTAnalysisType::type::amplitude][var].push_back((fitx[0] + fity[0]) / 2);

	// Atom number: sum(rawImg) - sum(background) if background available, else sum(rawImg) - min(rawImg) * size
	double sum = std::accumulate(rawImg.begin(), rawImg.end(), 0.0);
	if (background) {
		double bgSum = std::accumulate(background->begin(), background->end(), 0.0);
		sum -= bgSum;
	} else {
		auto rawIt = std::minmax_element(rawImg.begin(), rawImg.end());
		sum -= rawImg.size() * *(rawIt.first);
	}
	result2d[MOTAnalysisType::type::atomNum][var].push_back(sum);

	// perform average for 2d density using finalImg
	if (density2d[var].empty()) {
		density2d[var] = finalImg.toStdVector();
	}
	else {
		auto& den = density2d[var];
		std::transform(den.begin(), den.end(), finalImg.begin(), den.begin(), [rep](double old, double new_) {
			return (old * (rep) + new_) / (rep * 1.0); });
	}
	emit newPlotData2D(density2d[var], width, height, var);

	if (rep != result2d[MOTAnalysisType::allTypes[0]][var].size() - 1) {
		emit error("MOT analysis repetition number is inconsistent with stored result size \n"
			"A low level bug! \r\n");
		qDebug() << "MOTAnalysisThreadWoker::analyzeAndStore -> rep/var inconsistency: " << rep << var << " stored size:" << result2d[MOTAnalysisType::allTypes[0]][var].size() - 1;
		return;
	}

	// check if this variation is finished and is able to do statistics for 1d result
	std::vector<double> mean;
	for (auto& [key, val] : result2d) {
		double tmp = std::accumulate(val[var].begin(), val[var].end(), 0.0);
		mean.push_back(tmp / (val[var].size()));
	}
	if (rep) { // the very first round is finished, able to do statistics
		std::vector<double> stdev;
		for (size_t idx = 0; idx < result2d.size(); idx++)// loop through all analysis types
		{
			double tmp = 0.0;
			for (auto num : result2d[MOTAnalysisType::allTypes[idx]][var]) {
				tmp += (num - mean[idx]) * (num - mean[idx]);
			}
			stdev.push_back(sqrt(tmp / (result2d[MOTAnalysisType::allTypes[idx]][var].size() - 1)));
		}
		emit newPlotData1D(mean, stdev, var);
	}
	else {
		emit newPlotData1D(mean, std::vector<double>(mean.size(), 0.0), var);
	}

	size_t currentNum = 0;
	for (auto val : result2d[MOTAnalysisType::allTypes[0]]) {
		currentNum += val.size();
	}
	if (currentNum == input.camSet.totalPictures()) {
		emit finished();
	}

}

void MOTAnalysisThreadWoker::aborting()
{
	// cleanup and signal thread shutdown
	emit finished();
}