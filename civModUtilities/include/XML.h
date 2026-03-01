#pragma once

#include "Tools.h"
#include "Crc32b.h"
#include "Array.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
//--
struct XML_ATTRIBUTE {
	CRC ElementID;
	const char* pStartContent;
	unsigned int ContentLength;

	template <unsigned int _BUFFER_SIZE> void AsString(OUT char* Value, OUT unsigned int& Length) {
		if (_BUFFER_SIZE <= ContentLength) {
			memcpy(Value, pStartContent, _BUFFER_SIZE - 1);
			Value[_BUFFER_SIZE - 1] = '\0';
			Length = _BUFFER_SIZE - 1;
		} else {
			memcpy(Value, pStartContent, ContentLength);
			Value[ContentLength] = '\0';
			Length = ContentLength;
		}
	}

	void AsCrc(OUT CRC& ValueCrc) {
		ValueCrc = Crc32(pStartContent, ContentLength);
	}

	void AsInt32(OUT int& ValueInt) {
		ValueInt = atoi(pStartContent);
	}

	void AsFloat(OUT float& ValueFloat) {
		ValueFloat = (float)atof(pStartContent);
	}

	void AsBool(OUT bool& ValueBool) {
		if (*pStartContent >= '0' && *pStartContent <= '9') {
			ValueBool = atoi(pStartContent) != 0;
		} else {
			ValueBool = !_strnicmp(pStartContent, "true", ContentLength);
		}
	}
};
//--
struct XML_NODE:XML_ATTRIBUTE {
	//--
	DATA_ARRAY<XML_ATTRIBUTE> Attributes;
	OBJECT_ARRAY<XML_NODE> Nodes;
	//--
	XML_ATTRIBUTE* Get(int ID) {
		for (unsigned int iElement = 0; iElement < Attributes->Count; ++iElement) {
			XML_ATTRIBUTE& ChildElement = Attributes[iElement];
			if (ID == ChildElement.ElementID) {
				return &ChildElement;
			}
		}
		for (unsigned int iElement = 0; iElement < Nodes->Count; ++iElement) {
			XML_ATTRIBUTE& ChildElement = Nodes[iElement];
			if (ID == ChildElement.ElementID) {
				return &ChildElement;
			}
		}
		return (XML_ATTRIBUTE*)NULL;
	}

	XML_NODE* GetNode(int ID) {
		for (unsigned int iElement = 0; iElement < Nodes->Count; ++iElement) {
			XML_NODE& ChildElement = Nodes[iElement];
			if (ID == ChildElement.ElementID) {
				return &ChildElement;
			}
		}
		return (XML_NODE*)NULL;
	}

	template <unsigned int _BUFFER_SIZE> bool GetAsString(int ID, OUT char* Value, OUT unsigned int& Length) {
		XML_ATTRIBUTE* pChildElement = Get(ID);
		if (pChildElement) {
			pChildElement->AsString<_BUFFER_SIZE>(OUT Value, OUT Length);
			return true;
		} else {
			Value[0] = '\0';
			Length = 0;
			return false;
		}
	}

	bool GetAsCrc(int ID, OUT CRC& ValueCrc) {
		XML_ATTRIBUTE* pChildElement = Get(ID);
		if (pChildElement) {
			pChildElement->AsCrc(OUT ValueCrc);
			return true;
		} else {
			ValueCrc = 0;
			return false;
		}
	}

	bool GetAsInt32(int ID, OUT int& ValueInt, int Default = 0) {
		XML_ATTRIBUTE* pChildElement = Get(ID);
		if (pChildElement) {
			pChildElement->AsInt32(OUT ValueInt);
			return true;
		} else {
			ValueInt = Default;
			return false;
		}
	}

	bool GetAsFloat(int ID, OUT float& ValueFloat, float Default = 0) {
		XML_ATTRIBUTE* pChildElement = Get(ID);
		if (pChildElement) {
			pChildElement->AsFloat(OUT ValueFloat);
			return true;
		} else {
			ValueFloat = Default;
			return false;
		}
	}

	bool GetAsBool(int ID, OUT bool& ValueBool, bool bDefault = false) {
		XML_ATTRIBUTE* pChildElement = Get(ID);
		if (pChildElement) {
			pChildElement->AsBool(OUT ValueBool);;
			return true;
		} else {
			ValueBool = bDefault;
			return false;
		}
	}
};

inline CRC /*Crc*/ GetNextToken(IN_OUT const char*& pCur, OUT bool& bAttribute) {
	bAttribute = false;
	int Len = 0;
	while (isspace(*pCur)) ++pCur;
	if (*pCur == '<') {
		++pCur;
		while (isspace(*pCur)) ++pCur;
	}
	const char* pStart = pCur;
	while (*pCur != '>' && *pCur != '/') {
		char Cur = *(pCur);
		bool bLeave = false;
		if (isspace(Cur)) {
			bLeave = true;
			while (isspace(*pCur)) {
				++pCur;
				Cur = *pCur;
			}
		}
		if (Cur == '=') {
			++pCur;
			bAttribute = true;
			bLeave = true;
		}
		if (bLeave)
			break;
		Len++;
		++pCur;
	}
	CRC ID = Crc32_Low(pStart, Len);
	return ID;
}
//--
inline CRC /*Crc*/ GetNextChild(IN_OUT const char*& pCur) {
	int Len = 0;
	//--
	if (!*pCur || *(pCur + 1) == '/')
		return 0;
	//--
	++pCur;
	//--
	bool bAttribute;
	CRC ID = GetNextToken(pCur, OUT bAttribute);
	return ID;
}
//--
inline int PreParseAttributeContent(IN_OUT const char*& pCur, OUT const char*& pOut) {
	int Len = 0;
	while (isspace(*pCur)) ++pCur;
	VkAssert(*pCur == '"');
	++pCur;
	pOut = pCur;
	while (true) {
		if (*pCur == '"')
			break;
		++pCur;
		++Len;
	}
	VkAssert(*pCur == '"');
	++pCur;
	while (isspace(*pCur)) ++pCur;
	return Len;
}
//--
inline bool ParseNextAttribute(IN_OUT const char*& pCur, OUT CRC& CurrentID) {
	if (*pCur != '/' && *pCur != '>' && *pCur != '?') {
		bool bAttribute;
		CurrentID = GetNextToken(IN_OUT pCur, OUT bAttribute);
		if (CurrentID) {
			VkAssert(bAttribute);
			return true;
		}
	}
	return false;
}
//--
inline const char* GetFinish(const CRC CurrentID, const char* pFinishCur, OUT const char*& pAfterEndBalise) {
	while (*pFinishCur) {
		if (*pFinishCur == '<' && *(pFinishCur + 1) == '/') {
			const char* pEndFinishCur = pFinishCur + 2;
			while (*pEndFinishCur != '>') {
				++pEndFinishCur;
			}
			if (CurrentID == Crc32_Low(pFinishCur + 2, (unsigned int)(pEndFinishCur - (pFinishCur + 2)))) {
				pAfterEndBalise = pEndFinishCur + 1;
				return pFinishCur;
			}
			pFinishCur = pEndFinishCur;
		}
		++pFinishCur;
	}
	pAfterEndBalise = NULL;
	return NULL;
}
//--
inline void ParseXml(XML_NODE& Node, IN_OUT const char*& pCur);
//--
inline bool SkipComments(IN_OUT const char*& pCur) {
	if (!strncmp(pCur, "<!--", 4)) {
		pCur += 4;
		while (strncmp(pCur, "-->", 3)) ++pCur;
		pCur += 3;
		return true;
	}
	return false;
}
//--
inline bool ParseNextChild(XML_NODE& ParentNode, IN_OUT const char*& pCur) {
	while (*pCur && isspace(*pCur)) ++pCur;
	//--
	if (!*pCur || *pCur != '<')
		return false;
	//--
	if (SkipComments(IN_OUT pCur))
		return true;
	//--
	CRC ChildID = GetNextChild(IN_OUT pCur);
	if (!ChildID)
		return false;
	//--
	XML_NODE& Node = ParentNode.Nodes.Add();
	bool bAttributes = false;
	Node.ElementID = ChildID;
	Node.pStartContent = NULL;
	Node.ContentLength = 0;
	//--
	ParseXml(Node, IN_OUT pCur);
	//--
	return true;
}
//--
inline void ParseXml(XML_NODE& Node, IN_OUT const char*& pCur) {
	bool bSkipNextEnd = false;
	bool bAttributes = false;
	//--
	CRC CurrentID;
	while (ParseNextAttribute(IN_OUT pCur, OUT CurrentID)) {
		XML_ATTRIBUTE& ChildElement = Node.Attributes.Add();
		ChildElement.ElementID = CurrentID;
		ChildElement.ContentLength = PreParseAttributeContent(IN_OUT pCur, OUT ChildElement.pStartContent);
		bAttributes = true;
	}
	//--
	bool bParseChilds = true;
	if (*pCur == '/') {
		++pCur;
		bParseChilds = false;
	} else if (*pCur == '?') {
		++pCur;
		bParseChilds = false;
	}
	//--
	while (isspace(*pCur)) ++pCur;
	VkAssert(*pCur == '>');
	++pCur;
	//--
	if (bParseChilds) {
		Node.pStartContent = pCur;
		//--
		const char* pSaveCur = pCur;
		while (ParseNextChild(Node, IN_OUT pCur)) {
		}
		//--
		while (*pCur != '<') ++pCur;
		//--
		Node.ContentLength = unsigned int(pCur - Node.pStartContent);
	}
	//--
	if (bParseChilds) {
		if (*pCur == '<' && *(pCur + 1) == '/') {
			while (*pCur != '>') ++pCur;
			++pCur;
		}
	}
	//--
	while (*pCur && isspace(*pCur)) ++pCur;
	//--
	if (*pCur && *pCur != '<') {
		//VkBreak();
		while (*pCur && *pCur != '<') ++pCur;
	}
}
//--
inline void StartParseXml(XML_NODE& RootNode, IN_OUT const char*& pCur) {
	while (*pCur && *pCur != '<') ++pCur;
	//--
	while (ParseNextChild(RootNode, IN_OUT pCur)) {
	}
}