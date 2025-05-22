#pragma once
#include <ParameterSystem/Expression.h>

enum class StaticDDSGrid : size_t
{
	numPERunit = 1,
	numOFunit = 1,
	total = numPERunit * numOFunit
};

struct StaticDDSSettings
{
	std::array<std::array<Expression,2>, size_t((StaticDDSGrid::total))*2> staticDDSs;
	bool ctrlDDS;
};