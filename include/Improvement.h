#pragma once

struct Improvement
{
	int x;
	int y;
	unsigned int type;
	unsigned int district;

	Improvement()
	{
	}

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(x, y, district, type);
	}
};