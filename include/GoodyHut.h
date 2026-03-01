#pragma once

struct GoodyHut
{
	unsigned int type;
	int playerId;
	int turn;

	GoodyHut()
	{
	}

	GoodyHut(unsigned int _type, int _playerId, int _turn)
	{
		type = _type;
		playerId = _playerId;
		turn = _turn;
	}
};