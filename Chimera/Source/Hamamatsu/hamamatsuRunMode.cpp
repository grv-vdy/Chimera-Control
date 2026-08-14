// created by Mark O. Brown
#include "stdafx.h"
#include "HamamatsuRunMode.h"

const std::array<HamamatsuRunModes::mode, 2> HamamatsuRunModes::allModes = { mode::Standard,
mode::Slow};

std::string HamamatsuRunModes::toStr ( HamamatsuRunModes::mode mode )
{
	if ( mode == HamamatsuRunModes::mode::Slow )
	{
		return "slow-scan";
	}
	else if ( mode == HamamatsuRunModes::mode::Standard )
	{
		return "standard-scan";
	}
	else
	{
		// Never bare-throw here: with no active exception that calls std::terminate(). Use a catchable
		// ChimeraError instead so bad/uninitialized values surface as an error, not a crash.
		thrower ( "ERROR: unrecognized Hamamatsu run mode value: " + str( static_cast<int>( mode ) ) );
	}
}

HamamatsuRunModes::mode HamamatsuRunModes::fromStr ( std::string txt )
{
	for ( auto m : HamamatsuRunModes::allModes )
	{
		if ( txt == toStr ( m ) )
		{
			return m;
		}
	}
	thrower ("Failed to convert to hamamatsu mode from string!");
	return mode::Standard;
}


