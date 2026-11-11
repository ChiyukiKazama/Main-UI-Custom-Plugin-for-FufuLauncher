// HUD drawing and complete-region layouts use separate per-page caches.
#include "Engine.h"
#include "ElementPolicy.h"
#include "LayoutMath.h"
#include "Native.h"
#include "Support.h"
#include "HookSetupGuard.h"
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace HUD
{
    namespace
    {
        struct Color {float r,g,b,a;};
        using ObjectFn=void*(__fastcall*)(void*,void*);
        using StringFn=void*(__fastcall*)(const char*);
        using IdFn=int(__fastcall*)(void*,void*);
        using ResolveFn=void*(__fastcall*)(int,void*);
        using NamedFn=void*(__fastcall*)(void*,void*,void*);
        using ChildFn=void*(__fastcall*)(void*,int,void*);
        using BoolGet=bool(__fastcall*)(void*,void*);
        using BoolSet=void(__fastcall*)(void*,bool,void*);
        using FloatSet=void(__fastcall*)(void*,float,void*);
        using ColorGet=void(__fastcall*)(void*,Color*,void*);
        using V2Get=void(__fastcall*)(void*,V2*,void*);
        using V2Set=void(__fastcall*)(void*,const V2*,void*);
        using V3Get=void(__fastcall*)(void*,V3*,void*);
        using V3Set=void(__fastcall*)(void*,const V3*,void*);
        using RectGet=void(__fastcall*)(void*,Rect*,void*);
        using RotationGet=void(__fastcall*)(void*,Rotation*,void*);
        using RenderFn=void(__fastcall*)(void*);
        using AwakeFn=void(__fastcall*)(void*,void*);
        using EffectSetObjectFn=void(__fastcall*)(void*,void*,void*);
        using AddOptionFn=void*(__fastcall*)(void*,int,void*);
        using PrefabFn=void*(__fastcall*)(void*,int,void*,void*,void*);
        struct Api
        {
            StringFn stringNew=nullptr;ObjectFn find=nullptr,transform=nullptr,owner=nullptr,parent=nullptr,name=nullptr;
            IdFn id=nullptr,count=nullptr;ResolveFn resolve=nullptr;NamedFn component=nullptr,childPath=nullptr;ChildFn child=nullptr;
            BoolGet active=nullptr,rendererEnabled=nullptr;BoolSet setRendererEnabled=nullptr;
            ColorGet color=nullptr;FloatSet setAlpha=nullptr;
            V3Get scale=nullptr,local=nullptr;V3Set setScale=nullptr,setLocal=nullptr;
            V2Get position=nullptr;V2Set setPosition=nullptr;RectGet rect=nullptr;RotationGet rotation=nullptr;
            AwakeFn maskDirty=nullptr,maskRebuild=nullptr,rectClip=nullptr;
        } api;
        std::shared_ptr<const UI::Settings> pending=std::make_shared<UI::Settings>(),settings;
        std::string acceptedConfig;
        std::atomic<bool> ready{false},faulted{false};
        std::atomic<DWORD> uiThread{0};
        RenderFn originalRender=nullptr;
        AwakeFn originalAwake=nullptr;
        AwakeFn originalGraphicEnable=nullptr,originalGraphicLightActivate=nullptr,originalEffectEnable=nullptr,originalQuestEnable=nullptr,originalQuestTraceTick=nullptr;
        AwakeFn originalInteeKeyRefresh=nullptr,originalInteeSwitchKeyRefresh=nullptr;
        AwakeFn originalFeatureEffectMount=nullptr;
        EffectSetObjectFn originalEffectSetObject=nullptr;
        BoolSet originalEffectPlay=nullptr;
        AddOptionFn originalDialogAdd=nullptr,originalPickupAdd=nullptr;
        PrefabFn originalPrefabInstance=nullptr;
        struct VisualEvent {int id;void* token;UI::Preset preset;const char* type=nullptr;bool subtree=false,gameObject=false;};
        std::mutex visualMutex;
        std::vector<VisualEvent> visualEvents;
        std::atomic<bool> watchVisuals{false},haveVisualEvents{false};
        std::atomic<int> pendingPage{0};
        std::atomic<unsigned> commands{0};
        bool restored=false;
        constexpr const char* QuestHintTypes[]={"MonoQuestTrace","MonoQuestHintText","MonoQuestHintActiveController",
            "MonoProgressGuideHint","MonoProgressGuideListedHint","MonoNearbyTrackableQuestHint","MonoHandbookQuestGuideHint","MonoReturnToQuestBtn"};
        thread_local bool applying=false;
        int rootId=0,rootTransformId=0,pageId=0;
        std::vector<int> layoutIds;
        std::unordered_map<std::string,int> layoutPaths;
        uint32_t effectiveMask=0;
        std::vector<Region> regions;
        struct Record
        {
            int id=0,object=0,transform=0,graphic=0,anchor=0,anchorTransform=0,smoothMask=0,rectMask=0;
            std::string path,kind,scopePath;Match match;
            std::vector<std::pair<int,int>> chain;
            bool renderer=false,rect=false,rectKnown=false,layoutTarget=false,layoutOnly=false,layoutRoot=false,marginRoot=false,scaleRoot=false,applyScale=false;
            bool alphaChanged=false,rendererChanged=false;
            float alpha=1;bool enabled=true;
            Layout layout;V3 local{};V2 currentPosition{},canvasShift{};float factor=1;
            void* token=nullptr;void* graphicToken=nullptr;void* smoothMaskToken=nullptr;void* rectMaskToken=nullptr;
        };
        std::unordered_map<int,Record> records;
        std::unordered_map<int,void*> resolved;
        std::unordered_map<int,int> parents;
        std::unordered_map<int,bool> activeCache;
        std::unordered_map<int,Matrix> matrices;
        std::unordered_map<int,Record*> layoutByTransform;
        std::unordered_set<int> invalidMatrices;
        struct Node
        {
            int transform=0,object=0,graphic=0,children=-1;
            std::vector<int> childIds;
            void* graphicToken=nullptr;
            std::string path;
            Match match;
            std::vector<std::pair<int,int>> chain;
            bool forbidden=false;
        };
        struct UnitBinding {int transform=0,button=0;bool rect=false;void* token=nullptr;void* buttonToken=nullptr;};
        std::unordered_map<int,Node> nodes;
        std::unordered_set<std::string> covered,attempted;
        std::vector<bool> boundFields;
        std::unordered_map<std::string,UnitBinding> unitBindings;
        std::unordered_set<int> visibilityDirty,layoutDirty,added;
        std::unordered_set<int> changedLayouts;
        void* rootToken=nullptr;
        void* pageToken=nullptr;
        template<class Fn> bool Guard(Fn&& fn)
        {__try{fn();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}

        int Id(void* object){return object?api.id(object,nullptr):0;}
        void* Resolve(int id)
        {if(!id)return nullptr;auto it=resolved.find(id);if(it!=resolved.end())return it->second;auto* object=api.resolve(id,nullptr);resolved.emplace(id,object);return object;}
        void ClearFrame(){resolved.clear();parents.clear();activeCache.clear();matrices.clear();invalidMatrices.clear();layoutByTransform.clear();}
        int Parent(int transform)
        {auto it=parents.find(transform);if(it!=parents.end())return it->second;auto* p=Resolve(transform);const int parent=p?Id(api.parent(p,nullptr)):0;parents[transform]=parent;return parent;}
        bool Active(int transform,int depth=0)
        {
            if(!transform)return true;if(depth>48)return false;
            auto it=activeCache.find(transform);if(it!=activeCache.end())return it->second;
            auto* t=Resolve(transform);auto* object=t?api.owner(t,nullptr):nullptr;
            const bool active=object && api.active(object,nullptr) && Active(Parent(transform),depth+1);
            activeCache[transform]=active;return active;
        }
        void* Component(void* object,const char* type)
        {if(!object)return nullptr;auto* text=api.stringNew(type);return text?api.component(object,text,nullptr):nullptr;}
        std::string Name(void* object)
        {
            auto* s=static_cast<const unsigned char*>(api.name(object,nullptr));if(!s)return {};
            const int count=*reinterpret_cast<const int*>(s+16);if(count<=0 || count>512)return {};
            const auto* chars=reinterpret_cast<const wchar_t*>(s+20);
            const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,chars,count,nullptr,0,nullptr,nullptr);if(bytes<=0)return {};
            std::string result(static_cast<size_t>(bytes),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,chars,count,result.data(),bytes,nullptr,nullptr);return result;
        }
        std::string Path(void* transform,void* anchor)
        {
            std::string path;
            for(unsigned depth=0;transform && transform!=anchor && depth<48;++depth)
            {const auto name=Name(transform);if(name.empty())return {};path=name+(path.empty()?"":"/"+path);transform=api.parent(transform,nullptr);}
            return transform==anchor?path:std::string{};
        }
        void* Find(const char* path)
        {auto* text=api.stringNew(path);return text?api.find(text,nullptr):nullptr;}
        void* Relative(void* anchor,const std::string& path)
        {auto* text=api.stringNew(path.c_str());return anchor && text?api.childPath(anchor,text,nullptr):nullptr;}
        bool Identity(const Record& record,bool full=false)
        {
            auto* component=Resolve(record.id);auto* object=Resolve(record.object);auto* anchor=Resolve(record.anchor);
            if(!component || component!=record.token || !object || !anchor || api.owner(component,nullptr)!=object)return false;
            if(!record.renderer && Resolve(record.graphic)!=record.graphicToken)return false;
            for(const auto& pair:record.chain)if(!Resolve(pair.first) || Parent(pair.first)!=pair.second)return false;
            if(full)
            {
                auto* anchorTransform=Resolve(record.anchorTransform);auto* transform=Resolve(record.transform);
                const auto relative=record.scopePath.empty()?record.path:
                    (record.path==record.scopePath?std::string{}:record.path.substr(record.scopePath.size()+1));
                if(!anchorTransform || !transform || Path(transform,anchorTransform)!=relative)return false;
                if(!record.renderer && Resolve(record.graphic)!=record.graphicToken)return false;
            }
            return true;
        }
        bool HiddenNow(const Record& record)
        {
            if(!settings || restored || faulted.load())return false;
            if(record.match.presets&effectiveMask)return true;
            for(const auto& selected:settings->hiddenPaths)
            {
                if(selected==UI::MainPagePath)return record.match.presets!=0;
                if(Within(selected,UI::MainPagePath) && Within(record.path,selected.substr(std::char_traits<char>::length(UI::MainPagePath)+1)))return record.match.presets!=0;
            }
            return false;
        }
        void RefreshMasks()
        {
            if(changedLayouts.empty())return;
            std::unordered_set<int> refreshed;
            for(const auto& pair:records)
            {
                const auto& r=pair.second;if(!r.smoothMask && !r.rectMask)continue;
                const bool changed=changedLayouts.count(r.transform) || std::any_of(r.chain.begin(),r.chain.end(),
                    [](const auto& edge){return changedLayouts.count(edge.second)!=0;});
                if(!changed)continue;
                for(bool rectangular:{false,true})
                {
                    const int id=rectangular?r.rectMask:r.smoothMask;
                    if(!id || !refreshed.insert(id).second)continue;
                    Guard([&]{
                        auto* mask=Resolve(id);auto* object=Resolve(r.object);
                        if(!mask || mask!=(rectangular?r.rectMaskToken:r.smoothMaskToken) || !object || api.owner(mask,nullptr)!=object ||
                            api.transform(object,nullptr)!=Resolve(r.transform) || !Active(r.transform))return;
                        if(rectangular)api.rectClip(mask,nullptr);
                        else {api.maskDirty(mask,nullptr);api.maskRebuild(mask,nullptr);}
                    });
                }
            }
            changedLayouts.clear();
        }
        void Restore(Record& record)
        {
            if(!record.alphaChanged && !record.rendererChanged && !record.layout.changed)return;
            auto* component=Resolve(record.id);auto* object=Resolve(record.object);auto* transform=Resolve(record.transform);
            // A pooled visual may have left its old UI branch. Release only our
            // saved property on the exact same typed component, never a new ID owner.
            if(!component || component!=record.token || !object || !transform || api.owner(component,nullptr)!=object ||
                api.transform(object,nullptr)!=transform ||
                (!record.renderer && Resolve(record.graphic)!=record.graphicToken))return;
            if(record.alphaChanged)
            {Color color{};api.color(component,&color,nullptr);if(Near(color.a,0,.00001f))api.setAlpha(component,record.alpha,nullptr);record.alphaChanged=false;}
            if(record.rendererChanged)
            {if(!api.rendererEnabled(component,nullptr))api.setRendererEnabled(component,record.enabled,nullptr);record.rendererChanged=false;}
            if(record.layout.changed && transform)
            {
                V3 scale{};api.scale(transform,&scale,nullptr);
                if(Same(scale,record.layout.appliedScale))api.setScale(transform,&record.layout.scale,nullptr);
                V2 position{};V3 local{};
                if(record.rect)api.position(transform,&position,nullptr);else {api.local(transform,&local,nullptr);position={local.x,local.y};}
                if(Near(position.x,record.layout.appliedPosition.x))position.x=record.layout.position.x;
                if(Near(position.y,record.layout.appliedPosition.y))position.y=record.layout.position.y;
                if(record.rect)api.setPosition(transform,&position,nullptr);
                else {local.x=position.x;local.y=position.y;api.setLocal(transform,&local,nullptr);}
                changedLayouts.insert(record.transform);
                record.layout={};
            }
        }
        void RestoreAll(){for(auto& pair:records)Guard([&]{Restore(pair.second);});RefreshMasks();}
        void RestoreLayout(Record& record)
        {auto layoutOnly=record;layoutOnly.alphaChanged=layoutOnly.rendererChanged=false;Restore(layoutOnly);record.layout=layoutOnly.layout;}
        bool Demand()
        {
            if(!settings)return false;
            if(settings->scale!=1 || settings->alignRightButtons || settings->marginLeft || settings->marginRight || settings->marginTop || settings->marginBottom || !settings->hiddenPaths.empty())return true;
            for(bool hide:settings->hiddenPresets)if(hide)return true;return false;
        }
        bool LayoutDemand(){return settings && (settings->scale!=1 || settings->alignRightButtons || settings->marginLeft || settings->marginRight || settings->marginTop || settings->marginBottom);}
        bool RegionNeeded(const Region& region)
        {
            if(settings->scale!=1)return true;
            if(settings->alignRightButtons && (region.preset==UI::TopRightButtons ||
                (Edges(region.preset)==(Right|Top) && region.preset!=UI::Ping)))return true;
            if((settings->marginLeft && (region.edges&Left)) || (settings->marginRight && (region.edges&Right)) ||
                (settings->marginTop && (region.edges&Top)) || (settings->marginBottom && (region.edges&Bottom)))return true;
            if(effectiveMask&(uint32_t{1}<<region.preset))return true;
            const auto absolute=std::string(UI::MainPagePath)+"/"+region.path;
            for(const auto& path:settings->hiddenPaths)if(Within(path,absolute) || Within(absolute,path))return true;
            return false;
        }
        bool LayoutNeeded(const Match& match)
        {
            return settings->scale!=1 || (settings->alignRightButtons && match.topRight);
        }
        bool MarginNeeded(unsigned edges)
        {return (settings->marginLeft && (edges&Left)) || (settings->marginRight && (edges&Right)) ||
            (settings->marginTop && (edges&Top)) || (settings->marginBottom && (edges&Bottom));}
        bool LayoutNeeded(const Record& record)
        {return record.layoutRoot && ((record.marginRoot && MarginNeeded(record.match.edges)) ||
            (record.scaleRoot && settings->scale!=1) || (record.match.topRight && settings->alignRightButtons));}
        void ResetCache()
        {
            RestoreAll();layoutIds.clear();layoutPaths.clear();records.clear();nodes.clear();covered.clear();attempted.clear();unitBindings.clear();
            regions.clear();boundFields.clear();visibilityDirty.clear();layoutDirty.clear();added.clear();ClearFrame();
        }
        bool DiscoverRoot(int captured,bool& replaced)
        {
            if(!captured && rootId && Resolve(rootId)==rootToken && Resolve(pageId)==pageToken)return true;
            auto* page=captured?Resolve(captured):nullptr;
            auto* root=page?api.owner(page,nullptr):Find(UI::MainPagePath);
            if(root && (Name(root)!="InLevelMainPage" || !(page=Component(root,"MonoInLevelMainPage"))))return false;
            const int id=Id(root);
            if(root!=rootToken || id!=rootId || page!=pageToken)
            {
                ResetCache();rootId=id;rootToken=root;pageToken=page;
                rootTransformId=root?Id(api.transform(root,nullptr)):0;pageId=Id(page);replaced=true;
            }
            return rootId && rootTransformId && pageId;
        }
        bool KnownRegionSource(int id,void* token,UI::Preset preset)
        {return std::any_of(regions.begin(),regions.end(),[&](const Region& r){return r.source==id && r.sourceToken==token && r.preset==preset;});}
        bool RegisterTypedRegion(void* component,UI::Preset preset,const char* hintType=nullptr)
        {
            if(!component)return false;
            const int source=Id(component);
            if(KnownRegionSource(source,component,preset))return true;
            const char* type=hintType?hintType:(preset==UI::QuestTracking?"MonoQuestHintText":"MonoInteePanel");
            auto* object=component?api.owner(component,nullptr):nullptr;
            if(!object || Component(object,type)!=component)return false;
            auto* transform=api.transform(object,nullptr);if(!transform)return false;
            Region region;region.preset=preset;region.edges=Edges(preset);region.source=source;region.sourceToken=component;
            region.path=Path(transform,Resolve(rootTransformId));
            if(region.path.empty())
            {
                // Some HUD hints are mounted on another page. Only this exact
                // typed component under the UI canvas may become an extra scope.
                auto* canvas=Find("/Canvas");auto* canvasTransform=canvas?api.transform(canvas,nullptr):nullptr;
                if(!canvasTransform || Path(transform,canvasTransform).empty())return false;
                region.object=Id(object);region.transform=Id(transform);region.token=object;
                region.path=std::string("GrpMainPage/@")+type+"/"+std::to_string(region.object);
            }
            if(!ValidRegion(region.path,preset))return false;
            const auto existing=std::find_if(regions.begin(),regions.end(),[&](const Region& r){return r.path==region.path && r.preset==preset && r.token==region.token;});
            if(existing!=regions.end())
            {existing->source=source;existing->sourceToken=component;return true;}
            else
            {
                if(regions.size()>=256)return false;
                regions.push_back(std::move(region));
                for(auto& pair:nodes)pair.second.match=Classify(pair.second.path,regions);
                for(auto& pair:records)if(!pair.second.layoutRoot)
                {auto& r=pair.second;r.match=Classify(r.path,regions);added.insert(r.id);}
            }
            return true;
        }
        void LoadRegions(bool all=false)
        {
            if(regions.empty())regions=FixedRegions();
            if(boundFields.empty())boundFields.resize(Bindings().size());
            auto* rootTransform=Resolve(rootTransformId);auto* page=Resolve(pageId);bool changed=false;
            if(page)for(size_t i=0;i<Bindings().size();++i)
            {
                const auto& binding=Bindings()[i];
                const bool typed=binding.preset==UI::QuestTracking || binding.preset==UI::Interaction;
                if((boundFields[i] && !typed) || (!all && !RegionNeeded({"",binding.preset,Edges(binding.preset)}) && settings->hiddenPaths.empty()))continue;
                auto* value=*reinterpret_cast<void**>(static_cast<unsigned char*>(page)+binding.offset);
                if(!value)continue;
                if(typed){boundFields[i]=RegisterTypedRegion(value,binding.preset,binding.type);continue;}
                auto* object=value && !binding.gameObject?api.owner(value,nullptr):value;if(!object)continue;
                auto* transform=api.transform(object,nullptr);
                const auto base=Path(transform,rootTransform);if(base.empty() || Forbidden(base))continue;
                if(binding.child){transform=Relative(transform,binding.child);if(!transform)continue;}
                const auto path=Path(transform,rootTransform);if(!ValidRegion(path,binding.preset))continue;
                boundFields[i]=true;
                if(std::none_of(regions.begin(),regions.end(),[&](const Region& r){return r.path==path && r.preset==binding.preset;}))
                {regions.push_back({path,binding.preset,Edges(binding.preset)});changed=true;}
            }
            if(changed)
            {
                for(auto& pair:nodes)pair.second.match=Classify(pair.second.path,regions);
                for(auto& pair:records)
                {
                    auto& r=pair.second;if(r.layoutRoot)continue;auto match=Classify(r.path,regions);
                    if(match.presets!=r.match.presets || match.edges!=r.match.edges || match.unit!=r.match.unit)added.insert(r.id);
                    r.match=std::move(match);
                }
            }
        }
        bool Covered(const std::string& path)
        {return std::any_of(covered.begin(),covered.end(),[&](const std::string& parent){return Within(path,parent);});}
        void DiscoverRegion(const Region& region,bool refresh=false,bool descendants=true,void* knownTransform=nullptr)
        {
            const Region* scope=nullptr;
            for(const auto& candidate:regions)if(candidate.transform && Within(region.path,candidate.path)){scope=&candidate;break;}
            const int anchorId=scope?scope->object:rootId,anchorTransformId=scope?scope->transform:rootTransformId;
            const std::string scopePath=scope?scope->path:std::string{};
            if(scope && Resolve(scope->object)!=scope->token)return;
            auto* anchorTransform=Resolve(anchorTransformId);
            const auto relative=scope?(region.path==scopePath?std::string{}:region.path.substr(scopePath.size()+1)):region.path;
            auto* transform=knownTransform?knownTransform:(relative.empty()?anchorTransform:Relative(anchorTransform,relative));
            if(!refresh)attempted.insert(region.path);
            if(!transform)return;
            struct Item{void* transform;std::string path;unsigned depth;};
            std::vector<Item> work{{transform,region.path,0}};std::unordered_set<int> visited;
            while(!work.empty() && nodes.size()<8192 && records.size()<4096)
            {
                auto item=std::move(work.back());work.pop_back();const int transformId=Id(item.transform);
                if(!transformId || !visited.insert(transformId).second || (!refresh && Covered(item.path)))continue;
                auto* object=api.owner(item.transform,nullptr);if(!object)continue;
                Node node;node.transform=transformId;node.object=Id(object);node.path=item.path;node.forbidden=Forbidden(item.path);
                node.match=Classify(item.path,regions);
                int current=transformId;
                for(unsigned depth=0;current && current!=anchorTransformId && depth<48;++depth)
                {const int parent=Parent(current);node.chain.push_back({current,parent});current=parent;}
                if(current!=anchorTransformId)continue;
                nodes[transformId]=std::move(node);const auto& cached=nodes.at(transformId);
                if(cached.forbidden || !cached.match.presets)continue;
                auto* graphic=Component(object,"Graphic");auto* canvas=graphic?Component(object,"CanvasRenderer"):nullptr;
                nodes[transformId].graphic=Id(graphic);nodes[transformId].graphicToken=graphic;
                auto* smoothMask=Component(object,"SmoothMask");
                auto* rectMask=Component(object,"RectMask2D");
                bool expected=false;if(graphic)for(const char* type:{"Image","RawImage","Text","SimpleText"})if(Component(object,type)){expected=true;break;}
                struct Drawing {void* component;const char* kind;bool renderer=false,layoutOnly=false;};
                std::vector<Drawing> drawings;
                if(expected && canvas)drawings.push_back({canvas,"CanvasRenderer"});
                // Every approved HUD region owns its descendant drawing
                // components. Renderer covers particles, meshes, trails and
                // lines without region-specific names or MonoUIEffect checks.
                if(auto* renderer=Component(object,"Renderer"))
                {
                    drawings.push_back({renderer,"Renderer",true});
                    // Preserve multiple drawing components on the same object.
                    for(const char* type:{"ParticleSystemRenderer","MeshRenderer","SkinnedMeshRenderer","TrailRenderer","LineRenderer","SpriteRenderer","BillboardRenderer","GPUParticleSystemRenderer"})if(auto* extra=Component(object,type))
                        if(std::none_of(drawings.begin(),drawings.end(),[&](const Drawing& d){return d.component==extra;}))
                            drawings.push_back({extra,type,true});
                }
                else if(auto* particles=Component(object,"ParticleSystemRenderer"))
                    drawings.push_back({particles,"ParticleSystemRenderer",true});
                // Non-drawing masks are cached for parameter refresh only.
                if(drawings.empty() && smoothMask)drawings.push_back({smoothMask,"SmoothMask",false,true});
                if(drawings.empty() && rectMask)drawings.push_back({rectMask,"RectMask2D",false,true});
                for(const auto& drawing:drawings)
                {
                    auto* component=drawing.component;
                    const int id=Id(component);auto old=records.find(id);
                    if(old==records.end() || old->second.token!=component || old->second.object!=cached.object || old->second.path!=item.path ||
                        old->second.chain!=cached.chain || old->second.graphicToken!=graphic)
                    {
                        if(old!=records.end())Restore(old->second);
                        Record r;r.id=id;r.object=cached.object;r.transform=transformId;r.graphic=Id(graphic);r.graphicToken=graphic;
                        r.anchor=anchorId;r.anchorTransform=anchorTransformId;r.path=item.path;r.scopePath=scopePath;r.kind=drawing.kind;r.match=cached.match;
                        r.renderer=drawing.renderer;r.token=component;r.chain=cached.chain;r.layoutOnly=drawing.layoutOnly;
                        r.smoothMask=Id(smoothMask);r.smoothMaskToken=smoothMask;
                        r.rectMask=Id(rectMask);r.rectMaskToken=rectMask;
                        records[id]=std::move(r);added.insert(id);
                    }
                }
                // Keep each complete row together. The enclosing container is
                // animated independently, so it must retain its native position.
                const std::string teamContent="GrpMainPage/TeamBtnContainer/Content";
                if(Within(item.path,teamContent) && item.path.size()>teamContent.size() &&
                    item.path.find('/',teamContent.size()+1)==std::string::npos && !records.count(transformId) && Component(object,"RectTransform"))
                {
                    Record r;r.id=transformId;r.object=cached.object;r.transform=transformId;r.token=item.transform;
                    r.anchor=rootId;r.anchorTransform=rootTransformId;r.path=item.path;r.kind="HUDLayout";
                    r.match=cached.match;r.match.unit=item.path;r.chain=cached.chain;r.layoutOnly=r.layoutRoot=r.marginRoot=r.rect=r.rectKnown=true;
                    records.emplace(transformId,std::move(r));added.insert(transformId);layoutIds.push_back(transformId);layoutPaths.emplace(item.path,transformId);
                }
                if(!descendants)continue;
                const int count=api.count(item.transform,nullptr);nodes[transformId].children=count;
                if(item.depth>=32 || count<0 || count>1024)continue;
                auto& childIds=nodes[transformId].childIds;childIds.reserve(static_cast<size_t>(count));
                for(int i=0;i<count;++i){auto* child=api.child(item.transform,i,nullptr);childIds.push_back(Id(child));if(child)work.push_back({child,item.path+"/"+Name(child),item.depth+1});}
            }
            if(!refresh && descendants && work.empty())covered.insert(region.path);
        }
        void EnsureRegions()
        {
            LoadRegions();
            for(const auto& region:regions)
            {
                if(!RegionNeeded(region) || Covered(region.path) || attempted.count(region.path))continue;
                bool nested=false;
                for(const auto& other:regions)if(other.path!=region.path && RegionNeeded(other) && Within(region.path,other.path)){nested=true;break;}
                if(!nested)DiscoverRegion(region);
            }
        }
        void RegisterVisualSources(const std::vector<VisualEvent>& events)
        {
            if(regions.empty())regions=FixedRegions();
            for(const auto& event:events)if(event.preset!=UI::PresetCount && Resolve(event.id)==event.token)
                RegisterTypedRegion(event.token,event.preset,event.type);
        }
        void DiscoverVisuals(const std::vector<VisualEvent>& events)
        {
            if(events.empty())return;
            for(auto it=records.begin();it!=records.end();)
                if(Resolve(it->first)!=it->second.token)it=records.erase(it);else ++it;
            for(auto it=nodes.begin();it!=nodes.end();)
                if(!Resolve(it->first))it=nodes.erase(it);else ++it;
            layoutIds.erase(std::remove_if(layoutIds.begin(),layoutIds.end(),[](int id){return !records.count(id);}),layoutIds.end());
            for(auto it=layoutPaths.begin();it!=layoutPaths.end();)
                if(!records.count(it->second))it=layoutPaths.erase(it);else ++it;
            for(auto it=unitBindings.begin();it!=unitBindings.end();)
                if(Resolve(it->second.transform)!=it->second.token)it=unitBindings.erase(it);else ++it;
            regions.erase(std::remove_if(regions.begin(),regions.end(),[](const Region& r){
                return (r.source && Resolve(r.source)!=r.sourceToken) || (r.transform && Resolve(r.object)!=r.token);
            }),regions.end());
            struct Request {std::string path;int transform;bool subtree;};
            std::vector<Request> requests;
            std::unordered_map<int,std::string> eventPaths;
            std::unordered_set<int> outside;
            eventPaths.emplace(rootTransformId,"");
            for(const auto& scope:regions)if(scope.transform && Resolve(scope.object)==scope.token)
                eventPaths.emplace(scope.transform,scope.path);
            auto eventPath=[&](auto&& self,int transform,unsigned depth)->bool {
                if(eventPaths.count(transform))return true;
                if(!transform || depth>=48 || outside.count(transform))return false;
                const int parent=Parent(transform);
                if(!self(self,parent,depth+1)){outside.insert(transform);return false;}
                auto* value=Resolve(transform);if(!value){outside.insert(transform);return false;}
                const auto& prefix=eventPaths.at(parent);
                eventPaths.emplace(transform,prefix+(prefix.empty()?"":"/")+Name(value));return true;
            };
            auto contains=[](int ancestor,int descendant){
                for(unsigned depth=0;descendant && depth<48;++depth)
                {if(descendant==ancestor)return true;descendant=Parent(descendant);}return false;
            };
            for(const auto& event:events)
            {
                auto* component=Resolve(event.id);if(!component || component!=event.token)continue;
                auto* object=event.gameObject?component:api.owner(component,nullptr);
                auto* transform=object?api.transform(object,nullptr):nullptr;if(!transform)continue;
                const int transformId=Id(transform),objectId=Id(object);
                if(const auto cached=nodes.find(transformId);cached!=nodes.end() && cached->second.object==objectId)
                {
                    const auto& node=cached->second;
                    const bool sameChain=std::all_of(node.chain.begin(),node.chain.end(),[](const auto& edge){return Resolve(edge.first) && Parent(edge.first)==edge.second;});
                    if(sameChain && !event.subtree && node.graphic==event.id && node.graphicToken==component)continue;
                    if(sameChain && event.subtree && !event.gameObject && node.children>=0 && static_cast<size_t>(node.children)==node.childIds.size() &&
                        api.count(transform,nullptr)==node.children)
                    {
                        bool sameChildren=true;
                        for(int i=0;i<node.children;++i)if(Id(api.child(transform,i,nullptr))!=node.childIds[static_cast<size_t>(i)]){sameChildren=false;break;}
                        if(sameChildren)continue;
                    }
                }
                // A new prefab root may itself be a typed HUD hint. Check only
                // this root, never every Graphic or its chain of ancestors.
                if(event.subtree && event.preset==UI::PresetCount && (effectiveMask&(uint32_t{1}<<UI::QuestTracking)))
                    for(const char* type:QuestHintTypes)if(auto* hint=Component(object,type))
                    {
                        if(RegisterTypedRegion(hint,UI::QuestTracking,type))
                            for(const auto& scope:regions)if(scope.transform==transformId)
                            {eventPaths[transformId]=scope.path;outside.erase(transformId);break;}
                        break;
                    }
                if(!eventPath(eventPath,transformId,0))continue;
                const auto& path=eventPaths.at(transformId);
                if(Forbidden(path) || !Classify(path,regions).presets)continue;
                if(std::none_of(regions.begin(),regions.end(),[&](const Region& r){return Within(path,r.path) && RegionNeeded(r);}))continue;
                // Unity permits siblings with identical names. Coalesce actual
                // transform ancestry so one clone never suppresses another.
                if(std::any_of(requests.begin(),requests.end(),[&](const Request& r){return (r.subtree && contains(r.transform,transformId)) || (r.transform==transformId && r.subtree==event.subtree);}))continue;
                if(event.subtree)requests.erase(std::remove_if(requests.begin(),requests.end(),[&](const Request& r){return contains(transformId,r.transform);}),requests.end());
                requests.push_back({path,transformId,event.subtree});
            }
            // Known nodes are reused. A new Graphic caches just its own node;
            // explicit prefab/effect/row events may fill a changed small subtree.
            for(const auto& request:requests)
                DiscoverRegion({request.path,UI::AllMainUi},true,request.subtree,Resolve(request.transform));
        }
        std::string MarginPath(const Region& region)
        {
            for(const char* path:{"GrpMainPage/MapInfo","GrpMainPage/ActionPanelContainer",
                "GrpMainPage/ChatArea/Chat/Content","GrpMainPage/GrpMainBtn/GrpMainToggle/GrpTopPanel","GrpMainPage/GameInfo/Layout"})
                if(Within(region.path,path))return path;
            return region.path;
        }
        Record* CacheLayoutRoot(const std::string& path,const Match& match,bool margin,bool scale)
        {
            if(Forbidden(path) || path=="GrpMainPage")return nullptr;
            if(const auto cached=layoutPaths.find(path);cached!=layoutPaths.end())
            {
                auto& r=records.at(cached->second);r.match.presets|=match.presets;r.match.edges|=match.edges;
                r.match.topRight=r.match.topRight || match.topRight;r.marginRoot=r.marginRoot || margin;r.scaleRoot=r.scaleRoot || scale;return &r;
            }
            auto* transform=Relative(Resolve(rootTransformId),path);if(!transform)return nullptr;
            const int id=Id(transform);if(records.count(id))return nullptr;
            auto* object=api.owner(transform,nullptr);if(!object || !Component(object,"RectTransform"))return nullptr;
            Record r;r.id=id;r.object=Id(object);r.transform=id;r.token=transform;r.path=path;r.kind="HUDLayout";
            r.anchor=rootId;r.anchorTransform=rootTransformId;r.layoutOnly=r.layoutRoot=r.rect=r.rectKnown=true;
            r.marginRoot=margin;r.scaleRoot=scale;r.match=match;r.match.unit=path;
            int current=id;
            for(unsigned depth=0;current && current!=rootTransformId && depth<48;++depth)
            {const int parent=Parent(current);r.chain.push_back({current,parent});current=parent;}
            if(current!=rootTransformId)return nullptr;
            r.smoothMaskToken=Component(object,"SmoothMask");r.smoothMask=Id(r.smoothMaskToken);
            r.rectMaskToken=Component(object,"RectMask2D");r.rectMask=Id(r.rectMaskToken);
            auto inserted=records.emplace(id,std::move(r));added.insert(id);layoutIds.push_back(id);layoutPaths.emplace(path,id);return &inserted.first->second;
        }
        void EnsureLayoutRoots()
        {
            for(int id:layoutIds){auto& r=records.at(id);r.scaleRoot=false;r.match.topRight=false;}
            for(const auto& region:regions)
            {
                if(region.transform || region.preset==UI::Team || !MarginNeeded(region.edges))continue;
                Match match;match.presets=uint32_t{1}<<region.preset;match.edges=region.edges;
                CacheLayoutRoot(MarginPath(region),match,true,false);
            }
            if(settings->scale==1 && !settings->alignRightButtons)return;
            std::unordered_map<std::string,Match> requests;
            for(const auto& pair:records)
            {
                const auto& r=pair.second;if(r.layoutOnly || !r.scopePath.empty() || !LayoutNeeded(r.match) || r.match.unit.empty())continue;
                const auto unit=Within(r.match.unit,"GrpMainPage/ChatArea/Chat/Content")?std::string("GrpMainPage/ChatArea/Chat/Content"):r.match.unit;
                auto& match=requests[unit];match.presets|=r.match.presets;match.edges|=r.match.edges;match.topRight=match.topRight || r.match.topRight;
            }
            // Units containing other independent units are grouping containers.
            // Their children own scaling so no subtree receives the factor twice.
            for(const auto& request:requests)
            {
                const bool leaf=std::none_of(requests.begin(),requests.end(),[&](const auto& other){return other.first!=request.first && Within(other.first,request.first);});
                if(leaf || (request.second.topRight && settings->alignRightButtons))CacheLayoutRoot(request.first,request.second,false,leaf);
            }
        }
        void SelectLayoutTargets()
        {
            for(auto& pair:records)
            {
                auto& r=pair.second;const bool scale=r.layoutRoot && r.scaleRoot && settings->scale!=1;
                if(r.applyScale!=scale){r.applyScale=scale;layoutDirty.insert(r.id);}
                const bool target=LayoutNeeded(r);
                if(target!=r.layoutTarget)layoutDirty.insert(r.id);
                if(target && !r.rectKnown && Identity(r))
                {r.rect=Component(Resolve(r.object),"RectTransform")!=nullptr;r.rectKnown=true;}
                r.layoutTarget=target;
            }
        }

        void ObserveLayouts()
        {
            for(auto& pair:records)
            {
                auto& r=pair.second;if(!layoutDirty.count(r.id) || !r.layoutTarget || !Identity(r))continue;auto* transform=Resolve(r.transform);
                if(!r.layoutRoot && !Active(r.transform)){RestoreLayout(r);continue;}
                V3 scale{},local{};V2 position{};api.scale(transform,&scale,nullptr);api.local(transform,&local,nullptr);
                if(r.rect)api.position(transform,&position,nullptr);else position={local.x,local.y};
                Observe(r.layout,scale,position);r.currentPosition=position;r.local=local;layoutByTransform[r.transform]=&r;
            }
        }
        Matrix MatrixOf(int transform,unsigned depth=0)
        {
            if(!transform || transform==rootTransformId)return {};
            if(depth>48){invalidMatrices.insert(transform);return {};}
            auto cached=matrices.find(transform);if(cached!=matrices.end())return cached->second;
            auto* t=Resolve(transform);if(!t){invalidMatrices.insert(transform);return {};}
            V3 scale{},local{};Rotation rotation{};api.scale(t,&scale,nullptr);api.local(t,&local,nullptr);api.rotation(t,&rotation,nullptr);
            if(auto it=layoutByTransform.find(transform);it!=layoutByTransform.end())
            {auto& r=*it->second;scale=r.layout.scale;local.x+=r.layout.position.x-r.currentPosition.x;local.y+=r.layout.position.y-r.currentPosition.y;}
            if(std::abs(rotation.x)+std::abs(rotation.y)>.0001f || !std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(scale.x) || !std::isfinite(scale.y))invalidMatrices.insert(transform);
            const int parent=Parent(transform);const auto result=Product(MatrixOf(parent,depth+1),Transform({local.x,local.y},scale,rotation));
            if(invalidMatrices.count(parent))invalidMatrices.insert(transform);matrices[transform]=result;return result;
        }
        struct Unit {int transform=0;V2 center{};bool active=false,visible=false,topRight=false;float alignment=0;};
        std::unordered_map<std::string,Unit> Units()
        {
            std::unordered_map<std::string,Unit> result;
            for(const auto& pair:records)
            {
                const auto& r=pair.second;if(!r.layoutTarget || !layoutDirty.count(r.id))continue;
                auto& unit=result[r.match.unit];unit.topRight=r.match.topRight;if(unit.transform)continue;
                auto it=unitBindings.find(r.match.unit);
                if(it==unitBindings.end())
                {
                    UnitBinding binding;auto* t=Relative(Resolve(rootTransformId),r.match.unit);
                    if(t){binding.transform=Id(t);binding.token=t;binding.rect=Component(api.owner(t,nullptr),"RectTransform")!=nullptr;binding.buttonToken=Relative(t,"Button");binding.button=Id(binding.buttonToken);}
                    it=unitBindings.emplace(r.match.unit,binding).first;
                }
                const auto& binding=it->second;auto* t=Resolve(binding.transform);
                if(!t || t!=binding.token || Path(t,Resolve(rootTransformId))!=r.match.unit)continue;
                unit.transform=binding.transform;
                unit.center=Point(MatrixOf(unit.transform),{});
                unit.active=Active(unit.transform) && (!binding.button || (Resolve(binding.button)==binding.buttonToken && Active(binding.button)));
            }
            for(const auto& pair:records)
            {const auto& r=pair.second;auto it=result.find(r.match.unit);if(!r.layoutOnly && it!=result.end() && !it->second.visible && !HiddenNow(r))it->second.visible=Active(r.transform);}
            if(settings->alignRightButtons)
            {
                std::vector<Unit*> slots;for(auto& pair:result)if(pair.second.topRight && pair.second.active)slots.push_back(&pair.second);
                std::sort(slots.begin(),slots.end(),[](const Unit* a,const Unit* b){return a->center.x<b->center.x;});
                const auto visible=static_cast<size_t>(std::count_if(slots.begin(),slots.end(),[](const Unit* unit){return unit->visible;}));
                size_t next=slots.size()-visible;for(auto* unit:slots)if(unit->visible)unit->alignment=slots[next++]->center.x-unit->center.x;
            }
            return result;
        }

        void ApplyLayouts()
        {
            ObserveLayouts();auto units=Units();Rect rootRect{};api.rect(Resolve(rootTransformId),&rootRect,nullptr);
            const bool horizontal=rootRect.width-settings->marginLeft-settings->marginRight>1;
            const bool vertical=rootRect.height-settings->marginTop-settings->marginBottom>1;
            for(auto& pair:records)
            {
                auto& r=pair.second;if(!layoutDirty.count(r.id) || !r.layoutTarget || !Identity(r))continue;auto* transform=Resolve(r.transform);
                if(!r.layoutRoot && !Active(r.transform))continue;
                const int parent=Parent(r.transform);const auto parentMatrix=MatrixOf(parent);if(invalidMatrices.count(parent))continue;
                const auto unit=units.find(r.match.unit);if(unit==units.end() || !unit->second.transform)continue;
                V2 shift{unit->second.alignment,0};
                if(horizontal && r.marginRoot)
                {
                    if(r.match.edges&Left)shift.x+=settings->marginLeft;
                    if(r.match.edges&Right)shift.x-=settings->marginRight;
                }
                if(vertical && r.marginRoot){if(r.match.edges&Top)shift.y-=settings->marginTop;if(r.match.edges&Bottom)shift.y+=settings->marginBottom;}
                r.canvasShift=shift;r.factor=r.applyScale?settings->scale:1.f;
                const V3 desiredScale{r.layout.scale.x*r.factor,r.layout.scale.y*r.factor,r.layout.scale.z};V2 delta{};
                // Scale at the native pivot. Large layout rectangles and empty
                // placeholder slots must not introduce a centering translation.
                if(!InverseVector(parentMatrix,shift,delta))continue;
                const auto desiredPosition=Add(r.layout.position,delta);
                if(!Identity(r,true))continue;
                V3 currentScale{};api.scale(transform,&currentScale,nullptr);
                if(!Same(currentScale,desiredScale)){api.setScale(transform,&desiredScale,nullptr);r.layout.changed=true;changedLayouts.insert(r.transform);}
                if(!Same(r.currentPosition,desiredPosition))
                {
                    if(r.rect)api.setPosition(transform,&desiredPosition,nullptr);
                    else {V3 localPosition=r.local;localPosition.x=desiredPosition.x;localPosition.y=desiredPosition.y;api.setLocal(transform,&localPosition,nullptr);}
                    r.layout.changed=true;changedLayouts.insert(r.transform);
                }
                api.scale(transform,&r.layout.appliedScale,nullptr);
                if(r.rect)api.position(transform,&r.layout.appliedPosition,nullptr);
                else {V3 current{};api.local(transform,&current,nullptr);r.layout.appliedPosition={current.x,current.y};}
                r.layout.applied=true;
            }
        }
        void ApplyVisibility(bool maintain=false)
        {
            for(auto& pair:records)
            {
                auto& r=pair.second;if(r.layoutOnly || (!maintain && !visibilityDirty.count(r.id)))continue;
                if(!HiddenNow(r) && !r.alphaChanged && !r.rendererChanged)continue;
                // Live component tokens and the cached parent chain prove the
                // region membership; toggles do not need to resolve names again.
                Guard([&]{
                if(!Identity(r)){Restore(r);return;}
                const bool hidden=HiddenNow(r);if(hidden && !Active(r.transform))return;
                auto* component=Resolve(r.id);
                if(r.renderer)
                {
                    const bool current=api.rendererEnabled(component,nullptr);
                    if(hidden && current){r.enabled=current;r.rendererChanged=true;api.setRendererEnabled(component,false,nullptr);}
                    else if(!hidden && r.rendererChanged){if(!current)api.setRendererEnabled(component,r.enabled,nullptr);r.rendererChanged=false;}
                }
                else
                {
                    Color color{};api.color(component,&color,nullptr);if(!std::isfinite(color.a))return;
                    if(hidden && !Near(color.a,0,.00001f))
                    {r.alpha=color.a;r.alphaChanged=true;api.setAlpha(component,0.f,nullptr);}
                    else if(!hidden && r.alphaChanged)
                    {if(Near(color.a,0,.00001f))api.setAlpha(component,r.alpha,nullptr);r.alphaChanged=false;}
                }
                });
            }
        }
        void MaintainVisibility()
        {
            if(!settings || restored || faulted.load() || records.empty())return;
            if(!effectiveMask && settings->hiddenPaths.empty())return;
            if(api.resolve(rootId,nullptr)!=rootToken || api.resolve(pageId,nullptr)!=pageToken)return;
            ClearFrame();ApplyVisibility(true);
        }
        bool ParentLinear(const Record& r,std::unordered_map<int,Matrix>& cache,Matrix& result)
        {
            result={};
            for(auto edge=r.chain.rbegin();edge!=r.chain.rend();++edge)
            {
                if(!Resolve(edge->first) || Parent(edge->first)!=edge->second)return false;
                const int id=edge->second;if(id==rootTransformId)continue;
                if(const auto found=cache.find(id);found!=cache.end()){result=found->second;continue;}
                auto* transform=Resolve(id);if(!transform)return false;
                V3 scale{};Rotation rotation{};api.scale(transform,&scale,nullptr);api.rotation(transform,&rotation,nullptr);
                if(!std::isfinite(scale.x) || !std::isfinite(scale.y) || std::abs(rotation.x)+std::abs(rotation.y)>.0001f)return false;
                result=Product(result,Transform({},scale,rotation));cache.emplace(id,result);
            }
            return true;
        }
        void MaintainLayouts()
        {
            if(!settings || restored || faulted.load() || layoutIds.empty() || !LayoutDemand())return;
            if(api.resolve(rootId,nullptr)!=rootToken)return;
            ClearFrame();std::unordered_map<int,Matrix> linears;
            // One controller owns each unit. Native animations keep all child
            // transforms; only the unit's position and scale are composed here.
            for(int id:layoutIds)
            {
                const auto it=records.find(id);if(it==records.end())continue;
                auto& r=it->second;if(!r.layoutTarget || (!r.applyScale && Same(r.canvasShift,V2{})))continue;
                auto* transform=api.resolve(r.transform,nullptr);if(!transform || transform!=r.token)continue;
                if(r.chain.empty() || api.parent(transform,nullptr)!=api.resolve(r.chain.front().second,nullptr))continue;
                V2 current{};api.position(transform,&current,nullptr);V3 scale=r.layout.applied?r.layout.appliedScale:r.layout.scale;
                if(r.applyScale)api.scale(transform,&scale,nullptr);
                Observe(r.layout,scale,current);
                Matrix parent{};V2 translation{};
                if(!ParentLinear(r,linears,parent) || !InverseVector(parent,r.canvasShift,translation))continue;
                V3 desiredScale=scale;
                if(r.applyScale)
                {
                    desiredScale={r.layout.scale.x*r.factor,r.layout.scale.y*r.factor,r.layout.scale.z};
                }
                const auto desired=Add(r.layout.position,translation);
                if(r.applyScale && !Same(scale,desiredScale)){api.setScale(transform,&desiredScale,nullptr);r.layout.changed=true;changedLayouts.insert(r.transform);}
                if(!Same(current,desired)){api.setPosition(transform,&desired,nullptr);r.layout.changed=true;changedLayouts.insert(r.transform);}
                r.layout.appliedPosition=desired;if(r.applyScale)r.layout.appliedScale=desiredScale;r.layout.applied=true;
            }
            RefreshMasks();
        }
        bool Key(int key,bool& held)
        {DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);const bool down=key && pid==GetCurrentProcessId() && (GetAsyncKeyState(key)&0x8000);const bool pressed=down && !held;held=down;return pressed;}
        void Export()
        {
            std::ofstream file(Support::PluginDirectory()/L"ui-tree.txt",std::ios::binary|std::ios::trunc);if(!file)return;
            file<<"# MainUICustomizer: activeSelf<TAB>absolute UI path; HUD layout metadata; no text contents.\n";
            std::unordered_map<int,const Record*> byTransform;
            for(const auto& pair:records)if(pair.second.layoutRoot || !byTransform.count(pair.second.transform))byTransform[pair.second.transform]=&pair.second;
            struct Item{void* transform;std::string path,scopePath;unsigned depth;};
            auto* canvas=Find("/Canvas");auto* exportRoot=canvas?api.transform(canvas,nullptr):nullptr;
            std::vector<Item> work{{exportRoot?exportRoot:Resolve(rootTransformId),exportRoot?"/Canvas":UI::MainPagePath,"",0}};size_t count=0;
            while(!work.empty() && count++<20000)
            {
                auto item=std::move(work.back());work.pop_back();if(!item.transform)continue;
                const int transform=Id(item.transform);auto* object=api.owner(item.transform,nullptr);
                if(Within(item.path,UI::MainPagePath))item.scopePath=item.path==UI::MainPagePath?std::string{}:item.path.substr(std::char_traits<char>::length(UI::MainPagePath)+1);
                for(const auto& scope:regions)if(scope.transform==transform && Resolve(scope.object)==scope.token){item.scopePath=scope.path;break;}
                const auto match=Classify(item.scopePath,regions);
                file<<(api.active(object,nullptr)?1:0)<<'\t'<<item.path<<"\tapproved_region="<<(match.presets!=0);
                bool questHint=false;
                for(const char* type:QuestHintTypes)if(Component(object,type)){file<<"\tquest_component="<<type;questHint=true;}
                if(match.presets || questHint || byTransform.count(transform))
                {
                    file<<"\tid="<<transform<<"\tcomponents=";
                    for(const char* type:{"Image","RawImage","Text","SimpleText","SmoothMask","Mask","RectMask2D","Selectable","EventTrigger","ScrollRect",
                        "MonoActionBtn","MonoBaseSkillBtn","MonoTeamButtonContainer","Animator","ContentSizeFitter","HorizontalLayoutGroup","VerticalLayoutGroup","LayoutElement"})
                        if(Component(object,type))file<<type<<',';
                    if(!item.scopePath.empty())file<<"\tscope="<<item.scopePath;
                    V3 local{},scale{};api.local(item.transform,&local,nullptr);api.scale(item.transform,&scale,nullptr);
                    file<<"\tlocal="<<local.x<<','<<local.y<<','<<local.z<<"\tscale="<<scale.x<<','<<scale.y<<','<<scale.z;
                    if(Component(object,"RectTransform"))
                    {
                        V2 anchored{};Rect rect{};api.position(item.transform,&anchored,nullptr);api.rect(item.transform,&rect,nullptr);
                        file<<"\tanchored="<<anchored.x<<','<<anchored.y<<"\trect="<<rect.x<<','<<rect.y<<','<<rect.width<<','<<rect.height;
                    }
                    if(const auto it=byTransform.find(transform);it!=byTransform.end())
                    {
                        const auto& r=*it->second;
                        file<<"\tkind="<<r.kind<<"\ttarget="<<r.layoutTarget<<"\tchanged="<<r.layout.changed
                            <<"\tbaseline="<<r.layout.position.x<<','<<r.layout.position.y
                            <<"\tapplied="<<r.layout.appliedPosition.x<<','<<r.layout.appliedPosition.y
                            <<"\tscale_baseline="<<r.layout.scale.x<<','<<r.layout.scale.y<<','<<r.layout.scale.z
                            <<"\tscale_applied="<<r.layout.appliedScale.x<<','<<r.layout.appliedScale.y<<','<<r.layout.appliedScale.z;
                        if(r.layoutRoot)file<<"\tscale_unit="<<r.scaleRoot<<"\tmargin_region="<<r.marginRoot<<"\tfactor="<<r.factor;
                    }
                }
                file<<'\n';
                const int children=api.count(item.transform,nullptr);if(item.depth>=48 || children<0 || children>4096)continue;
                for(int i=children-1;i>=0;--i){auto* child=api.child(item.transform,i,nullptr);if(child){const auto name=Name(child);work.push_back({child,item.path+"/"+name,item.scopePath.empty()?std::string{}:item.scopePath+"/"+name,item.depth+1});}}
            }
            if(!work.empty())file<<"# truncated: UI tree exceeded the export limit.\n";
        }
        uint32_t HiddenMask(const UI::Settings& value)
        {uint32_t mask=0;const auto flags=UI::GetEffectiveHiddenPresets(value);for(unsigned i=0;i<UI::PresetCount;++i)if(flags[i])mask|=uint32_t{1}<<i;return mask;}
        bool SameUi(const UI::Settings& a,const UI::Settings& b)
        {
            return a.scale==b.scale && a.alignRightButtons==b.alignRightButtons &&
                a.marginLeft==b.marginLeft && a.marginRight==b.marginRight && a.marginTop==b.marginTop && a.marginBottom==b.marginBottom &&
                a.hiddenPresets==b.hiddenPresets && a.hiddenPaths==b.hiddenPaths;
        }
        bool PathsTouch(const Record& r,const UI::Settings& value)
        {
            for(const auto& p:value.hiddenPaths)
            {if(p==UI::MainPagePath)return true;if(Within(p,UI::MainPagePath) && Within(r.path,p.substr(std::char_traits<char>::length(UI::MainPagePath)+1)))return true;}
            return false;
        }
        void Update()
        {
            const auto next=std::atomic_load_explicit(&pending,std::memory_order_acquire);
            const int captured=pendingPage.exchange(0,std::memory_order_acq_rel);const auto command=commands.exchange(0,std::memory_order_acq_rel);
            std::vector<VisualEvent> events;
            if(haveVisualEvents.load(std::memory_order_acquire))
            {std::lock_guard<std::mutex> lock(visualMutex);events.swap(visualEvents);haveVisualEvents.store(false,std::memory_order_release);}
            if(next==settings && !captured && !command && events.empty())return;
            const auto previous=settings;settings=next;
            if(previous && SameUi(*previous,*settings) && !captured && !command && events.empty())return;
            ClearFrame();visibilityDirty.clear();layoutDirty.clear();added.clear();
            bool force=!previous || captured!=0;
            if(command&1){restored=!restored;force=true;}
            effectiveMask=HiddenMask(*settings);
            watchVisuals.store(Demand(),std::memory_order_release);
            bool replaced=false;
            if(captured && !DiscoverRoot(captured,replaced))return;
            if(!Demand() && events.empty() && !(command&2)){RestoreAll();return;}
            if(!captured && !DiscoverRoot(0,replaced))return;force=force || replaced;
            RegisterVisualSources(events);
            if(!Demand() && !(command&2)){RestoreAll();return;}
            if(command&2){LoadRegions(true);MaintainLayouts();Export();ResetCache();LoadRegions(true);force=true;}
            if(faulted.load()){RestoreAll();return;}
            EnsureRegions();DiscoverVisuals(events);EnsureLayoutRoots();
            if(restored){RestoreAll();return;}
            const UI::Settings defaults;const auto& old=previous?*previous:defaults;
            const auto hideChanged=HiddenMask(old)^effectiveMask;
            const bool pathsChanged=old.hiddenPaths!=settings->hiddenPaths;
            const bool scaleChanged=old.scale!=settings->scale,alignChanged=old.alignRightButtons!=settings->alignRightButtons;
            const unsigned edges=(old.marginLeft!=settings->marginLeft?Left:0)|(old.marginRight!=settings->marginRight?Right:0)|
                (old.marginTop!=settings->marginTop?Top:0)|(old.marginBottom!=settings->marginBottom?Bottom:0);
            bool topVisibility=false;
            for(const auto& pair:records)
            {
                const auto& r=pair.second;const bool pathDirty=pathsChanged && (PathsTouch(r,old) || PathsTouch(r,*settings));
                const bool visibility=force || added.count(r.id) || (r.match.presets&hideChanged) || pathDirty;
                if(visibility){visibilityDirty.insert(r.id);if(r.match.topRight)topVisibility=true;}
                if((force || added.count(r.id) || scaleChanged || (r.match.edges&edges) || (r.match.topRight && alignChanged)) &&
                    (LayoutNeeded(r) || r.layout.changed))layoutDirty.insert(r.id);
            }
            if(topVisibility && (old.alignRightButtons || settings->alignRightButtons))
                for(const auto& pair:records)if(pair.second.match.topRight)layoutDirty.insert(pair.first);
            if(!layoutDirty.empty())
            {
                SelectLayoutTargets();
                for(int id:layoutDirty){auto& r=records.at(id);if(!r.layoutTarget && r.layout.changed)RestoreLayout(r);}
                if(LayoutDemand())ApplyLayouts();
            }
            ApplyVisibility();RefreshMasks();
        }

        void __fastcall OnAwake(void* page,void* method)
        {
            originalAwake(page,method);
            if(!ready.load(std::memory_order_acquire))return;
            Guard([&]{pendingPage.store(Id(page),std::memory_order_release);});
        }
        bool CanQueueVisual(UI::Preset preset)
        {
            if(applying || !ready.load(std::memory_order_acquire) ||
                (preset==UI::PresetCount && !watchVisuals.load(std::memory_order_acquire)))return false;
            const DWORD thread=uiThread.load();return !thread || thread==GetCurrentThreadId();
        }
        bool QueueVisual(void* component,UI::Preset preset=UI::PresetCount,const char* type=nullptr,bool subtree=false,bool gameObject=false)
        {
            if(!component || !CanQueueVisual(preset))return false;
            bool queued=false;
            try{Guard([&]{
                const int id=Id(component);if(!id)return;
                std::lock_guard<std::mutex> lock(visualMutex);
                if(visualEvents.size()>=2048)return;
                visualEvents.push_back({id,component,preset,type,subtree,gameObject});haveVisualEvents.store(true,std::memory_order_release);queued=true;
            });}catch(...){}
            return queued;
        }
        void __fastcall OnGraphicEnable(void* graphic,void* method)
        {originalGraphicEnable(graphic,method);QueueVisual(graphic);}
        void __fastcall OnGraphicLightActivate(void* graphic,void* method)
        {originalGraphicLightActivate(graphic,method);QueueVisual(graphic);}
        bool QueueEffectInstance(void* effect)
        {
            if(!effect || !CanQueueVisual(UI::PresetCount))return false;
            bool queued=false;
            Guard([&]{
                // Both effect owners store the mounted GameObject at 0x20 in both 7.2 builds.
                auto* instance=*reinterpret_cast<void**>(static_cast<unsigned char*>(effect)+0x20);
                queued=QueueVisual(instance,UI::PresetCount,nullptr,true,true);
            });
            return queued;
        }
        void __fastcall OnEffectEnable(void* effect,void* method)
        {originalEffectEnable(effect,method);if(!QueueEffectInstance(effect))QueueVisual(effect,UI::PresetCount,nullptr,true);}
        void __fastcall OnEffectSetObject(void* effect,void* prefab,void* method)
        {originalEffectSetObject(effect,prefab,method);QueueEffectInstance(effect);}
        void __fastcall OnEffectPlay(void* effect,bool includeChildren,void* method)
        {originalEffectPlay(effect,includeChildren,method);QueueEffectInstance(effect);}
        void __fastcall OnFeatureEffectMount(void* effect,void* method)
        {originalFeatureEffectMount(effect,method);QueueEffectInstance(effect);}
        void __fastcall OnQuestEnable(void* quest,void* method)
        {originalQuestEnable(quest,method);QueueVisual(quest,UI::QuestTracking,"MonoQuestHintText",true);}
        void __fastcall OnQuestTraceTick(void* trace,void* method)
        {
            originalQuestTraceTick(trace,method);
            if(!trace || !ready.load(std::memory_order_acquire))return;
            const DWORD thread=uiThread.load();if(thread && thread!=GetCurrentThreadId())return;
            // Report only instances missing from the ordinary region registry.
            // No separate quest rendering state or cache is maintained here.
            Guard([&]{if(!KnownRegionSource(Id(trace),trace,UI::QuestTracking))
                QueueVisual(trace,UI::QuestTracking,"MonoQuestTrace",true);});
        }
        void* __fastcall OnDialogAdd(void* panel,int index,void* method)
        {auto* result=originalDialogAdd(panel,index,method);QueueVisual(panel,UI::Interaction,"MonoInteePanel",true);return result;}
        void* __fastcall OnPickupAdd(void* panel,int index,void* method)
        {auto* result=originalPickupAdd(panel,index,method);QueueVisual(panel,UI::Interaction,"MonoInteePanel",true);return result;}
        void QueueInteeKeys(void* panel)
        {
            if(!panel || !ready.load(std::memory_order_acquire))return;
            const DWORD thread=uiThread.load();if(thread && thread!=GetCurrentThreadId())return;
            Guard([&]{
                if(!KnownRegionSource(Id(panel),panel,UI::Interaction))QueueVisual(panel,UI::Interaction,"MonoInteePanel",true);
                if(!watchVisuals.load(std::memory_order_acquire))return;
                // MonoInteePanel owns the current-option and switch-key
                // instances. They can be cloned or reparented after row setup.
                for(size_t offset:{size_t{0x50},size_t{0x68}})
                    QueueVisual(*reinterpret_cast<void**>(static_cast<unsigned char*>(panel)+offset),UI::PresetCount,nullptr,true);
            });
        }
        void __fastcall OnInteeKeyRefresh(void* panel,void* method)
        {originalInteeKeyRefresh(panel,method);QueueInteeKeys(panel);}
        void __fastcall OnInteeSwitchKeyRefresh(void* panel,void* method)
        {originalInteeSwitchKeyRefresh(panel,method);QueueInteeKeys(panel);}
        void* __fastcall OnPrefabInstance(void* plugin,int index,void* parent,void* mount,void* method)
        {auto* result=originalPrefabInstance(plugin,index,parent,mount,method);QueueVisual(result,UI::PresetCount,nullptr,true);return result;}
        void __fastcall OnRender(void* method)
        {
            originalRender(method);if(!ready.load(std::memory_order_acquire) || applying)return;
            DWORD empty=0;uiThread.compare_exchange_strong(empty,GetCurrentThreadId());if(uiThread.load()!=GetCurrentThreadId())return;
            applying=true;bool ok=false;try{ok=Guard([]{Update();MaintainLayouts();MaintainVisibility();});}catch(...){}
            if(!ok){faulted.store(true);ready.store(false);try{RestoreAll();}catch(...){}}
            applying=false;
        }
        bool Initialize(const Native::Profile& profile,uintptr_t base)
        {
            bool compatible=false;
            Guard([&]{const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000)return;
                const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
                compatible=nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 &&
                    nt->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
                    profile.MatchesFingerprint(nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage);});
            if(!compatible)return false;bool valid=true;
            using Native::Entry;
            auto address=[&](Entry entry)->void*{
                const auto& method=profile.Get(entry);
                auto* p=reinterpret_cast<void*>(base+method.rva);
                const bool match=Support::MatchesEntryBytes(p,method.bytes);
                if(!match)valid=false;
                return match?p:nullptr;
            };
#define LOAD(field,method) api.field=reinterpret_cast<decltype(api.field)>(address(Entry::method))
            LOAD(stringNew,StringNew);LOAD(find,FindObject);LOAD(transform,ObjectTransform);LOAD(owner,ComponentObject);LOAD(parent,Parent);LOAD(name,Name);
            LOAD(id,InstanceId);LOAD(resolve,FromId);LOAD(count,ChildCount);LOAD(component,ComponentByName);LOAD(childPath,FindTransform);LOAD(child,Child);LOAD(active,ActiveSelf);
            LOAD(rendererEnabled,ReadRendererEnabled);LOAD(setRendererEnabled,SetRendererEnabled);LOAD(scale,ReadScale);LOAD(local,ReadLocalPosition);LOAD(setScale,SetScale);
            LOAD(position,ReadPosition);LOAD(setPosition,SetPosition);LOAD(rect,ReadRect);LOAD(rotation,ReadRotation);
            LOAD(maskDirty,MaskSetParamsDirty);LOAD(maskRebuild,MaskRebuild);
            LOAD(rectClip,RectMaskPerformClipping);
#undef LOAD
            api.color=reinterpret_cast<ColorGet>(address(Entry::ReadCanvasColor));api.setAlpha=reinterpret_cast<FloatSet>(address(Entry::SetCanvasAlpha));
            api.setLocal=reinterpret_cast<V3Set>(address(Entry::SetLocalPosition));auto* target=address(Entry::WillRender);auto* awake=address(Entry::MainAwake);
            struct Hook {void* target;void* callback;void** original;};
            const Hook hooks[]={
                {target,reinterpret_cast<void*>(&OnRender),reinterpret_cast<void**>(&originalRender)},
                {awake,reinterpret_cast<void*>(&OnAwake),reinterpret_cast<void**>(&originalAwake)},
                {address(Entry::GraphicEnable),reinterpret_cast<void*>(&OnGraphicEnable),reinterpret_cast<void**>(&originalGraphicEnable)},
                {address(Entry::GraphicLightActivate),reinterpret_cast<void*>(&OnGraphicLightActivate),reinterpret_cast<void**>(&originalGraphicLightActivate)},
                {address(Entry::EffectEnable),reinterpret_cast<void*>(&OnEffectEnable),reinterpret_cast<void**>(&originalEffectEnable)},
                {address(Entry::EffectSetObject),reinterpret_cast<void*>(&OnEffectSetObject),reinterpret_cast<void**>(&originalEffectSetObject)},
                {address(Entry::EffectPlay),reinterpret_cast<void*>(&OnEffectPlay),reinterpret_cast<void**>(&originalEffectPlay)},
                {address(Entry::FeatureEffectMount),reinterpret_cast<void*>(&OnFeatureEffectMount),reinterpret_cast<void**>(&originalFeatureEffectMount)},
                {address(Entry::QuestEnable),reinterpret_cast<void*>(&OnQuestEnable),reinterpret_cast<void**>(&originalQuestEnable)},
                {address(Entry::QuestTraceTick),reinterpret_cast<void*>(&OnQuestTraceTick),reinterpret_cast<void**>(&originalQuestTraceTick)},
                {address(Entry::InteeDialogAdd),reinterpret_cast<void*>(&OnDialogAdd),reinterpret_cast<void**>(&originalDialogAdd)},
                {address(Entry::InteePickupAdd),reinterpret_cast<void*>(&OnPickupAdd),reinterpret_cast<void**>(&originalPickupAdd)},
                {address(Entry::InteeKeyRefresh),reinterpret_cast<void*>(&OnInteeKeyRefresh),reinterpret_cast<void**>(&originalInteeKeyRefresh)},
                {address(Entry::InteeSwitchKeyRefresh),reinterpret_cast<void*>(&OnInteeSwitchKeyRefresh),reinterpret_cast<void**>(&originalInteeSwitchKeyRefresh)},
                {address(Entry::PrefabInstance),reinterpret_cast<void*>(&OnPrefabInstance),reinterpret_cast<void**>(&originalPrefabInstance)}
            };
            if(!valid)return false;Support::HookSetupGuard setup;if(!setup || MH_Initialize()!=MH_OK)return false;
            auto rollback=[&]{for(const auto& hook:hooks){MH_DisableHook(hook.target);MH_RemoveHook(hook.target);}};
            for(const auto& hook:hooks)if(MH_CreateHook(hook.target,hook.callback,hook.original)!=MH_OK){rollback();return false;}
            for(const auto& hook:hooks)if(MH_EnableHook(hook.target)!=MH_OK){rollback();return false;}
            ready.store(true,std::memory_order_release);return true;
        }
        bool Reload()
        {
            std::ifstream file(Support::PluginDirectory()/L"config.ini",std::ios::binary);if(!file)return false;
            std::string text((std::istreambuf_iterator<char>(file)),{});UI::Settings next;std::string error;
            if(!file.bad() && text==acceptedConfig)return true;
            if(file.bad() || text.size()>262144 || !UI::ParseSettings(text,next,error))return false;
            const auto current=std::atomic_load_explicit(&pending,std::memory_order_acquire);
            if(!SameUi(*current,next) || current->restoreHotkey!=next.restoreHotkey || current->exportHotkey!=next.exportHotkey)Publish(std::make_shared<const UI::Settings>(std::move(next)));
            acceptedConfig=std::move(text);return true;
        }
    }
    void Publish(std::shared_ptr<const UI::Settings> value){std::atomic_store_explicit(&pending,std::move(value),std::memory_order_release);}
    void Run(const Native::Profile& profile)
    {
        const auto directory=Support::PluginDirectory();if(directory.empty())return;Reload();
        if(!Initialize(profile,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))))return;
        const auto notification=FindFirstChangeNotificationW(directory.c_str(),FALSE,FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_SIZE);
        bool restoreHeld=false,dumpHeld=false,reloadPending=false;uint64_t reloadAfter=0;
        for(;;)
        {
            if(notification!=INVALID_HANDLE_VALUE && WaitForSingleObject(notification,50)==WAIT_OBJECT_0)
            {if(!FindNextChangeNotification(notification))break;reloadPending=true;reloadAfter=GetTickCount64()+50;}
            else if(notification==INVALID_HANDLE_VALUE)Sleep(50);
            if(reloadPending && GetTickCount64()>=reloadAfter){reloadPending=false;Reload();}
            const auto keys=std::atomic_load_explicit(&pending,std::memory_order_acquire);
            if(Key(keys->restoreHotkey,restoreHeld))commands.fetch_or(1,std::memory_order_release);
            if(Key(keys->exportHotkey,dumpHeld))commands.fetch_or(2,std::memory_order_release);
        }
        if(notification!=INVALID_HANDLE_VALUE)FindCloseChangeNotification(notification);
    }

}
