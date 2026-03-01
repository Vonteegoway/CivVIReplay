#pragma once
#include <vector>
#include <map>
#include "Tile.h"
#include "Player.h"
#include "PlayerInfo.h"

#ifndef OUT
#define OUT
#endif
typedef unsigned char byte;

struct CivVIState;
struct CivVIGame;

enum DATA_TYPES
{
	BOOLEAN_=1,
	INTEGER=2,
	POINTER=3,
	STRING=5,
	UTF_STRING=6,
	OBJECT=10,
	ARRAY=11,
	TIMESTAMP=20,
	COMPRESSED=24
};

struct CivVIPacket
{
	unsigned int marker;
	unsigned int type;
	unsigned int length;
	unsigned int info;
	char found; 
	union
	{
		int int_;
		double float_;
		void* pointer;
		bool bool_;
	} data;
	std::map<unsigned int, CivVIPacket> packets;
	void copyString(unsigned int marker, char* pseudo);
	unsigned int getInt(unsigned int marker);
};

struct Node {
	std::vector<unsigned int> parents;
	unsigned int id;
	unsigned int cost;
};

struct CivVIParser
{
public:
	void Write(const char* file_name, const std::vector<byte>& data);
	void ParseAll(const char* file_name);
	void ParseBinaries(const char* file_name);
	void ParseTurn(std::vector<byte>& data, OUT int& turn);
	void ParseVisibility(std::vector<byte>& data, OUT std::vector<Player>& players);
	void ParseMap(std::vector<byte>& data, OUT int& width, OUT std::vector<Tile>& tiles);
	void SetGameInfo(CivVIGame& Game);
	int GetGameSeed();
	void ParsePlayersInfo(CivVIGame& Game); // in civsave
	void ParseTrees(std::vector<byte>& data, std::vector<Node>& civicTree, std::vector<Node>& techTree);
	void ParsePlayers(std::vector<byte>& data, CivVIState* pState); // in binary
	void ParseDisaster(std::vector<byte>& data, int width, std::vector<Tile>& tiles);
	std::vector<CivVIPacket> packets;
	std::vector<std::vector<byte>> compressedData;
	uint64_t timestamp;
private:
	std::vector<byte> buffer;
	std::vector<byte>::iterator it;
	bool ParseEntry(OUT CivVIPacket& result, int iArray = -1);
	void ParsePacketArray(std::vector<byte>::iterator& it, std::vector<CivVIPacket>& packets);

	void ReadBoolean(OUT CivVIPacket& result);
	void ReadInt(OUT CivVIPacket& result);
	void ReadString(OUT CivVIPacket& result);
	void ReadUtfString(OUT CivVIPacket& result);
	void ReadTimestamp(OUT CivVIPacket& result);
	bool ReadCompressed(OUT CivVIPacket& result);
	bool ReadArray(OUT CivVIPacket& result, bool isArray);

};
