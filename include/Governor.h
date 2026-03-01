#pragma once
#include "cereal/types/vector.hpp"
struct Governor
{
	unsigned int id;
	unsigned int type;
	unsigned int player;
	unsigned int city;
	unsigned int turn;
	std::vector<unsigned int> promotions;


	Governor()
	{
		turn = 0;
	}

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(id, type, player, city, turn, CEREAL_NVP(promotions));
	}
};