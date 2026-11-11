// created by Mark O. Brown
#include "stdafx.h"
#include "HamamatsuRunSettings.h"

unsigned __int64 HamamatsuRunSettings::totalPicsInVariation ( ){
	return repetitionsPerVariation * picsPerRepetition;
}

unsigned long long HamamatsuRunSettings::totalPicsInExperiment ( ){
	return  totalPicsInVariation() * totalVariations;
}

