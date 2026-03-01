#include "Types.h"

extern "C" {
#include <../LuaLib/lua.h>
#include <../LuaLib/lualib.h>
#include <../LuaLib/lauxlib.h>
}

#include <Windows.h>

static void* l_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
	if (ptr) {
		if (!nsize) {
			free(ptr);
			return NULL;
		} else {
			return realloc(ptr, nsize);
		}
	} else {
		return malloc(nsize);
	}
}

#define CHECK_LUA_ERROR(x) do if (x) { const char* sError = lua_tostring(pLuaState, -1); VkBreak();} while(0)

CRC_HASH_MAP<char*, 512> IncludeMap;

int LuaInclude(lua_State* pLuaState) {
	const char* szIncludeFile = luaL_checkstring(pLuaState, 1);
	//luaL_requiref();
	//--
	const char* szUtilityDirectories[] = {
		"C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\Base\\Assets\\Maps\\Utility\\",
		"C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\DLC\\Expansion2\\Maps\\Utility\\",
		NULL
	};

	//--
	unsigned int Length = (unsigned int)strlen(szIncludeFile);
	if (Length > 4 && !_strnicmp(szIncludeFile + Length - 4, ".lua", 4)) {
		Length -= 4;
	}
	//--
	CRC IncludeHash = Crc32_Low(szIncludeFile, Length);
	//--
	bool bAlreadyContained;
	char*& szValue = IncludeMap.Insert(IncludeHash, OUT bAlreadyContained);
	//--
	if (!bAlreadyContained) {
		for (int iUltilityDirectory = 0; szUtilityDirectories[iUltilityDirectory]; ++iUltilityDirectory) {
			char Buffer[512];
			snprintf(Buffer, sizeof(Buffer), "%s%.*s.lua", szUtilityDirectories[iUltilityDirectory], Length, szIncludeFile);
			//--
			if (GetFileAttributesA(Buffer) != INVALID_FILE_ATTRIBUTES) {
				//--
				CHECK_LUA_ERROR(luaL_dofile(pLuaState, Buffer));
				//--
				break;
			}
		}
	}
	//--
	return 0;
}

namespace LUA_GAMEINFO {
	struct _ {
		void* p;
	};

	int GetTable(lua_State* pLuaState) {
		_* pGameInfo = (_*)luaL_checkudata(pLuaState, 1, "_GameInfo");
		const char* szTableName = luaL_checkstring(pLuaState, 2);
		//--
		VkAssert(!pGameInfo->p);
		//--
		_* pLuaTable = (_*)lua_newuserdata(pLuaState, sizeof(_));
		TABLE** ppTable = TablesMap.Get(Crc32_Low(szTableName));
		if (ppTable) {
			pLuaTable->p = *ppTable;
		} else {
			pLuaTable->p = NULL;
		}
		luaL_setmetatable(pLuaState, "_GameInfo_Table");
		//--
		return 1;
	}

	int GetInstance(lua_State* pLuaState) {
		_* pLuaTable = (_*)luaL_checkudata(pLuaState, 1, "_GameInfo_Table");
		const char* szInstanceName = luaL_checkstring(pLuaState, 2);
		//--
		VkAssert(pLuaTable->p);
		//--
		TABLE& Table = *(TABLE*)pLuaTable->p;
		//--
		_* pLuaInstance = (_*)lua_newuserdata(pLuaState, sizeof(_));
		TABLE::INSTANCE** ppInstance = Table.InstanceMap.Get(Crc32_Low(szInstanceName));
		if (ppInstance) {
			pLuaInstance->p = *ppInstance;
		} else {
			pLuaInstance->p = NULL;
		}
		luaL_setmetatable(pLuaState, "_GameInfo_Instance");
		//--
		return 1;
	}

	int GetValue(lua_State* pLuaState) {
		_* pLuaInstance = (_*)luaL_checkudata(pLuaState, 1, "_GameInfo_Instance");
		const char* szValueName = luaL_checkstring(pLuaState, 2);
		//--
		VkAssert(pLuaInstance->p);
		//--
		TABLE::INSTANCE& Instance = *(TABLE::INSTANCE*)pLuaInstance->p;
		//--
		CRC ValueNameCrc = Crc32_Low(szValueName);
		switch (ValueNameCrc) {
			case "Index"_crc32_low: {
				if (Instance.pTable == pTableTypes) {
					lua_pushinteger(pLuaState, Instance.Values[1].iValue);
				} else {
					lua_pushinteger(pLuaState, (lua_Integer)&Instance);
				}
				//--
				return 1;
			}
			default: {
				VkBreak();
				break;
			}
		}
		//--
		return 0;
	}

	void Init(lua_State* pLuaState) {
		luaL_newmetatable(pLuaState, "_GameInfo");
		lua_pushcfunction(pLuaState, GetTable);
		lua_setfield(pLuaState, -2, "__index");
		lua_pop(pLuaState, 1);
		//--
		luaL_newmetatable(pLuaState, "_GameInfo_Table");
		lua_pushcfunction(pLuaState, GetInstance);
		lua_setfield(pLuaState, -2, "__index");
		lua_pop(pLuaState, 1);
		//--
		luaL_newmetatable(pLuaState, "_GameInfo_Instance");
		lua_pushcfunction(pLuaState, GetValue);
		lua_setfield(pLuaState, -2, "__index");
		lua_pop(pLuaState, 1);
		//--
		_* pGameInfo = (_*)lua_newuserdata(pLuaState, sizeof(_));
		pGameInfo->p = NULL;
		luaL_setmetatable(pLuaState, "_GameInfo");
		lua_setglobal(pLuaState, "GameInfo");
	}
}

namespace LUA_MAP {
	struct _ {
		void* p;
	};

	int GetGridSize(lua_State* pLuaState) {
		lua_pushinteger(pLuaState, 20);
		lua_pushinteger(pLuaState, 10);
		//--
		return 2;
	}

	int GetMember(lua_State* pLuaState) {
		_* pMap = (_*)luaL_checkudata(pLuaState, 1, "_Map");
		const char* szMemberName = luaL_checkstring(pLuaState, 2);
		//--
		VkAssert(!pMap->p);
		//--
		CRC MemberNameCrc = Crc32_Low(szMemberName);
		switch (MemberNameCrc) {
			case "GetGridSize"_crc32_low: {
				lua_pushcfunction(pLuaState, GetGridSize);
				return 1;
			}
			default: {
				VkBreak();
				break;
			}
		}
		//--
		return 0;
	}

	void Init(lua_State* pLuaState) {
		luaL_newmetatable(pLuaState, "_Map");
		//--
		lua_pushcfunction(pLuaState, GetMember);
		lua_setfield(pLuaState, -2, "__index");
		//--
		lua_pop(pLuaState, 1);
		//--
		_* pMap = (_*)lua_newuserdata(pLuaState, sizeof(_));
		pMap->p = NULL;
		luaL_setmetatable(pLuaState, "_Map");
		lua_setglobal(pLuaState, "Map");
	}
}

namespace LUA_MAPCONFIGURATION {
struct _ {
	void* p;
};

int GetValue(lua_State* pLuaState) {
	const char* szValueName = luaL_checkstring(pLuaState, 1);
	lua_pushinteger(pLuaState, 10);
	//--
	return 2;
}

int GetMember(lua_State* pLuaState) {
	_* pMapConfiguration = (_*)luaL_checkudata(pLuaState, 1, "_MapConfiguration");
	const char* szMemberName = luaL_checkstring(pLuaState, 2);
	//--
	VkAssert(!pMapConfiguration->p);
	//--
	CRC MemberNameCrc = Crc32_Low(szMemberName);
	switch (MemberNameCrc) {
		case "GetValue"_crc32_low: {
			lua_pushcfunction(pLuaState, GetValue);
			return 1;
		}
		default: {
			VkBreak();
			break;
		}
	}
	//--
	return 0;
}

void Init(lua_State* pLuaState) {
	luaL_newmetatable(pLuaState, "_MapConfiguration");
	//--
	lua_pushcfunction(pLuaState, GetMember);
	lua_setfield(pLuaState, -2, "__index");
	//--
	lua_pop(pLuaState, 1);
	//--
	_* pMapConfiguration = (_*)lua_newuserdata(pLuaState, sizeof(_));
	pMapConfiguration->p = NULL;
	luaL_setmetatable(pLuaState, "_MapConfiguration");
	lua_setglobal(pLuaState, "MapConfiguration");
}
};

namespace LUA_TERRAINBUILDER {
struct _ {
	void* p;
};

int GetFractalFlags(lua_State* pLuaState) {
	lua_pushinteger(pLuaState, 10);
	//--
	return 2;
}

int GetMember(lua_State* pLuaState) {
	_* pTerrainBuilder = (_*)luaL_checkudata(pLuaState, 1, "_TerrainBuilder");
	const char* szMemberName = luaL_checkstring(pLuaState, 2);
	//--
	VkAssert(!pTerrainBuilder->p);
	//--
	CRC MemberNameCrc = Crc32_Low(szMemberName);
	switch (MemberNameCrc) {
		case "GetFractalFlags"_crc32_low: {
			lua_pushcfunction(pLuaState, GetFractalFlags);
			return 1;
		}
		default: {
			VkBreak();
			break;
		}
	}
	//--
	return 0;
}

void Init(lua_State* pLuaState) {
	luaL_newmetatable(pLuaState, "_TerrainBuilder");
	//--
	lua_pushcfunction(pLuaState, GetMember);
	lua_setfield(pLuaState, -2, "__index");
	//--
	lua_pop(pLuaState, 1);
	//--
	_* pTerrainBuilder = (_*)lua_newuserdata(pLuaState, sizeof(_));
	pTerrainBuilder->p = NULL;
	luaL_setmetatable(pLuaState, "_TerrainBuilder");
	lua_setglobal(pLuaState, "TerrainBuilder");
}
};

void LuaCall_GenerateMap(const char* FileName) {
	lua_State* pLuaState;
	pLuaState = lua_newstate(l_alloc, NULL);
	luaL_openlibs(pLuaState);
	//--
	lua_pushcfunction(pLuaState, LuaInclude);
	lua_setglobal(pLuaState, "include");
	//--
	LUA_GAMEINFO::Init(pLuaState);
	LUA_MAP::Init(pLuaState);
	LUA_TERRAINBUILDER::Init(pLuaState);
	//--
	//CHECK_LUA_ERROR(luaL_dofile(pLuaState, "C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\Base\\Assets\\Maps\\Utility\\MapEnums.lua"));
	//CHECK_LUA_ERROR(luaL_dofile(pLuaState, "C:\\Program Files (x86)\\Steam\\SteamApps\\common\\Sid Meier's Civilization VI\\Base\\Assets\\Maps\\Utility\\MapUtilities.lua"));
	//--
	{
		//lua_State* pLuaThread = lua_newthread(pLuaState);
		CHECK_LUA_ERROR(luaL_dofile(pLuaState, FileName));
		//--
		lua_getglobal(pLuaState, "GenerateMap");
		CHECK_LUA_ERROR(lua_pcall(pLuaState, 0, 0, 0));
		//--
		lua_close(pLuaState);
	}
	//--
	lua_close(pLuaState);
}