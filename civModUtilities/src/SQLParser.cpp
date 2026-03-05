#include "Types.h"
#include <stdlib.h>

struct BBG_PARSER {
	const char* pCur;

	bool IsSpace() {
		return *pCur == ' ' || *pCur == '\t';
	}

	bool IsReturn() {
		return *pCur == '\n' || *pCur == '\r';
	}

	bool IsSeparator() {
		return *pCur == ',' || *pCur == ';' || *pCur == '(' || *pCur == ')' || *pCur == '"' || *pCur == '=';
	}

	void SkipSpaces() {
		while (IsSpace()) ++pCur;
	}

	void SkipSpacesAndReturns() {
		while (IsSpace() || IsReturn()) {
			++pCur;
			if (*pCur == '-' && *(pCur + 1) == '-') {
				ParseComment();
			}
		}
	}

	void SkipToSemiColon() {
		while (*pCur != ';') ++pCur;
	}

	template <unsigned int LENGTH> void CheckAndSkip(const char (&pCheck)[LENGTH]) {
		//VkAssert(!_strnicmp(pCur, pCheck, LENGTH - 1));
		pCur += LENGTH - 1;
	}

	template <unsigned int LENGTH> bool VerifyAndSkip(const char (&pCheck)[LENGTH]) {
		if (!_strnicmp(pCur, pCheck, LENGTH - 1)) {
			pCur += LENGTH - 1;
			return true;
		}
		return false;
	}

	const char* ParseNextWord(OUT unsigned int& Length) {
		SkipSpacesAndReturns();
		Length = 0;
		const char* pStart;
		if (*pCur == '"' || *pCur == '\'') {
			const char Separator = *pCur;
			pStart = ++pCur;
			while (*pCur != Separator) {
				++Length;
				++pCur;
			}
			++pCur;
			if (*pCur == Separator) { // 4 " / '
				++pCur;
				VkAssert(*pCur == Separator);
				++pCur;
			}
		} else {
			pStart = pCur;
			while (!IsSpace() && !IsSeparator() && !IsReturn()) {
				++Length;
				++pCur;
			}
		}
		return pStart;
	}

	const char* ParseNextExpression(OUT unsigned int& Length) {
		SkipSpacesAndReturns();
		Length = 0;
		const char* pStart;
		CheckAndSkip("'");
		pStart = pCur;
		while (*pCur != '\'') {
			++Length;
			++pCur;
		}
		++pCur;
		if (*pCur == '\'') { // 4 '"'
			++pCur;
			VkAssert(*pCur == '\'');
			++pCur;
		}
		return pStart;
	}

	void ParseComment() {
		while (*pCur && *pCur != '\n') ++pCur;
	}

	bool ParseOperator() {
		if (*pCur == '=') {
			++pCur;
			return true;
		} else if (*pCur == '<' && *(pCur + 1) == '>') {
			++pCur;
			++pCur;
			return true;
		}
		return false;
	}

	void ParseCondition(IN_OUT MODIFICATION& Modification) {
		CheckAndSkip("WHERE");
		//--
		while (1) {
			MODIFICATION::CONDITION& Condition = Modification.Conditions.Add();
			Condition.ValueCount = 0;
			unsigned int DescriptorNameLength;
			const char* pDescriptorName = ParseNextWord(OUT DescriptorNameLength);
			if (Modification.pTable)
				Modification.pTable->GetDescritor(pDescriptorName, DescriptorNameLength, OUT Condition.Descriptor);
			else
				Condition.Descriptor = -1;
			//--
			if (Condition.Descriptor == -1)
				//VkBreak();
				int a = 0;
			//--
			SkipSpaces();
			if (ParseOperator()) {
				unsigned int ValueLength;
				const char* pValue = ParseNextWord(OUT ValueLength);
				//--
				char* szValue = StringAllocator.Add(ValueLength + 1);
				memcpy(szValue, pValue, ValueLength);
				szValue[ValueLength] = '\0';
				//--
				MODIFICATION::VALUE& Value = Condition.Values[Condition.ValueCount++];
				Value.pValue = szValue;
				Value.ValueLength = ValueLength;
			} else if (VerifyAndSkip("IN") || VerifyAndSkip("NOT IN")) {
				SkipSpacesAndReturns();
				CheckAndSkip("(");
				SkipSpacesAndReturns();
				while (1) {
					if (VerifyAndSkip("SELECT")) {
						MODIFICATION SubModification;
						unsigned int DescriptorNameLength;
						const char* pDescriptorName = ParseNextWord(OUT DescriptorNameLength);
						SkipSpaces();
						CheckAndSkip("FROM");
						{
							unsigned int DatabaseNameLength;
							const char* pDatabaseName = ParseNextWord(OUT DatabaseNameLength);
							SubModification.pTable = GetTable(pDatabaseName, DatabaseNameLength);
							if (SubModification.pTable) {
								//SubModification.pTable->Modifications.Add() = &SubModification;
								SubModification.pTable->GetDescritor(pDescriptorName, DescriptorNameLength, OUT SubModification.Descriptors.Add());
							}
						}
						SkipSpacesAndReturns();
						ParseCondition(IN_OUT SubModification);
					} else {
						unsigned int ValueLength;
						const char* pValue = ParseNextWord(OUT ValueLength);
						//--
						char* szValue = StringAllocator.Add(ValueLength + 1);
						memcpy(szValue, pValue, ValueLength);
						szValue[ValueLength] = '\0';
						//--
						MODIFICATION::VALUE& Value = Condition.Values[Condition.ValueCount++];
						Value.pValue = szValue;
						Value.ValueLength = ValueLength;
						//--
						SkipSpacesAndReturns();
					}
					//--
					if (*pCur != ',')
						break;
					//--
					++pCur;
				}
				//--
				CheckAndSkip(")");
			} else if (VerifyAndSkip("LIKE")) {
				SkipSpacesAndReturns();
				unsigned int NameLength;
				const char* pName = ParseNextExpression(OUT NameLength);
			}
			//--
			SkipSpacesAndReturns();
			if (VerifyAndSkip("AND")) {
				VkAssert(!Modification.Flags.bOrCondition);
				continue;
			} else if (VerifyAndSkip("OR")) {
				Modification.Flags.bOrCondition = true;
				continue;
			}
			break;
		}
	}

	void ParseUpdate() {
		MODIFICATION& Update = ModificationAllocator.Add();
		Update.Type = MODIFICATION::MODIFICATION_TYPE::UPDATE_TYPE;
		Update.Flags._Raws = 0;
		//--
		bool bIgnore = false;
		if (VerifyAndSkip("OR")) {
			if (VerifyAndSkip(" IGNORE")) {
				bIgnore = true;
			}
		}
		//--
		{
			unsigned int DatabaseNameLength;
			const char* pDatabaseName = ParseNextWord(OUT DatabaseNameLength);
			Update.pTable = GetTable(pDatabaseName, DatabaseNameLength);
			if (Update.pTable)
				Update.pTable->Modifications.Add() = &Update;
		}
		//--
		SkipSpaces();
		CheckAndSkip("SET");
		//--
		DATA_ARRAY<MODIFICATION::VALUE>& Values = Update.ValueGroups.Add();
		//--
		while (1) {
			unsigned int DescriptorNameLength;
			const char* pDescriptorName = ParseNextWord(OUT DescriptorNameLength);
			if (Update.pTable)
				Update.pTable->GetDescritor(pDescriptorName, DescriptorNameLength, OUT Update.Descriptors.Add());
			//--
			SkipSpaces();
			CheckAndSkip("=");
			//--
			unsigned int ValueLength;
			const char* pValue = ParseNextWord(OUT ValueLength);
			//--
			char* szValue = StringAllocator.Add(ValueLength + 1);
			memcpy(szValue, pValue, ValueLength);
			szValue[ValueLength] = '\0';
			//--
			MODIFICATION::VALUE& Value = Values.Add();
			Value.pValue = szValue;
			Value.ValueLength = ValueLength;
			//--
			SkipSpaces();
			//--
			if (*pCur != ',') {
				break;
			}
			//--
			++pCur;
		}
		//--
		SkipSpacesAndReturns();
		ParseCondition(IN_OUT Update);
	}

	void ParseInsert() {
		MODIFICATION& Insert = ModificationAllocator.Add();
		Insert.Type = MODIFICATION::MODIFICATION_TYPE::INSERT_TYPE;
		Insert.Flags._Raws = 0;
		//--
		bool bIgnore = false;
		bool bReplace = false;
		//--
		if (VerifyAndSkip("OR")) {
			if (VerifyAndSkip(" IGNORE INTO")) {
				bIgnore = true;
			} else if (VerifyAndSkip(" REPLACE INTO")) {
				bReplace = true;
			}
		}
		else {
			CheckAndSkip("INTO");
		}
		//--
		{
			unsigned int DatabaseNameLength;
			const char* pDatabaseName = ParseNextWord(OUT DatabaseNameLength);
			Insert.pTable = GetTable(pDatabaseName, DatabaseNameLength);
			if (Insert.pTable)
				Insert.pTable->Modifications.Add() = &Insert;
		}
		//--
		SkipSpacesAndReturns();
		//--
		if (*pCur == '(') {
			++pCur;
			while (1) {
				unsigned int DescriptorNameLength;
				const char* pDescriptorName = ParseNextWord(OUT DescriptorNameLength);
				if (Insert.pTable)
					Insert.pTable->GetDescritor(pDescriptorName, DescriptorNameLength, OUT Insert.Descriptors.Add());
				//--
				SkipSpaces();
				//--
				if (*pCur != ',')
					break;
				//--
				++pCur;
			}
			//--
			CheckAndSkip(")");
		}
		//--
		SkipSpacesAndReturns();
		if (VerifyAndSkip("VALUES")) {
			while (1) {
				SkipSpacesAndReturns();
				CheckAndSkip("(");
				//--
				DATA_ARRAY<MODIFICATION::VALUE>& Values = Insert.ValueGroups.Add();
				//--
				while (1) {
					unsigned int ValueLength;
					const char* pValue = ParseNextWord(OUT ValueLength);
					//--
					char* szValue = StringAllocator.Add(ValueLength + 1);
					memcpy(szValue, pValue, ValueLength);
					szValue[ValueLength] = '\0';
					//--
					MODIFICATION::VALUE& Value = Values.Add();
					Value.pValue = szValue;
					Value.ValueLength = ValueLength;
					//--
					SkipSpaces();
					//--
					if (*pCur != ',')
						break;
					//--
					++pCur;
				}
				//--
				CheckAndSkip(")");
				//--
				if (*pCur != ',')
					break;
				//--
				++pCur;
			}
		} else if (VerifyAndSkip("SELECT")) {
			Insert.Flags.bSelect = true;
			SkipToSemiColon();
		} else {
			VkBreak();
		}
	}

	void ParseDelete() {
		MODIFICATION& Delete = ModificationAllocator.Add();
		Delete.Type = MODIFICATION::MODIFICATION_TYPE::DELETE_TYPE;
		Delete.Flags._Raws = 0;
		//--
		CheckAndSkip("FROM");
		//--
		{
			unsigned int DatabaseNameLength;
			const char* pDatabaseName = ParseNextWord(OUT DatabaseNameLength);
			Delete.pTable = GetTable(pDatabaseName, DatabaseNameLength);
			if (Delete.pTable)
				Delete.pTable->Modifications.Add() = &Delete;
		}
		//--
		SkipSpacesAndReturns();
		ParseCondition(IN_OUT Delete);
	}

	void ParseSql() {
		while (*pCur) {
			switch (*pCur) {
				case '-': {
					if (VerifyAndSkip("--")) {
						ParseComment();
					} else {
						VkBreak();
					}
					break;
				}
				case 'D': {
					if (VerifyAndSkip("DELETE ")) {
						ParseDelete();
					} else if (VerifyAndSkip("DROP TABLE ")) {
						unsigned int TableNameLength;
						const char* pTableName = ParseNextWord(OUT TableNameLength);
					} else {
						VkBreak();
					}
					CheckAndSkip(";");
					break;
				}
				case 'I': {
					if (VerifyAndSkip("INSERT ")) {
						ParseInsert();
					} else {
						//VkBreak();
					}
					CheckAndSkip(";");
					break;
				}
				case 'U': {
					if (VerifyAndSkip("UPDATE ")) {
						ParseUpdate();
					} else {
						VkBreak();
					}
					CheckAndSkip(";");
					break;
				}
				case 'C': {
					if (VerifyAndSkip("CREATE ")) {
						SkipToSemiColon();
					} else {
						VkBreak();
					}
					CheckAndSkip(";");
					break;
				}
				default: {
					ParseComment();
					//VkBreak();
					break;
				}
			}
			//--
			SkipSpacesAndReturns();
		}
	}
};

void UpdateInstances() {
	{
		OBJECT_ARRAY<TABLE::INSTANCE> TemporaryInstances;
		//--
		for (unsigned int iTable = 0; iTable < Tables->Count; ++iTable) {
			TABLE& Table = *Tables[iTable];
			for (unsigned int iModification = 0; iModification < Table.Modifications->Count; ++iModification) {
				MODIFICATION& Modification = *Table.Modifications[iModification];
				switch (Modification.Type) {
					case MODIFICATION::INSERT_TYPE: {
						TemporaryInstances.SetInitialCount(Modification.ValueGroups->Count);
						memset(TemporaryInstances.Data, 0, sizeof(TABLE::INSTANCE) * Modification.ValueGroups->Count);
						for (unsigned int iGroup = 0; iGroup < Modification.ValueGroups->Count; ++iGroup) {
							TABLE::INSTANCE& Instance = TemporaryInstances[iGroup];
							Instance.Flags._Raws = 0;
							Instance.Flags.bInsertedByBBG = true;
							Instance.pTable = &Table;
							Instance.Values.SetInitialCount(Table.Descriptors->Count);
							memset(Instance.Values.Data, 0, sizeof(TABLE::INSTANCE::VALUE) * Table.Descriptors->Count);
						}
						//--
						if (Modification.Flags.bSelect) {
							// ToDo
						} else {
							bool bMustBeUnique = false;
							bool bMissingFields = false;
							//--
							for (unsigned int iDescriptor = 0; iDescriptor < Table.Descriptors->Count; ++iDescriptor) {
								const TABLE::DESCRIPTOR& Descriptor = *Table.Descriptors[iDescriptor];
								//--
								const unsigned int iValueIndex = Modification.Descriptors.FindIndex(iDescriptor);
								const BOOL bOK = iValueIndex != -1;
								//--
								bMustBeUnique |= Descriptor.Flags.bUnique;
								for (unsigned int iGroup = 0; iGroup < Modification.ValueGroups->Count; ++iGroup) {
									TABLE::INSTANCE& Instance = TemporaryInstances[iGroup];
									TABLE::INSTANCE::VALUE& ValueInstance = Instance.Values[iDescriptor];
									if (bOK) {
										const MODIFICATION::VALUE& Value = Modification.ValueGroups[iGroup][iValueIndex];
										bMissingFields |= Table.SetInstanceValue(ValueInstance, iDescriptor, Descriptor, Value.pValue, Value.ValueLength);
									} else {
										bMissingFields |= Table.SetInstanceValue(ValueInstance, iDescriptor, Descriptor, NULL, 0);
									}
								}
							}
						}
						//--
						for (unsigned int iGroup = 0; iGroup < Modification.ValueGroups->Count; ++iGroup) {
							TABLE::INSTANCE& Instance = TemporaryInstances[iGroup];
							//--
							TABLE::INSTANCE& NewInstance = InstanceAllocator.Add();
							memcpy(&NewInstance, &Instance, sizeof(TABLE::INSTANCE));
							memset(&Instance, 0, sizeof(TABLE::INSTANCE));
							Table.Instances.Add() = &NewInstance;
						}
						//Table.InstanceMap.Insert(0) = &NewInstance;
						//--
						break;
					}
					case MODIFICATION::DELETE_TYPE: {
						VkAssert(!Modification.ValueGroups->Count);
						TABLE::INSTANCE* pInstance = Table.LookUpInstanceWithConditions(Modification.Conditions.Data, Modification.Conditions->Count, Modification.Flags.bOrCondition);
						if (pInstance)
							pInstance->Flags.bDeletedByBBG = true;
						break;
					}
					case MODIFICATION::UPDATE_TYPE: {
						TABLE::INSTANCE* pInstance = Table.LookUpInstanceWithConditions(Modification.Conditions.Data, Modification.Conditions->Count, Modification.Flags.bOrCondition);
						if (pInstance) {
							VkAssert(Modification.ValueGroups->Count == 1);
							for (unsigned int iValue = 0; iValue < Modification.ValueGroups[0]->Count; ++iValue) {
								int iDescriptor = Modification.Descriptors[iValue];
								if (iDescriptor == -1){
									continue;
								}
								//--
								const TABLE::DESCRIPTOR& Descriptor = *Table.Descriptors[iDescriptor];
								const MODIFICATION::VALUE& Value = Modification.ValueGroups[0][iValue];
								TABLE::INSTANCE::VALUE& ValueInstance = pInstance->Values[iDescriptor];
								//--
								ValueInstance.PreviousValue.szValue = ValueInstance.szValue;
								Table.SetInstanceValue(ValueInstance, iDescriptor, Descriptor, Value.pValue, Value.ValueLength);
								ValueInstance.pModification = &Modification;
							}
							pInstance->Flags.bUpdatedByBBG = true;
						}
						break;
					}
				}
			}
		}
	}
}

void ParseBBG(IN_OUT const char*& pCur) {
	BBG_PARSER Parser;
	Parser.pCur = pCur;
	Parser.ParseSql();
}