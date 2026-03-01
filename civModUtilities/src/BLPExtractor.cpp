#include <Tools.h>
#include <Array.h>
#include <Crc32b.h>
#include <Hash.h>

#include <string.h>

struct STRING {
	char* pStr;
};

struct BLP_HEADER {
	char Magic[6];            // 'CIVBLP'
	unsigned short Version;           // 1
	unsigned int PackageDataOffset; // Generally 0x200
	unsigned int PackageDataSize;
	unsigned int BigDataOffset;     // packageDataOffset + packageDataSize
	unsigned int BigDataCount;
	unsigned int FileSize;
};

struct PACKAGE_PREAMBLE {
	unsigned int PackageVersion;        // 5
	unsigned short SizeOfVoidPointer;     // 8
	unsigned short AlignOf64BitStructure; // 8
	unsigned int SizeOfPackageHeader;   // 0x48
	unsigned int EndianField;           // 1
};
struct PACKAGE_HEADER {
	struct STRIP_INFO { // A stripe is a block of serialized data
		unsigned int Start; // This is an offset from the start of the PackagePreamble value
		unsigned int Size;
	};
	STRIP_INFO ResourceLinkerDataStripe;
	STRIP_INFO PackageBlockStripe;        // This stripe contains package objects.
	STRIP_INFO TempDataStripe;            // This stripe contains linker data (which is information on how to fixup pointers), the root object, and String::Global objects.
	STRIP_INFO TypeInfoStripe;            // This stripe contains another package with type information for all the objects serialized in packageData
	STRIP_INFO RootTypeName;              // The root object of the package is this type; it is stored at the start of TempDataStripe
	unsigned int LinkerDataOffset;             // This is an offset from the start of TempDataStripe; it contains an array of PackageAllocation
	unsigned int ResourceListOffset;
	unsigned int LargestResource;
	unsigned int SecondLargestResource;
	unsigned int PackageBlockAlignment;        // 0x10
	unsigned int SizeOfTypeInfoStripe;         // 0x30
	unsigned int SizeOfPackageAllocation;      // 0x28
	unsigned int SizeOfResourceAllocationDesc; // 0x10
};

struct PACKAGE_ALLOCATION {
	unsigned char ByStripe;           // 0: PackageBlockStripe; 1: TempDataStripe
	unsigned char ByAllocType;
	unsigned char ByPadding[4];
	unsigned short ParentAlloc;
	unsigned int Offset;
	unsigned int AllocSize;
	unsigned int ElementCount;
	unsigned __int64 qwUserData;
	STRING TypeName;
};

struct LINEAR_ALLOC {
	template <typename T>
	struct LINEAR_BLOCK {
		T* pBlock;
		unsigned int NElements;
	};
};

template <typename T> struct VECTOR { // Note: sizeof(Vector) is 0x18 due to padding
	LINEAR_ALLOC::LINEAR_BLOCK<T> Block;
	unsigned int CurrSize;
};

struct TYPE_INFO_STRIP {
	struct FIELD_VERSION {
		STRING FieldName;
		STRING TypeName;  // type of the field value
		unsigned int Version;
		unsigned int Address;          // byte offset of the field in the struct
	};
	struct TYPE_VERSION {
		STRING TypeName;
		STRING UnderlyingTypeName;
		// Note:  fields are not ordered as they are defined in the struct.
		// Inherited fields are included in this vector.
		VECTOR<FIELD_VERSION> Fields;
		unsigned int Version;
		unsigned int Size;

		enum TYPE : unsigned int {
			NONE = 0x00,
			ARRAY = 0x01,
			POINTER = 0x02,
			FUNDAMENTAL = 0x04,
			POLYMORPHIC = 0x10,
		};
		TYPE TypeTraitFlags;
	};
	struct ENUM_VERSION {
		struct ENUM_CONSTANT {
			STRING sName;
			int iValue;
		};
		STRING TypeName;
		VECTOR<ENUM_CONSTANT> m_Constants;
	};

	VECTOR<TYPE_VERSION> Types;
	VECTOR<ENUM_VERSION> Enums;
};

template <typename Allocator, typename Encoding> struct BASIC {
	struct STORAGE {
		unsigned int Capacity;
		unsigned int Length;
		char str[1];
	};
	STORAGE* m_sz;
	const char* CStr() const {
		return m_sz->str;
	}
};

void ParseBlp(IN_OUT const char*& pCur) {
	const char* pOrigin = pCur;

	BLP_HEADER& BlpHeader = *(BLP_HEADER*)pCur;

	PACKAGE_PREAMBLE& PackagePreamble = *(PACKAGE_PREAMBLE*)(pOrigin + BlpHeader.PackageDataOffset);
	PACKAGE_HEADER& PackageHeader = *(PACKAGE_HEADER*)(pOrigin + BlpHeader.PackageDataOffset + sizeof(PACKAGE_PREAMBLE));
	int a = 0;
}