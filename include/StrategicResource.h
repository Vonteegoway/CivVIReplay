#pragma once

struct Resource
{
	unsigned int type;

	unsigned int max;
	unsigned int current;
	unsigned int prod;
	unsigned int get;
	unsigned int give;


	Resource()
	{
		max = 25;
	}

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(type, max, current, prod, get, give);
	}
};