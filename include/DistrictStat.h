#pragma once
#include "string.h"

struct DistrictStat
{
	unsigned int type;
	int playerId;

	int cityId;

	const char* name;
	int turnBegin;
	int turnEnd;
	int cost;
	int globalId;

	DistrictStat()
	{
		turnBegin = -1;
	}

	DistrictStat(unsigned int _type, int _playerId, int _turn, int _cost, int _globalId, int _cityId, const char* _name)
	{
		type = _type;
		playerId = _playerId;
		turnBegin = _turn;
		turnEnd = -1;
		cost = _cost;
		cityId = _cityId;
		globalId = _globalId;
		name = _name;
	}
};