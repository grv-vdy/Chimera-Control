// created by Mark O. Brown
#pragma once

#include <string>
#include <array>

struct HamamatsuTriggerMode{
	enum class mode	{
		// Each rising external TTL edge captures one frame using the configured exposure time.
		Edge,
		// The external TTL level holds the shutter open; exposure = TTL high-time (exposure edit ignored).
		Level
	};
	static const std::array<mode,2> allModes;
	static std::string toStr ( mode m );
	static mode fromStr ( std::string txt );
	//std::string HamamatsuTriggerModeText ( HamamatsuTriggerMode mode );
	//HamamatsuTriggerMode HamamatsuTriggerModeFromText ( std::string txt );
};





