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
	// trigger line (not used for DDS)
	std::make_pair(0, 0),
	// Configuration file delimiter
	"WIESERLABS_DDS"
};