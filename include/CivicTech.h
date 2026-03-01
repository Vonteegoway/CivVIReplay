#pragma once
#include <vector>

struct CivicTech {
	std::vector<unsigned int> parents;
	unsigned int id;
	unsigned int cost;
	unsigned int column;
	unsigned int row;
	CivicTech(){}
	CivicTech(unsigned int _id, unsigned int _cost, unsigned int _column, unsigned int _row, unsigned int parentCount, ...)
	{
		id = _id;
		cost = _cost;
		column = _column;
		row = _row;

        va_list args;
        va_start(args, parentCount);
        for(int i=0;i< parentCount;i++)
        {
            parents.push_back(va_arg(args, unsigned int));
        }

        va_end(args);
	}
};