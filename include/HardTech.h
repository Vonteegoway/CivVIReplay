#pragma once

struct HardTech
{
	unsigned int type;
	int playerId;
	int turn;

	HardTech()
	{
	}

	HardTech(unsigned int _type, int _playerId, int _turn)
	{
		type = _type;
		playerId = _playerId;
		turn = _turn;
	}
};