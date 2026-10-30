// Copyright (c) FufuLauncher Dev Team. AGPL-3.0.
#include "Engine.h"
#include "Support.h"
#include "Native.h"
#include <MinHook.h>
#include <atomic>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>

namespace
{
    struct Vec2 { float x, y; };
    struct Vec3 { float x, y, z; };
    struct Rect { float x, y, width, height; };
    struct Quaternion { float x, y, z, w; };
    using ObjectGetterFn = void*(__fastcall*)(void*, void*);
    using ManagedStringFactoryFn = void*(__fastcall*)(const char*);
    using ObjectFromInstanceIdFn = void*(__fastcall*)(int, void*);
    using IntegerGetterFn = int(__fastcall*)(void*, void*);
    using BooleanGetterFn = bool(__fastcall*)(void*, void*);
    using AlphaGetterFn = float(__fastcall*)(void*, void*);
    using AlphaSetterFn = void(__fastcall*)(void*, float, void*);
    using BooleanSetterFn = void(__fastcall*)(void*, bool, void*);
    using StringArgumentObjectFn = void*(__fastcall*)(void*, void*, void*);
    using TransformChildFn = void*(__fastcall*)(void*, int, void*);
    using Vector3GetterFn = void(__fastcall*)(void*, Vec3*, void*);
    using Vector3SetterFn = void(__fastcall*)(void*, const Vec3*, void*);
    using Vector2GetterFn = void(__fastcall*)(void*, Vec2*, void*);
    using Vector2SetterFn = void(__fastcall*)(void*, const Vec2*, void*);
    using RectGetterFn = void(__fastcall*)(void*, Rect*, void*);
    using RotationGetterFn = void(__fastcall*)(void*, Quaternion*, void*);
    using CanvasRenderFn = void(__fastcall*)(void*);
    using ComponentCallbackFn = void(__fastcall*)(void*, void*);
    using TransformParentFn = void(__fastcall*)(void*, void*, bool, void*);
    ManagedStringFactoryFn createManagedString = nullptr;
    ObjectGetterFn findActiveGameObject = nullptr;
    ObjectGetterFn getTransform = nullptr, getComponentGameObject = nullptr;
    ObjectGetterFn getUnityObjectName = nullptr, getParentTransform = nullptr, getQuestGuide = nullptr;
    ObjectFromInstanceIdFn getObjectFromInstanceId = nullptr;
    IntegerGetterFn getInstanceId = nullptr, getChildCount = nullptr;
    BooleanGetterFn getActiveSelf = nullptr, getRendererEnabled = nullptr;
    AlphaGetterFn getCanvasGroupAlpha = nullptr;
    AlphaSetterFn originalSetCanvasGroupAlpha = nullptr;
    BooleanSetterFn originalSetRendererEnabled = nullptr;
    StringArgumentObjectFn getComponentByName = nullptr, findChildTransform = nullptr, addComponentByName = nullptr;
    TransformChildFn getChildTransform = nullptr;
    Vector3GetterFn getLocalScale = nullptr, getLocalPosition = nullptr;
    Vector2GetterFn getAnchoredPosition = nullptr, getSizeDelta = nullptr, getPivot = nullptr;
    RectGetterFn getRect = nullptr;
    RotationGetterFn getLocalRotation = nullptr;
    BooleanSetterFn originalSetActive = nullptr;
    Vector3SetterFn originalSetLocalScale = nullptr;
    Vector2SetterFn originalSetAnchoredPosition = nullptr;
    Vector2SetterFn originalSetSizeDelta = nullptr;
    CanvasRenderFn originalCanvasWillRender = nullptr;
    ComponentCallbackFn originalMainPageAwake = nullptr, originalQuestTraceTick = nullptr;
    TransformParentFn originalSetParent = nullptr;
    ComponentCallbackFn originalRegisterEffectRenderers = nullptr;
    std::shared_ptr<const UI::Settings> pendingSettings = std::make_shared<UI::Settings>();
    std::shared_ptr<const UI::Settings> settings;
    std::atomic<bool> hooksReady{false};
    std::atomic<DWORD> uiThreadId{0};
    std::atomic<int> pendingPageInstanceId{0};
    int mainPageInstanceId = 0, mainRootInstanceId = 0, mainRootTransformInstanceId = 0;
    void* mainRootTransform = nullptr; // Compared only; re-resolved by instance ID before each operation.
    bool refreshPending = true, temporarilyRestored = false, rootLayoutCaptured = false, watchActivationChanges = false;
    thread_local bool isApplyingOverrides = false;
    bool restoreKeyHeld = false, exportKeyHeld = false;
    bool marginsValid = false;
    // Hiding never changes GameObject activation or input/interactable state.
    // Only drawing properties are overridden; native requests remain restorable.
    struct CanvasGroupOverride { int ownerInstanceId; float requestedAlpha; };
    struct RendererOverride { int ownerInstanceId; bool requestedEnabled; };
    std::unordered_set<int> hiddenRootIds, pendingVisualBranchIds, cachedVisualObjectIds;
    std::unordered_map<int,CanvasGroupOverride> canvasGroupOverrides;
    std::unordered_map<int,RendererOverride> rendererOverrides;
    enum Edge : unsigned { Left=1, Right=2, Top=4, Bottom=8 };
    struct ElementLayout
    {
        Vec3 scale, appliedScale;
        Vec2 position, appliedPosition;
        bool applied=false, scaled=false;
        unsigned edges=0;
        float alignmentOffset=0;
    };
    std::unordered_map<int,ElementLayout> elementLayoutOverrides;
    std::unordered_set<int> questTraceIds;
    bool scaleTargetsRefreshPending = true;

    template<typename Fn> bool TryNativeCall(Fn&& fn)
    {
        __try { fn(); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    bool IsOnUiThread() { return GetCurrentThreadId() == uiThreadId.load(std::memory_order_relaxed); }
    bool IsCustomizationActive() { return settings && !temporarilyRestored; }
    bool HasMargins() { return settings && (settings->marginLeft || settings->marginRight || settings->marginTop || settings->marginBottom); }
    void* ResolveObject(int id) { return id ? getObjectFromInstanceId(id, nullptr) : nullptr; }
    int GetObjectInstanceId(void* object) { return object ? getInstanceId(object, nullptr) : 0; }
    std::string GetObjectName(void* object)
    {
        auto* text = static_cast<const unsigned char*>(getUnityObjectName(object, nullptr));
        if (!text) return {};
        const int length = *reinterpret_cast<const int*>(text + 16);
        if (length <= 0 || length > 1024) return {};
        const auto* chars = reinterpret_cast<const wchar_t*>(text + 20);
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, chars, length, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return {};
        std::string result(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, chars, length, result.data(), bytes, nullptr, nullptr);
        return result;
    }
    void* FindActiveGameObject(const char* path)
    {
        auto* text = createManagedString(path);
        return text ? findActiveGameObject(text, nullptr) : nullptr;
    }
    bool IsUnderMainPage(void* object)
    {
        auto* liveRoot = ResolveObject(mainRootTransformInstanceId);
        if (!liveRoot || !object) return false;
        auto* transform = getTransform(object, nullptr);
        for (int depth = 0; transform && depth < 64; ++depth)
        {
            if (transform == liveRoot) return true;
            transform = getParentTransform(transform, nullptr);
        }
        return false;
    }
    struct PresetBinding { UI::Preset preset; size_t offset; bool isGameObjectField; const char* childPath = nullptr; };
    // MonoInLevelMainPage CN 7.1: named, serialized fields in dump71full a2.cs.
    constexpr PresetBinding presetBindings[] = {
        {UI::MiniMap,0x78,false}, {UI::Team,0x70,false}, {UI::QuestTracking,0x2E0,false},
        // Live CN 7.1 ui-tree: InputController is camera/movement input, while
        // ActionPanelContainer owns the actual attack/skill/jump/sprint HUD.
        {UI::SkillControls,0x2D8,false},
        // chatPanel is the shared ChatArea/Chat root. Never suppress that root:
        // the entry prompt and message viewport are sibling subtrees below it.
        {UI::ChatButton,0x3B8,true,"Content/Title/Key_PC/ChatIcon"},
        {UI::ChatButton,0x3B8,true,"Content/ActionButton_PS/Key_PS4"},
        {UI::ChatPanel,0x3B8,true,"Content/ScrollView"},
        {UI::ChatPanel,0x3B8,true,"Content/ImgBg"},
        // Keep the minimap and task text outside the left button group.
        {UI::TopLeftButtons,0x88,true},
        {UI::TopLeftButtons,0x80,true,"GrpFactor"}, {UI::TopLeftButtons,0x80,true,"GrpFactorContainer"},
        {UI::TopLeftButtons,0x80,true,"BtnToggle"},
        {UI::TopLeftButtons,0x130,false}, {UI::TopLeftButtons,0x398,false},
        {UI::TopLeftButtons,0x80,true,"BtnClock"},
        {UI::TopRightButtons,0x98,true}, {UI::Paimon,0x110,false},
        {UI::Activities,0x108,false}, {UI::Activities,0x1A0,false}, {UI::Bag,0xF0,false},
        {UI::Character,0xE0,false}, {UI::Gacha,0xF8,false}, {UI::BattlePass,0x1B0,false},
        {UI::Handbook,0x188,false}, {UI::Handbook,0x190,false}, {UI::Coop,0xB8,false},
        // The bottom avatar HUD belongs to MonoInLevelGameInfo (0x48).
        // MonoCombatBars (0x2E8) is not the player's bottom HP/status area.
        {UI::PlayerHealthStatus,0x48,false,"HPBarContainer"},
        {UI::PlayerHealthStatus,0x48,false,"HPBarCustomLevelContainer"},
        {UI::PlayerHealthStatus,0x48,false,"Layout"},
        {UI::PlayerHealthStatus,0x48,false,"SPBar"},
        {UI::PlayerHealthStatus,0x48,false,"SPBarV2"},
        {UI::PlayerHealthStatus,0x48,false,"ClimateBar"},
        {UI::QuestButton,0xD8,false},
        {UI::SnezhnayaIcon,0x80,true,"GrpFactor/GrpSituationButton/SituationButton_SnezhnayaBook(Clone)"},
        {UI::Interaction,0x1F8,false}, {UI::Ping,0x218,false},
        // _beyondRecommendedButton and its disabled/locked overlay, then _elemViewButton.
        {UI::MiliastraEntrance,0x1B8,false}, {UI::MiliastraEntrance,0x1C0,false},
        {UI::ElementalSight,0x370,false}
    };
    void* ResolvePresetObject(void* page, const PresetBinding& field)
    {
        auto* value = *reinterpret_cast<void**>(static_cast<unsigned char*>(page) + field.offset);
        auto* object = value && !field.isGameObjectField ? getComponentGameObject(value,nullptr) : value;
        if (!IsUnderMainPage(object)) return nullptr;
        if (field.childPath)
        {
            auto* transform = getTransform(object,nullptr);
            auto* path = createManagedString(field.childPath);
            auto* target = transform && path ? findChildTransform(transform,path,nullptr) : nullptr;
            object = target ? getComponentGameObject(target,nullptr) : nullptr;
        }
        return IsUnderMainPage(object) ? object : nullptr;
    }
    void* FindFirstPresetObject(void* page, UI::Preset preset)
    {
        for (const auto& field : presetBindings) if (field.preset == preset)
            if (auto* object = ResolvePresetObject(page,field)) return object;
        return nullptr;
    }
    std::string GetObjectPath(void* object)
    {
        auto* transform = object ? getTransform(object,nullptr) : nullptr;
        std::string path;
        for (int depth=0; transform && depth<64; ++depth)
        { path = "/" + GetObjectName(transform) + path; transform = getParentTransform(transform,nullptr); }
        return transform ? std::string{} : path;
    }
    bool IsAncestorOrSelf(void* ancestor, void* descendant)
    {
        if (!ancestor || !descendant) return false;
        auto* target = getTransform(ancestor,nullptr);
        auto* transform = getTransform(descendant,nullptr);
        for (int depth=0; transform && depth<64; ++depth)
        { if (transform == target) return true; transform = getParentTransform(transform,nullptr); }
        return false;
    }
    bool Same(float a,float b,float tolerance) { return std::abs(a-b)<tolerance; }
    bool Same(const Vec2& a,const Vec2& b) { return Same(a.x,b.x,.001f) && Same(a.y,b.y,.001f); }
    bool Same(const Vec3& a,const Vec3& b) { return Same(a.x,b.x,.00001f) && Same(a.y,b.y,.00001f) && Same(a.z,b.z,.00001f); }
    void ObserveElementLayout(void* transform,ElementLayout& layout)
    {
        Vec3 scale{}; Vec2 position{};
        getLocalScale(transform,&scale,nullptr); getAnchoredPosition(transform,&position,nullptr);
        if (!layout.applied) { layout.scale=scale; layout.position=position; return; }
        // Animator can bind one axis and leave the others at our last visual
        // override. Adopting the whole vector would feed those untouched axes
        // back into the baseline (Paimon's Y animation caused constant X drift).
        if (!Same(scale.x,layout.appliedScale.x,.00001f)) layout.scale.x=scale.x;
        if (!Same(scale.y,layout.appliedScale.y,.00001f)) layout.scale.y=scale.y;
        if (!Same(scale.z,layout.appliedScale.z,.00001f)) layout.scale.z=scale.z;
        if (!Same(position.x,layout.appliedPosition.x,.001f)) layout.position.x=position.x;
        if (!Same(position.y,layout.appliedPosition.y,.001f)) layout.position.y=position.y;
    }
    void RestoreElementLayout()
    {
        for (const auto& pair:elementLayoutOverrides) TryNativeCall([&] {
            if (auto* transform=ResolveObject(pair.first))
            {
                auto native=pair.second; ObserveElementLayout(transform,native);
                originalSetLocalScale(transform,&native.scale,nullptr); originalSetAnchoredPosition(transform,&native.position,nullptr);
            }
        });
        elementLayoutOverrides.clear();
        scaleTargetsRefreshPending=true;
    }
    void* FindRelativeObject(const char* path)
    {
        if (!mainRootTransform) return nullptr;
        auto* text=createManagedString(path);
        auto* transform=text?findChildTransform(mainRootTransform,text,nullptr):nullptr;
        return transform?getComponentGameObject(transform,nullptr):nullptr;
    }
    std::vector<void*> CollectScaleTargets()
    {
        std::unordered_map<int,void*> candidates;
        auto add=[&](void* object) { if (IsUnderMainPage(object) && GetObjectInstanceId(object)!=mainRootInstanceId) candidates.emplace(GetObjectInstanceId(object),object); };
        if (auto* page=ResolveObject(mainPageInstanceId)) for (const auto& field:presetBindings)
        {
            if (field.preset==UI::AllMainUi || field.preset==UI::Team || field.preset==UI::SkillControls ||
                field.preset==UI::TopLeftButtons || field.preset==UI::TopRightButtons || field.preset==UI::PlayerHealthStatus) continue;
            add(ResolvePresetObject(page,field));
        }
        auto children=[&](const char* path,bool numericOnly=false,const char* skip=nullptr) {
            auto* object=FindRelativeObject(path);
            auto* transform=object?getTransform(object,nullptr):nullptr;
            const int count=transform?getChildCount(transform,nullptr):0;
            if (count<0 || count>256) return;
            for (int i=0;i<count;++i)
            {
                auto* item=getChildTransform(transform,i,nullptr); const auto itemName=GetObjectName(item);
                if ((skip && itemName==skip) || (numericOnly && (itemName.empty() || itemName.find_first_not_of("0123456789")!=std::string::npos))) continue;
                add(getComponentGameObject(item,nullptr));
            }
        };
        children("GrpMainPage/GrpMainBtn/GrpMainToggle/GrpTopPanel");
        children("GrpMainPage/GrpMainBtn/GrpMainToggle/GrpLeftPanel");
        children("GrpMainPage/MapInfo/GrpButtons");
        children("GrpMainPage/TeamBtnContainer/Content",true);
        children("GrpMainPage/ActionPanelContainer/ActionBtnPanel",false,"GrpSkill");
        children("GrpMainPage/ActionPanelContainer/ActionBtnPanel/GrpSkill");
        children("GrpMainPage/AimContainer/SlotLeft");
        for (const char* path:{"GrpMainPage/MapInfo/GrpMiniMap","GrpMainPage/MapInfo/BtnPlayerProfile",
            "GrpMainPage/MapInfo/BtnToggleQuest","GrpMainPage/MapInfo/BtnExitDungeon","GrpMainPage/MapInfo/BtnClock",
            "GrpMainPage/GameInfo/HPBarContainer","GrpMainPage/GameInfo/HPBarCustomLevelContainer",
            "GrpMainPage/GameInfo/Layout/BuffElement","GrpMainPage/GameInfo/Layout/SPBarV1",
            "GrpMainPage/GameInfo/SPBar","GrpMainPage/GameInfo/SPBarV2","GrpMainPage/GameInfo/ClimateBar",
            "GrpMainPage/BtnReturnToQuestV2"}) add(FindRelativeObject(path));
        std::vector<void*> result;
        // Candidate regions are disjoint: never apply the factor twice to a
        // button whose outer visual container was already selected.
        for (const auto& pair:candidates)
        {
            bool nested=false;
            auto* ancestor=getParentTransform(getTransform(pair.second,nullptr),nullptr);
            for (int depth=0;ancestor && depth<64;++depth)
            {
                if (candidates.count(GetObjectInstanceId(getComponentGameObject(ancestor,nullptr)))) { nested=true; break; }
                ancestor=getParentTransform(ancestor,nullptr);
            }
            if (!nested) result.push_back(pair.second);
        }
        return result;
    }
    Vec2 GetScaledRectCenter(void* transform,const Vec3& scale)
    {
        Rect rect{}; Quaternion rotation{};
        getRect(transform,&rect,nullptr); getLocalRotation(transform,&rotation,nullptr);
        const float x=(rect.x+rect.width*.5f)*scale.x;
        const float y=(rect.y+rect.height*.5f)*scale.y;
        const float dot=rotation.x*x+rotation.y*y;
        const float axis2=rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z;
        return {2*dot*rotation.x+(rotation.w*rotation.w-axis2)*x-2*rotation.w*rotation.z*y,
            2*dot*rotation.y+(rotation.w*rotation.w-axis2)*y+2*rotation.w*rotation.z*x};
    }
    std::unordered_map<int,unsigned> CollectMarginTargets()
    {
        std::unordered_map<int,unsigned> candidates;
        auto add=[&](void* object,unsigned edges) {
            if (object && IsUnderMainPage(object) && GetObjectInstanceId(object)!=mainRootInstanceId)
                candidates[GetObjectInstanceId(getTransform(object,nullptr))]|=edges;
        };
        if (auto* page=ResolveObject(mainPageInstanceId)) for (const auto& field:presetBindings)
        {
            unsigned edges=0;
            switch (field.preset)
            {
            case UI::MiniMap: case UI::TopLeftButtons: case UI::Paimon: case UI::Coop:
            case UI::QuestButton: case UI::SnezhnayaIcon: case UI::ElementalSight: edges=Left|Top; break;
            case UI::TopRightButtons: case UI::Activities: case UI::Bag: case UI::Character:
            case UI::Gacha: case UI::BattlePass: case UI::Handbook: case UI::MiliastraEntrance:
            case UI::Ping: edges=Right|Top; break;
            case UI::Team: edges=Right; break;
            case UI::SkillControls: edges=Right|Bottom; break;
            case UI::ChatButton: case UI::ChatPanel: edges=Left|Bottom; break;
            case UI::QuestTracking: edges=Left; break;
            case UI::PlayerHealthStatus: edges=Bottom; break;
            default: break;
            }
            if (edges) add(ResolvePresetObject(page,field),edges);
        }
        add(FindRelativeObject("GrpMainPage/MapInfo"),Left|Top);
        add(FindRelativeObject("GrpMainPage/ChatArea"),Left|Bottom);
        add(FindRelativeObject("GrpMainPage/GrpMainBtn/GrpMainToggle/GrpLeftPanel"),Left|Bottom);
        add(FindRelativeObject("GrpMainPage/AimContainer/SlotLeft"),Left|Bottom);
        // Apply each edge at the outermost matching container only. Scaling can
        // still target its individual children without multiplying the margin.
        auto result=candidates;
        for (auto& pair:result)
        {
            auto* transform=ResolveObject(pair.first);
            auto* ancestor=transform?getParentTransform(transform,nullptr):nullptr;
            for (int depth=0;ancestor && depth<64;++depth)
            {
                if (auto it=candidates.find(GetObjectInstanceId(ancestor));it!=candidates.end()) pair.second&=~it->second;
                ancestor=getParentTransform(ancestor,nullptr);
            }
        }
        return result;
    }
    void ApplyElementOverride(void* transform,ElementLayout& native)
    {
        const float factor=IsCustomizationActive() && native.scaled?settings->scale:1;
        const auto center=factor!=1?GetScaledRectCenter(transform,native.scale):Vec2{};
        Vec2 shift{};
        if (IsCustomizationActive() && marginsValid)
        {
            if (native.edges&Left) shift.x+=settings->marginLeft;
            if (native.edges&Right) shift.x-=settings->marginRight;
            if (native.edges&Top) shift.y-=settings->marginTop;
            if (native.edges&Bottom) shift.y+=settings->marginBottom;
        }
        const Vec3 scale{native.scale.x*factor,native.scale.y*factor,native.scale.z};
        const Vec2 position{native.position.x+(1-factor)*center.x+shift.x+native.alignmentOffset,
            native.position.y+(1-factor)*center.y+shift.y};
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(scale.x) || !std::isfinite(scale.y)) return;
        Vec3 currentScale{}; Vec2 currentPosition{};
        getLocalScale(transform,&currentScale,nullptr); getAnchoredPosition(transform,&currentPosition,nullptr);
        if (!Same(scale,currentScale)) { originalSetLocalScale(transform,&scale,nullptr); getLocalScale(transform,&currentScale,nullptr); }
        if (!Same(position,currentPosition)) { originalSetAnchoredPosition(transform,&position,nullptr); getAnchoredPosition(transform,&currentPosition,nullptr); }
        // Anchored/local conversion can round the value Unity actually stores.
        // Compare future native writes against that readback, not our input.
        native.appliedScale=currentScale; native.appliedPosition=currentPosition; native.applied=true;
    }
    std::vector<void*> CollectTopRightButtons()
    {
        std::vector<void*> result;
        auto* page=ResolveObject(mainPageInstanceId); auto* top=page?FindFirstPresetObject(page,UI::TopRightButtons):nullptr;
        auto* transform=top?getTransform(top,nullptr):nullptr;
        const int count=transform?getChildCount(transform,nullptr):0;
        if (count<0 || count>256) return result;
        for (int i=0;i<count;++i) result.push_back(getComponentGameObject(getChildTransform(transform,i,nullptr),nullptr));
        return result;
    }
    bool IsObjectActive(void* object) { return object && getActiveSelf(object,nullptr); }
    bool IsVisuallyHidden(void* object)
    {
        auto* transform=object?getTransform(object,nullptr):nullptr;
        for (int depth=0;transform && depth<64;++depth)
        {
            if (hiddenRootIds.count(GetObjectInstanceId(getComponentGameObject(transform,nullptr)))) return true;
            transform=getParentTransform(transform,nullptr);
        }
        return false;
    }
    void* FindComponentByName(void* object,const char* type)
    {
        auto* text=createManagedString(type);
        return object && text?getComponentByName(object,text,nullptr):nullptr;
    }
    void HideCanvasGroup(void* object,bool createIfMissing)
    {
        auto* group=FindComponentByName(object,"CanvasGroup");
        if (!group && createIfMissing)
        {
            auto* type=createManagedString("CanvasGroup");
            if (type) group=addComponentByName(object,type,nullptr);
        }
        const int id=GetObjectInstanceId(group);
        if (!id) return;
        if (!canvasGroupOverrides.count(id))
        {
            const float alpha=getCanvasGroupAlpha(group,nullptr);
            if (!std::isfinite(alpha)) return;
            canvasGroupOverrides.emplace(id,CanvasGroupOverride{GetObjectInstanceId(object),alpha});
        }
        else if (const float current=getCanvasGroupAlpha(group,nullptr); current!=0 && std::isfinite(current)) canvasGroupOverrides.at(id).requestedAlpha=current;
        if (getCanvasGroupAlpha(group,nullptr)!=0) originalSetCanvasGroupAlpha(group,0,nullptr);
    }
    void HideRenderer(void* object)
    {
        auto* renderer=FindComponentByName(object,"Renderer"); const int id=GetObjectInstanceId(renderer);
        if (!id) return;
        if (!rendererOverrides.count(id)) rendererOverrides.emplace(id,RendererOverride{GetObjectInstanceId(object),getRendererEnabled(renderer,nullptr)});
        else if (getRendererEnabled(renderer,nullptr)) rendererOverrides.at(id).requestedEnabled=true;
        if (getRendererEnabled(renderer,nullptr)) originalSetRendererEnabled(renderer,false,nullptr);
    }
    void CollectBranchVisuals(void* object)
    {
        struct Branch { void* transform; int depth; };
        std::vector<Branch> work{{getTransform(object,nullptr),0}};
        size_t visited=0;
        while (!work.empty() && visited++<20000)
        {
            const auto branch=work.back(); work.pop_back();
            if (!branch.transform) continue;
            auto* item=getComponentGameObject(branch.transform,nullptr);
            cachedVisualObjectIds.insert(GetObjectInstanceId(item));
            // Cover existing nested groups, including ignoreParentGroups, and
            // particle/mesh renderers without disabling their scripts or inputs.
            HideCanvasGroup(item,false); HideRenderer(item);
            const int count=getChildCount(branch.transform,nullptr);
            if (branch.depth>=64 || count<0 || count>20000) continue;
            for (int i=0;i<count;++i) work.push_back({getChildTransform(branch.transform,i,nullptr),branch.depth+1});
        }
    }
    void UpdateCachedVisuals()
    {
        // Animation can update native drawing properties without invoking the
        // managed setter. Check only cached components at the render event.
        for (auto it=canvasGroupOverrides.begin();it!=canvasGroupOverrides.end();)
        {
            auto* group=ResolveObject(it->first); auto* object=ResolveObject(it->second.ownerInstanceId);
            if (!group || !object) { it=canvasGroupOverrides.erase(it); continue; }
            const float current=getCanvasGroupAlpha(group,nullptr);
            if (current!=0 && std::isfinite(current)) it->second.requestedAlpha=current;
            const bool hidden=IsCustomizationActive() && IsVisuallyHidden(object);
            const float desired=hidden?0:it->second.requestedAlpha;
            if (current!=desired) originalSetCanvasGroupAlpha(group,desired,nullptr);
            if (!hidden) it=canvasGroupOverrides.erase(it); else ++it;
        }
        for (auto it=rendererOverrides.begin();it!=rendererOverrides.end();)
        {
            auto* renderer=ResolveObject(it->first); auto* object=ResolveObject(it->second.ownerInstanceId);
            if (!renderer || !object) { it=rendererOverrides.erase(it); continue; }
            const bool current=getRendererEnabled(renderer,nullptr);
            if (current) it->second.requestedEnabled=true;
            const bool hidden=IsCustomizationActive() && IsVisuallyHidden(object);
            const bool desired=hidden?false:it->second.requestedEnabled;
            if (current!=desired) originalSetRendererEnabled(renderer,desired,nullptr);
            if (!hidden) it=rendererOverrides.erase(it); else ++it;
        }
    }
    void ApplyHiddenTargets(std::unordered_set<int> targets)
    {
        const auto previous=std::move(hiddenRootIds); hiddenRootIds=std::move(targets);
        for (int id:hiddenRootIds) if (auto* object=ResolveObject(id))
        {
            HideCanvasGroup(object,true);
            if (!previous.count(id)) pendingVisualBranchIds.insert(id);
        }
        // Only newly hidden or newly activated branches are traversed. Native
        // rendering of known components never rescans the UI hierarchy.
        const auto branches=std::move(pendingVisualBranchIds); pendingVisualBranchIds.clear();
        for (int id:branches) if (auto* object=ResolveObject(id)) if (IsVisuallyHidden(object))
        {
            bool nested=false;
            auto* ancestor=getParentTransform(getTransform(object,nullptr),nullptr);
            for (int depth=0;ancestor && depth<64;++depth)
            {
                if (branches.count(GetObjectInstanceId(getComponentGameObject(ancestor,nullptr)))) { nested=true; break; }
                ancestor=getParentTransform(ancestor,nullptr);
            }
            if (!nested) CollectBranchVisuals(object);
        }
        UpdateCachedVisuals();
        if (hiddenRootIds.empty()) cachedVisualObjectIds.clear();
    }
    void RestoreVisualOverrides()
    {
        for (const auto& pair:canvasGroupOverrides) TryNativeCall([&] { if (auto* group=ResolveObject(pair.first)) originalSetCanvasGroupAlpha(group,pair.second.requestedAlpha,nullptr); });
        for (const auto& pair:rendererOverrides) TryNativeCall([&] { if (auto* renderer=ResolveObject(pair.first)) originalSetRendererEnabled(renderer,pair.second.requestedEnabled,nullptr); });
        canvasGroupOverrides.clear(); rendererOverrides.clear(); hiddenRootIds.clear(); pendingVisualBranchIds.clear(); cachedVisualObjectIds.clear();
    }
    bool IsVisibleForAlignment(void* object)
    {
        auto* transform=object?getTransform(object,nullptr):nullptr;
        for (int depth=0;transform && depth<64;++depth)
        {
            auto* owner=getComponentGameObject(transform,nullptr);
            if (!getActiveSelf(owner,nullptr) || hiddenRootIds.count(GetObjectInstanceId(owner))) return false;
            transform=getParentTransform(transform,nullptr);
        }
        return !transform;
    }
    void ApplyElementLayout()
    {
        if (!IsCustomizationActive() || (settings->scale==1 && !settings->alignRightButtons && !HasMargins())) { RestoreElementLayout(); return; }
        Rect rootRect{};
        if (auto* root=ResolveObject(mainRootTransformInstanceId)) getRect(root,&rootRect,nullptr);
        marginsValid=std::isfinite(rootRect.width) && std::isfinite(rootRect.height) &&
            rootRect.width-settings->marginLeft-settings->marginRight>1 && rootRect.height-settings->marginTop-settings->marginBottom>1;
        const auto rightButtons=settings->alignRightButtons?CollectTopRightButtons():std::vector<void*>{};
        const bool rebuild=scaleTargetsRefreshPending;
        scaleTargetsRefreshPending=false;
        for (auto& pair:elementLayoutOverrides) if (auto* transform=ResolveObject(pair.first)) ObserveElementLayout(transform,pair.second);
        std::unordered_set<int> requiredTransformIds;
        if (rebuild)
        {
            auto candidates=settings->scale!=1?CollectScaleTargets():std::vector<void*>{};
            std::unordered_set<int> scaledIds;
            for (auto* object:candidates) scaledIds.insert(GetObjectInstanceId(getTransform(object,nullptr)));
            candidates.insert(candidates.end(),rightButtons.begin(),rightButtons.end());
            const auto margins=HasMargins()?CollectMarginTargets():std::unordered_map<int,unsigned>{};
            for (const auto& pair:margins) if (pair.second) if (auto* transform=ResolveObject(pair.first)) candidates.push_back(getComponentGameObject(transform,nullptr));
            for (auto* object:candidates)
            {
                auto* transform=getTransform(object,nullptr); const int id=GetObjectInstanceId(transform);
                if (!id) continue;
                if (!elementLayoutOverrides.count(id))
                {
                    ElementLayout native{};
                    if (!TryNativeCall([&] { getLocalScale(transform,&native.scale,nullptr); getAnchoredPosition(transform,&native.position,nullptr); })) continue;
                    elementLayoutOverrides.emplace(id,native);
                }
                auto& native=elementLayoutOverrides.at(id);
                native.scaled=scaledIds.count(id)!=0;
                const auto margin=margins.find(id);
                native.edges=margin!=margins.end()?margin->second:0;
                requiredTransformIds.insert(id);
            }
        }
        std::unordered_map<int,float> offsets;
        struct Slot { int id; float center; bool visible; };
        std::vector<Slot> slots;
        for (auto* object:rightButtons)
        {
            auto* transform=getTransform(object,nullptr); const int id=GetObjectInstanceId(transform);
            auto it=elementLayoutOverrides.find(id); if (it==elementLayoutOverrides.end()) continue;
            // Most entries have a Button child; UGC uses the wrapper itself.
            auto* buttonPath=createManagedString("Button");
            auto* buttonTransform=buttonPath?findChildTransform(transform,buttonPath,nullptr):nullptr;
            auto* button=buttonTransform?getComponentGameObject(buttonTransform,nullptr):object;
            if (!IsObjectActive(object) || !IsObjectActive(button)) continue;
            Vec3 local{}; Vec2 current{};
            getLocalPosition(transform,&local,nullptr); getAnchoredPosition(transform,&current,nullptr);
            const auto center=GetScaledRectCenter(transform,it->second.scale);
            // local - anchored is the anchor reference in the common parent.
            // Substituting the native anchored value removes our prior offset.
            const float x=local.x-current.x+it->second.position.x+center.x;
            if (std::isfinite(x)) slots.push_back({id,x,IsVisibleForAlignment(button)});
        }
        std::sort(slots.begin(),slots.end(),[](const Slot& a,const Slot& b) { return a.center<b.center; });
        const size_t visible=std::count_if(slots.begin(),slots.end(),[](const Slot& slot) { return slot.visible; });
        size_t next=slots.size()-visible;
        for (const auto& slot:slots) if (slot.visible) offsets[slot.id]=slots[next++].center-slot.center;
        for (auto it=elementLayoutOverrides.begin();it!=elementLayoutOverrides.end();)
        {
            auto* transform=ResolveObject(it->first);
            if (!transform) { it=elementLayoutOverrides.erase(it); continue; }
            auto& native=it->second;
            if (rebuild && !requiredTransformIds.count(it->first))
            {
                TryNativeCall([&] { originalSetLocalScale(transform,&native.scale,nullptr); originalSetAnchoredPosition(transform,&native.position,nullptr); });
                it=elementLayoutOverrides.erase(it); continue;
            }
            TryNativeCall([&] {
                native.alignmentOffset=offsets[it->first];
                ApplyElementOverride(transform,native);
            });
            ++it;
        }
    }
    void CaptureMainPage(void* page)
    {
        if (!page) return;
        auto* root = getComponentGameObject(page, nullptr);
        if (!root || GetObjectName(root) != "InLevelMainPage") return;
        auto* transform = getTransform(root, nullptr);
        const int newRoot = GetObjectInstanceId(root);
        if (mainRootInstanceId == newRoot && ResolveObject(mainRootInstanceId) && rootLayoutCaptured) { mainPageInstanceId = GetObjectInstanceId(page); return; }
        RestoreElementLayout();
        mainPageInstanceId = GetObjectInstanceId(page); mainRootInstanceId = newRoot; mainRootTransformInstanceId = GetObjectInstanceId(transform); mainRootTransform = transform;
        rootLayoutCaptured = transform!=nullptr;
        refreshPending = true;
    }
    void DiscoverMainPage()
    {
        if (auto* page = ResolveObject(pendingPageInstanceId.exchange(0))) CaptureMainPage(page);
        if (ResolveObject(mainRootInstanceId)) { mainRootTransform = ResolveObject(mainRootTransformInstanceId); return; }
        RestoreElementLayout();
        mainPageInstanceId = mainRootInstanceId = mainRootTransformInstanceId = 0; mainRootTransform = nullptr; rootLayoutCaptured = false;
        auto* root = FindActiveGameObject(UI::MainPagePath);
        if (!root) return;
        // Unity's string lookup uses the managed short type name, not an IL2CPP class layout.
        auto* type = createManagedString("MonoInLevelMainPage");
        if (type) CaptureMainPage(getComponentByName(root, type, nullptr));
    }
    void ApplySettings()
    {
        DiscoverMainPage();
        std::unordered_set<int> targets;
        auto add = [&](void* object) { const int id = GetObjectInstanceId(object); if (id) targets.insert(id); };
        if (IsCustomizationActive())
        {
            const auto hidden=UI::GetEffectiveHiddenPresets(*settings);
            if (hidden[UI::AllMainUi]) add(ResolveObject(mainRootInstanceId));
            if (auto* page = ResolveObject(mainPageInstanceId))
            {
                for (const auto& field : presetBindings) if (hidden[field.preset])
                {
                    add(ResolvePresetObject(page,field));
                }
                if (hidden[UI::QuestTracking])
                { auto* guide = getQuestGuide(page, nullptr); if (guide) { auto* object = getComponentGameObject(guide, nullptr); if (IsUnderMainPage(object)) add(object); } }
            }
            for (const auto& path : settings->hiddenPaths)
            {
                if (path == UI::MainPagePath) { add(ResolveObject(mainRootInstanceId)); continue; }
                if (!mainRootTransform) continue;
                auto* text = createManagedString(path.c_str() + std::char_traits<char>::length(UI::MainPagePath) + 1);
                auto* transform = text ? findChildTransform(mainRootTransform, text, nullptr) : nullptr;
                if (transform) add(getComponentGameObject(transform, nullptr));
            }
            for (auto it=questTraceIds.begin();it!=questTraceIds.end();)
            {
                if (auto* object=ResolveObject(*it))
                { if (hidden[UI::QuestTracking]) add(object); ++it; }
                else it=questTraceIds.erase(it);
            }
        }
        ApplyHiddenTargets(std::move(targets));
        ApplyElementLayout();
    }
    void ExportUiNode(void* transform, const std::string& prefix, std::ofstream& output, int depth, size_t& count)
    {
        if (!transform || depth > 64 || count >= 20000) return;
        ++count;
        const auto path = prefix + "/" + GetObjectName(transform);
        auto* object = getComponentGameObject(transform, nullptr);
        output << (getActiveSelf(object, nullptr) ? "1\t" : "0\t") << path << '\n';
        const int children = getChildCount(transform, nullptr);
        if (children < 0 || children > 20000) return;
        for (int i = 0; i < children && count < 20000; ++i) ExportUiNode(getChildTransform(transform, i, nullptr), path, output, depth + 1, count);
    }
    void ExportUiTree()
    {
        DiscoverMainPage();
        std::ofstream output(Support::PluginDirectory() / L"ui-tree.txt", std::ios::binary | std::ios::trunc);
        if (!output) return;
        output << "# UTF-8; activeSelf<TAB>absolute path; inactive descendants included.\n";
        if (!mainRootTransform) { output << "# Main UI is not currently available. Enter the world and export again.\n"; return; }
        if (auto* page = ResolveObject(mainPageInstanceId))
        {
            for (const auto& field : presetBindings) if (auto* object = ResolvePresetObject(page,field))
                output << "# " << UI::PresetConfigKeys[field.preset] << '\t' << GetObjectPath(object) << '\n';
        }
        size_t count = 0;
        ExportUiNode(mainRootTransform, "/Canvas/Pages", output, 0, count);
        for (int id:questTraceIds) if (auto* object=ResolveObject(id)) if (!IsUnderMainPage(object))
        {
            output << "# HideQuestTrack\t" << GetObjectPath(object) << '\n';
            auto* transform=getTransform(object,nullptr);
            const auto path=GetObjectPath(object); const auto slash=path.find_last_of('/');
            ExportUiNode(transform,path.substr(0,slash),output,0,count);
        }
        if (count >= 20000) output << "# Truncated at 20000 nodes.\n";
    }
    bool IsGameForeground()
    { DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid); return pid == GetCurrentProcessId(); }
    bool IsKeyPressed(int key, bool& previous)
    {
        const bool down = key != 0 && IsGameForeground() && (GetAsyncKeyState(key) & 0x8000);
        const bool pressed = down && !previous; previous = down; return pressed;
    }
    void UpdateMainUi()
    {
        const auto next = std::atomic_load_explicit(&pendingSettings, std::memory_order_acquire);
        if (settings != next)
        {
            settings = next; refreshPending = true; scaleTargetsRefreshPending=true;
            watchActivationChanges = !settings->hiddenPaths.empty() || settings->scale!=1 || settings->alignRightButtons || HasMargins();
            for (bool hide : settings->hiddenPresets) watchActivationChanges = watchActivationChanges || hide;
        }
        if (pendingPageInstanceId.load()) refreshPending = true;
        if (IsKeyPressed(settings->restoreHotkey, restoreKeyHeld))
        { temporarilyRestored = !temporarilyRestored; refreshPending = true; }
        const bool dump = IsKeyPressed(settings->exportHotkey, exportKeyHeld);
        if (refreshPending) { refreshPending = false; ApplySettings(); }
        else { UpdateCachedVisuals(); if (!elementLayoutOverrides.empty()) ApplyElementLayout(); }
        if (dump) ExportUiTree();
    }
    void RestoreAfterFailure()
    {
        RestoreElementLayout();
        RestoreVisualOverrides();
    }
    void __fastcall OnCanvasWillRender(void* method)
    {
        originalCanvasWillRender(method);
        if (!hooksReady.load(std::memory_order_acquire) || isApplyingOverrides) return;
        DWORD empty = 0;
        uiThreadId.compare_exchange_strong(empty, GetCurrentThreadId());
        if (!IsOnUiThread()) return;
        isApplyingOverrides = true;
        bool ok = false;
        try { ok = TryNativeCall([] { UpdateMainUi(); }); } catch (...) { }
        if (!ok)
        {
            hooksReady.store(false);
            try { RestoreAfterFailure(); } catch (...) { }
        }
        isApplyingOverrides = false;
    }
    void __fastcall OnSetActive(void* object, bool active, void* method)
    {
        const bool observe=hooksReady.load(std::memory_order_acquire) && !isApplyingOverrides && IsOnUiThread();
        bool changed=true;
        if (observe) TryNativeCall([&] { changed=getActiveSelf(object,nullptr)!=active; });
        // Forward the exact native request. SetActive(false) on an input owner
        // unregisters skill/shortcut handlers, so it must never implement hiding.
        originalSetActive(object,active,method);
        if (observe)
        {
            try { TryNativeCall([&] {
                if (IsCustomizationActive() && IsVisuallyHidden(object) && (changed || !cachedVisualObjectIds.count(GetObjectInstanceId(object))))
                { if (active) pendingVisualBranchIds.insert(GetObjectInstanceId(object)); refreshPending=true; }
                if ((changed && IsCustomizationActive() && watchActivationChanges) || !mainRootInstanceId)
                {
                    if (object==ResolveObject(mainRootInstanceId) || IsUnderMainPage(object)) { refreshPending=true; if (active) scaleTargetsRefreshPending=true; }
                    else if (GetObjectName(object)=="InLevelMainPage") refreshPending=true;
                }
            }); } catch (...) { }
        }
    }
    void __fastcall OnSetCanvasGroupAlpha(void* group,float alpha,void* method)
    {
        float desired=alpha;
        if (hooksReady.load(std::memory_order_acquire) && !isApplyingOverrides && IsOnUiThread()) TryNativeCall([&] {
            auto it=canvasGroupOverrides.find(GetObjectInstanceId(group));
            if (it!=canvasGroupOverrides.end())
            {
                it->second.requestedAlpha=alpha;
                if (IsCustomizationActive() && IsVisuallyHidden(ResolveObject(it->second.ownerInstanceId))) desired=0;
            }
            else if (IsCustomizationActive()) if (auto* object=getComponentGameObject(group,nullptr)) if (IsVisuallyHidden(object))
            { canvasGroupOverrides.emplace(GetObjectInstanceId(group),CanvasGroupOverride{GetObjectInstanceId(object),alpha}); desired=0; }
        });
        originalSetCanvasGroupAlpha(group,desired,method);
    }
    void __fastcall OnSetRendererEnabled(void* renderer,bool enabled,void* method)
    {
        bool desired=enabled;
        if (hooksReady.load(std::memory_order_acquire) && !isApplyingOverrides && IsOnUiThread()) TryNativeCall([&] {
            auto it=rendererOverrides.find(GetObjectInstanceId(renderer));
            if (it!=rendererOverrides.end())
            {
                it->second.requestedEnabled=enabled;
                if (IsCustomizationActive() && IsVisuallyHidden(ResolveObject(it->second.ownerInstanceId))) desired=false;
            }
            else if (IsCustomizationActive()) if (auto* object=getComponentGameObject(renderer,nullptr)) if (IsVisuallyHidden(object))
            { rendererOverrides.emplace(GetObjectInstanceId(renderer),RendererOverride{GetObjectInstanceId(object),enabled}); desired=false; }
        });
        originalSetRendererEnabled(renderer,desired,method);
    }
    void __fastcall OnSetLocalScale(void* transform, const Vec3* value, void* method)
    {
        originalSetLocalScale(transform,value,method);
        if (hooksReady.load(std::memory_order_acquire) && !isApplyingOverrides && IsOnUiThread() && value)
        {
            TryNativeCall([&] {
                auto it=elementLayoutOverrides.find(GetObjectInstanceId(transform));
                if (it!=elementLayoutOverrides.end())
                {
                    ObserveElementLayout(transform,it->second); it->second.scale=*value;
                    ApplyElementOverride(transform,it->second); refreshPending=true;
                }
            });
        }
    }
    void __fastcall OnSetAnchoredPosition(void* transform, const Vec2* value, void* method)
    {
        originalSetAnchoredPosition(transform,value,method);
        if (hooksReady.load(std::memory_order_acquire) && !isApplyingOverrides && IsOnUiThread() && value)
        {
            TryNativeCall([&] {
                auto it=elementLayoutOverrides.find(GetObjectInstanceId(transform));
                if (it!=elementLayoutOverrides.end())
                {
                    ObserveElementLayout(transform,it->second); it->second.position=*value;
                    ApplyElementOverride(transform,it->second); refreshPending=true;
                }
            });
        }
    }
    ElementLayout* FindNativeLayout(void* transform)
    {
        if (!hooksReady.load(std::memory_order_acquire) || isApplyingOverrides || !IsOnUiThread() || elementLayoutOverrides.empty()) return nullptr;
        auto it=elementLayoutOverrides.find(GetObjectInstanceId(transform));
        if (it==elementLayoutOverrides.end()) return nullptr;
        ObserveElementLayout(transform,it->second);
        return &it->second;
    }
    void __fastcall OnGetLocalScale(void* transform,Vec3* value,void* method)
    {
        getLocalScale(transform,value,method);
        if (value) TryNativeCall([&] { if (auto* native=FindNativeLayout(transform)) *value=native->scale; });
    }
    void __fastcall OnGetAnchoredPosition(void* transform,Vec2* value,void* method)
    {
        getAnchoredPosition(transform,value,method);
        if (value) TryNativeCall([&] { if (auto* native=FindNativeLayout(transform)) *value=native->position; });
    }
    void __fastcall OnGetLocalPosition(void* transform,Vec3* value,void* method)
    {
        getLocalPosition(transform,value,method);
        if (value) TryNativeCall([&] { if (auto* native=FindNativeLayout(transform)) {
            Vec2 current{}; getAnchoredPosition(transform,&current,nullptr);
            value->x+=native->position.x-current.x; value->y+=native->position.y-current.y;
        } });
    }
    void __fastcall OnSetSizeDelta(void* transform, const Vec2* value, void* method)
    {
        if (hooksReady.load(std::memory_order_acquire) && !isApplyingOverrides && IsOnUiThread() && value)
        {
            TryNativeCall([&] {
                if (transform==mainRootTransform && ResolveObject(mainRootTransformInstanceId)==transform) refreshPending=true;
                else if (elementLayoutOverrides.count(GetObjectInstanceId(transform))) refreshPending=true;
                else if (IsCustomizationActive() && settings->alignRightButtons && IsUnderMainPage(getComponentGameObject(transform,nullptr))) refreshPending=true;
                else if (IsCustomizationActive() && (settings->marginLeft || settings->marginRight || settings->marginTop || settings->marginBottom) &&
                    mainRootTransform && IsAncestorOrSelf(getComponentGameObject(transform,nullptr),ResolveObject(mainRootInstanceId))) refreshPending=true;
            });
        }
        originalSetSizeDelta(transform,value,method);
    }
    void ObserveVisualBranch(void* object)
    {
        if (!object || !hooksReady.load(std::memory_order_acquire) || isApplyingOverrides || !IsOnUiThread()) return;
        if (IsCustomizationActive() && IsVisuallyHidden(object))
        { pendingVisualBranchIds.insert(GetObjectInstanceId(object)); refreshPending=true; }
        if (IsCustomizationActive() && watchActivationChanges && IsUnderMainPage(object))
        { scaleTargetsRefreshPending=true; refreshPending=true; }
    }
    void __fastcall OnSetParent(void* transform,void* parent,bool worldPositionStays,void* method)
    {
        originalSetParent(transform,parent,worldPositionStays,method);
        if (!hooksReady.load(std::memory_order_acquire) || isApplyingOverrides || !IsOnUiThread()) return;
        TryNativeCall([&] { ObserveVisualBranch(getComponentGameObject(transform,nullptr)); });
    }
    void __fastcall OnRegisterEffectRenderers(void* object,void* method)
    {
        originalRegisterEffectRenderers(object,method);
        // MonoUIEffect calls this after instantiating its particle hierarchy,
        // even when no alpha controller exists. The initial HUD scan may have
        // seen only its empty placeholder, so collect the completed branch.
        TryNativeCall([&] { ObserveVisualBranch(object); });
    }
    void __fastcall OnMainPageAwake(void* page, void* method)
    { originalMainPageAwake(page, method); if (hooksReady.load()) TryNativeCall([&] { pendingPageInstanceId.store(GetObjectInstanceId(page)); }); }
    void __fastcall OnQuestTraceTick(void* trace, void* method)
    {
        originalQuestTraceTick(trace,method);
        if (!hooksReady.load(std::memory_order_acquire) || isApplyingOverrides || !IsOnUiThread()) return;
        // MonoQuestTrace owns the persistent title, objective icon and distance.
        // It is created by its own context, outside InLevelMainPage on CN 7.1.
        TryNativeCall([&] {
            auto* object=getComponentGameObject(trace,nullptr); const int id=GetObjectInstanceId(object);
            if (id && questTraceIds.insert(id).second) refreshPending=true;
        });
    }
}
namespace Engine
{
    void PublishSettings(std::shared_ptr<const UI::Settings> value)
    { std::atomic_store_explicit(&pendingSettings, std::move(value), std::memory_order_release); }
    bool Initialize()
    {
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        bool imageMatches = false;
        TryNativeCall([&] {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            imageMatches = nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
                nt->FileHeader.TimeDateStamp == Native::Timestamp && nt->OptionalHeader.SizeOfImage == Native::ImageSize;
        });
        if (!imageMatches) return false;
        bool valid = true;
        auto address = [&](const Native::Method& entry) -> void* {
            auto* ptr = reinterpret_cast<void*>(base + entry.rva);
            const bool matched = &entry == &Native::SetActive ? Support::IsCompatibleSetActiveEntry(ptr,entry.bytes) : Support::MatchesEntryBytes(ptr,entry.bytes);
            if (!matched) valid = false;
            return matched ? ptr : nullptr;
        };
#define LOAD(field, entry) field = reinterpret_cast<decltype(field)>(address(Native::entry))
        LOAD(createManagedString,StringNew); LOAD(findActiveGameObject,FindObject); LOAD(getTransform,ObjectTransform);
        LOAD(getComponentGameObject,ComponentObject); LOAD(getComponentByName,ComponentByName); LOAD(getUnityObjectName,Name);
        LOAD(addComponentByName,AddComponent); LOAD(getCanvasGroupAlpha,ReadAlpha); LOAD(getRendererEnabled,ReadRendererEnabled);
        LOAD(getInstanceId,InstanceId); LOAD(getObjectFromInstanceId,FromId); LOAD(getActiveSelf,ActiveSelf); LOAD(getParentTransform,Parent);
        LOAD(getChildCount,ChildCount); LOAD(getChildTransform,Child); LOAD(findChildTransform,FindTransform);
        LOAD(getQuestGuide,QuestGuide);
        LOAD(getSizeDelta,ReadSize); LOAD(getPivot,ReadPivot); LOAD(getRect,ReadRect);
        LOAD(getLocalRotation,ReadRotation);
#undef LOAD
        struct Hook { const Native::Method* entry; void* replacement; void** original; void* target; };
        Hook hooks[] = {
            {&Native::ReadScale,reinterpret_cast<void*>(&OnGetLocalScale),reinterpret_cast<void**>(&getLocalScale),nullptr},
            {&Native::ReadPosition,reinterpret_cast<void*>(&OnGetAnchoredPosition),reinterpret_cast<void**>(&getAnchoredPosition),nullptr},
            {&Native::ReadLocalPosition,reinterpret_cast<void*>(&OnGetLocalPosition),reinterpret_cast<void**>(&getLocalPosition),nullptr},
            {&Native::SetActive,reinterpret_cast<void*>(&OnSetActive),reinterpret_cast<void**>(&originalSetActive),nullptr},
            {&Native::SetAlpha,reinterpret_cast<void*>(&OnSetCanvasGroupAlpha),reinterpret_cast<void**>(&originalSetCanvasGroupAlpha),nullptr},
            {&Native::SetRendererEnabled,reinterpret_cast<void*>(&OnSetRendererEnabled),reinterpret_cast<void**>(&originalSetRendererEnabled),nullptr},
            {&Native::SetScale,reinterpret_cast<void*>(&OnSetLocalScale),reinterpret_cast<void**>(&originalSetLocalScale),nullptr},
            {&Native::SetPosition,reinterpret_cast<void*>(&OnSetAnchoredPosition),reinterpret_cast<void**>(&originalSetAnchoredPosition),nullptr},
            {&Native::SetSize,reinterpret_cast<void*>(&OnSetSizeDelta),reinterpret_cast<void**>(&originalSetSizeDelta),nullptr},
            {&Native::WillRender,reinterpret_cast<void*>(&OnCanvasWillRender),reinterpret_cast<void**>(&originalCanvasWillRender),nullptr},
            {&Native::MainAwake,reinterpret_cast<void*>(&OnMainPageAwake),reinterpret_cast<void**>(&originalMainPageAwake),nullptr},
            {&Native::QuestTraceTick,reinterpret_cast<void*>(&OnQuestTraceTick),reinterpret_cast<void**>(&originalQuestTraceTick),nullptr},
            {&Native::SetParent,reinterpret_cast<void*>(&OnSetParent),reinterpret_cast<void**>(&originalSetParent),nullptr},
            {&Native::RegisterEffectRenderers,reinterpret_cast<void*>(&OnRegisterEffectRenderers),reinterpret_cast<void**>(&originalRegisterEffectRenderers),nullptr}
        };
        for (auto& hook : hooks) hook.target = address(*hook.entry);
        if (!valid || MH_Initialize() != MH_OK) return false;
        size_t installed = 0;
        for (auto& hook : hooks)
        {
            if (MH_CreateHook(hook.target,hook.replacement,hook.original) != MH_OK) break;
            ++installed;
        }
        if (installed == std::size(hooks))
        {
            for (auto& hook : hooks) MH_QueueEnableHook(hook.target);
            if (MH_ApplyQueued() == MH_OK) { hooksReady.store(true,std::memory_order_release); return true; }
        }
        for (size_t i=0;i<installed;++i) { MH_DisableHook(hooks[i].target); MH_RemoveHook(hooks[i].target); }
        return false;
    }
}
