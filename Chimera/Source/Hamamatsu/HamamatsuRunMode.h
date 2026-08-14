// created by Mark O. Brown
#pragma once

#include <string>
#include <array>

struct HamamatsuRunModes
{
	enum class mode
	{
		Standard = 1,
		Slow = 2
	};
	static const std::array<mode, 2> allModes;
	static std::string toStr ( mode m );
	static mode fromStr ( std::string txt );
};


