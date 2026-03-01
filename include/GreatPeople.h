#pragma once

struct GreatPeople
{
	GreatPeople()
	{
		cost = 0;
	}
	unsigned int type;
	unsigned int name;
	int cost;
	unsigned int era;
	unsigned int player; // for recruted

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(type, name,cost, era, player);
	}
};
