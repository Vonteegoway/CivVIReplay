#pragma once

#include "Tools.h"
#include <Array.h>
#include "Crc32b.h"
#include <Hash.h>

#include <string.h>

#define BUFFER_SIZE 256
typedef unsigned char       BYTE;

struct TABLE;

struct MODIFICATION {
	enum MODIFICATION_TYPE {
		DELETE_TYPE = 0,
		UPDATE_TYPE,
		INSERT_TYPE
	};
	//--
	struct VALUE {
		const char* pValue;
		unsigned int ValueLength;
	};
	//--
	struct CONDITION {
		unsigned int Descriptor;
		VALUE Values[16];
		unsigned int ValueCount;
	};
	//--
	union {
		struct {
			bool bSelect;
			bool bOrCondition;
		};
		unsigned int _Raws;
	} Flags;
	//--
	MODIFICATION_TYPE Type;
	TABLE* pTable;
	DATA_ARRAY<unsigned int> Descriptors;
	OBJECT_ARRAY<DATA_ARRAY<VALUE>> ValueGroups;
	DATA_ARRAY<CONDITION> Conditions;
};

struct TABLE {
	struct DESCRIPTOR {
		enum TYPE {
			TEXT,
			BOOLEAN,
			INTEGER,
			REAL,
			FOREIGN_KEY,
			FIRAXIS_TYPE_LOCALIZED_TEXT,
		};
		//--
		static const char* GetNameFromType(TYPE Type) {
			switch (Type) {
				case TEXT: return "TEXT";
				case BOOLEAN: return "BOOLEAN";
				case INTEGER: return "INTEGER";
				case REAL: return "REAL";
				case FOREIGN_KEY: return "FOREIGN_KEY";
				case FIRAXIS_TYPE_LOCALIZED_TEXT: return "FIRAXIS_TYPE_LOCALIZED_TEXT";
			}
			return "UNKNOWN";
		}
		//--
		struct FLAGS {
			union {
				struct {
					bool bNotNull:1;
					bool bUnique:1;
					bool bCheck:1;
					bool bDefault:1;
					bool bPrimaryKey:1;
					bool bTypeTable:1;
					bool bModifierTable:1;
				};
				int _Raw;
			};
		};
		//--
		struct EXTERNAL_DESCRIPTOR {
			TABLE* pTable;
			DESCRIPTOR* pDescriptor;
		};
		//--
		const char* Name;
		unsigned int NameLength;
		unsigned int ID;
		TYPE Type;
		FLAGS Flags;
		TABLE* pOtherTable;
		DESCRIPTOR* pDescriptorFromOtherTable;
		DATA_ARRAY<EXTERNAL_DESCRIPTOR> ExternalDescriptors;
	};
	//--
	struct INSTANCE {
		TABLE* pTable;
		//--
		struct VALUE {
			union {
				char* szValue;
				bool bValue;
				int iValue;
				float fValue;
			};
			MODIFICATION* pModification;
			union {
				char* szValue;
				bool bValue;
				int iValue;
				float fValue;
			} PreviousValue;
			INSTANCE* pOtherInstance;
		};
		//--
		union {
			struct {
				bool bInsertedByBBG;
				bool bUpdatedByBBG;
				bool bDeletedByBBG;
			};
			unsigned int _Raws;
		} Flags;
		//--
		DATA_ARRAY<VALUE> Values;
		DATA_ARRAY<INSTANCE*> Childs;
		//--
		VALUE* GetValue(const DATA_ARRAY<DESCRIPTOR*>& Descriptors, const char* pName, unsigned int NameLength);
		unsigned int GetValueIndex(const DATA_ARRAY<DESCRIPTOR*>& Descriptors, const char* pName, unsigned int NameLength);
	};
	//--
	const char* Name;
	unsigned int NameLength;
	unsigned int ID;
	DATA_ARRAY<DESCRIPTOR*> Descriptors;
	INSTANCE DefaultInstance;
	DATA_ARRAY<INSTANCE*> Instances;
	//--
	CRC_HASH_MAP<TABLE::DESCRIPTOR*, 0x100> DescriptorMap;
	DYN_CRC_HASH_MAP<TABLE::INSTANCE*> InstanceMap;
	//--
	DATA_ARRAY<MODIFICATION*> Modifications;
	//--
	DESCRIPTOR* GetDescritor(const char* pName, unsigned int NameLength);
	DESCRIPTOR* GetDescritor(const char* pName, unsigned int NameLength, OUT unsigned int& Index);
	const DESCRIPTOR* GetDescritor(unsigned int ID) const;
	const DESCRIPTOR* GetDescritor(unsigned int ID, OUT unsigned int& Index) const;
	DESCRIPTOR* CreateDescriptor(const char* pName, unsigned int NameLength);
	DESCRIPTOR* GetOrCreateDescritor(const char* pName, unsigned int NameLength);
	DESCRIPTOR* GetOrCreateDescritor(const char* pName, unsigned int NameLength, OUT unsigned int& Index);
	//--
	TABLE::INSTANCE* LookUpInstanceWithConditions(const MODIFICATION::CONDITION* pConditions, unsigned int ConditionCount, bool bOrConditions, OUT unsigned int& iOutInstance);
	inline TABLE::INSTANCE* LookUpInstanceWithConditions(const MODIFICATION::CONDITION* pConditions, unsigned int ConditionCount, bool bOrConditions = false) {
		unsigned int iOutInstance;
		return LookUpInstanceWithConditions(pConditions, ConditionCount, bOrConditions, OUT iOutInstance);
	}
	bool /*bMissing*/ SetInstanceValue(TABLE::INSTANCE::VALUE& ValueInstance, int iDescriptor, const TABLE::DESCRIPTOR& Descriptor, const char* szValue, unsigned int ValueLength);
	void DeleteInstance(int iInstance);
	//--
	TABLE() :InstanceMap(0x100) {

	}
};

extern STRING_ALLOCATOR<16*1024> StringAllocator;

extern FIXED_MEMORY_ALLOCATOR<TABLE::DESCRIPTOR, 256> TableDescriptorAllocator;
extern FIXED_MEMORY_ALLOCATOR<TABLE, 128> TableAllocator;
extern DATA_ARRAY<TABLE*> Tables;
extern CRC_HASH_MAP<TABLE*, 0x400> TablesMap;

extern FIXED_MEMORY_ALLOCATOR<TABLE::INSTANCE, 256> InstanceAllocator;
extern FIXED_MEMORY_ALLOCATOR<MODIFICATION, 128> ModificationAllocator;

extern TABLE* pTableKinds;
extern TABLE* pTableTypes;
extern TABLE* pTableModifiers;

TABLE* CreateTable(const char* pName, unsigned int NameLength);
TABLE* GetOrCreateTable(const char* pName, unsigned int NameLength);

extern "C" __declspec(dllexport) TABLE * GetTable(const char* pName, unsigned int NameLength);
TABLE * GetTable(unsigned int ID);
extern "C" __declspec(dllexport) DATA_ARRAY<TABLE*>* GetTables();