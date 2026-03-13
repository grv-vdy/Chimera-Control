#pragma once
#include "LowLevel/constants.h"
#include "WieserlabsDDSStructures.h"

const wieserlabsDdsSettings WIESERLABS_DDS_SETTINGS = {
	// safemode
	WIESERLABS_SAFEMODE,
	// IP address
	WIESERLABS_IPADDRESS,
	// IP port
	WIESERLABS_IPPORT,
	// device name
	"Wieserlabs DDS",
	// trigger line for Channel 0 (BNC_IN_A)
	WIESERLABS_CH0_TRIGGER_LINE,
	// trigger line for Channel 1 (BNC_IN_A)
	WIESERLABS_CH1_TRIGGER_LINE,
	// Configuration file delimiter
	"WIESERLABS_DDS"
};