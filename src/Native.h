// CN 7.1, captured from dump71full a2.cs and cn71.exe.
// SHA256: 7f89938da606c1281659607d464702cddb9a09a7d4320a196630f60a811ec38e
#pragma once
#include <cstdint>
namespace Native {
inline constexpr uint32_t Timestamp = 0x6AA122D0;
inline constexpr uint32_t ImageSize = 0x1B0AD000;
struct Method { uintptr_t rva; const char* bytes; };
inline constexpr Method StringNew{0x196ACF20, "56 48 83 EC 20 48 85 C9 74 17 48 89 CE E8 AE 23 5F E8 48 89 F1 89 C2 48 83 C4 20 5E E9 9F 9E E7"};
inline constexpr Method FindObject{0x1453720, "40 53 48 83 EC 50 48 89 4C 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 16 E7 06 00 48 8B 08 48 85 C9"};
inline constexpr Method ObjectTransform{0x1452DA0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 0F 84 85 00 00 00 E8 09 80 FF FF 48 85 C0 74 7B 48 8B 40 40"};
inline constexpr Method ComponentObject{0x1453C60, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 4A E8 4D 71 FF FF 48 85 C0 74 40 48 89 7C 24 38 48 8B 78"};
inline constexpr Method ComponentByName{0x1452790, "40 53 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 A3 F6 06 00 48 85 DB"};
inline constexpr Method AddComponent{0x1452C50, "48 89 5C 24 18 57 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 DF F1 06"};
inline constexpr Method ReadAlpha{0x116E690, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 15 E8 1D C7 2D 00 48 85 C0 74 0B F3 0F 10 40 50 48 83 C4"};
inline constexpr Method SetAlpha{0x116E6C0, "40 53 48 83 EC 30 0F 29 74 24 20 0F 28 F1 48 8B D9 48 85 C9 74 1F E8 E5 C6 2D 00 48 85 C0 74 15"};
inline constexpr Method ReadRendererEnabled{0x982F40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 1C E8 6D 7E AC 00 48 85 C0 74 12 48 8B 10 48 8B C8 48 83"};
inline constexpr Method SetRendererEnabled{0x982F90, "48 89 5C 24 10 57 48 83 EC 20 0F B6 FA 48 8B D9 48 85 C9 74 27 E8 16 7E AC 00 48 85 C0 74 1D 4C"};
inline constexpr Method Name{0x14389F0, "40 53 48 81 EC 80 00 00 00 48 8B D9 48 85 C9 0F 84 DC 00 00 00 E8 B6 23 01 00 48 85 C0 0F 84 CE"};
inline constexpr Method InstanceId{0x19CB2FF0, "56 48 83 EC 20 48 89 CE 48 8B 49 10 48 85 C9 74 25 8B 05 79 17 AD EB 83 F8 FF 75 0F E8 FF 57 78"};
inline constexpr Method FromId{0x1439120, "48 83 EC 28 8B D1 48 8B 0D 5B B0 28 04 48 85 C9 75 14 33 D2 48 8D 4C 24 38 E8 E2 FB 08 00 48 8B"};
inline constexpr Method ActiveSelf{0x1452F20, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 17 E8 8D 7E FF FF 48 85 C0 74 0D 0F B6 80 F6 01 00 00 48"};
inline constexpr Method SetActive{0x1452EE0, "48 89 5C 24 08 57 48 83 EC 20 0F B6 FA 48 8B D9 48 85 C9 74 22 E8 C6 7E FF FF 48 85 C0 74 18 40"};
inline constexpr Method Parent{0x14374B0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 24 E8 FD 38 01 00 48 85 C0 74 1A 48 8B 90 A0 00 00 00 48"};
inline constexpr Method ChildCount{0x1437D10, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 16 E8 9D 30 01 00 48 85 C0 74 0C 8B 80 90 00 00 00 48 83"};
inline constexpr Method Child{0x1438070, "48 89 5C 24 10 57 48 83 EC 20 48 63 FA 48 8B D9 48 85 C9 74 39 E8 36 2D 01 00 48 85 C0 74 2F 85"};
inline constexpr Method FindTransform{0x1437EB0, "48 89 5C 24 18 57 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 7F 9F 08"};
inline constexpr Method ReadScale{0x1436F70, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 36 3E 01 00 48 85 C0 74 26 48"};
inline constexpr Method ReadLocalPosition{0x1436D90, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 16 40 01 00 48 85 C0 74 26 48"};
inline constexpr Method ReadRotation{0x1436ED0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 28 E8 D6 3E 01 00 48 85 C0 74 1E 48"};
inline constexpr Method SetScale{0x1437210, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 96 3B 01 00 48 85 C0 74 15 48"};
inline constexpr Method ReadPosition{0x1443E20, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F2 48 8B D9 48 85 C9 74 4A E8 81 6F 00 00 48"};
inline constexpr Method SetPosition{0x1443E90, "48 89 5C 24 10 57 48 83 EC 20 F3 0F 10 02 48 8B D9 F3 0F 10 4A 04 F3 0F 11 44 24 30 F3 0F 11 4C"};
inline constexpr Method ReadSize{0x1443F30, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 2C E8 76 6E 00 00 48 85 C0 74 22 F3"};
inline constexpr Method SetSize{0x1443F80, "48 89 5C 24 10 57 48 83 EC 20 F3 0F 10 02 48 8B D9 F3 0F 10 4A 04 F3 0F 11 44 24 30 F3 0F 11 4C"};
inline constexpr Method ReadPivot{0x1444020, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 2C E8 86 6D 00 00 48 85 C0 74 22 F3"};
inline constexpr Method ReadRect{0x1443BF0, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 23 E8 B6 71 00 00 48 85 C0 74 19 48"};
inline constexpr Method WillRender{0x19D7B2B0, "48 8B 05 31 8B A0 EB 48 8B 80 00 0F 00 00 48 85 C0 74 0F 4C 8B 40 18 48 8B 50 28 48 8B 48 40 49"};
inline constexpr Method MainAwake{0x1220A080, "56 57 53 48 83 EC 20 48 89 CE 80 3D B4 D8 54 F3 00 0F 85 A1 01 00 00 48 8B 7E 50 48 8B 5E 60 48"};
inline constexpr Method QuestGuide{0x121FF700, "56 48 83 EC 20 48 89 CE 80 3D 0B 95 8A F3 00 74 2C 80 3D 55 81 55 F3 00 75 3D 48 8B 8E 28 02 00"};
inline constexpr Method QuestTraceTick{0x858F420, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 0F 29 BD 80 00"};
}
