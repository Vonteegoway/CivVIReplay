#include <Windows.h>
#include <stdio.h>

#include "Types.h"
#include <XML.h>
#include <Logger.h>

//--

// Base: C:\Program Files (x86)\Steam\SteamApps\common\Sid Meier's Civilization VI\Base\Assets\Gameplay\Data
//--
TABLE::INSTANCE::VALUE* CreateKind(TABLE::INSTANCE& Instance) {
	TABLE& Table = *pTableKinds;
	unsigned int iDescriptorHash = Instance.GetValueIndex(Table.Descriptors, "Hash", 4);
	unsigned int iDescriptorKind = Instance.GetValueIndex(Table.Descriptors, "Kind", 4);
	VkAssert(iDescriptorHash != -1 && Table.Descriptors[iDescriptorHash]->Type == TABLE::DESCRIPTOR::TYPE::INTEGER);
	VkAssert(iDescriptorKind != -1 && Table.Descriptors[iDescriptorKind]->Type == TABLE::DESCRIPTOR::TYPE::TEXT);
	//--
	TABLE::INSTANCE::VALUE& HashValue = Instance.Values[iDescriptorHash];
	HashValue.iValue = ~Crc32_Low(Instance.Values[iDescriptorKind].szValue);
	return &HashValue;
}

TABLE::INSTANCE::VALUE* CreateType(TABLE::INSTANCE& Instance) {
	TABLE& Table = *pTableTypes;
	unsigned int iDescriptorHash = Instance.GetValueIndex(Table.Descriptors, "Hash", 4);
	unsigned int iDescriptorType = Instance.GetValueIndex(Table.Descriptors, "Type", 4);
	VkAssert(iDescriptorHash != -1 && Table.Descriptors[iDescriptorHash]->Type == TABLE::DESCRIPTOR::TYPE::INTEGER);
	VkAssert(iDescriptorType != -1 && Table.Descriptors[iDescriptorType]->Type == TABLE::DESCRIPTOR::TYPE::TEXT);
	//--
	TABLE::INSTANCE::VALUE& HashValue = Instance.Values[iDescriptorHash];
	HashValue.iValue = ~Crc32_Low(Instance.Values[iDescriptorType].szValue);
	return &HashValue;
}

void CheckCurrent(XML_NODE& RootNode) {
	//--
	XML_NODE* pGameNode = RootNode.GetNode("GameInfo"_crc32_low);
	if (!pGameNode) {
		pGameNode = RootNode.GetNode("GameData"_crc32_low);
	}
	for (unsigned int iTable = 0; iTable < pGameNode->Nodes->Count; ++iTable) {
		XML_NODE& TableNode = pGameNode->Nodes[iTable];
		TABLE* pCurrentTable = GetTable(TableNode.ElementID);
		//--
		if (!pCurrentTable)
			continue;
		//--
		for (unsigned int iRow = 0; iRow < TableNode.Nodes->Count; ++iRow) {
			XML_NODE& RowNode = TableNode.Nodes[iRow];
			switch (RowNode.ElementID) {
				case "Row"_crc32_low:
				case "InsertOrIgnore"_crc32_low: {
					bool bMustBeUnique = false;
					bool bMissingFields = false;
					unsigned int iDescriptorPrimary = -1;
					//--
					const DATA_ARRAY<TABLE::DESCRIPTOR*>& Descriptors = pCurrentTable->Descriptors;
					//--
					TABLE::INSTANCE Instance;
					Instance.Flags._Raws = 0;
					Instance.pTable = pCurrentTable;
					Instance.Values.SetInitialCount(Descriptors->Count);
					memset(Instance.Values.Data, 0, sizeof(TABLE::INSTANCE::VALUE) * Descriptors->Count);
					//--
					for (unsigned int iDescriptor = 0; iDescriptor < Descriptors->Count; ++iDescriptor) {
						const TABLE::DESCRIPTOR& Descriptor = *Descriptors[iDescriptor];
						TABLE::INSTANCE::VALUE& ValueInstance = Instance.Values[iDescriptor];
						//--
						bMustBeUnique |= Descriptor.Flags.bUnique;
						if (Descriptor.Flags.bPrimaryKey && iDescriptorPrimary == -1) {
							iDescriptorPrimary = iDescriptor;
						}
						//--
						switch (Descriptor.Type) {
							case TABLE::DESCRIPTOR::TEXT: {
								char StringValue[BUFFER_SIZE];
								unsigned int Length;
								bool bOK = RowNode.GetAsString<BUFFER_SIZE>(Descriptor.ID, OUT StringValue, OUT Length);
								if (bOK) {
									char* szValue = StringAllocator.Add(Length + 1);
									memcpy(szValue, StringValue, Length);
									szValue[Length] = 0;
									ValueInstance.szValue = szValue;
								} else {
									if (Descriptor.Flags.bDefault) {
										ValueInstance.szValue = pCurrentTable->DefaultInstance.Values[iDescriptor].szValue;
									} else if (Descriptor.Flags.bNotNull) {
										bMissingFields = true;
										break;
									}
								}
								break;
							}
							case TABLE::DESCRIPTOR::BOOLEAN: {
								bool BoolValue;
								bool bOK = RowNode.GetAsBool(Descriptor.ID, OUT BoolValue);
								if (bOK) {
									ValueInstance.bValue = BoolValue;
								} else {
									if (Descriptor.Flags.bDefault) {
										ValueInstance.bValue = atoi(pCurrentTable->DefaultInstance.Values[iDescriptor].szValue);
									} else if (Descriptor.Flags.bNotNull) {
										bMissingFields = true;
										break;
									}
								}
								break;
							}
							case TABLE::DESCRIPTOR::INTEGER: {
								int IntValue;
								bool bOK = RowNode.GetAsInt32(Descriptor.ID, OUT IntValue);
								if (bOK) {
									ValueInstance.iValue = IntValue;
								} else {
									if (Descriptor.Flags.bDefault) {
										ValueInstance.iValue = atoi(pCurrentTable->DefaultInstance.Values[iDescriptor].szValue);
									} else if (Descriptor.Flags.bNotNull) {
										bMissingFields = true;
										break;
									}
								}
								break;
							}
							case TABLE::DESCRIPTOR::REAL: {
								float FloatValue;
								bool bOK = RowNode.GetAsFloat(Descriptor.ID, OUT FloatValue);
								if (bOK) {
									ValueInstance.fValue = FloatValue;
								} else {
									if (Descriptor.Flags.bDefault) {
										ValueInstance.fValue = (float)atof(pCurrentTable->DefaultInstance.Values[iDescriptor].szValue);
									} else if (Descriptor.Flags.bNotNull) {
										bMissingFields = true;
										break;
									}
								}
								break;
							}
							case TABLE::DESCRIPTOR::FOREIGN_KEY: {
								char StringValue[BUFFER_SIZE];
								unsigned int Length;
								bool bOK = RowNode.GetAsString<BUFFER_SIZE>(Descriptor.ID, OUT StringValue, OUT Length);
								if (bOK) {
									char* szValue = StringAllocator.Add(Length + 1);
									memcpy(szValue, StringValue, Length);
									szValue[Length] = 0;
									ValueInstance.szValue = szValue;
								} else {
									if (Descriptor.Flags.bDefault) {
										ValueInstance.szValue = pCurrentTable->DefaultInstance.Values[iDescriptor].szValue;
									} else if (Descriptor.Flags.bNotNull) {
										bMissingFields = true;
										break;
									}
								}
								break;
							}
						}
					}
					//--
					bool bAlreadyContained = false;
					TABLE::INSTANCE** pToInsert = NULL;
					if (pCurrentTable->ID == "Kinds"_crc32_low) {
						TABLE::INSTANCE::VALUE* pValueHash = CreateKind(Instance);
						pToInsert = &pTableKinds->InstanceMap.Insert(pValueHash->iValue, OUT bAlreadyContained);
					} else if (pCurrentTable->ID == "Types"_crc32_low) {
						TABLE::INSTANCE::VALUE* pValueHash = CreateType(Instance);
						pToInsert = &pTableTypes->InstanceMap.Insert(pValueHash->iValue, OUT bAlreadyContained);
					} else if (iDescriptorPrimary != -1) {
						if (Descriptors[iDescriptorPrimary]->Type == TABLE::DESCRIPTOR::TEXT || Descriptors[iDescriptorPrimary]->Type == TABLE::DESCRIPTOR::FOREIGN_KEY) {
							if (Instance.Values[iDescriptorPrimary].szValue) {
								pToInsert = &pCurrentTable->InstanceMap.Insert(~Crc32_Low(Instance.Values[iDescriptorPrimary].szValue), OUT bAlreadyContained);
							}
						} else {
							int a = 0;
						}
					}
					//--
					if (!bAlreadyContained || !bMustBeUnique) {
						TABLE::INSTANCE& NewInstance = InstanceAllocator.Add();
						memcpy(&NewInstance, &Instance, sizeof(TABLE::INSTANCE));
						memset(&Instance, 0, sizeof(TABLE::INSTANCE));
						pCurrentTable->Instances.Add() = &NewInstance;
						//--
						if (pToInsert)
							*pToInsert = &NewInstance;
					} else {
						for (unsigned int iValue = 0; iValue < Instance.Values->Count; ++iValue) {
							switch (Descriptors[iValue]->Type) {
								case TABLE::DESCRIPTOR::TEXT:
								case TABLE::DESCRIPTOR::FOREIGN_KEY: {
									if (Instance.Values[iValue].szValue) {
										StringAllocator.Remove(Instance.Values[iValue].szValue);
									}
									break;
								}
							}
						}
					}
					break;
				}
				case "Update"_crc32_low: {
					MODIFICATION::CONDITION Conditions[16];
					unsigned int ConditionCount = 0;
					const XML_NODE* pWhereNode = RowNode.GetNode("Where"_crc32_low);
					if (pWhereNode) {
						BOOL bValidCondition = true;
						//--
						for (unsigned int iAttribute = 0; iAttribute < pWhereNode->Attributes->Count; ++iAttribute) {
							const XML_ATTRIBUTE& ConditionNode = pWhereNode->Attributes[iAttribute];
							//--
							MODIFICATION::CONDITION& Condition = Conditions[ConditionCount++];
							Condition.Descriptor = -1;
							pCurrentTable->GetDescritor(ConditionNode.ElementID, OUT Condition.Descriptor);
							Condition.Values[0].pValue = ConditionNode.pStartContent;
							Condition.Values[0].ValueLength = ConditionNode.ContentLength;
							Condition.ValueCount = 1;
							//--
							if (Condition.Descriptor == -1)
								bValidCondition = false;
						}
						for (unsigned int iAttribute = 0; iAttribute < pWhereNode->Nodes->Count; ++iAttribute) {
							const XML_ATTRIBUTE& ConditionNode = pWhereNode->Nodes[iAttribute];
							//--
							MODIFICATION::CONDITION& Condition = Conditions[ConditionCount++];
							Condition.Descriptor = -1;
							pCurrentTable->GetDescritor(ConditionNode.ElementID, OUT Condition.Descriptor);
							Condition.Values[0].pValue = ConditionNode.pStartContent;
							Condition.Values[0].ValueLength = ConditionNode.ContentLength;
							Condition.ValueCount = 1;
							//--
							if (Condition.Descriptor == -1)
								bValidCondition = false;
						}
						//--
						const XML_NODE* pSetNode = RowNode.GetNode("Set"_crc32_low);
						//--
						if (pSetNode) {
							if (bValidCondition) {
								//--
								unsigned int iDescriptorToSet = -1;
								const char* pContentToSet = NULL;
								unsigned int ContentLengthToSet;
								{
									for (unsigned int iAttribute = 0; iAttribute < pSetNode->Attributes->Count; ++iAttribute) {
										const XML_ATTRIBUTE& ToSetNode = pSetNode->Attributes[iAttribute];
										pCurrentTable->GetDescritor(ToSetNode.ElementID, OUT iDescriptorToSet);
										pContentToSet = ToSetNode.pStartContent;
										ContentLengthToSet = ToSetNode.ContentLength;
									}
									for (unsigned int iAttribute = 0; iAttribute < pSetNode->Nodes->Count; ++iAttribute) {
										const XML_ATTRIBUTE& ToSetNode = pSetNode->Nodes[iAttribute];
										pCurrentTable->GetDescritor(ToSetNode.ElementID, OUT iDescriptorToSet);
										pContentToSet = ToSetNode.pStartContent;
										ContentLengthToSet = ToSetNode.ContentLength;
									}
								}
								//--
								if (iDescriptorToSet != -1) {
									TABLE::INSTANCE* pInstance = pCurrentTable->LookUpInstanceWithConditions(Conditions, ConditionCount);
									if (pInstance)
										pCurrentTable->SetInstanceValue(pInstance->Values[iDescriptorToSet], iDescriptorToSet, *pCurrentTable->Descriptors[iDescriptorToSet], pContentToSet, ContentLengthToSet);
								}
							} else {
								VkBreak();
							}

						}
					}
					break;
				}
				case "Delete"_crc32_low: {
					MODIFICATION::CONDITION Conditions[16];
					unsigned int ConditionCount = 0;
					//--
					for (unsigned int iAttribute = 0; iAttribute < RowNode.Attributes->Count; ++iAttribute) {
						const XML_ATTRIBUTE& ConditionNode = RowNode.Attributes[iAttribute];
						MODIFICATION::CONDITION& Condition = Conditions[ConditionCount++];
						Condition.ValueCount = 0;
						pCurrentTable->GetDescritor(ConditionNode.ElementID, OUT Condition.Descriptor);
						Condition.Values[Condition.ValueCount].pValue = ConditionNode.pStartContent;
						Condition.Values[Condition.ValueCount].ValueLength = ConditionNode.ContentLength;
						Condition.ValueCount++;
					}
					for (unsigned int iAttribute = 0; iAttribute < RowNode.Nodes->Count; ++iAttribute) {
						const XML_ATTRIBUTE& ConditionNode = RowNode.Nodes[iAttribute];
						MODIFICATION::CONDITION& Condition = Conditions[ConditionCount++];
						Condition.ValueCount = 0;
						pCurrentTable->GetDescritor(ConditionNode.ElementID, OUT Condition.Descriptor);
						Condition.Values[Condition.ValueCount].pValue = ConditionNode.pStartContent;
						Condition.Values[Condition.ValueCount].ValueLength = ConditionNode.ContentLength;
						Condition.ValueCount++;
					}
					//--
					if (ConditionCount) {
						unsigned int iInstance;
						TABLE::INSTANCE* pInstance = pCurrentTable->LookUpInstanceWithConditions(Conditions, ConditionCount, false, OUT iInstance);
						if (iInstance != -1) {
							pCurrentTable->DeleteInstance(iInstance);
						}
					}
					break;
				}
				case "Replace"_crc32_low: {
					//Not processed, only language and MODE configuration
					break;
				}
				default: {
					VkBreak();
				}
			}
		}
	}
}

void PreConnectGameplay() {
	pTableKinds = GetTable("Kinds"_crc32_low);
	pTableTypes = GetTable("Types"_crc32_low);
	pTableModifiers = GetTable("Modifiers"_crc32_low);
}

int CompareInstanceTypeCB(void const* pA, void const* pB) {
	const TABLE::INSTANCE* InstanceA = *(const TABLE::INSTANCE**)pA;
	const TABLE::INSTANCE* InstanceB = *(const TABLE::INSTANCE**)pB;
	//--
	return strcmp(InstanceA->Values[0].szValue, InstanceB->Values[0].szValue);
}

void CreateTypesUsingTable(TABLE& InputTable, CRC DescriptorToUse, CRC KindToPopulate) {
	const DATA_ARRAY<TABLE::DESCRIPTOR*>& Descriptors = pTableTypes->Descriptors;
	TABLE::INSTANCE* const* ppKindModifierInstance = pTableKinds->InstanceMap.Get(KindToPopulate);
	//--
	unsigned int iModifierTypeDescriptor;
	const TABLE::DESCRIPTOR* pModifierTypeDescriptor = InputTable.GetDescritor(DescriptorToUse, OUT iModifierTypeDescriptor);
	//--
	VkAssert(ppKindModifierInstance && pModifierTypeDescriptor);
	//--
	const TABLE::INSTANCE::VALUE& KindModifiersName = (*ppKindModifierInstance)->Values[0];
	//--
	unsigned int iTypeDescriptor;
	{
		pTableTypes->GetDescritor("Type"_crc32_low, OUT iTypeDescriptor);
		const TABLE::DESCRIPTOR& TypeDescriptor = *Descriptors[iTypeDescriptor];
		VkAssert(TypeDescriptor.Type == TABLE::DESCRIPTOR::TEXT);
	}
	//--
	unsigned int iHashDescriptor;
	{
		pTableTypes->GetDescritor("Hash"_crc32_low, OUT iHashDescriptor);
		const TABLE::DESCRIPTOR& HashDescriptor = *Descriptors[iHashDescriptor];
		VkAssert(HashDescriptor.Type == TABLE::DESCRIPTOR::INTEGER);
	}
	//--
	unsigned int iKindDescriptor;
	{
		pTableTypes->GetDescritor("Kind"_crc32_low, OUT iKindDescriptor);
		const TABLE::DESCRIPTOR& KindDescriptor = *Descriptors[iKindDescriptor];
		VkAssert(KindDescriptor.Type == TABLE::DESCRIPTOR::FOREIGN_KEY);
	}
	//--
	for (unsigned int iInstance = 0; iInstance < InputTable.Instances->Count; ++iInstance) {
		const TABLE::INSTANCE::VALUE& ModifierTypeValue = InputTable.Instances[iInstance]->Values[iModifierTypeDescriptor];
		//--
		if (!ModifierTypeValue.szValue)
			continue;
		//--
		bool bAlreadyContained;
		CRC TypeHash = ~Crc32_Low(ModifierTypeValue.szValue);
		TABLE::INSTANCE** pToInsert = &pTableTypes->InstanceMap.Insert(TypeHash, OUT bAlreadyContained);
		//--
		if (bAlreadyContained)
			continue;
		//--
		TABLE::INSTANCE Instance;
		Instance.Flags._Raws = 0;
		Instance.pTable = pTableTypes;
		Instance.Values.SetInitialCount(Descriptors->Count);
		memset(Instance.Values.Data, 0, sizeof(TABLE::INSTANCE::VALUE) * Descriptors->Count);
		//--
		{
			TABLE::INSTANCE::VALUE& ValueInstance = Instance.Values[iTypeDescriptor];
			//--
			size_t Length = strlen(ModifierTypeValue.szValue);
			char* szValue = StringAllocator.Add(Length + 1);
			memcpy(szValue, ModifierTypeValue.szValue, Length);
			szValue[Length] = 0;
			ValueInstance.szValue = szValue;
		}
		//--
		{
			TABLE::INSTANCE::VALUE& ValueInstance = Instance.Values[iKindDescriptor];
			//--
			size_t Length = strlen(KindModifiersName.szValue);
			char* szValue = StringAllocator.Add(Length + 1);
			memcpy(szValue, KindModifiersName.szValue, Length);
			szValue[Length] = 0;
			ValueInstance.szValue = szValue;
		}
		//--
		Instance.Values[iHashDescriptor].iValue = TypeHash;
		//--
		TABLE::INSTANCE& NewInstance = InstanceAllocator.Add();
		memcpy(&NewInstance, &Instance, sizeof(TABLE::INSTANCE));
		memset(&Instance, 0, sizeof(TABLE::INSTANCE));
		pTableTypes->Instances.Add() = &NewInstance;
		//--
		*pToInsert = &NewInstance;
	}

}

TABLE::INSTANCE* GetKindFromDescriptor(TABLE::DESCRIPTOR& Descriptor) {
	char KindName[256];
	char* pCur = KindName;
	char* pEndCur = pCur + sizeof(KindName);
	pCur += snprintf(KindName, pEndCur - pCur, "KIND");

	const char* pNamePosition = Descriptor.Name;
	unsigned int RemainingLength = Descriptor.NameLength - 4;

	while (RemainingLength) {
		if (*pNamePosition >= 'A' && *pNamePosition <= 'Z') {
			*(pCur++) = '_';
		}
		*(pCur++) = *(pNamePosition++);
		--RemainingLength;
	}
	//--
	*pCur = '\0';
	//--
	TABLE::INSTANCE** ppKindInstance = pTableKinds->InstanceMap.Get(~Crc32_Low(KindName));

	if (ppKindInstance) {
		return *ppKindInstance;
	}
	return NULL;
}

void ConnectGameplay() {
#if 1
	{ // ALL THIS PART SHOULD BE ADDED VIA BBG
		CreateTypesUsingTable(*GetTable("Requirements"_crc32_low), "RequirementType"_crc32_low, ~"KIND_REQUIREMENT"_crc32_low);
		CreateTypesUsingTable(*GetTable("RequirementSets"_crc32_low), "RequirementSetType"_crc32_low, ~"KIND_REQUIREMENTSET"_crc32_low);
	}
	//--
#endif
	//--
	struct TO_CONNECT_DESCRIPTOR {
		TABLE* pTable;
		CRC TableID;
		CRC ModifierID;
	};
	//--
	TO_CONNECT_DESCRIPTOR ToConnectDescriptors[] = {
		{ pTableModifiers, "Modifiers"_crc32_low, "ModifierID"_crc32_low },
		{ pTableTypes, "Type"_crc32_low, "UnitAbilityType"_crc32_low },
		{ pTableTypes, "Type"_crc32_low, "WeaponType"_crc32_low },
		{ pTableTypes, "Type"_crc32_low, "FormationClass"_crc32_low},
	};
	//--
	for (unsigned int iTable = 0; iTable < Tables->Count; ++iTable) {
		TABLE& Table = *Tables[iTable];
		//--
		if (&Table != pTableTypes) { // ALL THIS PART SHOULD BE IN AN EXTERNAL CONFIG FILE
			for (unsigned int iDescriptor = 0; iDescriptor < Table.Descriptors->Count; ++iDescriptor) {
				TABLE::DESCRIPTOR& Descriptor = *Table.Descriptors[iDescriptor];
				if (Descriptor.Type == TABLE::DESCRIPTOR::TEXT) {
					if (!strncmp(Descriptor.Name + Descriptor.NameLength - 4, "Type", 4)) {
						TABLE::INSTANCE* pKindInstance = GetKindFromDescriptor(Descriptor);
						if (pKindInstance) {
							Descriptor.Type = TABLE::DESCRIPTOR::FOREIGN_KEY;
							Descriptor.pOtherTable = pTableTypes;
							Descriptor.pDescriptorFromOtherTable = pTableTypes->Descriptors[0];
							Descriptor.Flags.bTypeTable = true;
							//--
							pTableTypes->Descriptors[0]->ExternalDescriptors.Add() = { &Table, &Descriptor };
							//--
							continue;
						}
					}
					//--
					for (int iToConnectDescriptors = 0; iToConnectDescriptors < ARRAYSIZE(ToConnectDescriptors); ++iToConnectDescriptors) {
						const TO_CONNECT_DESCRIPTOR& ToConnectDescriptor = ToConnectDescriptors[iToConnectDescriptors];
						if (Table.ID != ToConnectDescriptor.TableID && Descriptor.ID == ToConnectDescriptor.ModifierID) {
							Descriptor.Type = TABLE::DESCRIPTOR::FOREIGN_KEY;
							Descriptor.pOtherTable = ToConnectDescriptor.pTable;
							Descriptor.pDescriptorFromOtherTable = ToConnectDescriptor.pTable->Descriptors[0];
							Descriptor.Flags.bModifierTable = true;
							//--
							ToConnectDescriptor.pTable->Descriptors[0]->ExternalDescriptors.Add() = { &Table, &Descriptor };
						}
					}
				} else if (Descriptor.Type == TABLE::DESCRIPTOR::FOREIGN_KEY) {
					if (Descriptor.pOtherTable == pTableTypes) {
						Descriptor.Flags.bTypeTable = true;
					}
				}
			}
		}
		//--
		for (unsigned int iInstance = 0; iInstance < Table.Instances->Count; ++iInstance) {
			TABLE::INSTANCE& Instance = *Table.Instances[iInstance];
			//--
			for (unsigned int iValue = 0; iValue < Instance.Values->Count; ++iValue) {
				TABLE::INSTANCE::VALUE& InstanceValue = Instance.Values[iValue];
				const TABLE::DESCRIPTOR& Descriptor = *Table.Descriptors[iValue];
				if (Descriptor.Type == TABLE::DESCRIPTOR::FOREIGN_KEY) {
					if (!InstanceValue.szValue || !InstanceValue.szValue[0]) {
						continue;
					}
					//--
					bool bFound = false;
					TABLE& OtherTable = *Descriptor.pOtherTable;
					//--
					CRC ValueHash = Crc32_Low(InstanceValue.szValue);
					//--
					if (ValueHash == "NULL"_crc32)
						continue;
					//--
					{
						TABLE::INSTANCE** ppOtherInstance = OtherTable.InstanceMap.Get(~ValueHash);
						if (ppOtherInstance) {
							InstanceValue.pOtherInstance = *ppOtherInstance;
							(*ppOtherInstance)->Childs.Add() = &Instance;
							bFound = true;
						}
					}
					//--
					if (!bFound && &Table != pTableTypes) {
						for (unsigned int iOtherInstance = 0; (iOtherInstance < OtherTable.Instances->Count && !bFound); ++iOtherInstance) {
							TABLE::INSTANCE& OtherInstance = *OtherTable.Instances[iOtherInstance];
							//--
							for (unsigned int iOtherValue = 0; iOtherValue < OtherInstance.Values->Count; ++iOtherValue) {
								const TABLE::DESCRIPTOR& OtherDescriptor = *OtherTable.Descriptors[iOtherValue];
								if (&OtherDescriptor == Descriptor.pDescriptorFromOtherTable) {
									if (OtherDescriptor.Type == TABLE::DESCRIPTOR::TEXT) {
										const TABLE::INSTANCE::VALUE& OtherValue = OtherInstance.Values[iOtherValue];
 										if (OtherValue.szValue && !strcmp(OtherValue.szValue, InstanceValue.szValue)) {
											InstanceValue.pOtherInstance = &OtherInstance;
											OtherInstance.Childs.Add() = &Instance;
											bFound = true;
											break;
										}
									} else if (OtherDescriptor.Type == TABLE::DESCRIPTOR::FOREIGN_KEY) {
										const TABLE::INSTANCE::VALUE& OtherValue = OtherInstance.Values[iOtherValue];
										if (OtherValue.szValue && !strcmp(OtherValue.szValue, InstanceValue.szValue)) {
											InstanceValue.pOtherInstance = &OtherInstance;
											OtherInstance.Childs.Add() = &Instance;
											bFound = true;
											break;
										}
									} else if (OtherDescriptor.Type == TABLE::DESCRIPTOR::INTEGER) {
										const TABLE::INSTANCE::VALUE& OtherValue = OtherInstance.Values[iOtherValue];
										if (OtherValue.iValue == atoi(InstanceValue.szValue)) {
											InstanceValue.pOtherInstance = &OtherInstance;
											OtherInstance.Childs.Add() = &Instance;
											bFound = true;
											break;
										}
									}
								}
							}
						}
					}
					//--
					if (!bFound) {
						LogA("[Civ6ModUtilities]Unresolved foreign constraint %s:%s => %s\n", Table.Name, InstanceValue.szValue, Descriptor.pOtherTable->Name);
//						VkBreak();
					}
				} else {

				}
			}
		}
	}
	//--
	for (unsigned int iInstance = 0; iInstance < pTableKinds->Instances->Count; ++iInstance) {
		TABLE::INSTANCE& Instance = *pTableKinds->Instances[iInstance];
		qsort(Instance.Childs.Data, Instance.Childs->Count, sizeof(Instance.Childs.Data), CompareInstanceTypeCB);
	}
}