// created by Mark O. Brown
#include "stdafx.h"
#include <algorithm>
#include <numeric>
#include "PictureStats.h"
#include <qlayout.h>


// as of right now, the position of this control is not affected by the mode or the trigger mode.
void PictureStats::initialize( IChimeraQtWindow* parent )
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	pictureStatsHeader = new QLabel ("Raw Counts", parent);
	repetitionIndicator = new QLabel ("Repetition ?/?", parent);
	layout->addWidget(pictureStatsHeader);
	layout->addWidget(repetitionIndicator);
	/// Picture labels ////////////////////////////////////////////////////////////
	statsScrollArea = new QScrollArea(this);
	statsScrollArea->setWidgetResizable(true);
	statsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	statsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	statsRowsWidget = new QWidget(statsScrollArea);
	statsRowsLayout = new QGridLayout(statsRowsWidget);
	statsRowsLayout->setContentsMargins(0, 0, 0, 0);
	statsRowsLayout->setHorizontalSpacing(6);
	statsRowsLayout->setVerticalSpacing(2);

	collumnHeaders[0] = new QLabel ("Pic:", parent);
	collumnHeaders[1] = new QLabel ("Max:", parent);
	collumnHeaders[2] = new QLabel ("Min:", parent);
	collumnHeaders[3] = new QLabel ("Avg:", parent);
	collumnHeaders[4] = new QLabel ("Sel:", parent);
	for (auto idx : range(collumnHeaders.size())) {
		statsRowsLayout->addWidget(collumnHeaders[idx], 0, idx);
	}

	picNumberIndicators.resize(MAX_STATS_PICTURES);
	maxCounts.resize(MAX_STATS_PICTURES);
	minCounts.resize(MAX_STATS_PICTURES);
	avgCounts.resize(MAX_STATS_PICTURES);
	selCounts.resize(MAX_STATS_PICTURES);

	for (auto picInc : range(MAX_STATS_PICTURES)) {
		int row = static_cast<int>(picInc) + 1;
		picNumberIndicators[picInc] = new QLabel(cstr("#" + str(picInc + 1) + ":"), statsRowsWidget);
		maxCounts[picInc] = new QLabel("-", statsRowsWidget);
		minCounts[picInc] = new QLabel("-", statsRowsWidget);
		avgCounts[picInc] = new QLabel("-", statsRowsWidget);
		selCounts[picInc] = new QLabel("-", statsRowsWidget);
		statsRowsLayout->addWidget(picNumberIndicators[picInc], row, 0);
		statsRowsLayout->addWidget(maxCounts[picInc], row, 1);
		statsRowsLayout->addWidget(minCounts[picInc], row, 2);
		statsRowsLayout->addWidget(avgCounts[picInc], row, 3);
		statsRowsLayout->addWidget(selCounts[picInc], row, 4);
	}
	statsScrollArea->setWidget(statsRowsWidget);
	layout->addWidget(statsScrollArea);
	setVisiblePictureCount(1); // also sizes the panel height to fit the visible rows
}


void PictureStats::reset(){
	setVisiblePictureCount(visiblePictureCount);
	for (auto& control : maxCounts)	{
		control->setText("-");
	}
	for (auto& control : minCounts)	{
		control->setText ("-");
	}
	for (auto& control : avgCounts)	{
		control->setText ("-");
	}
	for (auto& control : selCounts){
		control->setText ("-");
	}
	repetitionIndicator->setText ( "Repetition ---/---" );
}


void PictureStats::updateType(std::string typeText){
	displayDataType = typeText;
	pictureStatsHeader->setText (cstr(typeText));
}


statPoint PictureStats::getMostRecentStats ( ){
	return mostRecentStat;
}


void PictureStats::setVisiblePictureCount(unsigned count)
{
	visiblePictureCount = std::max(1u, std::min(count, MAX_STATS_PICTURES));
	for (auto picInc : range(picNumberIndicators.size())) {
		bool show = picInc < visiblePictureCount;
		picNumberIndicators[picInc]->setVisible(show);
		maxCounts[picInc]->setVisible(show);
		minCounts[picInc]->setVisible(show);
		avgCounts[picInc]->setVisible(show);
		selCounts[picInc]->setVisible(show);
	}
	// Grow the panel so all visible rows show without scrolling (at least 4, capped at 10).
	if (statsScrollArea != nullptr && !picNumberIndicators.empty() && collumnHeaders[0] != nullptr) {
		const int headerHeight = collumnHeaders[0]->sizeHint().height();
		const int rowHeight = picNumberIndicators[0]->sizeHint().height() + statsRowsLayout->verticalSpacing();
		const unsigned rowsToShow = std::min<unsigned>(std::max(4u, visiblePictureCount), 10u);
		const int targetHeight = headerHeight + rowHeight * static_cast<int>(rowsToShow) + 20;
		statsScrollArea->setMinimumHeight(targetHeight);
		statsScrollArea->setMaximumHeight(targetHeight);
	}
}


std::pair<int, int> PictureStats::update ( Matrix<long> image, unsigned imageNumber, coordinate selectedPixel, 
										   int currentRepetitionNumber, int totalRepetitionCount ){
	repetitionIndicator->setText ( cstr ( "Repetition " + str ( currentRepetitionNumber ) + "/"
												+ str ( totalRepetitionCount ) ) );
	if ( image.size ( ) == 0 ){
		// hopefully this helps with stupid imaging bug...
		return { 0,0 };
	}
	statPoint currentStatPoint;
	currentStatPoint.selv = image ( selectedPixel.row, selectedPixel.column );
	currentStatPoint.minv = 65536;
	currentStatPoint.maxv = 1;
	// for all pixels... find the max and min of the picture.
	for (auto pixel : image)	{
		try	{
			if ( pixel > currentStatPoint.maxv ){
				currentStatPoint.maxv = pixel;
			}
			if ( pixel < currentStatPoint.minv ){
				currentStatPoint.minv = pixel;
			}
		}
		catch ( std::out_of_range& ){
			// I haven't seen this error in a while, but it was a mystery when we did.
			errBox ( "ERROR: caught std::out_of_range while updating picture statistics! experimentImagesInc = "
					 + str ( imageNumber ) + ", pixelInc = " + str ( "NA" ) + ", image.size() = " + str ( image.size ( ) )
					 + ". Attempting to continue..." );
			return { 0,0 };
		}
	}
	currentStatPoint.avgv = std::accumulate ( image.data.begin ( ), image.data.end ( ), 0.0 ) / image.size ( );

	if (imageNumber >= maxCounts.size()) {
		imageNumber = static_cast<unsigned>(maxCounts.size() - 1);
	}
	if ( displayDataType == RAW_COUNTS ){
		maxCounts[ imageNumber ]->setText ( cstr ( currentStatPoint.maxv, 1 ) );
		minCounts[ imageNumber ]->setText ( cstr ( currentStatPoint.minv, 1 ) );
		selCounts[ imageNumber ]->setText ( cstr ( currentStatPoint.selv, 1 ) );
		avgCounts[ imageNumber ]->setText ( cstr ( currentStatPoint.avgv, 5 ) );
		mostRecentStat = currentStatPoint;
	}
	else if ( displayDataType == CAMERA_PHOTONS ){
		statPoint camPoint;
		//double selPhotons, maxPhotons, minPhotons, avgPhotons;
		//if (eEMGainMode)
		if ( false ){
			camPoint = (currentStatPoint - convs.EMGain200BackgroundCount ) * convs.countToCameraPhotonEM200;
		}
		else{
			camPoint = ( currentStatPoint - convs.conventionalBackgroundCount ) * convs.countToCameraPhoton;
		}
		maxCounts[ imageNumber ]->setText ( cstr ( camPoint.maxv, 1 ) );
		minCounts[ imageNumber ]->setText ( cstr ( camPoint.minv, 1 ) );
		selCounts[ imageNumber ]->setText ( cstr ( camPoint.selv, 1 ) );
		avgCounts[ imageNumber ]->setText ( cstr ( camPoint.avgv, 1 ) );
		mostRecentStat = camPoint;
	}
	else if ( displayDataType == ATOM_PHOTONS ){
		statPoint atomPoint;
		//if (eEMGainMode)
		if ( false ){
			atomPoint = ( currentStatPoint - convs.EMGain200BackgroundCount ) * convs.countToScatteredPhotonEM200;
		}
		else{
			atomPoint = ( currentStatPoint - convs.conventionalBackgroundCount ) * convs.countToScatteredPhoton;
		}
		maxCounts[ imageNumber ]->setText ( cstr ( atomPoint.maxv, 1 ) );
		minCounts[ imageNumber ]->setText ( cstr ( atomPoint.minv, 1 ) );
		selCounts[ imageNumber ]->setText ( cstr ( atomPoint.selv, 1 ) );
		avgCounts[ imageNumber ]->setText ( cstr ( atomPoint.avgv, 1 ) );
		mostRecentStat = atomPoint;
	}	
	return { currentStatPoint.minv, currentStatPoint.maxv };
}