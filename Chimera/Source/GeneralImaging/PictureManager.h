// created by Mark O. Brown
#pragma once
#include "PictureControl.h"
#include "RealTimeDataAnalysis/atomGrid.h"
#include "ConfigurationSystems/Version.h"
#include <array>
#include <fstream>
#include "PrimaryWindows/IChimeraQtWindow.h"

class PictureManager : public QWidget
{
	Q_OBJECT
	public:
		static constexpr unsigned MAX_PICTURES = 64;
		PictureManager ( bool histOption, std::string configurationFileDelim, bool autoscaleDefault,
						 Qt::TransformationMode mode );
		void updatePlotData ( );
		void handleSaveConfig(ConfigStream& saveFile);
		void handleOpenConfig(ConfigStream& configFile);
		void initialize(IChimeraQtWindow* widget, int scaleFactor=50);
		void drawGrids(QPainter& painter);
		unsigned getNumberActive( );
		void setParameters( imageParameters parameters );
		void setPalletes(const std::vector<int>& palleteIds);
		void setSoftwareAccumulationOptions ( const std::vector<softwareAccumulationOption>& opts );
		// draw pictures...
		void drawBitmap (Matrix<long> picData, std::pair<int, int> minMax, unsigned whichPicCtrl,
						 std::vector<atomGrid> grids, unsigned pictureNumber,
						 bool includingAnalysisMarkers, QPainter& painter);
		void createPalettes( );
		void handleEditChange( unsigned id );
		void setAlwaysShowGrid(bool showOption, QPainter& painter);
		void redrawPictures(coordinate selectedLocation, std::vector<atomGrid> gridInfo, bool forceGrid, 
			unsigned picNumber, QPainter& painter);
		void setNumberPicturesActive( int numberActive );
		void setDisplayMask(const std::vector<bool>& mask);
		coordinate getSelLocation();
		void setSinglePicture( imageParameters imageParams );
		void setMultiplePictures( imageParameters imageParams, unsigned numberActivePics );
		void setAutoScalePicturesOption(bool autoScaleOption);
		void setSpecialLessThanMin(bool option);
		void setSpecialGreaterThanMax(bool option);
		void resetPictureStorage();
		const std::string configDelim;
		void setTransformationMode (Qt::TransformationMode mode);
		std::array<PictureControl, MAX_PICTURES> pictures;


	private:
		std::array<QVector<QRgb>,4> palettes;
		QVBoxLayout* containerLayout = nullptr;
		QVBoxLayout* overflowLayout = nullptr;
		QGridLayout* picLayout = nullptr;
		QVector<QRgb> inferno, greys;
		QPoint picturesLocation;
		// to be removed
		int picturesWidth;
		int picturesHeight;
		bool autoScalePictures;
		bool specialGreaterThanMax;
		bool specialLessThanMin;
		bool alwaysShowGrid;
		std::vector<bool> displayMask;

		IChimeraQtWindow* parentWin = nullptr;
};

