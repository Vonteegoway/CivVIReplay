#pragma once
#include "Tools.h"
#include <memory.h>
//------------------------------------------
#pragma warning(push)
#pragma warning(disable:4285)
#pragma warning(disable:4180)
//------------------------------------------
template <typename INDEX> struct ARRAY_SIZE {
	INDEX Count = 0;
	INDEX Size = 0;
};
//------------------------------------------
template<typename TYPE, typename INDEX, INDEX SIZE>
struct STATIC_ARRAY:ARRAY_SIZE<INDEX>, NON_COPYABLE {
	TYPE* Data = NULL;

	inline void _Init() {
		VkAssert(Data == NULL);
		_Create<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size, SIZE);
	}

	STATIC_ARRAY() { _Init(); }
	~STATIC_ARRAY() { _Destroy<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size); }

	inline unsigned int Add() {
		VkAssert(ARRAY_SIZE<INDEX>::Count < ARRAY_SIZE<INDEX>::Size);
		return ARRAY_SIZE<INDEX>::Count++;
	}

	inline unsigned int Add(const TYPE& Object) {
		VkAssert(ARRAY_SIZE<INDEX>::Count < ARRAY_SIZE<INDEX>::Size);
		Data[ARRAY_SIZE<INDEX>::Count] = Object;
		return ARRAY_SIZE<INDEX>::Count++;
	}

	inline void Remove(INDEX Index) {
		VkAssert(Index < ARRAY_SIZE<INDEX>::Count);
		ARRAY_SIZE<INDEX>::Count -= 1;
		Data[Index] = Data[ARRAY_SIZE<INDEX>::Count];
	}

	inline void RemoveAll() {
		ARRAY_SIZE<INDEX>::Count = 0;
	}

	inline TYPE& operator[](INDEX Index) {
		VkAssert(Index >= 0 && Index < ARRAY_SIZE<INDEX>::Count);
		return Data[Index];
	}

	inline const TYPE& operator[](INDEX Index) const {
		VkAssert(Index >= 0 && Index < ARRAY_SIZE<INDEX>::Count);
		return Data[Index];
	}

	inline void SetInitialCount(INDEX NewCount) {
		VkAssert(NewCount <= ARRAY_SIZE<INDEX>::Size);
		ARRAY_SIZE<INDEX>::Count = NewCount;
	}

	inline const ARRAY_SIZE<INDEX>* operator->() { return (const ARRAY_SIZE<INDEX>*)this; }
};
//------------------------------------------
template<typename TYPE, typename INDEX = unsigned int, INDEX GROW_FACTOR_N = 3, INDEX GROW_FACTOR_D = 2>
struct DATA_ARRAY:ARRAY_SIZE<INDEX>, NON_COPYABLE {
	TYPE* Data;

	DATA_ARRAY(INDEX _Size = 0) {
		if (_Size) {
			_Create<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size, GROW_FACTOR_N);
		} else {
			Data = NULL;
		}
	}

	~DATA_ARRAY() {
		if (Data)
			_Destroy<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size);
	}

	inline void _GrowInternal() {
		if (ARRAY_SIZE<INDEX>::Size) {
			_Grow<TYPE, GROW_FACTOR_N, GROW_FACTOR_D, false>(Data, ARRAY_SIZE<INDEX>::Size);
		} else {
			_Create<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size, GROW_FACTOR_N);
		}
	}

	inline void _ResizeInternal(INDEX NewSize) {
		if (ARRAY_SIZE<INDEX>::Size) {
			_Resize<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size, NewSize);
		} else {
			_Create<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size, NewSize);
		}
	}

	inline TYPE& Add() {
		if (ARRAY_SIZE<INDEX>::Count == ARRAY_SIZE<INDEX>::Size) {
			_GrowInternal();
		}
		return Data[ARRAY_SIZE<INDEX>::Count++];
	}

	inline TYPE& Add(OUT INDEX& Index) {
		if (ARRAY_SIZE<INDEX>::Count == ARRAY_SIZE<INDEX>::Size) {
			_GrowInternal();
		}
		Index = ARRAY_SIZE<INDEX>::Count++;
		return Data[Index];
	}

	inline void AddItems(INDEX ToAdd) {
		if (ARRAY_SIZE<INDEX>::Count + ToAdd >= ARRAY_SIZE<INDEX>::Size) {
			_ResizeInternal(ARRAY_SIZE<INDEX>::Count + ToAdd);
		}
		ARRAY_SIZE<INDEX>::Count += ToAdd;
	}

	inline void Remove(INDEX Index) {
		VkAssert(Index < ARRAY_SIZE<INDEX>::Count);
		ARRAY_SIZE<INDEX>::Count -= 1;
		Data[Index] = Data[ARRAY_SIZE<INDEX>::Count];
	}

	inline void RemoveAndPreserveOrder(INDEX Index, INDEX Size = 1) {
		VkAssert(Index + Size <= ARRAY_SIZE<INDEX>::Count);
		memcpy(Data + Index, Data + Index + Size, sizeof(TYPE) * (ARRAY_SIZE<INDEX>::Count - Index - Size));
		ARRAY_SIZE<INDEX>::Count -= Size;
	}

	inline void RemoveLast() {
		VkAssert(ARRAY_SIZE<INDEX>::Count);
		ARRAY_SIZE<INDEX>::Count -= 1;
	}

	inline INDEX GetIndex(const TYPE& Value) const {
		return (INDEX)(&Value - Data);
	}

	inline INDEX FindIndex(const TYPE& Value) const {
		for (INDEX iIndex = 0; iIndex < ARRAY_SIZE<INDEX>::Count; ++iIndex) {
			if (Data[iIndex] == Value)
				return iIndex;
		}
		return (INDEX)-1;
	}

	inline void RemoveAll() {
		ARRAY_SIZE<INDEX>::Count = 0;
	}

	__forceinline TYPE& operator[](INDEX Index) {
		VkAssert(Index >= 0 && Index < ARRAY_SIZE<INDEX>::Count);
		return Data[Index];
	}

	__forceinline const TYPE& operator[] (INDEX Index) const {
		VkAssert(Index >= 0 && Index < ARRAY_SIZE<INDEX>::Count);
		return Data[Index];
	}

	inline void SetSize(INDEX NewSize) {
		if (NewSize > ARRAY_SIZE<INDEX>::Size) {
			_ResizeInternal(NewSize);
			ARRAY_SIZE<INDEX>::Size = NewSize;
		}
	}

	inline void SetInitialCount(INDEX NewCount) {
		SetSize(NewCount);
		ARRAY_SIZE<INDEX>::Count = NewCount;
	}

	__forceinline const ARRAY_SIZE<INDEX>* operator->() const { return (const ARRAY_SIZE<INDEX>*)this; }
};
//------------------------------------------
template<class TYPE, typename INDEX = unsigned int, INDEX GROW_FACTOR_N = 3, INDEX GROW_FACTOR_D = 2>
struct OBJECT_ARRAY:ARRAY_SIZE<INDEX>, NON_COPYABLE {
	TYPE* Data;

	OBJECT_ARRAY(INDEX _Size = 0) {
		if (_Size) {
			_Create<TYPE, true>(Data, ARRAY_SIZE<INDEX>::Size, GROW_FACTOR_N);
		} else {
			Data = NULL;
		}
	}

	~OBJECT_ARRAY() {
		RemoveAndCleanAll();
		if (Data)
			_Destroy<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size);
	}

	inline void _GrowInternal() {
		if (ARRAY_SIZE<INDEX>::Size) {
			_Grow<TYPE, GROW_FACTOR_N, GROW_FACTOR_D, true>(Data, ARRAY_SIZE<INDEX>::Size);
		} else {
			_Create<TYPE, true>(Data, ARRAY_SIZE<INDEX>::Size, GROW_FACTOR_N);
		}
	}

	inline void _ResizeInternal(INDEX NewSize) {
		if (ARRAY_SIZE<INDEX>::Size) {
			_Resize<TYPE, true>(Data, ARRAY_SIZE<INDEX>::Size, NewSize);
		} else {
			_Create<TYPE, true>(Data, ARRAY_SIZE<INDEX>::Size, NewSize);
		}
	}

	inline TYPE& Add() {
		if (ARRAY_SIZE<INDEX>::Count == ARRAY_SIZE<INDEX>::Size) {
			_GrowInternal();
		}
		new(&Data[ARRAY_SIZE<INDEX>::Count]) TYPE();
		return Data[ARRAY_SIZE<INDEX>::Count++];
	}

	inline TYPE& Add(OUT INDEX& Index) {
		if (ARRAY_SIZE<INDEX>::Count == ARRAY_SIZE<INDEX>::Size) {
			_GrowInternal();
		}
		new(&Data[ARRAY_SIZE<INDEX>::Count]) TYPE();
		Index = ARRAY_SIZE<INDEX>::Count++;
		return Data[Index];
	}

	inline void SetSize(INDEX NewSize) {
		if (NewSize > ARRAY_SIZE<INDEX>::Size) {
			_ResizeInternal(NewSize);
			ARRAY_SIZE<INDEX>::Size = NewSize;
		}
	}

	inline void SetInitialCount(INDEX NewCount) {
		SetSize(NewCount);
		ARRAY_SIZE<INDEX>::Count = NewCount;
	}

	inline void RemoveAndCleanAll() {
#if 0
		if (Count)
			delete[](Data, Count, &Data[0]);
#else
		for (INDEX i = 0; i < ARRAY_SIZE<INDEX>::Count; ++i) {
			Data[i].~TYPE();
		}
#endif
		ARRAY_SIZE<INDEX>::Count = 0;
	}

	inline INDEX GetIndex(const TYPE& Value) const {
		return (INDEX)(&Value - Data);
	}

	inline void Remove(INDEX Index) {
		VkAssert(Index < ARRAY_SIZE<INDEX>::Count);
		ARRAY_SIZE<INDEX>::Count -= 1;
		Data[Index] = Data[ARRAY_SIZE<INDEX>::Count];
	}

	inline TYPE& operator[](INDEX Index) {
		VkAssert(Index >= 0 && Index < ARRAY_SIZE<INDEX>::Count);
		return Data[Index];
	}

	inline const TYPE& operator[](INDEX Index) const {
		VkAssert(Index >= 0 && Index < ARRAY_SIZE<INDEX>::Count);
		return Data[Index];
	}

	inline const ARRAY_SIZE<INDEX>* operator->() const { return (const ARRAY_SIZE<INDEX>*)this; }
};
//------------------------------------------
template<class TYPE, typename INDEX, unsigned int SIZE>
struct STATIC_STACK:ARRAY_SIZE<INDEX>, NON_COPYABLE {
	TYPE* Data;

	STATIC_STACK() { _Create<TYPE, SIZE, false>(Data, ARRAY_SIZE<INDEX>::Size); }
	~STATIC_STACK() { _Destroy<TYPE, false>(Data, ARRAY_SIZE<INDEX>::Size); }

	//inline unsigned int Push() {
	//	assert(Count < Size);
	//	return Count++;
	//}

	inline unsigned int Push(const TYPE& Object) {
		VkAssert(ARRAY_SIZE<INDEX>::Count < ARRAY_SIZE<INDEX>::Size);
		Data[ARRAY_SIZE<INDEX>::Count] = Object;
		return ARRAY_SIZE<INDEX>::Count++;
	}

	inline TYPE& Last() {
		VkAssert(ARRAY_SIZE<INDEX>::Count > 0);
		return Data[ARRAY_SIZE<INDEX>::Count - 1];
	}

	inline void Pop() {
		VkAssert(ARRAY_SIZE<INDEX>::Count > 0);
		ARRAY_SIZE<INDEX>::Count -= 1;
		//return Data[Count];
	}

	inline void RemoveAll() {
		ARRAY_SIZE<INDEX>::Count = 0;
	}

	inline const ARRAY_SIZE<INDEX>* operator->() const { return (const ARRAY_SIZE<INDEX>*)this; }

};
//------------------------------------------
template<class TYPE, typename INDEX, unsigned int SIZE>
struct STATIC_CYCLICAL_ARRAY:ARRAY_SIZE<INDEX>, NON_COPYABLE {
	TYPE Data[SIZE];
	int Start;

	STATIC_CYCLICAL_ARRAY() {
		Start = 0;
		ARRAY_SIZE<INDEX>::Size = SIZE;
	}

	inline TYPE& Push() {
		VkAssert(ARRAY_SIZE<INDEX>::Count < SIZE);
		return Data[ARRAY_SIZE<INDEX>::Count++];
	}

	inline TYPE& Pop() {
		VkAssert(ARRAY_SIZE<INDEX>::Count > 0);
		TYPE& Elt = Data[Start];
		Start = (Start + 1) % SIZE;
		ARRAY_SIZE<INDEX>::Count--;
		return Elt;
	}

	inline const ARRAY_SIZE<INDEX>* operator->() const { return (const ARRAY_SIZE<INDEX>*)this; }
};
//------------------------------------------
template<class TYPE, unsigned int CHUNK_SIZE>
struct FIXED_MEMORY_ALLOCATOR {
//	CT_ASSERT(CHUNK_SIZE > 1);
	typedef char CBOOL;
	//--
	struct CHUNK {
		int Available;
		int FirstFree;
		CBOOL FreeData[CHUNK_SIZE];
		TYPE Data[CHUNK_SIZE];
		//--
		CHUNK() {
			Available = CHUNK_SIZE;
			FirstFree = 0;
			memset(FreeData, true, CHUNK_SIZE);
		}
	};

	DATA_ARRAY<CHUNK*> Chunks;

	inline TYPE& Add() {
		for (unsigned int iChunk = 0; iChunk < Chunks->Count; ++iChunk) {
			CHUNK& Chunk = *Chunks[iChunk];
			if (Chunk.Available != 0) {
				TYPE& NewValue = Chunk.Data[Chunk.FirstFree];
				Chunk.FreeData[Chunk.FirstFree] = false;
				--Chunk.Available;
				//--
				if (Chunk.Available) {
					for (int iData = Chunk.FirstFree + 1; iData < CHUNK_SIZE; ++iData) {
						if (Chunk.FreeData[iData]) {
							Chunk.FirstFree = iData;
							break;
						}
					}
				} else {
					Chunk.FirstFree = -1;
				}
				//--
				return NewValue;
			}
		}
		//--
		CHUNK*& pNewChunk = Chunks.Add();
		pNewChunk = new CHUNK();
		//--
		TYPE& NewValue = pNewChunk->Data[pNewChunk->FirstFree];
		pNewChunk->FreeData[pNewChunk->FirstFree] = false;
		--pNewChunk->Available;
		++pNewChunk->FirstFree;
		//--
		return NewValue;
	}

	inline void Remove(TYPE& Value) {
		for (unsigned int iChunk = 0; iChunk < Chunks->Count; ++iChunk) {
			CHUNK& Chunk = *Chunks[iChunk];
			if (Chunk.Data <= &Value && &Value < Chunk.Data + CHUNK_SIZE) {
				int iData = int(&Value - Chunk.Data);
				++Chunk.Available;
				if (Chunk.FirstFree == -1 || Chunk.FirstFree > iData)
					Chunk.FirstFree = iData;
				Chunk.FreeData[iData] = true;
				return;
			}
		}
		VkBreak();
	}
};
//------------------------------------------
template<unsigned int CHUNK_SIZE>
struct STRING_ALLOCATOR {
	//	CT_ASSERT(CHUNK_SIZE > 1);
	typedef char CBOOL;
	//--
	struct CHUNK {
		struct FREE_INFO {
			char* pNextFree;
			size_t Size;
		};

		union {
			char* pFirstFree;
			FREE_INFO* pFreeInfo;
		};
		char Data[CHUNK_SIZE];
		//--
		CHUNK() {
			pFirstFree = Data;
			pFreeInfo->Size = CHUNK_SIZE;
			pFreeInfo->pNextFree = NULL;
		}
	};

#pragma pack(push, 1)
	struct HEADER {
		char Guard[1];
		unsigned int Length;
	};

	struct TAIL {
		char Guard[1];
	};
#pragma pack(pop)

	DATA_ARRAY<CHUNK*> Chunks;

	inline char* Add(size_t StringSize) {
		if (StringSize < sizeof(CHUNK::FREE_INFO)) {
			StringSize = sizeof(CHUNK::FREE_INFO);
		}
		//--
		size_t AllocSize = StringSize + sizeof(HEADER) + sizeof(TAIL);
		//--
		for (unsigned int iChunk = 0; iChunk < Chunks->Count; ++iChunk) {
			CHUNK& Chunk = *Chunks[iChunk];
			if (!Chunk.pFirstFree)
				continue;
			VkAssert((Chunk.pFirstFree == Chunk.Data) || (*(Chunk.pFirstFree - 1) == '¤'));
			typename CHUNK::FREE_INFO* pPreviousFree = NULL;
			typename CHUNK::FREE_INFO* pFreeInfo = Chunk.pFreeInfo;
			while (pFreeInfo) {
				if (pFreeInfo->Size >= AllocSize) {
					char* szValue;
					if (pFreeInfo->Size - AllocSize < sizeof(CHUNK::FREE_INFO)) {
						szValue = (char*)pFreeInfo;
						//--
						typename CHUNK::FREE_INFO FreeInfo = *pFreeInfo;
						//--
						((HEADER*)szValue)->Guard[0] = '¤';
						((HEADER*)szValue)->Length = (unsigned int)FreeInfo.Size - sizeof(HEADER) - sizeof(TAIL);
						szValue += sizeof(HEADER);
						//--
						((TAIL*)szValue + FreeInfo.Size - sizeof(HEADER) - sizeof(TAIL))->Guard[0] = '¤';
						//--
						if (pPreviousFree) {
							pPreviousFree->pNextFree = FreeInfo.pNextFree;
						} else {
							Chunk.pFirstFree = FreeInfo.pNextFree;
						}
						//--
						return szValue;
					} else {
						szValue = (char*)pFreeInfo;
						//--
						typename CHUNK::FREE_INFO FreeInfo = *pFreeInfo;
						//--
						((HEADER*)szValue)->Guard[0] = '¤';
						((HEADER*)szValue)->Length = (unsigned int)StringSize;
						szValue += sizeof(HEADER);
						//--
						((TAIL*)szValue + StringSize)->Guard[0] = '¤';
						//--
						if (pPreviousFree) {
							pPreviousFree->pNextFree += AllocSize;
							typename CHUNK::FREE_INFO* pNewFreeInfo = (typename CHUNK::FREE_INFO*)pPreviousFree->pNextFree;
							pNewFreeInfo->Size = FreeInfo.Size - AllocSize;
							pNewFreeInfo->pNextFree = FreeInfo.pNextFree;
						} else {
							Chunk.pFirstFree += AllocSize;
							Chunk.pFreeInfo->Size = FreeInfo.Size - AllocSize;
							Chunk.pFreeInfo->pNextFree = FreeInfo.pNextFree;
						}
					}
					//--
					return szValue;
				}
				//--
				pPreviousFree = pFreeInfo;
				pFreeInfo = (typename CHUNK::FREE_INFO*)pFreeInfo->pNextFree;
			}
		}
		//--
		CHUNK*& pNewChunk = Chunks.Add();
		pNewChunk = new CHUNK();
		typename CHUNK::FREE_INFO FreeInfo = *pNewChunk->pFreeInfo;
		pNewChunk->pFirstFree += AllocSize;
		//--
		char* szValue = pNewChunk->Data;
		((HEADER*)szValue)->Guard[0] = '¤';
		((HEADER*)szValue)->Length = (unsigned int)StringSize;
		szValue += sizeof(HEADER);
		//--
		((TAIL*)szValue + StringSize)->Guard[0] = '¤';
		//--
		pNewChunk->pFreeInfo->Size = FreeInfo.Size - AllocSize;
		pNewChunk->pFreeInfo->pNextFree = FreeInfo.pNextFree;
		//--
		return szValue;
	}

	inline void Remove(char*& Value) {
		for (unsigned int iChunk = 0; iChunk < Chunks->Count; ++iChunk) {
			CHUNK& Chunk = *Chunks[iChunk];
			if (Chunk.Data <= Value && Value < Chunk.Data + CHUNK_SIZE) {
				const HEADER& Header = *(HEADER*)(Value - sizeof(HEADER));
				const TAIL& Tail = *(TAIL*)(Value + Header.Length);
				VkAssert(Header.Guard[0] == '¤' && Tail.Guard[0] == '¤');
				//--
				size_t Size = Header.Length + sizeof(HEADER) + sizeof(TAIL);
				char* pPreviousFree = Chunk.pFirstFree;
				Chunk.pFirstFree = (char*)&Header;
				Chunk.pFreeInfo->pNextFree = pPreviousFree;
				Chunk.pFreeInfo->Size = Size;
				return;
			}
		}
		VkBreak();
		return;
	}
};
//------------------------------------------
#pragma warning(pop)
//------------------------------------------
