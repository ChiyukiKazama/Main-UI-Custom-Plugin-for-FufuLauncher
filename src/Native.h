#pragma once
#include <cstdint>

namespace Native
{
    struct Method { uintptr_t rva; const char* bytes; };

    struct Profile
    {
        const wchar_t* executableName;
        uint32_t timestamp, imageSize;
        Method ReadCanvasColor;
        Method SetCanvasAlpha;
        Method SetLocalPosition;
        Method StringNew;
        Method FindObject;
        Method ObjectTransform;
        Method ComponentObject;
        Method ComponentByName;
        Method ReadRendererEnabled;
        Method SetRendererEnabled;
        Method Name;
        Method InstanceId;
        Method FromId;
        Method ActiveSelf;
        Method Parent;
        Method ChildCount;
        Method Child;
        Method FindTransform;
        Method ReadScale;
        Method ReadLocalPosition;
        Method ReadRotation;
        Method SetScale;
        Method ReadPosition;
        Method SetPosition;
        Method ReadRect;
        Method MaskSetParamsDirty;
        Method MaskRebuild;
        Method RectMaskPerformClipping;
        Method WillRender;
        Method MainAwake;
        Method GraphicEnable;
        Method GraphicLightActivate;
        Method EffectEnable;
        Method EffectSetObject;
        Method EffectPlay;
        Method FeatureEffectMount;
        Method PrefabInstance;
        Method QuestEnable;
        Method InteeDialogAdd;
        Method InteePickupAdd;
        Method QuestTraceTick;
        Method InteeKeyRefresh;
        Method InteeSwitchKeyRefresh;
    };

    // The Unity icalls use the E9 destination and its output-buffer ABI.
    // SHA256: 8b014f64b5dc12fbc476e0c362b053abfdd7a1378cd282e4ad81057621dbd6b5
    inline constexpr Profile CN72{
        L"YuanShen.exe", 0x6AD886FB, 0x1B65F000,
        {0x117E860, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 44 E8 E6 D5 2D 00 48 85 C0 74 3A 48"}, // ReadCanvasColor
        {0x117E980, "40 53 48 83 EC 40 0F 29 74 24 30 0F 28 F1 48 8B D9 48 85 C9 74 42 E8 C5 D4 2D 00 48 85 C0 74 38"}, // SetCanvasAlpha
        {0x1447C10, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 36 42 01 00 48 85 C0 74 15 48"}, // SetLocalPosition
        {0x19C21C90, "56 48 83 EC 20 48 85 C9 74 17 48 89 CE E8 3E 4B 09 E8 48 89 F1 89 C2 48 83 C4 20 5E E9 3F DF 90"}, // StringNew
        {0x1464800, "40 53 48 83 EC 50 48 89 4C 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 96 E8 06 00 48 8B 08 48 85 C9"}, // FindObject
        {0x1463E80, "40 53 48 83 EC 20 48 8B D9 48 85 C9 0F 84 85 00 00 00 E8 C9 7F FF FF 48 85 C0 74 7B 48 8B 40 40"}, // ObjectTransform
        {0x1464D40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 4A E8 0D 71 FF FF 48 85 C0 74 40 48 89 7C 24 38 48 8B 78"}, // ComponentObject
        {0x1463870, "40 53 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 23 F8 06 00 48 85 DB"}, // ComponentByName
        {0x98C0B0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 1C E8 9D FD AC 00 48 85 C0 74 12 48 8B 10 48 8B C8 48 83"}, // ReadRendererEnabled
        {0x98C100, "48 89 5C 24 10 57 48 83 EC 20 0F B6 FA 48 8B D9 48 85 C9 74 27 E8 46 FD AC 00 48 85 C0 74 1D 4C"}, // SetRendererEnabled
        {0x1449820, "40 53 48 81 EC 80 00 00 00 48 8B D9 48 85 C9 0F 84 DC 00 00 00 E8 26 26 01 00 48 85 C0 0F 84 CE"}, // Name
        {0x1A226BE0, "56 48 83 EC 20 48 89 CE 48 8B 49 10 48 85 C9 74 25 8B 05 89 57 64 EB 83 F8 FF 75 0F E8 3F 2A 22"}, // InstanceId
        {0x1449F50, "48 83 EC 28 8B D1 48 8B 0D 73 FC 35 04 48 85 C9 75 14 33 D2 48 8D 4C 24 38 E8 22 00 09 00 48 8B"}, // FromId
        {0x1464000, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 17 E8 4D 7E FF FF 48 85 C0 74 0D 0F B6 80 F6 01 00 00 48"}, // ActiveSelf
        {0x14482E0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 24 E8 6D 3B 01 00 48 85 C0 74 1A 48 8B 90 A0 00 00 00 48"}, // Parent
        {0x1448B40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 16 E8 0D 33 01 00 48 85 C0 74 0C 8B 80 90 00 00 00 48 83"}, // ChildCount
        {0x1448EA0, "48 89 5C 24 10 57 48 83 EC 20 48 63 FA 48 8B D9 48 85 C9 74 39 E8 A6 2F 01 00 48 85 C0 74 2F 85"}, // Child
        {0x1448CE0, "48 89 5C 24 18 57 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 AF A3 08"}, // FindTransform
        {0x1447DA0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 A6 40 01 00 48 85 C0 74 26 48"}, // ReadScale
        {0x1447BC0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 86 42 01 00 48 85 C0 74 26 48"}, // ReadLocalPosition
        {0x1447D00, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 28 E8 46 41 01 00 48 85 C0 74 1E 48"}, // ReadRotation
        {0x1448040, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 06 3E 01 00 48 85 C0 74 15 48"}, // SetScale
        {0x1454EC0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F2 48 8B D9 48 85 C9 74 4A E8 81 6F 00 00 48"}, // ReadPosition
        {0x1454F30, "48 89 5C 24 10 57 48 83 EC 20 F3 0F 10 02 48 8B D9 F3 0F 10 4A 04 F3 0F 11 44 24 30 F3 0F 11 4C"}, // SetPosition
        {0x1454C90, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 23 E8 B6 71 00 00 48 85 C0 74 19 48"}, // ReadRect
        {0x1A3358A0, "C6 81 30 01 00 00 01 C3"}, // MaskSetParamsDirty
        {0x1A337CD0, "56 53 48 83 EC 28 48 89 CE C6 81 50 01 00 00 00 80 B9 30 01 00 00 00 75 35 48 8B 4E 70 48 85 C9"}, // MaskRebuild
        {0x1A3192D0, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 44 0F 29 A5 80"}, // RectMaskPerformClipping
        {0x1A2EE4C0, "48 8B 05 39 D1 57 EB 48 8B 80 50 12 00 00 48 85 C0 74 0F 4C 8B 40 18 48 8B 50 28 48 8B 48 40 49"}, // WillRender
        {0x125EE4D0, "56 57 53 48 83 EC 20 48 89 CE 80 3D 64 F6 25 F3 00 0F 85 A1 01 00 00 48 8B 7E 50 48 8B 5E 60 48"}, // MainAwake
        {0x1A310ED0, "56 57 48 83 EC 28 48 89 CE E8 02 FB FF FF 48 8B 4E 48 48 85 C9 74 07 48 83 79 10 00 75 0C 48 89"}, // GraphicEnable
        {0x1A310FC0, "56 48 83 EC 20 48 89 CE 48 8B 01 31 D2 FF 90 C8 01 00 00 80 7E 79 00 74 12 48 8B 06 48 89 F1 31"}, // GraphicLightActivate
        {0xABF5060, "56 48 83 EC 20 48 89 CE 80 3D 4B 87 C4 FA 00 75 3B 48 89 F1 E8 B7 FE FF FF 48 8B 4E 20 48 85 C9"}, // EffectEnable
        {0xABF5210, "56 57 53 48 83 EC 40 48 89 D7 48 89 CE 80 3D 9A 85 C4 FA 00 0F 85 0E 01 00 00 48 8B 46 18 48 89"}, // EffectSetObject
        {0xABF5610, "56 57 53 48 83 EC 20 89 D3 48 89 CA 80 3D 9F 81 C4 FA 00 75 63 48 8B 42 20 48 85 C0 74 48 48 83"}, // EffectPlay
        {0x9F29D80, "41 56 56 57 55 53 48 81 EC A0 00 00 00 0F 29 B4 24 90 00 00 00 48 89 CE 80 3D 98 4E 91 FB 00 0F"}, // FeatureEffectMount
        {0xCF4E5D0, "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 68 01 00 00 4D 89 CD 4D 89 C1 41 89 D0 48 89 CA C7"}, // PrefabInstance
        {0xABC9AC0, "56 57 48 83 EC 28 48 89 CE 80 3D 88 85 C3 FA 00 0F 85 33 01 00 00 8B 86 D0 00 00 00 83 C0 FE 83"}, // QuestEnable
        {0xCF18CA0, "56 48 83 EC 20 41 89 D0 48 89 CA 80 3D 56 73 92 F8 00 75 76 48 8B 4A 20 45 85 C0 78 26 48 85 C9"}, // InteeDialogAdd
        {0xCF19350, "56 48 83 EC 20 41 89 D0 48 89 CA 80 3D AE 6C 92 F8 00 75 76 48 8B 4A 28 45 85 C0 78 26 48 85 C9"}, // InteePickupAdd
        {0x87746B0, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 0F 29 BD 80 00"}, // QuestTraceTick
        {0xCF18340, "41 56 56 57 53 48 83 EC 28 48 89 CE 80 3D B1 7C 92 F8 00 0F 85 1A 02 00 00 48 8B 46 50 48 85 C0"}, // InteeKeyRefresh
        {0xCF18ED0, "56 57 53 48 83 EC 30 48 89 CF 80 3D 35 71 92 F8 00 0F 85 BF 00 00 00 48 8B 77 68 48 85 F6 74 64"}, // InteeSwitchKeyRefresh
    };

    // SHA256: b3001fc110b63e0e0ce24a77386e71d0eefaffaae244ea0c72645a659cb5ea6e
    inline constexpr Profile OS72{
        L"GenshinImpact.exe", 0x6AD8992C, 0x1B690000,
        {0x117F860, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 44 E8 E6 D5 2D 00 48 85 C0 74 3A 48"}, // ReadCanvasColor
        {0x117F980, "40 53 48 83 EC 40 0F 29 74 24 30 0F 28 F1 48 8B D9 48 85 C9 74 42 E8 C5 D4 2D 00 48 85 C0 74 38"}, // SetCanvasAlpha
        {0x1448C10, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 36 42 01 00 48 85 C0 74 15 48"}, // SetLocalPosition
        {0x19C54AD0, "56 48 83 EC 20 48 85 C9 74 17 48 89 CE E8 FE 2C 06 E8 48 89 F1 89 C2 48 83 C4 20 5E E9 7F BF 8D"}, // StringNew
        {0x1465800, "40 53 48 83 EC 50 48 89 4C 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 96 E8 06 00 48 8B 08 48 85 C9"}, // FindObject
        {0x1464E80, "40 53 48 83 EC 20 48 8B D9 48 85 C9 0F 84 85 00 00 00 E8 C9 7F FF FF 48 85 C0 74 7B 48 8B 40 40"}, // ObjectTransform
        {0x1465D40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 4A E8 0D 71 FF FF 48 85 C0 74 40 48 89 7C 24 38 48 8B 78"}, // ComponentObject
        {0x1464870, "40 53 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 23 F8 06 00 48 85 DB"}, // ComponentByName
        {0x98D0B0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 1C E8 9D FD AC 00 48 85 C0 74 12 48 8B 10 48 8B C8 48 83"}, // ReadRendererEnabled
        {0x98D100, "48 89 5C 24 10 57 48 83 EC 20 0F B6 FA 48 8B D9 48 85 C9 74 27 E8 46 FD AC 00 48 85 C0 74 1D 4C"}, // SetRendererEnabled
        {0x144A820, "40 53 48 81 EC 80 00 00 00 48 8B D9 48 85 C9 0F 84 DC 00 00 00 E8 26 26 01 00 48 85 C0 0F 84 CE"}, // Name
        {0x1A258E80, "56 48 83 EC 20 48 89 CE 48 8B 49 10 48 85 C9 74 25 8B 05 69 33 61 EB 83 F8 FF 75 0F E8 9F 17 1F"}, // InstanceId
        {0x144AF50, "48 83 EC 28 8B D1 48 8B 0D F3 EA 35 04 48 85 C9 75 14 33 D2 48 8D 4C 24 38 E8 22 00 09 00 48 8B"}, // FromId
        {0x1465000, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 17 E8 4D 7E FF FF 48 85 C0 74 0D 0F B6 80 F6 01 00 00 48"}, // ActiveSelf
        {0x14492E0, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 24 E8 6D 3B 01 00 48 85 C0 74 1A 48 8B 90 A0 00 00 00 48"}, // Parent
        {0x1449B40, "40 53 48 83 EC 20 48 8B D9 48 85 C9 74 16 E8 0D 33 01 00 48 85 C0 74 0C 8B 80 90 00 00 00 48 83"}, // ChildCount
        {0x1449EA0, "48 89 5C 24 10 57 48 83 EC 20 48 63 FA 48 8B D9 48 85 C9 74 39 E8 A6 2F 01 00 48 85 C0 74 2F 85"}, // Child
        {0x1449CE0, "48 89 5C 24 18 57 48 83 EC 50 48 8B D9 48 89 54 24 60 48 8D 54 24 20 48 8D 4C 24 60 E8 AF A3 08"}, // FindTransform
        {0x1448DA0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 A6 40 01 00 48 85 C0 74 26 48"}, // ReadScale
        {0x1448BC0, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 30 E8 86 42 01 00 48 85 C0 74 26 48"}, // ReadLocalPosition
        {0x1448D00, "48 89 5C 24 08 57 48 83 EC 30 48 8B FA 48 8B D9 48 85 C9 74 28 E8 46 41 01 00 48 85 C0 74 1E 48"}, // ReadRotation
        {0x1449040, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 1F E8 06 3E 01 00 48 85 C0 74 15 48"}, // SetScale
        {0x1455EC0, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F2 48 8B D9 48 85 C9 74 4A E8 81 6F 00 00 48"}, // ReadPosition
        {0x1455F30, "48 89 5C 24 10 57 48 83 EC 20 F3 0F 10 02 48 8B D9 F3 0F 10 4A 04 F3 0F 11 44 24 30 F3 0F 11 4C"}, // SetPosition
        {0x1455C90, "48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 85 C9 74 23 E8 B6 71 00 00 48 85 C0 74 19 48"}, // ReadRect
        {0x1A3679E0, "C6 81 30 01 00 00 01 C3"}, // MaskSetParamsDirty
        {0x1A369E10, "56 53 48 83 EC 28 48 89 CE C6 81 50 01 00 00 00 80 B9 30 01 00 00 00 75 35 48 8B 4E 70 48 85 C9"}, // MaskRebuild
        {0x1A34B3A0, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 44 0F 29 A5 80"}, // RectMaskPerformClipping
        {0x1A320650, "48 8B 05 29 AE 54 EB 48 8B 80 80 12 00 00 48 85 C0 74 0F 4C 8B 40 18 48 8B 50 28 48 8B 48 40 49"}, // WillRender
        {0x12601150, "56 57 53 48 83 EC 20 48 89 CE 80 3D 69 12 21 F3 00 0F 85 A1 01 00 00 48 8B 7E 50 48 8B 5E 60 48"}, // MainAwake
        {0x1A342F90, "56 57 48 83 EC 28 48 89 CE E8 02 FB FF FF 48 8B 4E 48 48 85 C9 74 07 48 83 79 10 00 75 0C 48 89"}, // GraphicEnable
        {0x1A343080, "56 48 83 EC 20 48 89 CE 48 8B 01 31 D2 FF 90 C8 01 00 00 80 7E 79 00 74 12 48 8B 06 48 89 F1 31"}, // GraphicLightActivate
        {0xABF77F0, "56 48 83 EC 20 48 89 CE 80 3D A0 68 C3 FA 00 75 3B 48 89 F1 E8 F7 F9 FF FF 48 8B 4E 20 48 85 C9"}, // EffectEnable
        {0xABF7080, "56 57 53 48 83 EC 40 48 89 D7 48 89 CE 80 3D 02 70 C3 FA 00 0F 85 0E 01 00 00 48 8B 46 18 48 89"}, // EffectSetObject
        {0xABF7740, "56 57 53 48 83 EC 20 89 D3 48 89 CA 80 3D 4B 69 C3 FA 00 75 63 48 8B 42 20 48 85 C0 74 48 48 83"}, // EffectPlay
        {0x9F2DFE0, "41 56 56 57 55 53 48 81 EC A0 00 00 00 0F 29 B4 24 90 00 00 00 48 89 CE 80 3D DE AE 8F FB 00 0F"}, // FeatureEffectMount
        {0xCF56180, "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 68 01 00 00 4D 89 CD 4D 89 C1 41 89 D0 48 89 CA C7"}, // PrefabInstance
        {0xABCEC00, "56 57 48 83 EC 28 48 89 CE 80 3D BD 18 C2 FA 00 0F 85 33 01 00 00 8B 86 D0 00 00 00 83 C0 FE 83"}, // QuestEnable
        {0xCF21000, "56 48 83 EC 20 41 89 D0 48 89 CA 80 3D 25 F5 8A F8 00 75 76 48 8B 4A 20 45 85 C0 78 26 48 85 C9"}, // InteeDialogAdd
        {0xCF21380, "56 48 83 EC 20 41 89 D0 48 89 CA 80 3D AA F1 8A F8 00 75 76 48 8B 4A 28 45 85 C0 78 26 48 85 C9"}, // InteePickupAdd
        {0x8779D20, "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC 18 01 00 00 48 8D AC 24 80 00 00 00 0F 29 BD 80 00"}, // QuestTraceTick
        {0xCF20800, "41 56 56 57 53 48 83 EC 28 48 89 CE 80 3D 1E FD 8A F8 00 0F 85 1A 02 00 00 48 8B 46 50 48 85 C0"}, // InteeKeyRefresh
        {0xCF1FE50, "56 57 53 48 83 EC 30 48 89 CF 80 3D CE 06 8B F8 00 0F 85 BF 00 00 00 48 8B 77 68 48 85 F6 74 64"}, // InteeSwitchKeyRefresh
    };

}
