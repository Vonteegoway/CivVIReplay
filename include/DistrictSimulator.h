#pragma once
#include "SaveLoader.h"
#include "District.h"
#include <vector>

struct Viewer;
extern SaveLoader loader;

struct DistrictSimulator
{
public:
	int iCurrentDistrict;
	int iPlayer;
	std::vector<District> districts;
	bool enabled;
	Viewer& v;

	DistrictSimulator(Viewer& _v) : v(_v)
	{
		iPlayer = 0;
		iCurrentDistrict = 0;
		enabled = false;
		districts.reserve(1024);
	}
	void Enable();
	void RemoveAll(bool keepCity);
	void Disable();
	void Draw();
};