#pragma once
#include "ImGui/imgui.h"

enum TypePlayer
{
	UNKNOWN=0,
	AI,
	USELESS, // 56 ?
	PLAYER,
	RESERVED,
	NONE
};
enum TypeCiv
{
	FULL_CIV = 0x253718B0,
	CITY_STATE = 0xC2577075,
	FREE_CITIES = 0x04E6B0A0,
	TRIBE = 0xD959EC24,
};
enum TypeCityState
{
	NOT_CITY_STATE,
	TRADE,
	RELIGIOUS,
	SCIENTIFIC,
	INDUSTRIAL,
	MILITARISTIC,
	CULTURAL
};


template<class Archive>
void serialize(Archive& archive, ImVec4& v)
{
	archive(v.x, v.y, v.w, v.z);
}

struct PlayerInfo
{
	PlayerInfo()
	{
		pseudo[0] = 0;
		civStr[0] = 0;
		leaderStr[0] = 0;
		locCivName[0] = 0;
		primaryColor = ImVec4(1.f, 0.f, 0.f, 0.7f);
		secondaryColor = ImVec4(0.f, 0.f, 0.f, 0.7f);
		typeCiv = FREE_CITIES;
		typeCityState = NOT_CITY_STATE;
		team = -1;
		pseudoWithSteamId[0] = 0;
	}
	unsigned int team;
	unsigned int iColor;
	unsigned int leader;
	unsigned int civ;
	char		 civStr[512];
	char		 leaderStr[512];
	ImVec4		 primaryColor;
	ImVec4		 secondaryColor;
	TypePlayer	 typePlayer;
	TypeCiv	     typeCiv;
	char		 pseudo[256];
	char		 pseudoWithSteamId[512];
	TypeCityState typeCityState;
	char		 locCivName[512];

	template<class Archive>
	void serialize(Archive& archive)
	{
		archive(team, iColor, leader, civ, civStr, leaderStr, primaryColor, secondaryColor, typePlayer, typeCiv, pseudo, pseudoWithSteamId, typeCityState, locCivName);
	}
};