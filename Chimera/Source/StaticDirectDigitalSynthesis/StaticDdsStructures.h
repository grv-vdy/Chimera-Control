#pragma once
#include <LowLevel/constants.h>
#include <ParameterSystem/Expression.h>

enum class StaticDDSGrid : size_t
{
	numPERunit = 2,
	numOFunit = STATICDDS_NUMBER,
	total = numPERunit * numOFunit
};

struct StaticDDSSettings
{
	std::array<std::array<Expression,2>, size_t(StaticDDSGrid::numPERunit)*size_t((StaticDDSGrid::numOFunit))> staticDDSs;
	std::array<std::string, size_t(StaticDDSGrid::total)> channelNames;
	bool ctrlDDS;
};