#include <fstream>
#include <iostream>
#include <algorithm>
#include <assert.h>
#include "CivVIParser.h"
#include <zlib.h>
#include <chrono>
#include "CivVIState.h"
#include <Crc32b.h>

#ifdef _DEBUG
#define assertT(c) assert(c);
#else
#define assertT(c) if(!(c)) {throw "ParsingError";}
#endif


#define CHUNK 65536 

byte Civ6[] = { 0x43, 0x49, 0x56, 0x36 };
byte MAP_BEGIN[] = { 0x0A, 0, 0, 0, 0x0B, 0, 0, 0, 0x0C, 0, 0, 0, 0x0D, 0, 0, 0, 0x0E, 0, 0, 0, 0x0F, 0, 0, 0, 0x06, 0, 0, 0 };
byte VICTORIES_BEGIN[] = { 0x12, 0, 0, 0, 0x95, 0, 0xf5, 0x13, 0x07};
byte VISIBILITY_BEGIN[] = { 0xD2, 0xDE, 0x2D, 0x3F};
byte FEATURE_BEGIN[] = { 0x46, 0x65, 0x61, 0x74, 0x75, 0x72, 0x65}; //Feature
byte TERRITORY_BUILDER[] = { 0x54, 0x65, 0x72, 0x72, 0x69, 0x74, 0x6F, 0x72, 0x79, 0x42, 0x75, 0x69, 0x6C, 0x64, 0x65, 0x72 }; //TerritoryBuilder
byte BEGIN_COMPRESSED[] = { 0x78, 0x9C };
byte END_COMPRESSED[] = { 0,0,0xff,0xff, 0x02, 0, 0, 0 };
byte BEGIN_TIMESTAMP[] = { 0x64, 0x6D, 0xFE, 0x0E, 0x14, 0x00, 0x00, 0x00 };
byte END_IA_STUFF[] = { 0xBA, 0xF1, 0xBF, 0x93 };
byte END_PLAYER_STUFF[] = { 0xBC, 0x0A, 0x2B, 0xDE };

int buffToInteger(const byte& byte, int byteCount = 4)
{
    int a = 0;
    memcpy(&a, &byte, byteCount);
    return a;
}

uint64_t buffTolong(const byte& byte, int byteCount = 8)
{
    uint64_t a = 0;
    memcpy(&a, &byte, byteCount);
    return a;
}
int readInteger(std::vector<byte>::iterator& byte, int byteCount = 4)
{
    int a = buffToInteger(*byte, byteCount);
    byte += byteCount;
    return a;
}
void CivVIParser::Write(const char* file_name, const std::vector<byte>& data)
{
    std::ofstream outfile(file_name, std::ios::out | std::ios::binary);
    if (outfile.fail())
    {
        std::string error = strerror(errno);
        int a = 0;

    }
    outfile.write((char*)&data[0], data.size());
    outfile.close();
}

void CivVIParser::ParseBinaries(const char* file_name)
{
    std::ifstream bin_file(file_name, std::ios::binary);

    if (bin_file.good()) {
        buffer = std::vector<byte>((std::istreambuf_iterator<char>(bin_file)), (std::istreambuf_iterator<char>()));
        it = buffer.begin();
        if (memcmp(Civ6, &buffer[0], sizeof(Civ6)))
            return; //throw not a civ6 file
        it = std::search(it, buffer.end(), BEGIN_TIMESTAMP, BEGIN_TIMESTAMP + 8);
        if (it == buffer.end())
            return;
        CivVIPacket newP;
        ParseEntry(newP);
        while(it != buffer.end())
        {
            it = std::search(it, buffer.end(), BEGIN_COMPRESSED, BEGIN_COMPRESSED + 2);
            if (it == buffer.end())
                break;
            packets.push_back(CivVIPacket());
            std::vector<byte>::iterator end = std::search(it, buffer.end(), END_COMPRESSED, END_COMPRESSED + 8);
            packets.back().length = end - it + 4 + 12;
            ReadCompressed(packets.back());
        }

        bin_file.close();
    }
}
void CivVIParser::ParsePacketArray(std::vector<byte>::iterator& it, std::vector<CivVIPacket>& packets)
{
    packets.push_back(CivVIPacket());
    CivVIPacket& p = packets.back();
    p.type = ARRAY;
    std::map<unsigned int, CivVIPacket>& child = p.packets;
    unsigned count = readInteger(it); //20
    p.length = count;
    for (int i = 0; i < count; i++)
    {
        CivVIPacket newP;
        if (!ParseEntry(newP))
            assert(0);
        child[newP.marker] = newP;
    }
}
void CivVIParser::ParseAll(const char* file_name)
{
    std::ifstream bin_file(file_name, std::ios::binary);

    if (bin_file.good()) {
        buffer = std::vector<byte>((std::istreambuf_iterator<char>(bin_file)), (std::istreambuf_iterator<char>()));
        it = buffer.begin();
        if (memcmp(Civ6, &buffer[0], sizeof(Civ6)))
            return; //throw not a civ6 file
        it += 4;
        it += 4; // 01 ?
        ParsePacketArray(it, packets);

        it += 4; // 0E ?
        it += 4; // 01?
        ParsePacketArray(it, packets);

        it += 4; // 01?
        ParsePacketArray(it, packets);

        it += 4; // 01
        ParsePacketArray(it, packets);

        unsigned int count = readInteger(it); //01/02
        for (int i = 0; i < count; i++)
        {
            it += 4;
            ParsePacketArray(it, packets);
        }
        ParsePacketArray(it, packets);

        it += 4; //00 01
        packets.push_back(CivVIPacket());
        CivVIPacket result = packets.back();
        result.marker = 0xfffffffff;
        result.type = 24;
        std::vector<byte>::iterator end = std::search(it, buffer.end(), END_COMPRESSED, END_COMPRESSED + 8);
        result.length = end - it + 4 + 12; //HACK HERE to be in phase with other compressed
        ReadCompressed(result);

        bin_file.close();
    }
}
void CivVIPacket::copyString(unsigned int marker, char* pseudo)
{
    std::map<unsigned int, CivVIPacket>::iterator p = packets.find(marker);
    if (p != packets.end())
        strcpy(pseudo, (char*)p->second.data.pointer);
}

unsigned int CivVIPacket::getInt(unsigned int marker)
{
    std::map<unsigned int, CivVIPacket>::iterator p = packets.find(marker);
    return p != packets.end() ? p->second.data.int_ : -1;
}

void UpdatePlayerColor(PlayerInfo* playersInfo)
{
    std::map<unsigned int, std::vector<int>> colors =
    { 
        {0x991cff9d, {1,42,108,255, 249,249,249,255, 202,20,21,255, 249,249,249,255, 249,249,249,255, 1,42,108,255, 1,42,108,255, 202,20,21,255} },
        {0x2f120ae4, {247,216,1,255, 21,108,48,255, 21,108,48,255, 121,224,119,255, 121,224,119,255, 24,24,24,255, 134,114,2,255, 97,191,34,255} },
        {0x4111e7e3, {97,191,34,255, 247,216,1,255, 97,191,34,255, 1,42,108,255, 247,216,1,255, 21,108,48,255, 1,42,108,255, 121,224,119,255} },
        {0xa0c612a, {21,108,48,255, 249,249,249,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255, 247,216,1,255, 1,42,108,255} },
        {0x8aa3f23c, {1,79,81,255, 234,225,157,255, 234,225,157,255, 109,0,205,255, 234,225,157,255, 1,79,81,255, 109,0,205,255, 234,225,157,255} },
        {0xcf9f21e, {202,20,21,255, 249,249,249,255, 249,249,249,255, 202,20,21,255, 1,42,108,255, 202,20,21,255, 202,20,21,255, 1,42,108,255} },
        {0x98251cb4, {0,79,206,255, 234,225,157,255, 249,249,249,255, 134,114,2,255, 249,249,249,255, 0,79,206,255, 1,42,108,255, 202,20,21,255} },
        {0x36fe8dfe, {174,174,174,255, 24,24,24,255, 247,216,1,255, 24,24,24,255, 202,20,21,255, 249,249,249,255, 247,216,1,255, 202,20,21,255} },
        {0xd37275c3, {120,0,1,255, 116,163,243,255, 116,163,243,255, 120,0,1,255, 116,163,243,255, 249,249,249,255, 249,249,249,255, 116,163,243,255} },
        {0x5eae1c66, {116,163,243,255, 249,249,249,255, 249,249,249,255, 116,163,243,255, 116,163,243,255, 1,42,108,255, 1,42,108,255, 116,163,243,255} },
        {0xe5466693, {55,0,101,255, 0,192,155,255, 0,192,155,255, 55,0,101,255, 21,108,48,255, 255,178,60,255, 255,178,60,255, 21,108,48,255} },
        {0x70223815, {249,249,249,255, 120,0,1,255, 120,0,1,255, 249,249,249,255, 0,79,206,255, 249,249,249,255, 249,249,249,255, 0,79,206,255} },
        {0xbaf9d251, {247,216,1,255, 202,20,21,255, 202,20,21,255, 247,216,1,255, 121,224,119,255, 120,61,2,255, 120,61,2,255, 121,224,119,255} },
        {0xca547f73, {247,216,1,255, 24,24,24,255, 249,249,249,255, 0,79,206,255, 0,79,206,255, 249,249,249,255, 1,42,108,255, 229,117,116,255} },
        {0x3dfb0c1, {255,178,60,255, 120,0,1,255, 120,61,2,255, 249,249,249,255, 120,0,1,255, 234,225,157,255, 120,61,2,255, 234,225,157,255} },
        {0xf47df3e8, {202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255, 202,20,21,255, 1,42,108,255, 1,42,108,255, 202,20,21,255} },
        {0x8c112bc2, {1,42,108,255, 255,129,18,255, 1,42,108,255, 125,236,227,255, 255,129,18,255, 1,42,108,255, 1,42,108,255, 183,128,230,255} },
        {0xf7f035fa, {1,42,108,255, 202,20,21,255, 202,20,21,255, 1,42,108,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255} },
        {0x5a1fe842, {109,0,205,255, 247,216,1,255, 120,0,1,255, 247,216,1,255, 247,216,1,255, 55,0,101,255, 109,0,205,255, 249,249,249,255} },
        {0xb98fc669, {21,108,48,255, 125,236,227,255, 234,225,157,255, 21,108,48,255, 120,61,2,255, 255,129,18,255, 255,129,18,255, 234,225,157,255} },
        {0xfd059b83, {55,0,101,255, 234,225,157,255, 120,0,1,255, 247,216,1,255, 247,216,1,255, 120,0,1,255, 109,0,205,255, 247,216,1,255} },
        {0x4478242a, {125,236,227,255, 120,0,1,255, 120,0,1,255, 125,236,227,255, 255,129,18,255, 1,79,81,255, 1,79,81,255, 255,129,18,255} },
        {0xa10d0331, {21,108,48,255, 247,216,1,255, 249,249,249,255, 255,129,18,255, 1,42,108,255, 249,249,249,255, 249,249,249,255, 1,42,108,255} },
        {0x396c1aa5, {116,163,243,255, 1,42,108,255, 234,225,157,255, 1,42,108,255, 1,42,108,255, 249,249,249,255, 1,42,108,255, 247,216,1,255} },
        {0x43f725af, {249,249,249,255,134,114,2,255,0,79,206,255,234,225,157,255,249,249,249,255,0,79,206,255,1,42,108,255,202,20,21,255}},
        {0x8b9ce61, {247,216,1,255, 21,108,48,255, 21,108,48,255, 247,216,1,255, 202,20,21,255, 247,216,1,255, 21,108,48,255, 202,20,21,255} },
        {0xbe5298ef, {1,42,108,255, 247,216,1,255, 1,42,108,255, 202,20,21,255, 202,20,21,255, 1,42,108,255, 234,225,157,255, 120,0,1,255} },
        {0xcea3b6d4, {116,163,243,255, 1,79,81,255, 1,79,81,255, 116,163,243,255, 21,108,48,255, 234,225,157,255, 255,178,60,255, 1,79,81,255} },
        {0xa3797d87, {120,0,1,255, 0,192,155,255, 0,192,155,255, 120,0,1,255, 202,20,21,255, 249,249,249,255, 1,42,108,255, 0,192,155,255} },
        {0x2cb878ca, {117,0,115,255, 255,129,18,255, 1,42,108,255, 234,225,157,255, 120,61,2,255, 255,129,18,255, 255,129,18,255, 117,0,115,255} },
        {0x1997e0ce, {234,225,157,255, 120,0,1,255, 21,108,48,255, 249,249,249,255, 202,20,21,255, 234,225,157,255, 120,0,1,255, 247,216,1,255} },
        {0xd223b6f0, {255,129,18,255, 120,0,1,255, 247,216,1,255, 120,0,1,255, 202,20,21,255, 1,42,108,255, 120,0,1,255, 247,216,1,255} },
        {0xf42613d0, {247,216,1,255, 117,0,115,255, 247,216,1,255, 120,61,2,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 24,24,24,255} },
        {0x227e20c5, {174,174,174,255, 247,216,1,255, 1,42,108,255, 247,216,1,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 1,42,108,255} },
        {0x3f75f7dc, {183,128,230,255, 120,0,1,255, 120,0,1,255, 183,128,230,255, 202,20,21,255, 247,216,1,255, 0,192,155,255, 120,0,1,255} },
        {0x4ac5f744, {234,225,157,255, 120,61,2,255, 120,61,2,255, 234,225,157,255, 249,249,249,255, 120,61,2,255, 120,61,2,255, 249,249,249,255} },
        {0x9eb23337, {120,0,1,255, 229,117,116,255, 249,249,249,255, 229,117,116,255, 1,42,108,255, 249,249,249,255, 249,249,249,255, 1,42,108,255} },
        {0x7a25dd75, {202,20,21,255, 249,249,249,255, 1,42,108,255, 249,249,249,255, 249,249,249,255, 1,42,108,255, 1,42,108,255, 202,20,21,255} },
        {0xa7b274b9, {229,117,116,255, 249,249,249,255, 229,117,116,255, 247,216,1,255, 249,249,249,255, 202,20,21,255, 229,117,116,255, 1,42,108,255} },
        {0x75dde035, {229,117,116,255, 234,225,157,255, 0,79,206,255, 234,225,157,255, 234,225,157,255, 1,42,108,255, 229,117,116,255, 1,42,108,255} },
        {0x7b5c15e5, {120,0,1,255, 234,225,157,255, 234,225,157,255, 120,0,1,255, 134,114,2,255, 249,249,249,255, 134,114,2,255, 121,224,119,255} },
        {0x22c62a2f, {249,249,249,255, 21,108,48,255, 202,20,21,255, 249,249,249,255, 21,108,48,255, 249,249,249,255, 21,108,48,255, 229,117,116,255} },
        {0x54f4afe1, {120,61,2,255, 247,216,1,255, 255,178,60,255, 1,79,81,255, 247,216,1,255, 120,61,2,255, 1,79,81,255, 255,178,60,255} },
        {0x2fbb8b6e, {21,108,48,255, 255,129,18,255, 249,249,249,255, 21,108,48,255, 21,108,48,255, 202,20,21,255, 21,108,48,255, 249,249,249,255} },
        {0x23dea298, {202,20,21,255, 125,236,227,255, 125,236,227,255, 202,20,21,255, 0,192,155,255, 125,236,227,255, 120,61,2,255, 234,225,157,255} },
        {0xa0fad691, {109,0,205,255, 116,163,243,255, 109,0,205,255, 249,249,249,255, 55,0,101,255, 255,178,60,255, 249,249,249,255, 55,0,101,255} },
        {0x21b46b51, {249,249,249,255, 202,20,21,255, 202,20,21,255, 249,249,249,255, 120,0,1,255, 229,117,116,255, 229,117,116,255, 120,0,1,255} },
        {0x450feb6e, {116,163,243,255, 247,216,1,255, 247,216,1,255, 1,42,108,255, 0,79,206,255, 249,249,249,255, 249,249,249,255,  1,42,108,255}},
        {0xa6cca53a, {202,20,21,255,116,163,243,255,116,163,243,255,202,20,21,255,255,129,18,255,249,249,249,255,249,249,249,255,255,129,18,255}},
        {0x8ea8031a, {120,61,2,255, 249,249,249,255, 21,108,48,255, 234,225,157,255, 249,249,249,255, 120,61,2,255, 97,191,34,255, 24,24,24,255} },
        {0xc8b384c6, {1,42,108,255, 97,191,34,255, 97,191,34,255, 1,42,108,255, 116,163,243,255, 247,216,1,255, 97,191,34,255, 247,216,1,255} },
        {0x36cbc0b0, {249,249,249,255, 255,129,18,255, 249,249,249,255, 202,20,21,255, 120,0,1,255, 249,249,249,255, 247,216,1,255, 24,24,24,255} },
        {0xd37a9fde, {0,192,155,255, 55,0,101,255, 55,0,101,255, 0,192,155,255, 21,108,48,255, 255,178,60,255, 255,178,60,255, 21,108,48,255} },
        {0x6c498d7a, {0,79,206,255, 125,236,227,255, 247,216,1,255, 120,0,1,255, 234,225,157,255, 120,61,2,255, 116,163,243,255, 1,42,108,255} },
        {0x6f418f39, {120,0,1,255, 255,129,18,255, 255,129,18,255, 120,0,1,255, 234,225,157,255, 120,0,1,255, 120,0,1,255, 247,216,1,255} },
        {0x8175f0d9, {255,129,18,255, 0,79,206,255, 255,129,18,255, 249,249,249,255, 255,129,18,255, 255,178,60,255, 255,129,18,255, 120,0,1,255} },
        {0x3c3ccb83, {249,249,249,255, 0,79,206,255, 0,79,206,255, 249,249,249,255, 247,216,1,255, 202,20,21,255, 247,216,1,255, 0,79,206,255} },
        {0x507477e4, {249,249,249,255, 1,42,108,255, 97,191,34,255, 120,0,1,255, 1,42,108,255, 249,249,249,255, 120,0,1,255, 247,216,1,255} },
        {0xf43e7653, {120,0,1,255, 247,216,1,255, 109,0,205,255, 247,216,1,255, 247,216,1,255, 55,0,101,255, 109,0,205,255, 249,249,249,255} },
        {0xf3aa619d, {120,0,1,255, 249,249,249,255, 249,249,249,255, 120,0,1,255, 0,79,206,255, 249,249,249,255, 249,249,249,255, 0,79,206,255} },
        {0x25f7b317, {120,0,1,255, 183,128,230,255, 183,128,230,255, 120,0,1,255, 202,20,21,255, 247,216,1,255, 0,192,155,255, 120,0,1,255} },
        {0x57b72095, {21,108,48,255, 229,117,116,255, 202,20,21,255, 249,249,249,255, 21,108,48,255, 249,249,249,255, 249,249,249,255, 21,108,48,255} },
        {0xafb73252, {249,249,249,255, 1,42,108,255, 1,42,108,255, 249,249,249,255, 202,20,21,255, 249,249,249,255, 1,42,108,255, 202,20,21,255} },
        {0x71a2a578, {121,224,119,255, 120,61,2,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255, 120,61,2,255, 121,224,119,255} },
        {0x555373d4, {21,108,48,255, 121,224,119,255, 247,216,1,255, 21,108,48,255, 121,224,119,255, 24,24,24,255, 134,114,2,255, 97,191,34,255} },
        {0x180625b8, {249,249,249,255, 21,108,48,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255, 247,216,1,255, 1,42,108,255} },
        {0xc368c6f, {247,216,1,255, 1,42,108,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255, 21,108,48,255, 249,249,249,255} },
        {0xa2a74896, {202,20,21,255, 247,216,1,255, 21,108,48,255, 249,249,249,255, 247,216,1,255, 202,20,21,255, 247,216,1,255, 1,42,108,255} },
        {0x56060854, {234,225,157,255, 1,79,81,255, 234,225,157,255, 109,0,205,255, 1,79,81,255, 234,225,157,255, 109,0,205,255, 234,225,157,255} },
        {0xf54d6897, {234,225,157,255, 109,0,205,255, 1,79,81,255, 234,225,157,255, 234,225,157,255, 1,79,81,255, 109,0,205,255, 234,225,157,255} },
        {0xc2807a93, {234,225,157,255, 120,0,1,255, 120,0,1,255, 234,225,157,255, 134,114,2,255, 249,249,249,255, 134,114,2,255, 121,224,119,255} },
        {0xac36bca2, {202,20,21,255, 1,42,108,255, 1,42,108,255, 202,20,21,255, 202,20,21,255, 247,216,1,255, 247,216,1,255, 202,20,21,255} },
        {0x18b2fc2a, {1,42,108,255, 202,20,21,255, 249,249,249,255, 202,20,21,255, 202,20,21,255, 249,249,249,255, 202,20,21,255, 1,42,108,255} },
        {0x1055b553, {249,249,249,255, 202,20,21,255, 202,20,21,255, 249,249,249,255, 1,42,108,255, 202,20,21,255, 202,20,21,255, 1,42,108,255} },
        {0x430bc7e1, {109,0,205,255, 247,216,1,255, 120,0,1,255, 247,216,1,255, 247,216,1,255, 120,0,1,255, 55,0,101,255, 234,225,157,255} },
        {0x959f4619, {116,163,243,255, 249,249,249,255, 247,216,1,255, 24,24,24,255, 202,20,21,255, 249,249,249,255, 247,216,1,255, 202,20,21,255} },
        {0xba5472e9, {116,163,243,255, 202,20,21,255, 202,20,21,255, 116,163,243,255, 255,129,18,255, 249,249,249,255, 249,249,249,255, 255,129,18,255} }
    };

    std::map<unsigned int, TypeCityState> cityState =
    {
        {0xeea0cdb2, TRADE}, //CIVILIZATION_AMSTERDAM
        {0x82b70c70, INDUSTRIAL}, //CIVILIZATION_BRUSSELS
        {0xdf3ddde6, INDUSTRIAL}, //CIVILIZATION_BUENOS_AIRES
        {0x203f72cd, MILITARISTIC}, //CIVILIZATION_CARTHAGE
        {0x4f74f65e, SCIENTIFIC}, //CIVILIZATION_GENEVA
        {0x58d93aaf, SCIENTIFIC}, //CIVILIZATION_HATTUSA
        {0xef347ca9, INDUSTRIAL}, //CIVILIZATION_HONG_KONG
        {0x3f0c3a, TRADE}, //CIVILIZATION_JAKARTA
        {0x15489e24, RELIGIOUS}, //CIVILIZATION_JERUSALEM
        {0x5f2c64fb, MILITARISTIC}, //CIVILIZATION_KABUL
        {0x68325a64, RELIGIOUS}, //CIVILIZATION_KANDY
        {0x3e46dae2, CULTURAL}, //CIVILIZATION_KUMASI
        {0xa0f53ab4, RELIGIOUS}, //CIVILIZATION_LA_VENTA
        {0x4cc0c6d2, TRADE}, //CIVILIZATION_LISBON
        {0x89392463, CULTURAL}, //CIVILIZATION_MOHENJO_DARO
        {0x9120b449, CULTURAL}, //CIVILIZATION_NAN_MADOL
        {0xa12d7353, MILITARISTIC}, //CIVILIZATION_PRESLAV
        {0x8806bcbc, SCIENTIFIC}, //CIVILIZATION_SEOUL
        {0x88a0d1ea, SCIENTIFIC}, //CIVILIZATION_STOCKHOLM
        {0x727f7f0a, INDUSTRIAL}, //CIVILIZATION_TORONTO
        {0xf32dcee7, MILITARISTIC}, //CIVILIZATION_VALLETTA
        {0x5c80b833, CULTURAL}, //CIVILIZATION_VILNIUS
        {0x8f09f982, RELIGIOUS}, //CIVILIZATION_YEREVAN
        {0xe79cdf31, TRADE}, //CIVILIZATION_ZANZIBAR
        {0x5585f8d6, MILITARISTIC}, //CIVILIZATION_AKKAD
        {0xf1e5134e, SCIENTIFIC}, //CIVILIZATION_BOLOGNA
        {0xde43b0e9, TRADE}, //CIVILIZATION_CAHOKIA
        {0x396ae5c9, INDUSTRIAL}, //CIVILIZATION_CARDIFF
        {0x4ae2232d, SCIENTIFIC}, //CIVILIZATION_FEZ
        {0xe5c86738, INDUSTRIAL}, //CIVILIZATION_MEXICO_CITY
        {0xe7d0d029, RELIGIOUS}, //CIVILIZATION_NAZCA
        {0xbd20a6f6, MILITARISTIC}, //CIVILIZATION_NGAZARGAMU
        {0xc236ee62, CULTURAL}, //CIVILIZATION_RAPA_NUI
        {0x9ac3d809, CULTURAL}, //CIVILIZATION_AYUTTHAYA
        {0x8c8f22b5, RELIGIOUS}, //CIVILIZATION_CHINGUETTI
        {0xf6aad6, INDUSTRIAL}, //CIVILIZATION_JOHANNESBURG
        {0x19aebc7c, SCIENTIFIC}, //CIVILIZATION_NALANDA
        {0x589df3b6, TRADE}, //CIVILIZATION_SAMARKAND
        {0x18047830, MILITARISTIC}, //CIVILIZATION_WOLIN
        {0xbccbceb8, TRADE}, //CIVILIZATION_ANTIOCH
        {0x845c072d, SCIENTIFIC}, //CIVILIZATION_BABYLON
        {0xf2160088, INDUSTRIAL}, //CIVILIZATION_AUCKLAND
        {0x2445b40b, CULTURAL}, //CIVILIZATION_ANTANANARIVO
        {0x7454c257, RELIGIOUS}, //CIVILIZATION_ARMAGH
        {0xa2db1a40, MILITARISTIC}, //CIVILIZATION_GRANADA
        {0x40afd137, TRADE}, //CIVILIZATION_MUSCAT
        {0xf7ee15a3, SCIENTIFIC}, //CIVILIZATION_PALENQUE
        {0x216183d2, CULTURAL}, //CIVILIZATION_CAGUANA
        {0x37c447f5, TRADE}, //CIVILIZATION_HUNZA
        {0x9bce7b4a, MILITARISTIC}, //CIVILIZATION_LAHORE
        {0x577f6c8e, INDUSTRIAL}, //CIVILIZATION_SINGAPORE
        {0x6a8648ec, SCIENTIFIC}, //CIVILIZATION_TARUGA
        {0x1e773dbd, RELIGIOUS}, //CIVILIZATION_VATICAN_CITY
        {0xbe3cfc09, TRADE} //CIVILIZATION_VENICE
    };



    for (int iPlayer = 0; iPlayer < 64; iPlayer++)
    {
        PlayerInfo& playerInfo = playersInfo[iPlayer];
        if (playerInfo.typeCiv == FULL_CIV)
        {
            std::map<unsigned int, std::vector<int>>::iterator it = colors.find(playerInfo.leader);
            if (it != colors.end())
            {
                playerInfo.primaryColor = { (float)it->second[playerInfo.iColor * 8 + 0] / 256.f, (float)it->second[playerInfo.iColor * 8 + 1] / 256.f, (float)it->second[playerInfo.iColor * 8 + 2] / 256.f , (float)it->second[playerInfo.iColor * 8 + 3] / 256.f };
                playerInfo.secondaryColor = { (float)it->second[playerInfo.iColor * 8 + 4 + 0] / 256.f, (float)it->second[playerInfo.iColor * 8 + 4 + 1] / 256.f , (float)it->second[playerInfo.iColor * 8 + 4 + 2] / 256.f , (float)it->second[playerInfo.iColor * 8 + 4 + 3] / 256.f };
            }
        }
        else if (playerInfo.typeCiv == CITY_STATE)
        {
            playerInfo.secondaryColor = ImVec4(1.f, 1.f, 1.f, 1.f);
            playerInfo.primaryColor = ImVec4(0.f, 0.f, 0.f, 1.f);

            std::map<unsigned int, TypeCityState>::iterator it = cityState.find(playerInfo.civ);
            if (it != cityState.end())
            {
                playerInfo.typeCityState = it->second;
                switch (playerInfo.typeCityState)
                {
                    case TRADE:
                    {
                        playerInfo.secondaryColor = ImVec4(247.f / 255.f, 216.f / 255.f, 1.f / 255.f, 1.f);
                        break;
                    }
                    case RELIGIOUS:
                    {
                        playerInfo.secondaryColor = ImVec4(1.f, 1.f, 1.f, 1.f);
                        break;
                    }
                    case SCIENTIFIC:
                    {
                        playerInfo.secondaryColor = ImVec4(116.f / 255.f, 163.f / 255.f, 243.f / 255.f, 1.f);
                        break;
                    }
                    case INDUSTRIAL:
                    {
                        playerInfo.secondaryColor = ImVec4(255.f / 255.f, 129.f / 255.f, 18.f / 255.f, 1.f);
                        break;
                    }
                    case MILITARISTIC:
                    {
                        playerInfo.secondaryColor = ImVec4(202.f / 255.f, 20.f / 255.f, 21.f / 255.f, 1.f);
                        break;
                    }
                    case CULTURAL:
                    {
                        playerInfo.secondaryColor = ImVec4(255.f / 255.f, 0.f / 255.f, 255.f / 255.f, 1.f);
                        break;
                    }
                    default:
                    {
                        assert(0);
                    }
                }
            }

        }
        else if (playerInfo.typeCiv == TRIBE)
        {
            playerInfo.secondaryColor = ImVec4(1.f, 0.f, 0.f, 1.f);
            playerInfo.primaryColor = ImVec4(0.f, 0.f, 0.f, 1.f);
        }
        else
        {
            playerInfo.primaryColor = ImVec4(1.f, 0.f, 0.f, 1.f);
            playerInfo.secondaryColor = ImVec4(0.f, 0.f, 0.f, 1.f);

        }
    }
}

void CivVIParser::SetGameInfo(CivVIGame& Game)
{
    int iPacket = 0;
    packets[iPacket].copyString(0x584C6027, Game.map);
    packets[iPacket].copyString(0x5D9B099, Game.gamespeed);
    packets[iPacket].copyString(0xB835C40, Game.mapSize);
    
    int multi = packets[iPacket].getInt(0X7C546F81);
    Game.multiPlayer = multi == 0x453F23E2;
}

int CivVIParser::GetGameSeed()
{
    int iPacket = 2;
    assert(packets[iPacket].packets.size() == 67);
    return packets[iPacket].packets[0x7dd11279].getInt(0x96c5c77c); //8C 54 EE 04 or 7C C7 C5 96
}

void CivVIParser::ParsePlayersInfo(CivVIGame& Game)
{
    PlayerInfo* playersInfo = Game.playersInfo;
    int iPacket = 2;
    assert(packets[iPacket].packets.size() == 67);
    for (std::map<unsigned int, CivVIPacket>::iterator it = packets[iPacket].packets.begin(); it != packets[iPacket].packets.end(); it++)
    {
        CivVIPacket& p = it->second;
        unsigned int iPlayer = p.getInt(0x1A96522F);

        if (iPlayer == -1)
            continue;
        PlayerInfo& playerInfo = playersInfo[iPlayer];
        playerInfo.typePlayer = (TypePlayer)p.getInt(0xCE42B995);
        if (playerInfo.typePlayer == PLAYER || playerInfo.typePlayer == AI)
        {
            playerInfo.team = p.getInt(0x0D8AB454);
            playerInfo.iColor = p.getInt(0xCFAF60EF);
            playerInfo.leader = p.getInt(0x4C7FBA58);
            if (playerInfo.leader == 0xffffffff)
            {
                char leaderNameStr[512];
                p.copyString(0xE8CD5E5F, leaderNameStr);
                playerInfo.leader = ~Crc32(leaderNameStr);
            }
            playerInfo.civ = p.getInt(0xC7D7515B);
            playerInfo.typeCiv = (TypeCiv)p.getInt(0x830FFC46);
            p.copyString(0x9D5E5C2F, playerInfo.civStr);
            p.copyString(0xE8CD5E5F, playerInfo.leaderStr);
            p.copyString(0xDAB96BFD, playerInfo.pseudo);
            p.copyString(0x8E72249A, playerInfo.pseudoWithSteamId);
            p.copyString(0XB3EA1140, playerInfo.locCivName);
            // pins A2 02 2A 54
        }
        else if (playerInfo.typePlayer == UNKNOWN)
        {
            assert(0);
        }
    }

    UpdatePlayerColor(playersInfo);
}


bool CivVIParser::ReadCompressed(OUT CivVIPacket& result)
{
    int ret;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK];

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    strm.data_type = Z_BINARY;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    result.data.int_ = compressedData.size();
    compressedData.push_back(std::vector<byte>());
    std::vector<byte>& compressed = compressedData.back();

    /*std::vector<byte>::iterator end = std::search(it, buffer.end(), END_COMPRESSED, END_COMPRESSED + 4);
    result.length = end - it + 4; // we can avoid this when we have already the length but it doesn't take into account the 4 bytes so we should manage this
    */
    int length = result.length - 12;
    strm.next_in = &(*it);
    strm.avail_in = std::min(length,CHUNK);
    length -= strm.avail_in;
    it += strm.avail_in;
    do {
        do {
            do {
                strm.avail_out = CHUNK;
                strm.next_out = out;

                ret = inflate(&strm, Z_SYNC_FLUSH);
                assert(ret != Z_STREAM_ERROR);
                have = CHUNK - strm.avail_out;
                compressed.reserve(compressed.size() + have);
                compressed.insert(compressed.end(), out, out + have);
                switch (ret) {
                    case Z_NEED_DICT:
                        ret = Z_DATA_ERROR; 
                    case Z_DATA_ERROR:
                    case Z_MEM_ERROR:
                        it += length;
                        (void)inflateEnd(&strm);
                        return ret;
                }
            } while (strm.avail_out == 0);
            if (length == 0)
                break;
            if (buffToInteger(*it) != 0x00010000)
                int a = 0;
            it += 4; //Drop 4 bytes every chunk size
            length -= 4;
            strm.next_in = &(*it);
            strm.avail_in = std::min(length, CHUNK);
            length -= strm.avail_in;
            it += strm.avail_in;
        } while (strm.avail_in != 0);
        break;
    } while (ret != Z_STREAM_END);
    assert(buffToInteger(*(it - 4)) == 0xffff0000);
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}
void CivVIParser::ReadBoolean(OUT CivVIPacket& result)
{
    result.data.bool_ = *it;
    it+=4;
}

void CivVIParser::ReadInt(OUT CivVIPacket& result)
{
    result.data.int_ = buffToInteger(*it);
    it+=4;
}
void CivVIParser::ReadString(OUT CivVIPacket& result)
{
    char* str = new char[result.length];
    memcpy(str, &(*it), result.length*sizeof(char));
    result.data.pointer = str;
    it += result.length ? result.length : 4;
}
void CivVIParser::ReadUtfString(OUT CivVIPacket& result)
{
    char* str = new char[result.length*2];
    memcpy(str, &(*it), result.length*2 * sizeof(char));
    result.data.pointer = str;
    it += result.length * 2;
}
void CivVIParser::ReadTimestamp(OUT CivVIPacket& result)
{
    timestamp = buffTolong(*it);
    it += 8;
}

bool CivVIParser::ReadArray(OUT CivVIPacket& result, bool isArray)
{
    int arrayLen = buffToInteger(*it);
    it += 4;

    result.length = arrayLen;
    result.data.int_ = arrayLen;

    for (int i = 0; i < arrayLen; i++) {
        CivVIPacket newP;
        if (!ParseEntry(newP, isArray ? i : -1))
            assert(0);
        result.packets[newP.marker] = newP;
    }
    return true;
}

bool CivVIParser::ParseEntry(OUT CivVIPacket& result, int iArray)
{
    bool successfulParse = true;
    do {
        if (iArray == -1)
        {
            result.marker = buffToInteger(*it);
            it += 4;
        }
        else
            result.marker = iArray;

        result.type = buffToInteger(*(it));
        result.length = buffToInteger(*(it + 4), 3);
        result.found = *(it + 7);
        result.info = buffToInteger(*(it + 8));
        it += 12;

        switch (result.type) {
        case DATA_TYPES::BOOLEAN_:
            ReadBoolean(result);
            break;

        case DATA_TYPES::INTEGER:
            ReadInt(result);
            break;

        case POINTER:
            //don't know this type....
            ReadInt(result);
            break;

        case 0x15: // on 2 byte ? int64? int -> int ? 
            result.data.pointer = (void*)*it;
            it += 8;
            break;

        case 4:
        case DATA_TYPES::STRING:
            ReadString(result);
            break;

        case DATA_TYPES::UTF_STRING:
            ReadUtfString(result);
            break;

        case TIMESTAMP:
            ReadTimestamp(result);
            break;

        case COMPRESSED:
        {
            unsigned int unknown1 = buffToInteger(*it);
            unsigned int unknown2 = buffToInteger(*(it + 4));
            unsigned int unknown3 = buffToInteger(*(it + 8));
            it += 12;

            ReadCompressed(result);
            break;

        }

        case 0x0D:
            result.data.pointer = (void*)*it;
            it += 8;
            break;

        case OBJECT:
            if (!ReadArray(result, false))
                return false;
            break;

        case ARRAY:
            if (!ReadArray(result, true))
                return false;
            break;

        default:
            successfulParse = false;
            assert(0);
            it -= 12;
            break;
        }
    } while (!successfulParse);

    return true;
}

void CivVIParser::ParseTurn(std::vector<byte>& data, OUT int& turn)
{
   it = data.begin() + 377;
   turn = readInteger(it);
}

void CivVIParser::ParseMap(std::vector<byte>& data, OUT int& width, OUT std::vector<Tile>& tiles)
{
    it = std::search(data.begin(), data.end(), MAP_BEGIN, MAP_BEGIN + 28);
    assertT(it != data.end())
    std::vector<byte>::iterator begin = it; //for debug
    it += 28;

    int size = buffToInteger(*it);
    it += 4;

    tiles.resize(size);

    for (int i = 0; i < size; i++)
    {
        Tile& tile = tiles[i];

        tile.iTile = i;
        tile.group[0] = readInteger(it);
        tile.group[1] = readInteger(it);

        tile.landmass = readInteger(it);

        tile.terrain = readInteger(it);
        tile.feature = readInteger(it);

        it += 2;    //Unknown int16
        tile.continent = readInteger(it);    //Unknown int32
        it += 1;    //Unknown int8

        tile.resource= readInteger(it);
        tile.resourceCount = readInteger(it, 2);

        tile.improvment = readInteger(it); //IMPROVEMENT_MOUNTAIN_TUNNEL

        it += 1;//Unknown int8

        tile.road = readInteger(it, 1);
        tile.roadLvl = readInteger(it, 1);

        tile.appeal = readInteger(it,2);

        it += 3;//3 Unknown int8

        tile.riverCount = *it;
        it += 1;

        tile.river = *it;
        it += 1;

        tile.cliffbitmap = *it;
        it += 1;

        tile.pillage = readInteger(it, 1);    //Unknown int8 related to appeal
        int found = readInteger(it,1); //0x40 instead of 0 when near city --> bitmap??
        it += 1;    //Unknown int8

        tile.bOverlay = readInteger(it);

        if (tile.bOverlay)
        {
            unsigned int count = readInteger(it);
            for (int k = 0; k < count; k++)
            {
                it += 11; //05 B3 95 2F 0A 00 00 00 00 00 00 05 00 00 00 00
                int value = readInteger(it);
                it++;
                unsigned int count2 = readInteger(it);
                if (value)
                {
                    for (int k = 0; k < count2; k++)
                    {
                        it += 20; //C3 9B FA F9 03 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00
                    }
                }     
            }
        }

        if (found & 0x40)
        {
            tile.iCity = readInteger(it, 2);
            //assert((short)buffToInteger(*it, 2) == tile.iCity + 1);
            it += 2; // iCity + 1;
            //assert((short)buffToInteger(*it, 2) == tile.iCity);
            it += 4; //icity - iCity +1;
            tile.iDistrict = readInteger(it, 2);
            it += 2; //idistrict+1
            tile.iPlayer = readInteger(it, 1);
            tile.wonder = readInteger(it);
        }
    }
    it += 4;
    width = buffToInteger(*it);
}

std::string readString(std::vector<byte>::iterator& it)
{

    unsigned int count = buffToInteger(*it);
    it += 4;    
    std::string str((char*)&(*it), count);
    it += count;
    return str;
}

void readMap(std::vector<byte>::iterator& it, std::map<unsigned int, unsigned int>& map, int sizeValue = 4)
{
    unsigned int count = buffToInteger(*it);
    it += 4;
    assertT(count < 200000);
    for (int iValue = 0; iValue < count; iValue++)
    {
        unsigned int key = buffToInteger(*it);
        it += 4;
        unsigned int value = buffToInteger(*it, sizeValue);
        it += sizeValue;
        map[key] = value;
    }
}
float readFloat(std::vector<byte>::iterator& it)
{
    unsigned int value = readInteger(it);
    int a = value >> 8;
    int b = value & 0xff;
    float fvalue = a + (float)b / 256;//+ 1 / pow(2, b);
    return fvalue;
}
std::map<unsigned int, float> readMapFloat(std::vector<byte>::iterator& it, int sizeValue = 4)
{
    std::map<unsigned int, float> map;
    unsigned int count = buffToInteger(*it);
    it += 4;
    assertT(count < 200000);
    for (int iValue = 0; iValue < count; iValue++)
    {
        unsigned int key = buffToInteger(*it);
        it += 4;
        assert(sizeValue == 4);
        map[key] = readFloat(it);
    }
    return map;
}
std::map<unsigned int, bool> readMapBool(std::vector<byte>::iterator& it)
{
    std::map<unsigned int, bool> map;
    unsigned int count = buffToInteger(*it);
    it += 4;
    assertT(count < 200000);
    for (int iValue = 0; iValue < count; iValue++)
    {
        unsigned int key = buffToInteger(*it);
        it += 4;
        bool value = buffToInteger(*it, 1);
        it ++;
        map[key] = value;
    }
    return map;
}
std::map<unsigned int, unsigned int> readMap(std::vector<byte>::iterator& it, int sizeValue = 4)
{
    std::map<unsigned int, unsigned int> map;
    unsigned int count = buffToInteger(*it);
    it += 4;
    assertT(count < 200000);
    for (int iValue = 0; iValue < count; iValue++)
    {
        unsigned int key = buffToInteger(*it);
        it+=4;
        unsigned int value = buffToInteger(*it, sizeValue);
        it += sizeValue;
        map[key] = value;
    }
    return map;
}

std::vector<unsigned int> readArray(std::vector<byte>::iterator& it, int size=4, int sep = 1, int sizeCount = 4)
{
    std::vector<unsigned int> array;
    unsigned int count = buffToInteger(*it, sizeCount);
    it += sizeCount;
    assertT(count < 200000);
    array.resize(count);
    for (int iValue = 0; iValue < count; iValue++)
    {
        unsigned int value = buffToInteger(*it, size);
        it += size;
        array[iValue] = value;
        if(value != 0)
            it += sep;
    }
    return array;
}

void CivVIParser::ParseTrees(std::vector<byte>& data, std::vector<Node>& civicTree, std::vector<Node>& techTree)
{
    it = std::search(data.begin(), data.end(), VICTORIES_BEGIN, VICTORIES_BEGIN + 9);
    assertT(it != data.end());
    int diploCount = readInteger(it);
    for (int i = 0; i < diploCount; i++)
    {
        unsigned int type = readInteger(it);
        it += 4; // 7
        it += 524; // 00 FF
    }
    it += 4; //4
    int mapSize = readInteger(it);
    for (int i = 0; i < mapSize; i++)
    {
        it += 9; //04 00 00 00 00 00 00 00 01
    }
    it += 32; //00 ff 01 00 03 00 00 02
    int count = readInteger(it);
    assertT(count == 0x40);
    for (int i = 0; i < count + 1; i++)
    {
        int count2 = readInteger(it); //not sure on this one it was only the last
        assert(count2 == 0x36);  
        for (int j = 0; j < count2; j++)
        {
            int count3 = readInteger(it); // 00 or 02
            for (int k = 0; k < count3; k++)
            {
                it += 4;
            }
        }
    }
    it += 8; //0
    it += 4; //06
    int civicCount = readInteger(it);
    civicTree.resize(civicCount);
    for (int i = 0; i < civicCount; i++)
    {
        it += 4; //6
        civicTree[i].id = readInteger(it);
        it += 2; //1
        civicTree[i].cost = readInteger(it, 3);
        int count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            unsigned int parent = readInteger(it);
            civicTree[i].parents.push_back(parent);
        }

    }
    it += 4;//civicCount
    for (int i = 0; i < civicCount; i++)
    {
        it += 4;
    }
    int greatWorkCount = readInteger(it);
    for (int i = 0; i < greatWorkCount; i++)
    {
        it += 4; //6
        unsigned int greatWork = readInteger(it);
        it += 4;
        it += 4;
        readString(it);
    }
    it += 4; //03

    int techCount = readInteger(it);
    techTree.resize(techCount);
    for (int i = 0; i < techCount; i++)
    {
        it += 4; //3
        techTree[i].id = readInteger(it);
        it += 2; //1
        techTree[i].cost = readInteger(it, 3);
        int count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            unsigned int parent = readInteger(it);
            techTree[i].parents.push_back(parent);
        }

    }

}

void CivVIParser::ParseVisibility(std::vector<byte>& data, OUT std::vector<Player>& players)
{
    it = std::search(data.begin(), data.end(), VISIBILITY_BEGIN, VISIBILITY_BEGIN + 4);
    it -= 4; //36/37 00 00 00 
    //resource count
    readMap(it);
    readMap(it);
    it += 4; // 01 00 00 00
    unsigned int playerCount = readInteger(it);

    for (int iPlayer = 0; iPlayer < playerCount; iPlayer++)
    {
        Player& p = players[iPlayer];
        //--
        it += 16; //iPlayer 06 00 00 00 iPlayer 00 00 00 00
        it++;
        p.visible = readArray(it, 1, 0);
        it += 12; //05 00 00 00 26 10 35 11 00 00 00 00
        assertT(buffToInteger(*it) == p.visible.size());
        it += 8; //B8 11 00 00 54 00 00 00 
        p.inSightCount = readArray(it, 2, 0);
        it += 8; //CF 56 A3 F7 01 00 00 00
        assertT(buffToInteger(*it) == p.visible.size());
        it += 8; //B8 11 00 00 54 00 00 00 
        p.improvment = readArray(it, 2, 0);
        it += 8; //8F 28 80 D1 02 00 00 00
        assertT(buffToInteger(*it) == p.visible.size());
        it += 8; //B8 11 00 00 54 00 00 00
        p.resource = readArray(it, 2, 0);
        it += 8; //5C F9 A6 A8 03 00 00 00
        assertT(buffToInteger(*it) == p.visible.size());
        it += 8; //B8 11 00 00 54 00 00 00
        p.e = readArray(it, 2, 0);
        it += 8; //BD BE F3 C1 04 00 00 00 
        assertT(buffToInteger(*it) == p.visible.size());
        it += 8; //B8 11 00 00 54 00 00 00
        p.inSightUnitCount = readArray(it, 2, 0);

        unsigned int count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }
    }

}

void CivVIParser::ParseDisaster(std::vector<byte>& data, int width, IN_OUT std::vector<Tile>& tiles)
{
    it = std::search(data.begin(), data.end(), FEATURE_BEGIN, FEATURE_BEGIN + 7);
    assertT(it != data.end());
    std::vector<byte>::iterator begin = it; //for debug
    it += 7;

    it += 4; //??2C 8F 00 00
    it += 8; //07 00 00 00 04 00 00 00
    readMap(it); //06
    it += 4; //tile count
    std::vector<int> mapping;
    mapping.reserve(tiles.size());

    for (int i = 0; i < tiles.size(); i++)
    {
        int indexDisaster = readInteger(it);
        if (indexDisaster != 0xffffffff)
        {
            while (mapping.size() <= indexDisaster)
                mapping.push_back(-1);
            mapping[indexDisaster] = i;
        }
    }
    int count = readInteger(it);
    assertT(mapping.size() == count);

    for (int i = 0; i < count; i++)
    {
        tiles[mapping[i]].disaster = readMap(it); //06
    }

    /*
    
    int count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        readInteger(it); //05
        readInteger(it); //name
        readInteger(it); //itile
        readInteger(it); //civ
        ... 19 int
    }
    */


}

void CivVIParser::ParsePlayers(std::vector<byte>& data, OUT CivVIState* pState)
{
    it = std::search(data.begin(), data.end(), TERRITORY_BUILDER, TERRITORY_BUILDER + 16);
    std::vector<byte>::iterator begin = it; //debug purpose
    it += 16;

    it += 8; //00 07
    it += 8; //?

    std::vector<Player>& players = pState->players;
    std::vector<Religion>& religions = pState->religions;

    unsigned int count = readInteger(it);
    religions.resize(count);
    // RELIGION
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 7);
        it += 4;
        religions[i].iSymbol = readInteger(it);
        religions[i].iPlayer = readInteger(it);
        it += 12;
        religions[i].name = readString(it); //name of religion
        unsigned int count2 = readInteger(it); //beliefs
        for (int k = 0; k < count2; k++)
        {
            religions[i].beliefs.push_back(readInteger(it));
        }
        count2 = readInteger(it); //buildings
        for (int i = 0; i < count2; i++)
        {
            religions[i].buildings.push_back(readInteger(it));
        }
        count2 = readInteger(it); //units
        for (int i = 0; i < count2; i++)
        {
            religions[i].units.push_back(readInteger(it));
        }
        it += 11; //00
    }

    assertT(buffToInteger(*it) == 1);
    it += 4;

    unsigned int playerCount = buffToInteger(*it);
    it += 4;
    players.resize(64);

    for (int iPlayer = 0; iPlayer < playerCount; iPlayer++)
    {
        //assertT(buffToInteger(*it) == iPlayer || buffToInteger(*it) == 0x3E || buffToInteger(*it) == 0x3F); //3E free cities 3f barbarian
        int trueiPlayer = readInteger(it); //iPlayer
        Player& p = players[trueiPlayer];
        p.iPlayer = trueiPlayer;
        assertT(buffToInteger(*it) == 47); //2F 00 00 00
        it += 4; //2F 00 00 00
        assert(buffToInteger(*it) == p.iPlayer); //2F 00 00 00
        it += 4; //iPlayer

        it += 4;
        it += 4; //init pos ?
        it += 56;
        for (int i = 0; i < 10; i++) {
            it ++;
        }
        //assertT(buffToInteger(*it) == 0xf54624af || buffToInteger(*it) == 0x404a43ce || buffToInteger(*it) == 0x5c01a5d7 || buffToInteger(*it) == 0x5965989d || buffToInteger(*it) == 0xFFFFFFFF); //AF 24 46 F5
        it += 4;

        for (int i = 0; i < 7; i++) {
            it+=4;
        }
        // UNITS POSITION (does not count grouped units)
        unsigned int unitCount = buffToInteger(*it);
        it += 4;
        //p.units.resize(unitCount);
        for (int  iUnit= 0; iUnit < unitCount; iUnit++)
        {
            //Unit& unit = p.units[iUnit];
            for (int i = 0; i < 3; i++) {
                it += 4; //01 00 00 00 01 00 00 00 01 00 02 00
            }
            //unit.x = buffToInteger(*it);
            it += 4;

            //unit.y = buffToInteger(*it);
            it += 4;

            it += 8; // 00
            it += 12; //02 00 00 00 00 00 01 00 01 01 00 00 

            unsigned int count2 = buffToInteger(*it);
            it += 4;
            it += 8; //F1 D8 FF FF F1 D8 FF FF or 01 00 00 00 00 01 00 00
            for (int i = 0; i < count2; i++)
            {
                it += 20;
            }

            count2 = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count2; i++) {
                it += 4;
            }
        }

        it += 4;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        it += 12; //06 07 01

        p.goodyHuts = readMap(it); //08
        it += 4;
        readMap(it); //0b
        assert(buffToInteger(*it) == 0x0c);
        it += 4;
        p.diploFavor = readInteger(it);
        //assert(buffToInteger(*it) == p.diploFavor);
        it += 20;

        readMap(it); //3 same 6 map
        readMap(it);
        readMap(it);
        readArray(it, 4, 0); //05
        readMap(it); //06

        //CITIES 
        it += 33;
        assertT(buffToInteger(*it) == 0x10);
        it += 4;
        it += 4; 
        unsigned int cityCount = buffToInteger(*it);
        it += 4;
        p.cities.resize(cityCount);

        for (int iCity = 0; iCity < cityCount; iCity++) {
            City& city = p.cities[iCity];
            //assertT(buffToInteger(*it) == iCity);
            city.id = readInteger(it);
            assertT(buffToInteger(*it) == 0x33);
            it += 4;
            it += 4;
            city.x = readInteger(it);
            city.y = readInteger(it);

            it += 12; //0
            it += 4; //FF
            it += 4; // 00 00 01 00
            city.population = readInteger(it);
            it += 2; //01 01
            it += 4; //0x61723064 or 0x64307261 or 0xde340dba
            it += 8; //FF
            it += 4; //1F / FF

            for (int i = 0; i < 64; i++)
                it += 4;
            it++;

            it += 18; //00 / 0200

            for (int i = 0; i < 13; i++) //13?
            {
                assertT(buffToInteger(*it) == 6);
                readMap(it);
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 20; //06 00 00 00 FF FF FF FF 04 00 00 00 04 00 00 00 00 00 00 00
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 16; //IMPROVEMENT_TERRACE_FARM
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 8;
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; // open air-museum
            }
            it += 8; //FF 00
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                assertT(buffToInteger(*it) == 0x33);
                it += 4;
                it += 4;
                it += 8; //ff
                it += 4; //67 2A B7 3B
                it += 6; //00 00 FF FF FF FF
                it += 4;
                it += 8; //02 01
                readString(it);//LOC_DISTRICT_MINE_1_SCIENCE3
            }

            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 16; //33 00 00 00 11 7A 70 4A AB F8 81 3A 80 00 00 00 navigation school ! (science on coast)
            }
            it += 17; //maybe an array 02
            assertT(buffToInteger(*it) == 6);
            readMap(it);
            it+=13;
            city.name = readString(it);
            it += 8; //00
            it += 4;
            count = buffToInteger(*it);
            it += 4; //other city ?
            for (int i = 0; i < count; i++)
            {
                it += 4;
            }

            it += 8; 

            readArray(it, 4, 0);//25
            readArray(it, 4, 0);//25

            readArray(it);//06
            readArray(it);//06
            it += 9; //00 00 00 00 05 00 00 00 00
            it += 36; //11 ...
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 22;
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 20;
            }
            it += 4; //0 or 1
            it+=25;//05 00 00 00 5D 0D 00 00 00 00 00 00 00 00 00 00 00 0B 00 00 00 00 00 00 00

            readMap(it);//8c
            readMap(it);//85
            readMap(it);//22
            readMap(it);//11
            it += 4;
            readMap(it);//85
            readMap(it);//8c
            it += 5;
            readArray(it, 4, 0); //17
            it += 33; //00
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 8; //39 54 11 E9 02 00 00 00 //jungle
            }

            it += 4; //05
            city.religion = readInteger(it);
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 17; //05 00 00 00 05 3F E6 F2 00 00 00 00 00 10 00 00 00
            }
            it += 8; //religion and adept
            it += 18; //00
            it += 4; //FF
            it += 16; //0A 00 00 00 00 00 02 00 02 00 03 00 00 00 00 00
            
            readMap(it);//06
            readMap(it);//06
            it += 12;
            readMap(it);//11
            readMap(it);//36
            
            it += 4; //10
            count = buffToInteger(*it);
            it += 4; // productions 
            for (int i = 0; i < count; i++)
            {
                assertT(buffToInteger(*it) == 0x2C0F4A46);
                it += 4;
                it += 4; //02
                it += 4; //46 4A 0F 2C
                city.currentProdType = (ProductionType)readInteger(it);
                it += 4;
                if (city.currentProdType > 4) // 0 unit 1 building 2 district 3 ?
                    assertT(0);
                city.currentProds.push_back(readInteger(it));
                if (city.currentProdType == PROD_UNIT)
                    it += 4; // armada ?

                it += 12; //00 00 00 00 F1 D8 FF FF F1 D8 FF FF
            }
            it += 40; // 00 10 00 00 ??

            readMap(it);//46
            readMap(it);//46
            readMap(it, city.prod);//85
            readMap(it, 2);//85
            readMap(it, city.prod);//8C
            readMap(it, city.prod);//22
            readMap(it, city.prod);//23
            readMap(it,2);//22
            readMap(it);//85
            readMap(it);//8C
            readMap(it);//22
            readMap(it);//8C
            readArray(it);//46
            it += 8;
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; //10 00 00 00 36 73 11 98 26 77 b5 52 //DISTRICT_GOVERNMENT - FEATURE_FLOODPLAINS
            }
            readMap(it);//8C
            readMap(it);//23
            it += 9;
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; //10 00 00 00 10 5d b1 32 64 00 00 00  //Holysite
            }
            it += 4;
            readMap(it, city.built, 2);//85
            readArray(it);//85
            readMap(it, 2);//85
            readMap(it, 2);//85


            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; //FF 42 7A 2D E8 55 3F EB 32 00 00 00 //amphi//museum... - culture - ?
            }

            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12;
            }

            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12;
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 8; // 768e0f7f 00 //YIELD_FAITH
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 4; //AB EE 46 16 //building
                unsigned int count2 = readInteger(it);
                for (int i = 0; i < count2; i++)
                {
                    it += 12; //0A 04 FF
                }
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; //30 1A 36 66 04 00 00 00 02 00 00 00 //BUILDING_BANK free building ?
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 16; //0A 00 00 00 44 09 22 0A 04 00 00 00 01 00 00 00
            }
            //it += 4;
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 4;
            }
            it += 4; //07
            it += 37;

            count = buffToInteger(*it);
            assertT(count == 64);
            it += 4;
            for (int i = 0; i < count; i++)
            {
                it ++;
            }
            it += 4;

            for (int i = 0; i < 14; i++)
            {
                assertT(buffToInteger(*it) == 0x6);
                readMap(it);//06
            }
            it += 4;//0e
            it++;
            it += 4;//64

            it += 3; //1D 00 00 
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 13; //0E 00 00 00 03 00 00 00 27 10 00 00 00
            }
            it += 12; //01 00 00 00 0B 00 00 00 FF FF FF FF
            it += 49; //00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 80 FF FF FF FF 00 00 00 00 00 01 00 00 42 52 58 97 80 00 00 00 0A 00 00 00 00 00 00 00 00
            //ELECTRICITY AND USINE
            it += 12; //02 00 00 / 03 00 00 00 03 00 00 00 00 00 00 01
            it += 4; //4D 00 00 00
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 24; //0A 00 00 00 00 02 00 00 04 00 00 00 01 00 00 00 19 00 00 00 1E 00 00 00
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 20; //0A 00 00 00 00 02 00 00 DD 0E EC 64 04 00 02 00 02 00 03 00  COAL
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 16; //0A 00 00 00 00 02 00 00 A4 31 A5 BE FF FF FF FF//factory
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 32; //0A 00 00 00 00 02 00 00 DD 0E EC 64 00 01 00 00 04 00 02 00 02 00 03 00 17 00 00 00 1D 00 00 00
            }
            it += 4; //00
            //PLOT_PROPERTIES
            count = readInteger(it,3); //01 00 00 80
            it += 1;
            for (int i = 0; i < count; i++)
            {
                it += 8; //01 00 00 00 8B 1D A6 45
                unsigned int count2 = readInteger(it,2);
                it += count2; //CS_TRADERS
                it += 12;
                unsigned int count3 = readInteger(it, 3); //01 00 00 80
                it += 1;
                for (int j = 0; j < count3; j++)
                {
                    count2 = readInteger(it, 4);
                    for (int k = 0; k < count2; k++)
                    {
                        it += 22;
                    }
                }
            }
            it += 8; //FF FF FF FF 1A 00 00 00
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 4; //D9 2E d3 0E //IMPROVEMENT_CITY_PARK
            }

            readArray(it, 4, 0); //8c
            readMap(it);//06

            //it += 32; maybe out of this ?


        }
        it += 4;
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        it += 8; //06 07
        it += 5;//00
        it += 4; //CD 1E 48 CF  continent ?
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4; // other continent
        }
        it += 4;
        it += 8; //00
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //71 45 51 87 BUILDING_COAL_POWER_PLANT
        }
        it += 4; //00
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //CB EC C7 39
        }
        it += 4; //00
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8; //a0 f3 4b 87 00 a5 97 0c
        }
        assertT(buffToInteger(*it) == 0x32);
        it += 4;

        it += 3137;
        readMap(it); //2 same 6 map
        readMap(it);

        it += 53;
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 264; //76 77 9A 06 42 01 00 00 00 00 00 00 00
        }


        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 72; //EC 8A 9A E5 25 01 00 00 00 01 01 01 01 01 01 01 00+
        }

        // TOOLTIP 
        count = buffToInteger(*it);
        assertT(count == 64);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            unsigned int count2 = buffToInteger(*it);
            it += 4;
            for (int k = 0; k < count2; k++)
            {
                it += 4; //24 01 00 00
                it += 4; //0D 00 00 00
                it += 33;
                readString(it);
                it += 4;
            }
        }
        it += 12; //ff 00 0001

        for (int i = 0; i < 64; i++) {
            it += 16; // 00 except 1 64
        }
        for (int i = 0; i < 64; i++) {
            it += 4;
        }        
        for (int i = 0; i < 64; i++) {
            it += 4;
        }

        it += 768;

        unsigned int count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }


        for (int i = 0; i < 64; i++) {
            assertT(buffToInteger(*it) == 0x32);
            it += 13;
        }

        //4 00 to 4 array but not sure these one
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 21;
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 16;
        }
        count = readInteger(it); //00
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 16;
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //DIPLOACTION_DECLARE_LIBERATION_WAR / DIPLOACTION_DECLARE_SURPRISE_WAR
            it += 8; //8F 9D 4D 1B 87 CA 98 5E   //CIVIC_DEFENSIVE_TACTICS / CIVILIZATION_CANADA
        }

        it += 86;

        count = readInteger(it); // 40
        assertT(count == 64);
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        
        count = readInteger(it); // 40
        assertT(count == 64);
        for (int i = 0; i < count; i++)
        {
            it += 4; //FF
        }
        
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 16; //32 01 02f70b34 00
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8; // i 01
            unsigned int count2 = readInteger(it);
            for (int k = 0; k < count2; k++)
            {
                it += 8;
            }
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 14; // 01 00 00 00 01 01 00 00 00 00 6F F2 A0 14
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8;
            unsigned int count2 = readInteger(it);
            for (int k = 0; k < count2; k++)
            {
                it += 20;
            }
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8;
            unsigned int count2 = readInteger(it);
            for (int k = 0; k < count2; k++)
            {
                it += 4; //55 50 DF FA 
                unsigned int count3 = readInteger(it);
                for (int l = 0; l < count3; l++)
                {
                    it += 4; //32 00
                }
            }
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8;
            unsigned int count2 = readInteger(it);
            for (int k = 0; k < count2; k++)
            {
                it += 20;
            }
        }

        readArray(it); //8c (28 9D 64 D0...)

        readArray(it, 4, 0); // 40 * 00 00 00 00

        readMap(it); // 2a ? FC CC 36 D8

        it += 16; //08 00 00 00 01 00 01 00 10 00 00 00 08 00 00 00

        //DISTRICTS
        count = readInteger(it);
        p.districts.resize(count);
        for (int i = 0; i < count; i++)
        {
            District& d = p.districts[i];
            d.globalId = readInteger(it);
            assertT(buffToInteger(*it) == 0x0f);
            it += 4;
            d.id = readInteger(it,2);
            it += 2; //it+1
            d.x = readInteger(it);
            d.y = readInteger(it);
            d.cityId = readInteger(it,2);
            it += 2; //it+1
            d.type = readInteger(it);
            d.damage = readInteger(it);
            d.wallDamage = readInteger(it);
            it += 4; //9C FF FF FF
            d.wall = readInteger(it);
            d.cost = readInteger(it);
            d.built = readInteger(it); //01
            it++;
            for (int k = 0; k < 3; k++) {
                assertT(buffToInteger(*it) == 0x6);
                readMap(it);//06
            }
            it += 12; //00
            assertT(buffToInteger(*it) == 0x6);
            readMap(it);//06
            assertT(buffToInteger(*it) == 0x6);
            readMap(it);//06
            it += 4; //00
            unsigned int count2 = buffToInteger(*it);
            it += 4;
            for (int k = 0; k < count2; k++)
            {
                it += 16; 
            }
            readMap(it);//0A // personnages illustres

            it += 17;
            d.pillage = readInteger(it, 1);
        }

        it += 4;
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        it += 8; //06 07 or FFFF
        it += 8;

        readMap(it); // 22 00 districts (7D 00 32 8A...)
        readMap(it); // districts (7D 00 32 8A...)
        readMap(it); // districts (7D 00 32 8A...)
        readMap(it); // districts (7D 00 32 8A...)

        readMap(it); //70 ? 26 77 B5 52

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 22; //01 00 00 00 14 D1 DE 41 02 00 00 00 64 00 00 00 01 01 B1 29 09 0A
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }
        count = readInteger(it); // may be the next one
        for (int i = 0; i < count; i++)
        {
            it += 16;
        }
        count = readInteger(it); // may be the next one
        for (int i = 0; i < count; i++)
        {
            it += 12; //01 00 00 00 9E DC BC 5D D4 57 AB 3D //DISTRICT_CANAL <--> TECH_MASONRY = chinese with bbg
        }
        it += 4; //0
        it += 4; //2
        count = buffToInteger(*it);
        it += 4;
        assertT(buffToInteger(*it) == count);
        it += 4;
        it += count * 2;
        it += 4; //2d
        p.government = readInteger(it);
        it += 8; //FF
        p.lastTurnChangeGovernment = readInteger(it);
        it += 4;
        it++;
        readArray(it); //0D 00 00 00 9A 87 93 6E
        readMap(it); //0D 00 00 00 9A 87 93 6E -> Government used
        readMap(it); // -> government card 
        for(int i=0;i<5;i++)
            readArray(it); //8F 00 00 00 4F 96 0A 6B

        count = readInteger(it);
        it += 4; //same as count ?
        for (int i = 0; i < count; i++)
        {
            unsigned int policy = readInteger(it);
            unsigned int position = readInteger(it);
            if (position == 4)
                position = 3; //ugly tric to avoid 0-1-2-4...
            assertT(position < 4);
            p.policies[position].push_back(policy);
        }

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }
        it += 4; //04 or 00
        it += 8; //00

        p.techDogmeInfo[DOGME].found = readMapBool(it); //3D 00 00 00 E0 76 60 3B
        p.techDogmeInfo[DOGME].boost = readMapBool(it);
        p.techDogmeInfo[DOGME].research = readMapFloat(it);

        p.techDogmeInfo[DOGME].current = readArray(it, 4, 0);


        it += 21;
        it += 4; // ??
        count = buffToInteger(*it); //40
        it += 4;
        for (int i = 0; i < count; i++)
        {
            p.totalTourismByPlayer[i] = readInteger(it);
        }
        count = buffToInteger(*it); //40
        it += 4;
        for (int i = 0; i < count; i++)
        {
            p.touristUnknown[i] = readInteger(it,1);
        }
        count = buffToInteger(*it); //40
        it += 4;
        for (int i = 0; i < count; i++)
        {
            p.touristByPlayer[i] = readInteger(it);
        }

        it += 12; //00
        it += 12; //32 00 00
        it += 4; //39 17 17 00
        p.internalTourist = readInteger(it);
        it += 8; //3A 00 00 00 08 00 00 00*

        count = readInteger(it); //40
        assertT(count == 0x40);
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        it += 8; // 00 ff

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; // id location tourism generator == city.prod
            it += 4; // generated tourism
            unsigned int count2 = readInteger(it);
            for (int j = 0; j < count2; j++)
            {
                it += 4; //FF FF FF FF
                int externalTourist = readInteger(it);
                p.externalTourist += externalTourist;
            }
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 17;
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }

        it++;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            assertT(buffToInteger(*it) == 0x2D);
            it += 12;  //2C 00 00 00 44 09 22 0A 52 AF AF 2D//CIVIC_MEDIEVAL_FAIRES (science ?)
        }
        it += 4; //00

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            assertT(buffToInteger(*it) == 0x2D);
            it += 13;
        }
        it += 16;
        it += 4;
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }
        it += 4; //00
        it += 8; //FFFFFF7F FFFFFF7F
        it += 8; //00

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 13; //2c 01 04
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 16; //GOVERNMENT_MONARCHY
        }

        // SPY AND SPYING

        assertT(buffToInteger(*it) == 0x11);
        it += 8; //11 00 00 00 02 00 00 00 

        count = readInteger(it); //SPY
        for (int i = 0; i < count; i++)
        {
            it += 24; //11 00 00 00 00 00 00 00 A6 DA 73 23 0E 00 00 00 02 00 00 00 FF FF FF FF 
            unsigned int count2 = readInteger(it); //promote spy
            for (int k = 0; k < count2; k++)
            {
                it += 4;
            }
        }

        count = readInteger(it); //SPY
        for (int i = 0; i < count; i++)
        {
            it += 40; //11 00 00 00 A6 DA 73 23 0E 00 00 00 01 00 00 00 00 00 00 00 52 00 00 00 24 00 00 00 2E 00 00 00
            unsigned int count2 = readInteger(it); //promote spy
            for (int k = 0; k < count2; k++)
            {
                it += 4;
            }
            it += 4;
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 16; //11 00 00 00 05 00 00 00 02 00 03 00 5C 00 00 00 01 00 00 00 
        }
        it += 4; //00/01
        it += 4; //00
        it += 4; //00
        count = readInteger(it); 
        for (int i = 0; i < count; i++)
        {
            it += 44; //11 00 00 00 A6 DA 73 23 15 00 00 00 01 00 00 00 00 00 00 00 58 00 00 00 19 00 00 00 09 00 00 00 07 00 00 00 FF FF FF FF 00 00 00 00 17 00 53 00
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 16; //11 00 00 00 03 00 00 00 C8 00 00 00 01 00 00 00
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12; //11 00 00 00 03 00 00 00 C8 00 00 00
        }



        assertT(buffToInteger(*it) == 0x14);
        it += 4; //14
        it++;
        p.faith = readInteger(it);
        it += 3;
        p.pantheon = readInteger(it); //belief
        it += 4;
        it += 8; //f1d8ffff f1d8ffff

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 24; //14 00 00 00 04 00 00 00 04 00 00 00 02 00 00 00 01 00 00 00 1E DF 5F 88
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 13; //14 00 00 00 04 00 00 00 14 00 00 00 01
        }
        it += 4; //00
        it += 44; //maybe an array with 0A
        it += 2; // 00
        it += 4; //FF
        it += 20;


        readMap(it); // 15
        it += 16; //FF 00 00 FF

        readArray(it,4,0); //19 00000000
        it += 4; // 10
        readArray(it); //36

        it += 13; //00

        //Strategic Resources
        count = readInteger(it); //36 00 00 00
        p.strats.resize(count);
        for (int i = 0; i < count; i++)
        {
            Resource& s = p.strats[i];
            s.type = readInteger(it);
            assert(buffToInteger(*it) == 3);
            it += 4;
            s.current = readInteger(it);
            s.get = readInteger(it);
            s.give = readInteger(it);
        }
        assert(buffToInteger(*it) == count);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            Resource& s = p.strats[i];
            it += 4; //type
            s.prod = readFloat(it);
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }

        readMap(it); //36 00 00 00 D2 DE 2D 3F
        readMap(it); //36 00 00 00 D2 DE 2D 3F
        readMap(it); //36 00 00 00 D2 DE 2D 3F
        readMap(it); //36 00 00 00 D2 DE 2D 3F

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            Resource& s = p.strats[i];
            it += 4; //type
            s.prod += readInteger(it);
        }

        readMap(it); //36 00 00 00 D2 DE 2D 3F
        readMap(it); //36 00 00 00 D2 DE 2D 3F

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            Resource& s = p.strats[i];
            it += 4; //type
            s.max += readInteger(it);
        }
        readMap(it); //36 00 00 00 D2 DE 2D 3F
        readMap(it); //36 00 00 00 D2 DE 2D 3F

        it += 32;
        it += 65; //8c ...
        p.diploPoint = readInteger(it, 3);
        it += 3; //01 00 00

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 13; //00 00 00 00 A7 1F B0 2B FF FF FF FF 01 
            readString(it); //LOC_DVP_TOOLTIP_AGREEING_RESOLUTIONS
            it += 22;
        }
        it += 20;

        p.unitsCount = readMap(it);  //8C 00 00 00
        readMap(it); //23 00 00 00 BA F1 BF 93
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //55 09 00 00
        }
        readMap(it); // 06

        it += 1888; // 40 00 00 00

        p.techDogmeInfo[TECH].current = readArray(it, 4, 0);

        it += 12; //00

        p.techDogmeInfo[TECH].found = readMapBool(it); //4D 00 00 00 C5 28 C5 38
        p.techDogmeInfo[TECH].boost = readMapBool(it);
        p.techDogmeInfo[TECH].research = readMapFloat(it);

        it += 8;//00
        it += 4; //00 with maj
        it += 4;//0A
        it++;
        p.gold = readInteger(it);
        it += 4;//0
        it += 4;//06
        it += 20;//20
        it += 4;//05
        it += 3;

        readMap(it); //3A 00 00 00 BC 0A 2B DE

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 8; //7C 04 00 00 03 00 00 00
        }
        it += 16; //16 00 00 00 01 00 01 00 10 00 00 00 18 00 00 00

        //UNITS 
        count = readInteger(it);
        p.units.resize(count);
        for (int iUnit = 0; iUnit < p.units.size(); iUnit++)
        {
            Unit& unit = p.units[iUnit];
            unit.id = readInteger(it);
            count = buffToInteger(*it); //Not sure on this one ... 
            it += 4;
            it += 4;

            unit.type = readInteger(it);

            it += 4;
            it += 4; //FF FF FF FF

            it += 4; //F1 D8 FF FF
            it += 4;
            unit.x = readInteger(it);
            unit.y = readInteger(it);

            it += 17; //FF FF FF FF 00 00 00 00 00 00 00 00 00 02 00 06 00
            unit.army = readInteger(it);
            it += 8; //FF 2E 01 25 6B
            unit.damage = readInteger(it);
            it += 58;
            unit.fortified = readInteger(it);
            it += 11;

            it += 199;

            for (int i = 0; i < 4; i++) //maybe first is count
            {
                it += 4;
            }
            it += 4; //00 with maj
            it++; // new magic firaxis byte

            //Lot's of change here between saves
            count = readInteger(it);
            for (int i = 0; i < count; i++) 
            {
                it += 8;
            }

            readMap(it); //C9 00 00 00 71 75 08 E4

            readArray(it); //11 00 00 00 30 C6 E7 83
            readArray(it); //11 00 00 00 30 C6 E7 83
            readArray(it); //46 00 00 00 26 77 B5 52
            readMap(it); //0A 00 00 00 ED 6F 42 A0
            readMap(it);

            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 5; //5B E2 C8 D1 01//PROMOTION_CLASS_ANTI_CAVALRY
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 5; //5B E2 C8 D1 01//PROMOTION_CLASS_ANTI_CAVALRY
            }
            it += 6; //00
            unit.xp = readInteger(it);
            unit.level = readInteger(it);
            it += 12;
            readArray(it); //91 00 00 00 73 53 B0 32

            readString(it);

            it += 77;
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; //01 00 00 00 03 00 00 00 0A 00 00 00
            }
            it += 54;

            assertT(buffToInteger(*it) == 6);
            readMap(it); //06 00
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 4; //2c040000
            }
            readMap(it); //22 00 00 00 7D 00 32 8A
            readMap(it); //3A 00 00 00 BC 0A 2B DE
            readMap(it); //22 00 00 00 7D 00 32 8A
            readMap(it); //3A 00 00 00 BC 0A 2B DE


            it += 66;
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 13; //07 00 00 00 42 D7 F0 1C 01 02 00 00 00 00// something link to spy operations
            }
            count = readInteger(it);
            for (int i = 0; i < count; i++)
            {
                it += 12; //07 00 00 00 E8 47 16 C3 19 00 00 00 // something link to spy operations
            }
            it += 70;

            count = buffToInteger(*it); 
            it += 4;
            for (int i = 0; i < count; i++)
            {
                if (buffToInteger(*it) == 0x580F2F68) //UNITOPERATION_MOVE_TO
                    it += 44;
                else if (buffToInteger(*it) == 0xb2cca377) //UNITOPERATION_SPY_TRAVEL_NEW_CITY
                    it += 44;
                else if (buffToInteger(*it) == 0x8374D954) //UNITOPERATION_MAKE_TRADE_ROUTE
                    it += 56;
                else if (buffToInteger(*it) == 0x9C0B44C6) //UNITOPERATION_AUTOMATE_EXPLORE
                    it += 44;
                else if (buffToInteger(*it) == 0x09d0292a) //UNITOPERATION_RANGE_ATTACK
                    it += 44;
                else if (buffToInteger(*it) == 0x1d60e778) //UNITOPERATION_BUILD_IMPROVEMENT
                    it += 40;
                else if (buffToInteger(*it) == 0x886ffcd1) //UNITOPERATION_SPY_COUNTERSPY
                    it += 44;
                else if (buffToInteger(*it) == 0x4885d724) //UNITOPERATION_PILLAGE
                    it += 40;
                else if (buffToInteger(*it) == 0x7fa205d1) //UNITOPERATION_SWAP_UNITS
                    it += 44;
                else if (buffToInteger(*it) == 0xcfb9b561) //UNITOPERATION_COASTAL_RAID
                    it += 44;
                else if (buffToInteger(*it) == 0xc8ce5dfb) //UNITOPERATION_TELEPORT_TO
                    it += 44;
                else if (buffToInteger(*it) == 0x8ca367f) //UNITOPERATION_REPAIR
                    it += 40;
                else if (buffToInteger(*it) == 0x1f633b1e) //UNITOPERATION_AIR_ATTACK
                    it += 44;
                else if(buffToInteger(*it) == 0x6e68aef) //UNITOPERATION_BUILD_ROUTE
                    it += 40;
                else if (buffToInteger(*it) == 0x852ce4df) //UNITOPERATION_TOURISM_BOMB
                    it += 44;
                else if (buffToInteger(*it) == 0x98eca9ea) //UNITOPERATION_WMD_STRIKE
                    it += 48;
                else
                    it += 32;
            }

            //assertT(buffToInteger(*it) == 0x3F);
            readArray(it); //3F 00 00 00 1E 3B 63 1F

            it += 16; //05 00 00 00 FF FF FF FF FF FF 02 00 FF FF FF FF 

            count = readInteger(it, 3); //01 00 00 80 
            it++;
            for (int i = 0; i < count; i++)
            {
                it += 26; //01 00 00 00 07 CA 81 A2 00 00 02 00 00 00 00 00 00 00 00 00 00 00 FF FF FF FF 
            }
        }
        it += 4;

        count = buffToInteger(*it);
        it += 4; 
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        it += 8; //06 07

        readMap(it); //8C 00 00 00 28 9D 64 D0
        readMap(it);
        readMap(it);

        readMap(it); //15 00 00 00 C3 72 FA 30
        readArray(it); //8C 00 00 00 28 9D 64 D0

        it += 3; //00

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //7D 00 32 8A
            unsigned int count2 = readInteger(it);
            for (int i = 0; i < count2; i++)
            {
                it += 8; //28 9D 64 D0 01 00 00 00 / 32 75 D5 11 01 00 00 00
            }
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //UNIT_LAHORE_NIHANG
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //Trader
        }
        it += 4;

        if (buffToInteger(*it) == 0) //HACK !!! Don't know why sometimes it's 19 ?!
            it += 4;

        readMap(it); //8C 00 00 00 28 9D 64 D0

        it += 12; //00

        count = readInteger(it); // 0x26
        for (int i = 0; i < count; i++)
        {
            it += 12; //16/15 00 00 00 .. .. .. .. 16/15 00 00 00 
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++) // PROMOTION apostle!!
        {
            it += 12; //16 00 00 00 E3 B4 38 2B 01 00 19 00 
        }

        count = readInteger(it); 
        for (int i = 0; i < count; i++)
        {
            it += 12; //16 00 00 00 .. .. .. .. 16 00 00 00 
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++) // unit apostle ?
        {
            it += 4; //8b d1 fd b9 
        }

        it += 32; //B1 22 37 FF seems interesting here
        it += 44; //x02 may be an array

        readMap(it); //8C 00 00 00 28 9D 64 D0
        readMap(it); //36 00 00 00 D2 DE 2D 3F

        readArray(it, 4, 0); //02 00 00 00 57 C3 A5 91 57 C3 A5 91 --> 2 eras ?

        it += 4; //23 (21 before maj)
        it += 4; //00
        p.availableRoads = readInteger(it);
        p.unknownRoads = readInteger(it);
        it += 52; //00

        for (int i = 0; i < 2; i++)
        {
            it += 4;//40
            it += 64;//00
        }

        for (int i = 0; i < 13; i++) //13 (12 before maj)
        {
            assertT(buffToInteger(*it) == 6);
            readMap(it); //06 00
        }
        it += 4; // 00;        
        for (int i = 0; i < 72; i++) //73???
        {
            assertT(buffToInteger(*it) == 6);
            readMap(it); //06 00
        }

        //IMPROVMENTS YIELD
        for (int k = 0; k < 3; k++)
        {
            count = buffToInteger(*it); // 3A 00 00 00
            it += 4;
            for (int i = 0; i < count; i++)
            {
                it += 4;
                assertT(buffToInteger(*it) == 6);
                readMap(it); //06 00
            }
        }

        readMap(it); //06 00

        //TERRAIN YIELDs
        for (int k = 0; k <4; k++)
        {
            count = buffToInteger(*it); // 11 00 00 00
            it += 4;
            for (int i = 0; i < count; i++)
            {
                it += 4;
                assertT(buffToInteger(*it) == 6);
                readMap(it); //06 00
            }
        }
        it += 12; //00
        it += 8; //0d?

        p.greatPeopleCurrent = readMapFloat(it); //0A 00 00 00 ED 6F 42 A0
        p.greatPeopleProd = readMapFloat(it);
        p.greatPeopleUnknown = readMapFloat(it);
        it += 10;
        readMap(it); //06 00
        readMap(it); //0A 00 00 00 ED 6F 42 A0
        readMap(it); //06 00
        it += 4;
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 10; //GREAT_PERSON_CLASS_ENGINEER
        }
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //GREAT_PERSON_CLASS_ENGINEER
        }
        it += 41; //16 00 00 00 00 00 00 00 00 02 00 00 00 03 00 00 00 32 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00

        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            int emissaryCount = readInteger(it);
            if (emissaryCount != 0)
            {
                if (!p.emissaries.size())
                    p.emissaries.resize(playerCount);
                p.emissaries[i] = emissaryCount;
            }
        }
        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it ++; //00
        }

        readMap(it); //06 00

        it += 16;
        readMap(it); //06 
        readMap(it); //06 

        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        } 
        count = readInteger(it); //00/01
        for (int i = 0; i < count; i++)
        {
            it += 4; //32 00 00 00
        }
        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        it += 20; //00
        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        count = readInteger(it); //40
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }

        it += 12;
        readMap(it); // 02 00 00 00 4F E3 9B BC 00 00 00 00 1D 38 8A 57 00 00 00 00

        //GOVERNORS
        it += 4; //01
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 4; //60 0D 00 00
        }
        it += 8; //00 06
        it += 16; //03 0101 10 08 <- first is number of governors point

        count = readInteger(it);
        p.governors.resize(count);
        for (int i = 0; i < count; i++)
        {
            Governor& g = p.governors[i];
            assert(buffToInteger(*it) == i);
            g.id = readInteger(it);
            it += 4; //0F
            it += 4; //i->i+1
            g.type = readInteger(it);
            readString(it); //LOC_GOVERNOR_THE_RESOURCE_MANAGER_NAME
            g.player = readInteger(it, 2);
            assert(buffToInteger(*it, 2) == 2);
            it += 2; //0020
            g.city = readInteger(it,2);
            it += 2;
            it += 62; //00 and some 08 01        
            it += 4;
            it += 4; //00 after maj
            unsigned int count2 = readInteger(it);
            for (int i = 0; i < count2; i++)
            {
                it += 4; //0020
                it += 4; //i->i+1
                g.turn = readInteger(it);
            }
            std::map<unsigned int, unsigned int> promotions = readMap(it,1); // 30
            for (std::map<unsigned int, unsigned>::iterator it = promotions.begin(); it != promotions.end(); it++)
            {
                if (it->second == 1)
                {
                    g.promotions.push_back(it->first);
                }
            }
            it += 4;
            it++;   
        }

        it += 4;
        readArray(it, 4, 0);
        it += 8; //06 07
        it += 4; //00
        it++;
        it += 8; // FF 00

        it += 64; // 09 00
        it += 4; //00
        it += 4; //00 after maj
        
        count = buffToInteger(*it); // 40
        assertT(count == 64);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        count = buffToInteger(*it); // 40
        assertT(count == 64);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4; //00
        }
        it += 8; //00

        readArray(it); //8C 00 00 00 28 9D 64 D0
        readArray(it); //46 00 00 00 26 77 B5 52
        readArray(it); //2B 00 00 00 C9 16 21 5D

        it += 24; //02 00 00 00 00 00 00 00 FF FF FF FF 01 00 01 00 10 00 00 00 08 00 00 00
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 20; //07 00 00 00 02 00 00 00 07 00 14 00 02 00 00 00 02 00 00 00
        }
        it += 4;
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        it += 8;

        it += 2; //00 00 or 01 00
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            int type = buffToInteger(*it);
            it += 4;
            assertT(buffToInteger(*it) == 3);
            it += 4;
            if (type == 0)
                it += 61;
            else if (type == 1)
                it += 48;
            else if (type == 2)
                it += 44;
            else if (type == 3)
                it += 44;
            else if (type == 4)
                it += 44;
            else
                assertT(0);

        }
        assertT(buffToInteger(*it) == 2);
        it += 4;
        //it += 118;

        int tileCount = buffToInteger(*it);
        it += 4;
        it += tileCount*2;

        assertT(buffToInteger(*it) == tileCount);
        it += 4;
        it += tileCount*2;

        it += 24;
        it += 4;

        count = buffToInteger(*it); 
        it += 4;
        for (int i = 0; i < count; i++)
        {
            assertT(buffToInteger(*it) == p.cities[i].id);
            it += 4;
            assertT(buffToInteger(*it) == 0x11); //10 or 11 ?
            it += 4;
            it += 4; //00 00 02 00 ?
            it += 4; //i->i+1
            it += 4; //i->i+1
            it += 4; //00
            it += 4; //i->i+1
            assertT(buffToInteger(*it) == 0x6);
            p.cities[i].yields = readMapFloat(it);
            assertT(buffToInteger(*it) == 0x6);
            p.cities[i].yields2 = readMapFloat(it);
            readMap(it); //35
            it += 8; //something in 3a + FF
            it += 9; //0

            unsigned int count2 = buffToInteger(*it); //25
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 16; //01 00 00 00 FF FF FF FF 00 00 00 00 00 00 00 00 
            }
            count2 = buffToInteger(*it); //6
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 4; //FF
            }
            it += 12;//maybe an array here (01 FF FF)
            count2 = buffToInteger(*it); //0 or 3
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 12; //01 00 00 00 01 00 00 00 03 00 00 00
            }
            it ++;
            it += 16;

            count2 = buffToInteger(*it);
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 4;
            }

            count2 = buffToInteger(*it);
            it += 4;
            it += 4; //00 ?? count on 8 bits ? 
            for (int j = 0; j < count2; j++)
            {
                it += 8;
            }

            count2 = buffToInteger(*it);
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                assertT(buffToInteger(*it) == 0x04);
                it += 4;
                it += 4;
                assertT(buffToInteger(*it) == 0x64);
                it += 4;
                it += 35;
                assertT(buffToInteger(*it) == 0x40);
                unsigned int count3 = buffToInteger(*it); // 40
                it += 4;
                for (int k = 0; k < count3; k++)
                {
                    it += 4;
                }
                it += 12;
            }

            it += 20;
            count2 = buffToInteger(*it);
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 17; //01 00 00 00 03 00 02 00 00 00 01 00 00 00 00 00 00
            }
            count2 = buffToInteger(*it);
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 8; //06 00 02 00 00 00 01 00 
            }
            it += 12; //06 00 00 00 00 00 00 00 E6 FF FF FF
            it += 16; //00 with 1 19


            count2 = buffToInteger(*it); //25
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                assertT(buffToInteger(*it) == 1);
                it += 4;
                //assertT(buffToInteger(*it) == j);
                it += 4; // j or index
                it += 61; // useful info here !
            }
            
            
            count2 = buffToInteger(*it); //25
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                readMap(it); //85
                readMap(it); //22
                readMap(it); //3a
            }
            readMap(it); //8c''
            readMap(it); //8c
            readMap(it); //23
            it += 8; //00
            readMap(it);
        }

        it += 4;
        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        it += 8; // 06 07 or FF

        it += 4; // 0F

        it += 4;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }

        it += 8; //01 00 00 00 .. 00 00 00

        assertT(buffToInteger(*it) == 3);
        it += 4;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8; //0B 00 0D 00 1B 87 B5 90
        }
        it++;

        readMap(it); //35 00 00 00 78 A6 64 D3
        readMap(it); //35 00 00 00 78 A6 64 D3
        readMap(it); //05 00 00 00 6D 12 0E 51

        count = buffToInteger(*it); // 3D 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            unsigned int key = readInteger(it);
            assert(buffToInteger(*it) == 2);
            it += 4;
            it += 4; //0/1?
            assert(buffToInteger(*it) == key);
            it += 4;
            it += 4; //previous dogme needed
            it += 4; //0/1?
            it += 8; // unknown values
            unsigned int turnCount = readInteger(it);
            p.techDogmeInfo[DOGME].turnTo[key] = turnCount;
        }

        count = buffToInteger(*it); // 8f 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 34; //8 int + 2
        }

        count = buffToInteger(*it); //12 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 32;
        }
        readMap(it); //0D

        //IMPROVEMENT 
        count = buffToInteger(*it);
        it += 4;
        p.improvements.resize(count);
        for (int i = 0; i < p.improvements.size(); i++)
        {
            Improvement& improvement = p.improvements[i];
            it += 4;
            it += 4;
            it += 4;
            improvement.x = readInteger(it);
            improvement.y = readInteger(it);
            improvement.district = readInteger(it);
            improvement.type = readInteger(it);
            it++;
        }

        it += 8; //03 00

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }
        it += 4;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4; //0C 0B 00 00
        }
        it++;
        it += 4;

        it += 1844;

        // DIPLOMATIC STATE
        count = buffToInteger(*it); //40 00
        assertT(count == 64);
        it += 4;
        for (int k = 0; k < count; k++)
        {
            it += 8; // 05 00 ?
            assertT(buffToInteger(*it) == k);
            it += 4;

            std::string str = readString(it);
            it += 7;
            it += 4;

            int count2 = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count2; i++)
            {
                it += 4;
            }
            count2 = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count2; i++)
            {
                it += 4;
            }
            count2 = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count2; i++)
            {
                it += 4;
            }

            count2 = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count2; i++)
            {
                it += 20; //02 00 00 00 FF FF FF FF FF FF FF FF 00 00 00 00 02 00 00 00
                readString(it);

                it += 41;

                unsigned int count3 = buffToInteger(*it);
                it += 4;
                for (int j = 0; j < count3; j++)
                {
                    it += 4;
                }
                count3 = buffToInteger(*it);
                it += 4;
                for (int j = 0; j < count3; j++)
                {
                    it += 4;
                }
                count3 = buffToInteger(*it);
                it += 4;
                for (int j = 0; j < count3; j++)
                {
                    it += 4;
                }
            }

            count2 = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count2; i++)
            {
                it += 4; //01
                readString(it);
                it += 8;
            }

        }

        it += 5;
        count = buffToInteger(*it); //40 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            assertT(buffToInteger(*it) == 5);
            it += 4;
            unsigned int value = buffToInteger(*it);
            if (value == 0xffffffff)
            {
                it += 21; //FF FF FF FF FF FF FF FF 00 00 00 00 00 00 00 00 00 00 00 00 00 
            }
            else
            {
                it += 8;
                readMap(it); //2A
                readMap(it);
                it += 13;

            }
        }
        it += 593;

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12; //01 00 00 00 03 00 00 00 B3 03 00 00
        }

        it += 4;

        count = buffToInteger(*it); //40 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            readMap(it); // Diplo action 
        }
        count = readInteger(it); //40 00
        for (int i = 0; i < count; i++)
        {
            readMap(it); // diploaction
        }
        count = readInteger(it); //40 00
        for (int i = 0; i < count; i++)
        {
            readArray(it);// diploaction
        }

        count = readInteger(it); 
        for (int i = 0; i < count; i++)
        {
            //something about deal ?
            it += 24; //01 00 00 00 50 00 00 00 04 00 00 00 83 97 3F C0 00 00 00 00 33 00 00 00 
        }

        it += 8; // 00
        it++; //0-1 ?
        it += 4; //4
        it += 4; //iPlayer
        it++;
        it += 19;
        p.maintenance = readFloat(it);
        it += 4;
        it += 4;
        count = buffToInteger(*it); //4 or 0
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }
        it += 4; // 00

        readMap(it); //06 

        count = buffToInteger(*it); //05/00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 12; //01 00 00 00 01 00 00 00 02 00 00 00
        }

        it += 12;//03 00 00 00 01 00 00 00 00 00 00 00
        it += 8; //FF
        it += 8; //03 01
        it++;

        count = buffToInteger(*it); //08 00
        it += 4;
        //GOVERNOR INFO
        for (int i = 0; i < count; i++)
        {
            it += 4; //type
            it += 8; //int*2
            it += 4; //type
            int count2 = buffToInteger(*it); // link to playercount/citystate count ?
            it += 4;
            it += 8 * count2; //with ambassador
            count2 = buffToInteger(*it);
            it += 4;
            it += 4 * count2; // with cardinal
        }

        count = buffToInteger(*it); //30 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            int test = buffToInteger(*it);//31 8C 45 D8
            it += 4;
            it += 8;  //02 00 00 00 00 00 00 00
            int test2 = buffToInteger(*it);// 31 8C 45 D8
            it += 4;
            assertT(test == test2);
            int count2 = buffToInteger(*it);
            it += 4;
            for (int j = 0; j < count2; j++)
            {
                it += 4; // 46 27 0B 61 
            }
            it += 4; //00 00 00 00 
        }

        count = buffToInteger(*it);
        it += 4;
        it += count;

        it += 8;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        int count2 = buffToInteger(*it, 3);
        it += 3;
        for (int k = 0; k < count2; k++)
        {
            it += 13;

            count = buffToInteger(*it);
            it += 4;
            for (int i = 0; i < count; i++)
            {
                it += 16;
            }
            assertT(buffToInteger(*it) == 0x05);
            it += 39; //maybe 5 * 7 array


            readMap(it); //06 
            readMap(it); //06 
            readMap(it); //11 
            readMap(it); //36 
            readMap(it); //36 
            readMap(it); //46 
        }
        it += 12;
        for (int i = 0; i < 4; i++)
        {
            readArray(it); //10 00 00 00 26 C9 DE 38
        }
        readMap(it); //10 00 00 00 26 C9 DE 38
        readMap(it); //06 
        readMap(it); //06 
        readMap(it); //35 00 00 00 78 A6 64 D3 
        readMap(it); //35 00 00 00 78 A6 64 D3 

        it += 8;

        count = buffToInteger(*it); // 0B
        it += 4;
        //info on units ? 
        for (int i = 0; i < count; i++)
        {
            assertT(buffToInteger(*it) == 0x04);
            it += 4; //04
            unsigned int type = readInteger(it);
            UnitInfo& ui = p.unitsInfo[type];
            ui.type = type;
            ui.health = readInteger(it);
            it += 7; // ??
            ui.mp = readInteger(it);
            ui.range = readInteger(it);
            ui.attack = readInteger(it);
            ui.defense = readInteger(it);
            it += 4; //1 for cac and 0 for range?

            it += 280;
        }
        it++; // 01?

        readMap(it); //06 

        it += 168; //01 00 00 00 iplayer

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }

        it += 21; // 0E 00 00 00 05 00 00 00 D2 00 00 00 00 00 00 00 58 00 00 00 00
        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it+=4;
        }
        it += 21; //01 00 00 00 03 00 00 00 01 00 00 00 04 00 00 00 01 84 10 1B 8A

        count = readInteger(it); // 40 
        for (int i = 0; i < count; i++)
        {
            it++;
        }

        count = readInteger(it); // 40 
        for (int i = 0; i < count; i++)
        {
            it++;
        }

        readMap(it); //07
        readMap(it); //07
        readMap(it); //07

        it += 4;
         
        it += 4;// 10

        it += 4; //00 or 08 00 00

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 16; //i 06 01 07
            readString(it); //Settle New City or Naval SuperiorityG
            it += 17;
            readString(it); //Settle New City op or Naval Superiority Tree
            it += 4; //02/04
            unsigned int count2 = buffToInteger(*it);
            it += 4;
            for (int k = 0; k < count2; k++)
            {
                assertT(buffToInteger(*it) == 2);
                it += 25; //02 00 00 00 01 00 00 00 FF FF FF FF 00 00 00 00 00 00 40 40 00 FF FF FF FF 
                unsigned count3 = buffToInteger(*it);
                it += 4;
                for (int k = 0; k < count3; k++)
                {
                    assertT(buffToInteger(*it) == 2);
                    it += 29; //02 00 00 00 BC A7 BB 45 FF FF FF FF FF FF FF FF FF FF FF FF 00 00 80 3F 00 00 80 3F 00 
                }
            }

            it += 20; //FF
            it += 11; // 00 00 FF
            readString(it); // NONE / WAR
            it += 4; //00
            readString(it); // EMERGENCY_SOOTHSAYER
            count2 = readInteger(it);
            for (int k = 0; k < count2; k++)
            {
                it += 11;//03 00 00 00 01 01 01 D1 03 39 76 
                unsigned int count3 = readInteger(it);
                for (int l = 0; l < count3; l++)
                {
                    it += 4;
                }
                count3 = readInteger(it);
                for (int l = 0; l < count3; l++)
                {
                    it += 4;
                }
                count3 = readInteger(it);
                for (int l = 0; l < count3; l++)
                {
                    unsigned int count4 = readInteger(it);
                    it += count4 * 4;
                }
                count3 = readInteger(it);
                for (int l = 0; l < count3; l++)
                {
                    it += 8;
                }
                count3 = readInteger(it);
                for (int l = 0; l < count3; l++)
                {
                    it += 4;
                }
                it ++;
                count3 = readInteger(it); //06
                it += count3 * 8;
                it += 8; //00
            }

            for (int k = 0; k < 3; k++)
            {
                it += 16;
            }
            it += 8; //06 00 07 00 04 00 05 00 
            for (int k = 0; k < 3; k++)
            {
                it += 16;
            }
            it += 8; // 00 00 00 00 00 00 01 00
        }

        it += 4; //00 00 01 00

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it+=4;
        }

        it += 8; //FF or 06 07

        count = buffToInteger(*it); // 40
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it+=4;
        }
        
        readMap(it); //15
        readMap(it); //35


        count = buffToInteger(*it); // 00 / 0B
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 28; //01 00 00 00 00 00 00 00 02 00 00 00 03 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00
        }

        count = buffToInteger(*it); // 40
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        count = buffToInteger(*it); // 40
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        count = buffToInteger(*it); // 04
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 2;
        }

        readMap(it); //8C 
        readMap(it, 1); //4D

        count = buffToInteger(*it); // 4D
        it += 4;
        for (int i = 0; i < count; i++)
        {
            unsigned int key = readInteger(it);
            assert(buffToInteger(*it) == 3);
            it += 4;
            assert(buffToInteger(*it) == key);
            it += 4;
            it += 4; //previous tech needed
            it += 4; //0/1?
            unsigned int turnCount = readInteger(it);
            p.techDogmeInfo[TECH].turnTo[key] = turnCount;
            it += 8; // unknown values
            it += 4; //0 / 1
        }
        it++;

        count = buffToInteger(*it);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 12; //01 00 00 00 01 00 00 00 04 00 00 00
        }

        it += 4; //01
        it += 4; //iplayer
        it += 4; //03
        count = buffToInteger(*it); // 01 or 00
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }
        it += 4; //FF
        it += 4; //01
        it += 4; //iplayer
        it += 8; //01 00 02 00 01 00 00 00

        count = buffToInteger(*it); // 24
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }

        count = buffToInteger(*it); // 24
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }
        count = buffToInteger(*it); // 04
        if (count == 0xffffffff) //don't know maybe the ff before iPlayer
            it += 4;
        it += 4;
        it += 4;

        readMap(it, 1); //13
        it++;

        assertT(buffToInteger(*it) == 0x07);
        it += 4;
        it += 4; //iplayer

        count = buffToInteger(*it); 
        //info on cities?
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 51;
            count2 = buffToInteger(*it);
            it += 4;
            for (int k = 0; k < count2; k++)
            {
                it += 36;
            }
            readMap(it); //06
            readArray(it); // 85
            readArray(it); // 85
            readMap(it); //22
            count2 = buffToInteger(*it);
            it += 4;
            for (int k = 0; k < count2; k++)
            {
                it += 4;
            }
            readMap(it); //36
        }

        count = readInteger(it);
        for (int i = 0; i < count; i++)
        {
            it += 20; //01 00 00 00 5A 00 00 00 07 00 00 00 00 00 00 00 D3 EA 57 2E
        }

        count = readInteger(it); // 40
        for (int i = 0; i < count; i++)
        {
            it += 12;
        }
        readMap(it); //8C 

        count = readInteger(it); // 40
        for (int i = 0; i < count; i++)
        {
            it += 17;
        }

        int globalCount = buffToInteger(*it, 1); // 40
        it += 4;

        for (int iOtherPlayer = 0; iOtherPlayer < globalCount; iOtherPlayer++)
        {
            it += 4; // 4
            int boolean = buffToInteger(*it, 1);
            it++;

            if (!boolean)
                continue;

            //Unit infos
            count = buffToInteger(*it); // 8C
            it += 4;
            for (int k = 0; k < count; k++)
            {
                it += 4;
                int unitCount = buffToInteger(*it);
                it += 4;
                for (int iUnit = 0; iUnit < unitCount; iUnit++)
                {
                    it += 36; //Pos again maybe we want to take it or to check it there and other infos.
                }
            }

        }

        it += 92;

        readMap(it); //06
        readMap(it); //06
        count = buffToInteger(*it); // pos?
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 8;
        }

        it += 4;

        count = readInteger(it); // 1F
        for (int i = 0; i < count; i++)
        {
            it += 20; //1B 00 1C 00 01 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00
        }


        count = readInteger(it); // 18
        for (int i = 0; i < count; i++)
        {
            it += 20; //04 00 05 00 01 00 00 00 57 00 00 00 00 00 00 00 00 00 00
        }

        it = std::search(it, data.end(), END_IA_STUFF, END_IA_STUFF + 4); // ia stuff todo
        it -= 4;

        readMap(it); //23

        count = readInteger(it); 
        for (int i = 0; i < count; i++)
        {
            it += 13; //02 02 64
        }

        count = readInteger(it); // 18
        for (int i = 0; i < count; i++)
        {
            it += 16;
        }
        count = buffToInteger(*it); // 4
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4;
        }
        count = buffToInteger(*it,3); // 3 strange.... 01 00 00 80 doesn't work otherwise ?!
        it += 4;
        if(count != 0){
            //multiple lines of this kind
            // B4 C9 4D 48 00 00 02 00 00 00 00 00 00 00 00 00 00 00 36 00 00 00
            it = std::search(it, data.end(), END_PLAYER_STUFF, END_PLAYER_STUFF + 4); // ia stuff todo
            it -= 4;
        }

        readMap(it); //3A
        count = buffToInteger(*it, 4);
        it += 4;
        for (int i = 0; i < count; i++)
        {
            it += 4; //RANDOM_EVENT_FLOOD_MAJOR / RANDOM_EVENT_FLOOD_MODERATE / RANDOM_EVENT_FLOOD_1000_YEAR
        }

        readArray(it,4,0); //11
        readArray(it, 4, 0); //25
        readArray(it, 4, 0); //25

    }
    it += 28;

    int livePlayerCount = readInteger(it);
    for (int i = 0; i < playerCount; i++)
    {
        assertT(buffToInteger(*it) == 0x04);
        it += 4;
        it += 4; // player id
        assertT(buffToInteger(*it) == 0x02);
        it += 4;
        count = readInteger(it);
        for (int j = 0; j < count; j++)
        {
            assertT(buffToInteger(*it) == 0x03);
            it += 64; //03 00 00 00 02 00 02 00 02 00 03 00 0B 00 00 00 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 F1 D8 FF 00 00 00 00 
        }
        it += 61;
    }
    it += 4;
    unsigned int count2 = readInteger(it);
    for (int i = 0; i < count2; i++)
    {
        it += 14; //06 00 00 00 01 01 00 00 00 FF FF FF FF 00
        count = readInteger(it); //3b
        for (int j = 0; j < count; j++)
        {
            it += 4; //belief
            it += 8;
            it += 4;//belief
            it += 9;
        }

        count = readInteger(it); //00-1
        for (int j = 0; j < count; j++)
        {
            it += 12; //01 00 00 00 11 00 00 00 C7 00 00 00
        }

        it += 13;
        count = readInteger(it); //07
        for (int j = 0; j < count; j++)
        {
            it += 41; //04 00 00 00 0F 00 02 00 00 00 01 00 27 00 00 00 30 00 00 00 27 00 00 00 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01
            unsigned int count3 = readInteger(it);
            for (int k = 0; k < count3; k++)
            {
                it += 4;
            }

        }

        readMap(it);//06

    }
    it += 24; //00 04 00 00 04 00 00 00 19 00 00 00 32 00 00 00 64 00 00 00 32 00 00 00
    assertT(buffToInteger(*it) == 0x06);
    readMap(it);//06
    readMap(it);//06
    it += 28;
    it++;
    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it); //40
    for (int j = 0; j < count; j++)
    {
        it += 4;
    }
    count = readInteger(it); 
    for (int i = 0; i < count; i++)
    {
        it += 16; // 0F 00 00 00 3F 00 00 00 F0 66 5D E9 00 00 00 00 
        it += 65;
        count2 = readInteger(it); 
        for (int j = 0; j < count2; j++)
        {
            readString(it); //CLASS_MELEE
        }
        it += 12;
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 8; //03 00 02 00 00 00 01 00
        }
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 8; //00 00 02 00 00 00 01 00
        }

        it += 8;
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 20; //93 BC 92 5E 02 00 00 00 00 00 00 00 00 00 00 00 1C 00 00 00
        }
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 4; //67 CD 38 CF
        }
        it += 8;

        assertT(buffToInteger(*it) == 0x40);
        count2 = readInteger(it); //40
        for (int j = 0; j < count2; j++)
        {
            it += 4; //00
        }
        assertT(buffToInteger(*it) == 0x40);
        count2 = readInteger(it); //40
        for (int j = 0; j < count2; j++)
        {
            it += 24; //0F 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
            int count3 = readInteger(it);
            for (int k = 0; k < count3; k++)
            {
                it += 4; //C0 A6 72 6B
            }
            it += 20; //FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF
            
        }

        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 8; //02 00 00 00 03 00 00 00
        }
    }

    count = readInteger(it); //00-01
    for (int i = 0; i < count; i++)
    {
        it += 8; //01 00 00 00 B8 07 00 00
    }
    it += 4;//3
    count2 = readInteger(it);
    for (int i = 0; i < count2; i++)
    {
        it += 16; //03 00 00 00 93 13 37 BC FF FF FF FF FF FF FF FF
    }
    it += 4;//00
    it += 12;//03 00 00 00 3E 00 00 00 04 00 00 00
    count2 = readInteger(it);
    for (int i = 0; i < count2; i++)
    {
        it += 37;
        readArray(it, 4, 1); // 90
        readArray(it, 4, 1); // 8b
    }
    it += 4;//01
    it += 5;

    it += 21; //18 00 00 00 00 00 00 01 00 01 00 00 00 01 00 00 00 85 00 00 00
    count2 = readInteger(it);
    for (int i = 0; i < count2; i++)
    {
        it += 16; //01 00 00 00 00 00 00 00 05 00 00 00 00 00 01 00 
        count = readInteger(it);
        for (int j = 0; j < count; j++)
        {
            it += 28; //01 00 00 00 00 00 00 00 07 00 00 00 00 00 01 00 03 00 00 00 11 00 00 00 04 00 00 00 
            it += 20;//FF FF FF FF FF FF FF FF FF FF 01 00 FF FF FF FF FF FF FF FF
            it += 20;//00 00 00 00 FF FF 02 00 FF FF FF FF FF FF 02 00 FF FF FF FF
            assertT(buffToInteger(*it) == 0x06);
            readMap(it);//06
            readMap(it);//06
            it += 8;
            it++;
            unsigned int count3 = readInteger(it);
            for (int k = 0; k < count3; k++)
            {
                it += 4;
            }
            it += 37; //00 00 00 00 00 01 00 00 00 00 00 00 00 05 00 00 00 00 00 01 00 01 00 00 00 00 00 00 00 03 00 00 00 02 00 03 00 
        }

    }
    it += 8; //04 05
    it++;

    count2 = readInteger(it);
    std::vector<GreatPeople>& recruted = pState->greatPeopleRecruted;
    recruted.resize(count2);
    for (int i = 0; i < count2; i++)
    {
        assertT(buffToInteger(*it) == 0x05);
        it += 4;
        recruted[i].name = readInteger(it);
        recruted[i].type = readInteger(it);
        recruted[i].era = readInteger(it);
        recruted[i].cost = readInteger(it);
        recruted[i].player = readInteger(it);
        it += 4; // ??
    }

    count2 = readInteger(it);
    std::vector<GreatPeople>& gp = pState->greatPeople;
    gp.resize(count2);
    for (int i = 0; i < count2; i++)
    {
        gp[i].type = readInteger(it);
        assertT(buffToInteger(*it) == 0x05);
        it += 4; // 5 
        gp[i].name = readInteger(it);
        it += 4; // type
        gp[i].era = readInteger(it);
        gp[i].cost = readInteger(it);
        it += 8; //ff
    }

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x05);
        it += 12; //05 00 00 00 D1 6E 7F E2 02 00 00 00
    }

    //COMANDANTE_GENERAL
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x05);
        it += 16; //05 00 00 00 F5 1F 51 83 02 00 00 00 EC 6B 24 74
    }
    it += 8; //02 00 00 00 96 00 00 00

    //QUESTS
    int questParameterCount = 0;
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 16; //02 00 00 00 01 00 00 00 0F 00 00 00 00 00 00 00
        int quest = readInteger(it); //38 7D E9 89 
        it += 12; //05 00 00 00 0A 00 00 00 FF FF FF FF 
        if (quest != 0x89e97d38 && quest != 0x0c41aa65)
            questParameterCount++;
    }

    count2 = readInteger(it);
    for (int i = 0; i < count2; i++)
    {
        it += 16; //02 00 00 00 02 00 00 00 09 00 00 00 00 00 00 00
        int quest = readInteger(it); //80 B8 AC AE
        it += 12; //0C 00 00 00 0C 00 00 00 10 00 00 00 
        if (quest != 0x89e97d38 && quest != 0x0c41aa65)
            questParameterCount++;
    }

    for (int i = 0; i < questParameterCount; i++)
    {
        it += 8; //01 00 00 00 D3 EA 57 2E 
    }

    it += 4; //03

    //EMERGENCY
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 8; //0A 05
        it += 4; //type
        it += 8; //2D 00 00 00 2D 00 00 00
        it += 2; //01 01

        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 4;
        }
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 4; //15 0F 00 00
        }

        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it ++;
        }

        it += 4; //00

        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it ++;
        }
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            readString(it); //from
        }
        it++;
        assertT(buffToInteger(*it) == 0x40);
        count2 = readInteger(it); //40
        for (int j = 0; j < count2; j++)
        {
            it += 4; // world fair points
        }
    }

    //world fair
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 24; //01 00 00 00 09 00 00 00 94 76 FD A2 FF FF FF FF 75 00 00 00 FF 81 10 5A
    }

    it += 4; //0c

     // pollution ? (link to coal)
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 28; //1C 00 00 00 02 00 00 00 DD 0E EC 64 80 02 00 00 80 01 00 00 00 03 00 00 00 02 00 00
    }

    it += 4; //4
    it += 16; //00 00 00 00 00 00 00 00 40 33 00 00 D0 0C 00 00
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 12; //1C 00 00 00 00 00 00 00 A0 86 01 00
    }
    it += 4; //FF
    it += 16; //00 00 00 00 00 00 00 00 00 00 00 00 72 04 00 00
    it += 12; //4E 00 00 00 03 00 00 00 32 00 00 00
    it += 12; //E6 0D 20 6A DE 00 00 00 4D 01 00 00
    it += 6; //00

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it); //40
    for (int i = 0; i < count; i++)
    {
        it += 4;
    }
    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it); //40
    for (int i = 0; i < count; i++)
    {
        it += 4;
    }

    it += 4;
    it += 4; //00

    //RANDOM EVENT
    count = readInteger(it); 
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1C);
        it += 4;
        it += 4; //type
        it += 21; //02 00 00 00 00 09 00 00 00 4F 0F 00 00 F1 0F 00 00 05 00 00 00

        count2 = readInteger(it); //03
        for (int i = 0; i < count2; i++)
        {
            it += 4; //94 0D 00 00
        }

        count2 = readInteger(it);
        for (int i = 0; i < count2; i++)
        {
            it += 4; //94 0D 00 00
        }

        readString(it); //LOC_CITIZEN_KOREA_MODERN_MALE_1
    }

    //DROUGHT
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1C);
        it += 4;
        it += 4; //type
        it += 21; //02 00 00 00 00 09 00 00 00 4F 0F 00 00 F1 0F 00 00 05 00 00 00

        count2 = readInteger(it); //03
        for (int i = 0; i < count2; i++)
        {
            it += 4; //94 0D 00 00
        }

        readString(it); //LOC_CITY_NAME_ONDINI
    }

    //FIRE FOREST
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1C);
        it += 4;
        it += 4; //type
        it += 12; //01 00 00 00 06 00 00 00 E2 0A 00 00

        readString(it); //LOC_CITY_NAME_ONDINI

        it += 8; //01 00 00 00 E2 0A 00 00 
    }

    it += 36; //08 00 00 00 00 10 00 00 00 08 00 00 00 10 00 00 00 35 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x04);
        it += 4;
        it += 4; //type
        it += 12; //FF named things
        it += 8; //05 0E 00 00 05 0E 00 00
        it += 4; //FF
        it += 24; //02 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
        readString(it); //LOC_CITY_NAME_LISBON
        it++;

    }
    it++;
    it += 23; //03 00 00 00 01 00 00 00 00 00 00 00 00 00 00 FF FF FF FF 01 00 00 00

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 12;
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 4;
        }
        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it++;
        }

    }

    it += 4; //04

    readMap(it); //90
    readMap(it); //8b
    it += 24; //00

    it += 8; //34 00 00 00 02 00 00 00 

    //MOMENT
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x08);
        it += 4; //08 00 00 00
        it += 4; //type
        it += 4; //1
        it += 8; //F1 D8 FF FF F1 D8 FF FF
        it += 9; //05 00
        it += 4; //era

        count2 = readInteger(it);
        for (int i = 0; i < count2; i++)
        {
            it += 4; //moment data
            it += 4; //1E
        }

        readString(it);
        readString(it);
        readString(it);
        it += 4;
    }

    it += 4; //MOMENT count
    it += 4; //1E
    it += 4; //era
    it += 10; //3B 00 00 00 04 00 00 00 00 01

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        if (*it == 1)
        {
            Player& p = players[i];
            p.ageState = GOLDEN;
        }
        it++;
    }

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        if (*it == 1)
        {
            Player& p = players[i];
            p.ageState = DARK;
        }
        it++;
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        Player& p = players[i];
        assertT(buffToInteger(*it) == 0x1E);
        it += 4; //1E 00 00 00

        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            it += 4; //commemoration per era
        }
        it += 4; //00

        count2 = readInteger(it);
        for (int j = 0; j < count2; j++)
        {
            p.commemorations.push_back(readInteger(it));
            if(p.commemorations.size() == 3)
                p.ageState = HEROIC;
        }
        it += 4; //choice count to make
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1E);
        it += 4; //1E 00 00 00
        it += 4; //16

        Player& p = players[i];
        p.eraPoint = readInteger(it);
        for (int j = 0; j < 43; j++)
        {
            it += 4;
        }
    }

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1E);
        it += 4; //1E 00 00 00
        it += 4; //16
        Player& p = players[i];
        p.eraPointInitEra = readInteger(it);
        for (int j = 0; j < 43; j++)
        {
            it += 4;
        }
    }

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1E);
        it += 4; //1E 00 00 00
        it += 4; //16
        for (int j = 0; j < 44; j++)
        {
            it += 4;
        }
    }

    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1E);
        it += 4; //1E 00 00 00
        Player& p = players[i];
        p.eraPointHeroic = readInteger(it);
        p.eraPointNormal = readInteger(it);
        p.eraPointModifiers = 0;
        it += 4; // current?
        it += 4; // 0b
        for (int j = 0; j < 11; j++)
        {
            p.eraPointModifiers += readInteger(it);
        }
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 4;
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 4;
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 4;
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        it += 4;
    }

    assertT(buffToInteger(*it) == 0x40);
    count = readInteger(it);
    for (int i = 0; i < count; i++)
    {
        assertT(buffToInteger(*it) == 0x1E);
        it += 4; //1E 00 00 00
        it += 60;
    }

}