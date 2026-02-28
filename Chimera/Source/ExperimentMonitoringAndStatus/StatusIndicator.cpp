// created by Mark O. Brown
#include "stdafx.h"
#include "StatusIndicator.h"
#include <PrimaryWindows/IChimeraQtWindow.h>

void StatusIndicator::initialize(IChimeraQtWindow* parent ){
	status = new QLabel ("Passively Outputing Default Waveform", parent);
	status->setMinimumSize(360, 36);
	status->setMaximumSize(560, 44);
	status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	status->setStyleSheet ("QLabel {font: bold 14pt;}");
}

void StatusIndicator::setText(std::string text){
	status->setText (cstr (text));
}

void StatusIndicator::setColor(std::string color){
	currentColor = color;
}
