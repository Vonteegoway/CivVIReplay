#pragma once

struct Player;

struct UnitInfo
{
	unsigned int type;
	unsigned int health;
	unsigned int mp;
	unsigned int range;
	unsigned int attack;
	unsigned int defense;

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(type, health, mp, range, attack, defense);
	}


};

struct Unit
{
	int x;
	int y;
	unsigned int id;
	unsigned int type;
	unsigned int xp;
	unsigned int damage;
	unsigned int army;
	unsigned int level;
	unsigned int fortified;
	Player* player;

	Unit()
	{
		x = 0;
		y = 0;
	}
	bool exist()
	{
		return x != 0xffffd8f1 && y != 0xffffd8f1;
	}

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(x, y, id, type, xp, damage, army, level, fortified);
	}


};