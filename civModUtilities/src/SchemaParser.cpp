#include "Types.h"
#include "stdio.h"

struct SCHEMA_PARSER {
	const char* pCur;

	bool IsSpace() {
		return *pCur == ' ' || *pCur == '\t';
	}

	bool IsReturn() {
		return *pCur == '\n' || *pCur == '\r';
	}

	bool IsSeparator() {
		return *pCur == ',' || *pCur == '(' || *pCur == ')' || *pCur == '"';
	}

	void SkipSpaces() {
		while (IsSpace()) ++pCur;
	}

	void SkipSpacesAndReturns() {
		while (IsSpace() || IsReturn()) ++pCur;
	}

	void CheckAndSkip(const char* pCheck, unsigned int Length) {
		VkAssert(!_strnicmp(pCur, pCheck, Length));
		pCur += Length;
	}

	template <unsigned int LENGTH> void CheckAndSkip(const char (&pCheck)[LENGTH]) {
		VkAssert(!_strnicmp(pCur, pCheck, LENGTH - 1));
		pCur += LENGTH - 1;
	}

	const char* ParseNextWord(OUT unsigned int& Length) {
		SkipSpaces();
		Length = 0;
		const char* pStart;
		if (*pCur == '"') {
			pStart = ++pCur;
			while (*pCur != '"') {
				++Length;
				++pCur;
			}
			++pCur;
			if (*pCur == '"') { // 4 '"'
				++pCur;
				VkAssert(*pCur == '"');
				++pCur;
			}
		} else {
			pStart = pCur;
			while (!IsSpace() && !IsSeparator()) {
				++Length;
				++pCur;
			}
		}
		return pStart;
	}

	void ParseComment() {
		while (*pCur != '\n') ++pCur;
	}

	void ParsePragma() {
		SkipSpaces();
		CheckAndSkip("schema_version");
		SkipSpaces();
		CheckAndSkip("=");
		SkipSpaces();
		CheckAndSkip("1000");
		SkipSpaces();

	}

	void ParseCreate() {
		unsigned int Length;
		const char* pWord = ParseNextWord(OUT Length);
		if (Length != 5 && _strnicmp("TABLE", pWord, 5))
			VkBreak();
		//--
		unsigned int LengthTableName;
		const char* pTableName = ParseNextWord(OUT LengthTableName);
		//--
		TABLE& Table = *GetOrCreateTable(pTableName, LengthTableName);
		//--
		SkipSpaces();
		CheckAndSkip("(");
		SkipSpacesAndReturns();
		//--
		while (true) {
			unsigned int LengthMemberName;
			const char* pMemberName = ParseNextWord(OUT LengthMemberName);
			//--
			switch (Crc32_Low(pMemberName, LengthMemberName)) {
				case "PRIMARY"_crc32_low: {
					SkipSpaces();
					CheckAndSkip("KEY");
					//--
					SkipSpaces();
					CheckAndSkip("(");
					SkipSpaces();
					while (true) {
						unsigned int LengthMemberName;
						const char* pMemberName = ParseNextWord(OUT LengthMemberName);
						TABLE::DESCRIPTOR* pDescriptor = Table.GetDescritor(pMemberName, LengthMemberName);
						VkAssert(pDescriptor);
						pDescriptor->Flags.bPrimaryKey = true;
						SkipSpaces();
						//--
						VkAssert(*pCur == ',' || *pCur == ')');
						if (*pCur == ')')
							break;
						//--
						++pCur; // ,
					}
					CheckAndSkip(")");
					//--
					break;
				}
				case "FOREIGN"_crc32_low: {
					SkipSpaces();
					CheckAndSkip("KEY");
					//--
					SkipSpaces();
					CheckAndSkip("(");
					unsigned int LengthMemberName;
					const char* pMemberName = ParseNextWord(OUT LengthMemberName);
					TABLE::DESCRIPTOR* pDescriptor = Table.GetDescritor(pMemberName, LengthMemberName);
					VkAssert(pDescriptor);
					CheckAndSkip(")");
					SkipSpaces();
					//--
					CheckAndSkip("REFERENCES");
					//--
					SkipSpaces();
					unsigned int LengthOtherTableName;
					const char* pOtherTableName = ParseNextWord(OUT LengthOtherTableName);
					TABLE* pOtherTable = GetOrCreateTable(pOtherTableName, LengthOtherTableName);
					VkAssert(pOtherTable);
					CheckAndSkip("(");
					unsigned int LengthOtherTableMemberName;
					const char* pOtherTableMemberName = ParseNextWord(OUT LengthOtherTableMemberName);
					TABLE::DESCRIPTOR* pOtherDescriptor = pOtherTable->GetOrCreateDescritor(pOtherTableMemberName, LengthOtherTableMemberName);
					VkAssert(pOtherDescriptor);
					pDescriptor->Type = TABLE::DESCRIPTOR::FOREIGN_KEY;
					pDescriptor->pOtherTable = pOtherTable;
					pDescriptor->pDescriptorFromOtherTable = pOtherDescriptor;
					pOtherDescriptor->ExternalDescriptors.Add() = { &Table, pDescriptor };
					CheckAndSkip(")");
					SkipSpaces();
					//--
					CheckAndSkip("ON DELETE");
					SkipSpaces();
					unsigned int LengthOnDeleteAction;
					const char* pOnDeleteAction = ParseNextWord(OUT LengthOnDeleteAction);
					switch (Crc32_Low(pOnDeleteAction, LengthOnDeleteAction)) {
						case "CASCADE"_crc32_low: {
							break;
						}
						case "RESTRICT"_crc32_low: {
							break;
						}
						case "SET"_crc32_low: {
							SkipSpaces();
							unsigned int LengthOnDeleteSetAction;
							const char* pOnDeleteSetAction = ParseNextWord(OUT LengthOnDeleteSetAction);
							switch (Crc32_Low(pOnDeleteSetAction, LengthOnDeleteSetAction)) {
								case "DEFAULT"_crc32_low: {
									break;
								}
								case "NULL"_crc32_low: {
									break;
								}
								default: {
									VkBreak();
								}
							}
							break;
						}
						default: {
							VkBreak();
						}
					}
					SkipSpaces();
					CheckAndSkip("ON UPDATE");
					SkipSpaces();
					unsigned int LengthOnUpdateAction;
					const char* pOnUpdateAction = ParseNextWord(OUT LengthOnUpdateAction);
					switch (Crc32_Low(pOnUpdateAction, LengthOnUpdateAction)) {
						case "CASCADE"_crc32_low: {
							break;
						}
						case "RESTRICT"_crc32_low: {
							break;
						}
						case "SET"_crc32_low: {
							SkipSpaces();
							unsigned int LengthOnUpdateSetAction;
							const char* pOnUpdateSetAction = ParseNextWord(OUT LengthOnUpdateSetAction);
							switch (Crc32_Low(pOnUpdateSetAction, LengthOnUpdateSetAction)) {
								case "DEFAULT"_crc32_low: {
									break;
								}
								case "NULL"_crc32_low: {
									break;
								}
								default: {
									VkBreak();
								}
							}
							break;
						}
						default: {
							VkBreak();
						}
					}
					//--
					break;
				}
				default: {
					unsigned int LengthTypeName;
					const char* pTypeName = ParseNextWord(OUT LengthTypeName);
					//--
					SkipSpaces();
					//--
					unsigned int Index;
					TABLE::DESCRIPTOR& Descriptor = *Table.GetOrCreateDescritor(pMemberName, LengthMemberName, OUT Index);
					//--
					switch (Crc32(pTypeName, LengthTypeName)) {
						case "TEXT"_crc32: Descriptor.Type = TABLE::DESCRIPTOR::TEXT; break;
						case "BOOLEAN"_crc32: Descriptor.Type = TABLE::DESCRIPTOR::BOOLEAN; break;
						case "INTEGER"_crc32: Descriptor.Type = TABLE::DESCRIPTOR::INTEGER; break;
						case "REAL"_crc32: Descriptor.Type = TABLE::DESCRIPTOR::REAL; break;
						case "FOREIGN_KEY"_crc32: Descriptor.Type = TABLE::DESCRIPTOR::FOREIGN_KEY; break;
						case "LocalizedText"_crc32: Descriptor.Type = TABLE::DESCRIPTOR::FIRAXIS_TYPE_LOCALIZED_TEXT; break;
						default: VkBreak(); break;
					}
					//--
					if (!_strnicmp(pCur, "NOT NULL", 8)) {
						pCur += 8;
						Descriptor.Flags.bNotNull = true;
					}
					//--
					SkipSpaces();
					//--
					if (!_strnicmp(pCur, "UNIQUE", 6)) {
						pCur += 6;
						Descriptor.Flags.bUnique = true;
					}
					//--
					SkipSpaces();
					//--
					if (!_strnicmp(pCur, "CHECK", 5)) { // Just skipping check as it seems to only be used for BOOLEAN
						pCur += 5;
						//Descriptor.Flags.bDefault = true;
						SkipSpaces();
						CheckAndSkip("(");
						SkipSpaces();
						CheckAndSkip(Descriptor.Name, Descriptor.NameLength);
						SkipSpaces();
						CheckAndSkip("IN");
						SkipSpaces();
						CheckAndSkip("(0,1)");
						SkipSpaces();
						CheckAndSkip(")");
					}
					//--
					SkipSpaces();
					//--
					if (!_strnicmp(pCur, "DEFAULT", 7)) {
						pCur += 7;
						Descriptor.Flags.bDefault = true;
						//--
						unsigned int DefaultValueLength;
						const char* pDefaultValue = ParseNextWord(OUT DefaultValueLength);
						
						char* szValue = StringAllocator.Add(DefaultValueLength + 1);
						memcpy(szValue, pDefaultValue, DefaultValueLength);
						szValue[DefaultValueLength] = '\0';
						Table.DefaultInstance.Values[Index].szValue = szValue;
					}
					//--
					break;
				}
			}
			//--
			SkipSpaces();
			//--
			VkAssert(*pCur == ',' || *pCur == ')');
			if (*pCur == ')')
				break;
			//--
			++pCur; // ,
			//--
			SkipSpacesAndReturns();
		}
		//--
		++pCur; // )
		//--
		SkipSpacesAndReturns();
	}

	void ParseSql() {
		while (*pCur) {
			switch (*pCur) {
				case '-': {
					if (!_strnicmp("--", pCur, 2)) {
						pCur += 2;
						ParseComment();
					} else {
						VkBreak();
					}
					break;
				}
				case 'P': {
					if (!_strnicmp("PRAGMA ", pCur, 7)) {
						pCur += 7;
						ParsePragma();
					} else {
						VkBreak();
					}
					CheckAndSkip(";");
					break;
				}
				case 'C': {
					if (!_strnicmp("CREATE ", pCur, 7)) {
						pCur += 7;
						ParseCreate();
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

void ParseSchema(IN_OUT const char*& pCur) {
	SCHEMA_PARSER Parser;
	Parser.pCur = pCur;
	Parser.ParseSql();
}