// created by Mark O. Brown
#include "stdafx.h"
#include "HamamatsuTriggerModes.h"
const std::array<HamamatsuTriggerMode::mode, 2> HamamatsuTriggerMode::allModes = {
	HamamatsuTriggerMode::mode::Edge,
	HamamatsuTriggerMode::mode::Level};

std::string HamamatsuTriggerMode::toStr( HamamatsuTriggerMode::mode m )
{
	if ( m == HamamatsuTriggerMode::mode::Edge )
	{
		// Single-token label (no spaces): the config reader parses this with >>.
		return "Edge";
	}
	else if ( m == HamamatsuTriggerMode::mode::Level )
	{
		return "Level";
	}
	else
	{
		thrower ("HamamatsuTriggerMode not recognized?!");
	}
}


HamamatsuTriggerMode::mode HamamatsuTriggerMode::fromStr ( std::string txt )
{
	// Backwards-compatibility: older configs stored "External" or "Internal"; both now default to Edge.
	if ( txt == "External" || txt == "Internal" )
	{
		return HamamatsuTriggerMode::mode::Edge;
	}
	for ( auto m : allModes )
	{
		if ( txt == toStr ( m ) )
		{
			return m;
		}
	}
	thrower ( "HamamatsuTriggerMode string not recognized?!" );
}


