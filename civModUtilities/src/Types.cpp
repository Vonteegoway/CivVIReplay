#include "Types.h"
#include "Crc32b.h"
#include <stdlib.h>


STRING_ALLOCATOR<16*1024> StringAllocator;

FIXED_MEMORY_ALLOCATOR<TABLE::DESCRIPTOR, 256> TableDescriptorAllocator;
FIXED_MEMORY_ALLOCATOR<TABLE, 128> TableAllocator;
DATA_ARRAY<TABLE*> Tables;
CRC_HASH_MAP<TABLE*, 0x400> TablesMap;

FIXED_MEMORY_ALLOCATOR<TABLE::INSTANCE, 256> InstanceAllocator;
FIXED_MEMORY_ALLOCATOR<MODIFICATION, 128> ModificationAllocator;

TABLE* pTableKinds;
TABLE* pTableTypes;
TABLE* pTableModifiers;

TABLE::DESCRIPTOR* TABLE::GetDescritor(const char* pName, unsigned int NameLength) {
	TABLE::DESCRIPTOR** Data = DescriptorMap.Get(Crc32_Low(pName, NameLength));
	return Data ? *Data : NULL;
}

TABLE::DESCRIPTOR* TABLE::GetDescritor(const char* pName, unsigned int NameLength, OUT unsigned int& Index) {
	for (unsigned int iDescriptor = 0; iDescriptor < Descriptors->Count; ++iDescriptor) {
		TABLE::DESCRIPTOR& Descriptor = *Descriptors[iDescriptor];
		if (NameLength == Descriptor.NameLength && !_strnicmp(pName, Descriptor.Name, NameLength)) {
			Index = iDescriptor;
			return &Descriptor;
		}
	}
	Index = -1;
	return NULL;
}

const TABLE::DESCRIPTOR* TABLE::GetDescritor(unsigned int ID) const {
	TABLE::DESCRIPTOR* const* Data = DescriptorMap.Get(ID);
	return Data ? *Data : NULL;
}

const TABLE::DESCRIPTOR* TABLE::GetDescritor(unsigned int ID, OUT unsigned int& Index) const {
	for (unsigned int iDescriptor = 0; iDescriptor < Descriptors->Count; ++iDescriptor) {
		const TABLE::DESCRIPTOR& Descriptor = *Descriptors[iDescriptor];
		if (ID == Descriptor.ID) {
			Index = iDescriptor;
			return &Descriptor;
		}
	}
	Index = -1;
	return NULL;
}

TABLE::DESCRIPTOR* TABLE::CreateDescriptor(const char* pName, unsigned int NameLength) {
	TABLE::DESCRIPTOR*& pDescriptor = Descriptors.Add();
	pDescriptor = &TableDescriptorAllocator.Add();
	DefaultInstance.Values.Add().szValue = NULL;
	//--
	char* szName = StringAllocator.Add(NameLength + 1);
	memcpy(szName, pName, NameLength);
	szName[NameLength] = 0;
	//--
	pDescriptor->Name = szName;
	pDescriptor->NameLength = NameLength;
	pDescriptor->Flags._Raw = 0;
	pDescriptor->ID = Crc32_Low(pName, NameLength);
	//--
	DescriptorMap.Insert(pDescriptor->ID) = pDescriptor;
	//--
	return pDescriptor;
}


TABLE::DESCRIPTOR* TABLE::GetOrCreateDescritor(const char* pName, unsigned int NameLength) {
	TABLE::DESCRIPTOR* pDescriptor = GetDescritor(pName, NameLength);
	if (!pDescriptor)
		return CreateDescriptor(pName, NameLength);
	return pDescriptor;
}

TABLE::DESCRIPTOR* TABLE::GetOrCreateDescritor(const char* pName, unsigned int NameLength, OUT unsigned int& Index) {
	TABLE::DESCRIPTOR* pDescriptor = GetDescritor(pName, NameLength, OUT Index);
	if (!pDescriptor) {
		Index = Descriptors->Count;
		return CreateDescriptor(pName, NameLength);
	}
	return pDescriptor;
}

TABLE::INSTANCE::VALUE* TABLE::INSTANCE::GetValue(const DATA_ARRAY<DESCRIPTOR*>& Descriptors, const char* pName, unsigned int NameLength) {
	CRC ID = Crc32_Low(pName, NameLength);
	for (unsigned int iValue = 0; iValue < Values->Count; ++iValue) {
		const TABLE::DESCRIPTOR& Descriptor = *Descriptors[iValue];
		if (ID == Descriptor.ID) {
			return &Values[iValue];
		}
	}
	return NULL;
}

unsigned int TABLE::INSTANCE::GetValueIndex(const DATA_ARRAY<DESCRIPTOR*>& Descriptors, const char* pName, unsigned int NameLength) {
	CRC ID = Crc32_Low(pName, NameLength);
	for (unsigned int iValue = 0; iValue < Values->Count; ++iValue) {
		const TABLE::DESCRIPTOR& Descriptor = *Descriptors[iValue];
		if (ID == Descriptor.ID) {
			return iValue;
		}
	}
	return -1;
}

TABLE::INSTANCE* TABLE::LookUpInstanceWithConditions(const MODIFICATION::CONDITION* pConditions, unsigned int ConditionCount, bool bOrConditions /*= false*/, OUT unsigned int& iOutInstance) {
	for (unsigned int iInstance = 0; iInstance < Instances->Count; ++iInstance) {
		TABLE::INSTANCE& Instance = *Instances[iInstance];
		//--
		BOOL bValidateAllCondition = true;
		for (unsigned int iCondition = 0; iCondition < ConditionCount; ++iCondition) {
			const MODIFICATION::CONDITION& Condition = pConditions[iCondition];
			//--
			if (Condition.Descriptor == -1) {
				bValidateAllCondition = false;
				continue;
			}
			const TABLE::DESCRIPTOR& Descriptor = *Descriptors[Condition.Descriptor];
			const TABLE::INSTANCE::VALUE& ValueInstance = Instance.Values[Condition.Descriptor];
			//--
			BOOL bOK = false;
			for (unsigned int iValue = 0; iValue < Condition.ValueCount; ++iValue) {
				const MODIFICATION::VALUE& ConditionValue = Condition.Values[iValue];
				switch (Descriptor.Type) {
					case TABLE::DESCRIPTOR::TEXT: {
						if (ValueInstance.szValue && !_strnicmp(ValueInstance.szValue, ConditionValue.pValue, ConditionValue.ValueLength)) {
							bOK = true;
						}
						break;
					}
					case TABLE::DESCRIPTOR::BOOLEAN: {
						if (ValueInstance.bValue == (bool)atoi(ConditionValue.pValue)) {
							bOK = true;
						}
						break;
					}
					case TABLE::DESCRIPTOR::INTEGER: {
						if (ValueInstance.iValue == atoi(ConditionValue.pValue)) {
							bOK = true;
						}
						break;
					}
					case TABLE::DESCRIPTOR::REAL: {
						if (ValueInstance.fValue == atof(ConditionValue.pValue)) {
							bOK = true;
						}
						break;
					}
					case TABLE::DESCRIPTOR::FOREIGN_KEY: {
						if (ValueInstance.szValue && !_strnicmp(ValueInstance.szValue, ConditionValue.pValue, ConditionValue.ValueLength)) {
							bOK = true;
						}
						break;
					}
				}
				//--
				if (bOK)
					break;
			}
			//--
			if (bOrConditions) {
				if (bOK)
					break;
			}
			else {
				if (!bOK) {
					bValidateAllCondition = false;
					break;
				}
			}
		}
		//--
		if (bValidateAllCondition) {
			iOutInstance = iInstance;
			return &Instance;
		}
	}
	iOutInstance = -1;
	return NULL;
}


bool /*bMissing*/ TABLE::SetInstanceValue(TABLE::INSTANCE::VALUE& ValueInstance, int iDescriptor, const TABLE::DESCRIPTOR& Descriptor, const char* szValue, unsigned int ValueLength) {
	bool bMissingFields = false;
	switch (Descriptor.Type) {
		case TABLE::DESCRIPTOR::TEXT: {
			if (szValue) {
				char* szNewValue = StringAllocator.Add(ValueLength + 1);
				memcpy(szNewValue, szValue, ValueLength);
				szNewValue[ValueLength] = 0;
				ValueInstance.szValue = szNewValue;
			} else {
				if (Descriptor.Flags.bDefault) {
					ValueInstance.szValue = DefaultInstance.Values[iDescriptor].szValue;
				} else if (Descriptor.Flags.bNotNull) {
					bMissingFields = true;
					break;
				}
			}
			break;
		}
		case TABLE::DESCRIPTOR::BOOLEAN: {
			if (szValue) {
				ValueInstance.bValue = atoi(szValue);
			} else {
				if (Descriptor.Flags.bDefault) {
					ValueInstance.bValue = atoi(DefaultInstance.Values[iDescriptor].szValue);
				} else if (Descriptor.Flags.bNotNull) {
					bMissingFields = true;
					break;
				}
			}
			break;
		}
		case TABLE::DESCRIPTOR::INTEGER: {
			if (szValue) {
				ValueInstance.iValue = atoi(szValue);
			} else {
				if (Descriptor.Flags.bDefault) {
					ValueInstance.iValue = atoi(DefaultInstance.Values[iDescriptor].szValue);
				} else if (Descriptor.Flags.bNotNull) {
					bMissingFields = true;
					break;
				}
			}
			break;
		}
		case TABLE::DESCRIPTOR::REAL: {
			if (szValue) {
				ValueInstance.fValue = (float)atof(szValue);
			} else {
				if (Descriptor.Flags.bDefault) {
					ValueInstance.fValue = (float)atof(DefaultInstance.Values[iDescriptor].szValue);
				} else if (Descriptor.Flags.bNotNull) {
					bMissingFields = true;
					break;
				}
			}
			break;
		}
		case TABLE::DESCRIPTOR::FOREIGN_KEY: {
			if (szValue) {
				char* szNewValue = StringAllocator.Add(ValueLength + 1);
				memcpy(szNewValue, szValue, ValueLength);
				szNewValue[ValueLength] = 0;
				ValueInstance.szValue = szNewValue;
			} else {
				if (Descriptor.Flags.bDefault) {
					ValueInstance.szValue = DefaultInstance.Values[iDescriptor].szValue;
				} else if (Descriptor.Flags.bNotNull) {
					bMissingFields = true;
					break;
				}
			}
			break;
		}
	}
	return bMissingFields;
}

void TABLE::DeleteInstance(int iInstance) {
	TABLE::INSTANCE* pInstance = Instances[iInstance];
	Instances.Remove(iInstance);
	//--
	if (ID == "Kinds"_crc32_low) {
		unsigned int iHashDescriptor;
		GetDescritor("Hash"_crc32_low, OUT iHashDescriptor);
		const TABLE::DESCRIPTOR& HashDescriptor = *Descriptors[iHashDescriptor];
		VkAssert(HashDescriptor.Type == TABLE::DESCRIPTOR::INTEGER);
		//--
		InstanceMap.Remove(pInstance->Values[iHashDescriptor].iValue);
	} else if (ID == "Types"_crc32_low) {
		unsigned int iHashDescriptor;
		GetDescritor("Hash"_crc32_low, OUT iHashDescriptor);
		const TABLE::DESCRIPTOR& HashDescriptor = *Descriptors[iHashDescriptor];
		VkAssert(HashDescriptor.Type == TABLE::DESCRIPTOR::INTEGER);
		//--
		InstanceMap.Remove(pInstance->Values[iHashDescriptor].iValue);
	} else {
		int iDescriptorPrimary = -1;
		for (unsigned int iDescriptor = 0; iDescriptor < Descriptors->Count; ++iDescriptor) {
			const TABLE::DESCRIPTOR& Descriptor = *Descriptors[iDescriptor];
			//--
			if (Descriptor.Flags.bPrimaryKey && iDescriptorPrimary == -1) {
				iDescriptorPrimary = iDescriptor;
			}
		}
		if (iDescriptorPrimary != -1) {
			if (Descriptors[iDescriptorPrimary]->Type == TABLE::DESCRIPTOR::TEXT || Descriptors[iDescriptorPrimary]->Type == TABLE::DESCRIPTOR::FOREIGN_KEY) {
				if (pInstance->Values[iDescriptorPrimary].szValue) {
					InstanceMap.Remove(~Crc32_Low(pInstance->Values[iDescriptorPrimary].szValue));
				}
			} else {
				int a = 0;
			}
		}
	}
	//--
	InstanceAllocator.Remove(*pInstance);
}


TABLE* GetTable(const char* pName, unsigned int NameLength) {
	TABLE** Data = TablesMap.Get(Crc32_Low(pName, NameLength));
	return Data ? *Data : NULL;
}

DATA_ARRAY<TABLE*>* GetTables() {
	return &Tables;
}

TABLE* GetTable(unsigned int ID) {
	TABLE** Data = TablesMap.Get(ID);
	return Data ? *Data : NULL;
}


TABLE* CreateTable(const char* pName, unsigned int NameLength) {
	TABLE* pTable = &TableAllocator.Add();
	Tables.Add() = pTable;
	//--
	char* szName = StringAllocator.Add(NameLength + 1);
	memcpy(szName, pName, NameLength);
	szName[NameLength] = 0;
	//--
	pTable->Name = szName;
	pTable->NameLength = NameLength;
	pTable->ID = Crc32_Low(pName, NameLength);
	//--
	TablesMap.Insert(pTable->ID) = pTable;
	//--
	return pTable;
}

TABLE* GetOrCreateTable(const char* pName, unsigned int NameLength) {
	TABLE* pTable = GetTable(pName, NameLength);
	if (!pTable)
		return CreateTable(pName, NameLength);
	return pTable;
}

