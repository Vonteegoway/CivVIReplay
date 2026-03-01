#pragma once

#include "Crc32b.h"
#include "Tools.h"

typedef unsigned char BYTE;
typedef int           BOOL;

template <typename HASH_MAP_DERIVED, typename KEY, typename DATA, KEY SENTINEL_KEY, KEY TOMBSTONE_KEY, DATA NOT_FOUND_DATA> struct HASH_MAP_BASE: NON_COPYABLE {
	enum {
		MAX_LOAD_FACTOR_COEFF = 4,
		MAX_LOAD_FACTOR_DIV = 5,
	};

	struct PAIR {
		KEY		Key;
		DATA	Data;
	};

	static const DATA gInvalidData;
	unsigned int Count;
	unsigned int OccupiedCount;
	mutable unsigned int CollisionsCount;

	HASH_MAP_BASE() {
		Derived().InitEmpty();
	}

	explicit HASH_MAP_BASE(unsigned int InitialSize) {
		Derived().Init(InitialSize);
	}

	void RemoveAllItems() {
		PAIR EmptyPair = { SENTINEL_KEY, (DATA)NOT_FOUND_DATA };
		const unsigned int DataSize = Derived().Size();
		for (unsigned int iData = 0; iData < DataSize; ++iData) {
			Derived().Data[iData] = EmptyPair;
		}
		Count = 0;
		OccupiedCount = 0;
		CollisionsCount = 0;
	}

	DATA& Insert(const KEY& Key, OUT bool& _bContained) {
		VkAssert(Key != SENTINEL_KEY && Key != TOMBSTONE_KEY);
		Derived().EnsureSize(OccupiedCount + 1);
		PAIR& Pair = *(PAIR*)&__FindImpl<true>(Key);
		const bool bContained = (Pair.Key == Key);
		_bContained = bContained;
		if (!bContained) {
			if (Pair.Key != TOMBSTONE_KEY) {
				++OccupiedCount;
			}
			Pair.Key = Key;
			++Count;
			VkAssert(OccupiedCount * MAX_LOAD_FACTOR_DIV < Derived().Size() * MAX_LOAD_FACTOR_COEFF);
		}
		return Pair.Data;
	}

	DATA& Insert(const KEY& Key) {
		bool bDummy;
		return Insert(Key, OUT bDummy);
	}

	DATA* RemoveImpl(const KEY& Key) {
		VkAssert(Key != SENTINEL_KEY && Key != TOMBSTONE_KEY);
		PAIR& Pair = *(PAIR*)&__FindImpl<false>(Key);
		const bool bContained = Pair.Key == Key;
		if (bContained) {
			--Count;
			Pair.Key = TOMBSTONE_KEY;
			return &Pair.Data;
		}
		else {
			return NULL;
		}
	}

	bool Remove(const KEY& Key) {
		DATA* pData = RemoveImpl(Key);
		if (pData)
			*pData = NOT_FOUND_DATA;
		return pData != NULL ? true : false;
	}

	bool Remove(const KEY& Key, OUT DATA& OldDataCopy) {
		DATA* pData = RemoveImpl(Key);
		if (pData) {
			OldDataCopy = *pData;
			*pData = (DATA)NOT_FOUND_DATA;
		}
		return pData != NULL ? true : false;
	}

	DATA const* Get(const KEY& Key) const {
		const PAIR& Pair = __FindImpl<false>(Key);
		return (Pair.Key == Key) ? &Pair.Data : NULL;
	}

	DATA* Get(const KEY& Key) {
		PAIR& Pair = *(PAIR*)&__FindImpl<false>(Key);
		return (Pair.Key == Key) ? &Pair.Data : NULL;
	}

private:
	template<bool bAcceptTombstones> const PAIR& __FindImpl(const KEY& Key) const {
		VkAssert(Key != SENTINEL_KEY && Key != TOMBSTONE_KEY);
		const unsigned int Hash = Key;
		unsigned int iData = Hash % Derived().Size();
		PAIR const* pFirstTombstone = NULL;
		PAIR const* pResult;
		bool bCollided;
		for (;;) {
			pResult = &Derived().Data[iData];
			bCollided = pResult->Key != SENTINEL_KEY && pResult->Key != Key;
			if (bAcceptTombstones && !pFirstTombstone && pResult->Key == TOMBSTONE_KEY)
				pFirstTombstone = pResult;
			if (bCollided) {
				++CollisionsCount;
			}
			if (!bCollided)
				break;
			iData = (iData + 1) % Derived().Size();
		}
		if (bAcceptTombstones && pResult->Key == SENTINEL_KEY && pFirstTombstone)
			pResult = pFirstTombstone;
		return *pResult;
	}

	const HASH_MAP_DERIVED& Derived() const { return *(HASH_MAP_DERIVED*)this; }
	HASH_MAP_DERIVED& Derived() { return *(HASH_MAP_DERIVED*)this; }
};

template <typename HASH_MAP_DERIVED, typename KEY, typename DATA, KEY SENTINEL_KEY, KEY TOMBSTONE_KEY, DATA NOT_FOUND_DATA>
const DATA HASH_MAP_BASE<HASH_MAP_DERIVED, KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA>::gInvalidData = NOT_FOUND_DATA;

template <typename KEY, typename DATA, unsigned int STATIC_SIZE, KEY SENTINEL_KEY, KEY TOMBSTONE_KEY, DATA NOT_FOUND_DATA> struct HASH_MAP: HASH_MAP_BASE<HASH_MAP<KEY, DATA, STATIC_SIZE, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA>, KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA> {
	typedef HASH_MAP_BASE<HASH_MAP<KEY, DATA, STATIC_SIZE, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA>, KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA> BASE_CLASS;
	enum { SSIZE = STATIC_SIZE * 2 };

	typename BASE_CLASS::PAIR Data[SSIZE];

	HASH_MAP() : BASE_CLASS(SSIZE) {}

	unsigned int Size() const {
		return SSIZE;
	}

	void EnsureSize(unsigned int Size) {
		VkAssert(Size < SSIZE);
	}

	void InitEmpty() {
		BASE_CLASS::RemoveAllItems();
	}

	void Init(unsigned int Size) {
		VkAssert(Size == SSIZE);
		BASE_CLASS::RemoveAllItems();
	}
};

//-------------------------------------------------------------------------------------------------
template <typename KEY, typename DATA, KEY SENTINEL_KEY, KEY TOMBSTONE_KEY, DATA NOT_FOUND_DATA> struct DYN_HASH_MAP : HASH_MAP_BASE<DYN_HASH_MAP<KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA>, KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, (DATA)NOT_FOUND_DATA> {
	typedef HASH_MAP_BASE<DYN_HASH_MAP<KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, NOT_FOUND_DATA>, KEY, DATA, SENTINEL_KEY, TOMBSTONE_KEY, (DATA)NOT_FOUND_DATA> BASE_CLASS;
	typename BASE_CLASS::PAIR InvalidData;
	typename BASE_CLASS::PAIR* Data;
	unsigned int DataSize;
	unsigned int NextGrow;
	//-----------------------------------------------------------------------------------------------
	DYN_HASH_MAP() { }
	//-----------------------------------------------------------------------------------------------
	DYN_HASH_MAP(unsigned int InitialSize) : BASE_CLASS(InitialSize) { }
	//-----------------------------------------------------------------------------------------------
	~DYN_HASH_MAP() {
		if (Data != &InvalidData) {
			delete[]((BYTE*)Data);
		}
	}
	//-----------------------------------------------------------------------------------------------
	void InitEmpty() {
		InvalidData.Key = SENTINEL_KEY;
		InvalidData.Data = (DATA)NOT_FOUND_DATA;
		Data = &InvalidData;
		DataSize = 1;
		NextGrow = 0;
		this->Count = 0;
		this->OccupiedCount = 0;
	}
	//-----------------------------------------------------------------------------------------------
	void Init(int InitialSize) {
		Data = 0;
		DataSize = 0;
		SetInitialSize(InitialSize);
	}
	//-----------------------------------------------------------------------------------------------
	void SetInitialSize(unsigned int InitialSize) {
		VkAssert(InitialSize > 0);
		const unsigned int NewSize = (InitialSize * BASE_CLASS::MAX_LOAD_FACTOR_DIV + BASE_CLASS::MAX_LOAD_FACTOR_COEFF - 1) / BASE_CLASS::MAX_LOAD_FACTOR_COEFF;

		if (NewSize > DataSize) {
			if (Data != &InvalidData) {
				delete ((BYTE*)Data);
			}

			unsigned int GrowSize = (DataSize + 1) * 3 / 2;
			GrowSize = (GrowSize > NewSize) ? GrowSize : NewSize;
			Data = (typename BASE_CLASS::PAIR*) new BYTE[(sizeof(typename BASE_CLASS::PAIR) * GrowSize)];
			VkAssert(Data);
			DataSize = GrowSize;
		}
		RemoveAllItems();
	}
	//-----------------------------------------------------------------------------------------------
	void RemoveAllItemsAndClean() {
		if (Data != &InvalidData) {
			delete ((BYTE*)Data);
		}
		InitEmpty();
	}
	//-----------------------------------------------------------------------------------------------
	void RemoveAllItems() {
		BASE_CLASS::RemoveAllItems();
		NextGrow = DataSize * BASE_CLASS::MAX_LOAD_FACTOR_COEFF / BASE_CLASS::MAX_LOAD_FACTOR_DIV;
	}
	//-----------------------------------------------------------------------------------------------
	void Grow() {
		const unsigned int NewRequestedSize = (DataSize + 1) * 3 / 2;
		DYN_HASH_MAP NewData((NewRequestedSize * BASE_CLASS::MAX_LOAD_FACTOR_DIV + BASE_CLASS::MAX_LOAD_FACTOR_COEFF - 1) / BASE_CLASS::MAX_LOAD_FACTOR_COEFF);

		for (unsigned int iData = 0; iData < DataSize; ++iData) {
			const typename BASE_CLASS::PAIR& Pair = Data[iData];
			if (Pair.Key != SENTINEL_KEY && Pair.Key != TOMBSTONE_KEY) {
				bool bContained;
				NewData.Insert(Pair.Key, OUT bContained) = Pair.Data;
				VkAssert(!bContained);
			}
		}

		VkAssert(this->Count == NewData.Count);
		SwitchWith(NewData);
		NextGrow = (DataSize * BASE_CLASS::MAX_LOAD_FACTOR_COEFF) / BASE_CLASS::MAX_LOAD_FACTOR_DIV;
		VkAssert(BASE_CLASS::Count + 1 <= NextGrow);
	}
	//-----------------------------------------------------------------------------------------------
	void SwitchWith(DYN_HASH_MAP& Other) {
		typename BASE_CLASS::PAIR* TmpData = Other.Data;
		Other.Data = Data == &InvalidData ? &Other.InvalidData : Data;
		Data = TmpData == &Other.InvalidData ? &InvalidData : TmpData;

		Swap(DataSize, Other.DataSize);
		Swap(BASE_CLASS::Count, Other.Count);
		Swap(BASE_CLASS::OccupiedCount, Other.OccupiedCount);
		Swap(BASE_CLASS::CollisionsCount, Other.CollisionsCount);
		Swap(NextGrow, Other.NextGrow);
	}
	//-----------------------------------------------------------------------------------------------
	unsigned int Size() const {
		return DataSize;
	}
	//-----------------------------------------------------------------------------------------------
	void EnsureSize(unsigned int Size) {
		while (Size >= NextGrow) {
			Grow();
		}
	}
	//-----------------------------------------------------------------------------------------------
private:
	DYN_HASH_MAP(const DYN_HASH_MAP&);
	DYN_HASH_MAP& operator=(const DYN_HASH_MAP&);
	//-----------------------------------------------------------------------------------------------
};

template <typename DATA, unsigned int STATIC_SIZE, DATA NOT_FOUND_DATA = DATA(0)> struct CRC_HASH_MAP: HASH_MAP<CRC, DATA, STATIC_SIZE, 0, -1, NOT_FOUND_DATA> {
	typedef HASH_MAP<CRC, DATA, STATIC_SIZE, 0, -1, NOT_FOUND_DATA> PARENT_CLASS;
	CRC_HASH_MAP() {}
};


template <typename DATA, DATA NOT_FOUND_DATA = DATA(0)> struct DYN_CRC_HASH_MAP : DYN_HASH_MAP<CRC, DATA, 0, -1, NOT_FOUND_DATA> {
	typedef DYN_HASH_MAP<CRC, DATA, 0, -1, NOT_FOUND_DATA> PARENT_CLASS;
	DYN_CRC_HASH_MAP() {}
	DYN_CRC_HASH_MAP(unsigned int InitialSize) : PARENT_CLASS(InitialSize) {}
};
