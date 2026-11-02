// HUD changes are applied on page/configuration events, using per-page caches.
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
        struct Api
        {
            StringFn stringNew=nullptr;ObjectFn find=nullptr,transform=nullptr,owner=nullptr,parent=nullptr,name=nullptr;
            IdFn id=nullptr,count=nullptr;ResolveFn resolve=nullptr;NamedFn component=nullptr,childPath=nullptr;ChildFn child=nullptr;
            BoolGet active=nullptr,rendererEnabled=nullptr;BoolSet setRendererEnabled=nullptr;
            ColorGet color=nullptr;FloatSet setAlpha=nullptr;
            V3Get scale=nullptr,local=nullptr;V3Set setScale=nullptr,setLocal=nullptr;
            V2Get position=nullptr;V2Set setPosition=nullptr;RectGet rect=nullptr;RotationGet rotation=nullptr;
        } api;
        std::shared_ptr<const UI::Settings> pending=std::make_shared<UI::Settings>(),settings;
        std::string acceptedConfig;
        std::atomic<bool> ready{false},faulted{false};
        std::atomic<DWORD> uiThread{0};
        RenderFn originalRender=nullptr;
        AwakeFn originalAwake=nullptr;
        std::atomic<int> pendingPage{0};
        std::atomic<unsigned> commands{0};
        bool restored=false;
        thread_local bool applying=false;
        int rootId=0,rootTransformId=0,pageId=0;
        uint32_t effectiveMask=0;
        std::vector<Region> regions;
        struct Record
        {
            int id=0,object=0,transform=0,graphic=0,anchor=0,anchorTransform=0;
            std::string path,kind;Match match;
            std::vector<std::pair<int,int>> chain;
            bool particle=false,rect=false,rectKnown=false,layoutTarget=false;
            bool alphaChanged=false,rendererChanged=false;
            float alpha=1;bool enabled=true;
            Layout layout;V3 local{};V2 currentPosition{};
            void* token=nullptr;void* graphicToken=nullptr;
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
            int transform=0,object=0;
            std::string path;
            Match match;
            std::vector<std::pair<int,int>> chain;
            bool forbidden=false,inputChecked=false,input=false;
            void* objectToken=nullptr;
        };
        struct UnitBinding {int transform=0,button=0;bool rect=false;void* token=nullptr;void* buttonToken=nullptr;};
        std::unordered_map<int,Node> nodes;
        std::unordered_set<std::string> covered,attempted;
        std::vector<bool> boundFields;
        std::unordered_map<std::string,UnitBinding> unitBindings;
        std::unordered_set<int> visibilityDirty,layoutDirty,added;
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
            for(const auto& pair:record.chain)if(!Resolve(pair.first) || Parent(pair.first)!=pair.second)return false;
            if(full)
            {
                auto* anchorTransform=Resolve(record.anchorTransform);auto* transform=Resolve(record.transform);
                if(!anchorTransform || !transform || Path(transform,anchorTransform)!=record.path)return false;
                if(!record.particle && Resolve(record.graphic)!=record.graphicToken)return false;
            }
            return true;
        }
        bool HasInput(void* object)
        {for(const char* type:{"Selectable","EventTrigger","ScrollRect","MonoActionBtn","MonoBaseSkillBtn","MonoMiniMap","MonoTeamButtonContainer"})if(Component(object,type))return true;return false;}
        bool EffectOwner(void* transform,void* anchor)
        {
            for(unsigned depth=0;transform && depth<24;++depth)
            {if(Component(api.owner(transform,nullptr),"MonoUIEffect"))return true;if(transform==anchor)break;transform=api.parent(transform,nullptr);}return false;
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
        void Restore(Record& record)
        {
            if(!record.alphaChanged && !record.rendererChanged && !record.layout.changed)return;
            auto* component=Resolve(record.id);auto* object=Resolve(record.object);auto* transform=Resolve(record.transform);
            // A pooled visual may have left its old UI branch. Release only our
            // saved property on the exact same typed component, never a new ID owner.
            if(!component || component!=record.token || !object || !transform || api.owner(component,nullptr)!=object ||
                api.transform(object,nullptr)!=transform ||
                (!record.particle && Resolve(record.graphic)!=record.graphicToken))return;
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
                record.layout={};
            }
        }
        void RestoreAll(){for(auto& pair:records)Guard([&]{Restore(pair.second);});}
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
            return settings->scale!=1 || (settings->alignRightButtons && match.topRight) ||
                (settings->marginLeft && (match.edges&Left)) || (settings->marginRight && (match.edges&Right)) ||
                (settings->marginTop && (match.edges&Top)) || (settings->marginBottom && (match.edges&Bottom));
        }
        void ResetCache()
        {
            RestoreAll();records.clear();nodes.clear();covered.clear();attempted.clear();unitBindings.clear();
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
        void LoadRegions(bool all=false)
        {
            if(regions.empty())regions=FixedRegions();
            if(boundFields.empty())boundFields.resize(Bindings().size());
            auto* rootTransform=Resolve(rootTransformId);auto* page=Resolve(pageId);bool changed=false;
            if(page)for(size_t i=0;i<Bindings().size();++i)
            {
                const auto& binding=Bindings()[i];
                if(boundFields[i] || (!all && !RegionNeeded({"",binding.preset,Edges(binding.preset)}) && settings->hiddenPaths.empty()))continue;
                auto* value=*reinterpret_cast<void**>(static_cast<unsigned char*>(page)+binding.offset);
                if(!value)continue;
                boundFields[i]=true;
                auto* object=value && !binding.gameObject?api.owner(value,nullptr):value;if(!object)continue;
                auto* transform=api.transform(object,nullptr);
                const auto base=Path(transform,rootTransform);if(base.empty() || Forbidden(base))continue;
                if(binding.child){transform=Relative(transform,binding.child);if(!transform)continue;}
                const auto path=Path(transform,rootTransform);if(!ValidRegion(path,binding.preset))continue;
                if(std::none_of(regions.begin(),regions.end(),[&](const Region& r){return r.path==path && r.preset==binding.preset;}))
                {regions.push_back({path,binding.preset,Edges(binding.preset)});changed=true;}
            }
            if(changed)
            {
                for(auto& pair:nodes)pair.second.match=Classify(pair.second.path,regions);
                for(auto& pair:records)
                {
                    auto& r=pair.second;auto match=Classify(r.path,regions);
                    if(match.presets!=r.match.presets || match.edges!=r.match.edges || match.unit!=r.match.unit)added.insert(r.id);
                    r.match=std::move(match);
                }
            }
        }
        bool Covered(const std::string& path)
        {return std::any_of(covered.begin(),covered.end(),[&](const std::string& parent){return Within(path,parent);});}
        void DiscoverRegion(const Region& region)
        {
            auto* anchorTransform=Resolve(rootTransformId);auto* transform=Relative(anchorTransform,region.path);
            attempted.insert(region.path);if(!transform)return;
            struct Item{void* transform;std::string path;unsigned depth;};
            std::vector<Item> work{{transform,region.path,0}};std::unordered_set<int> visited;
            while(!work.empty() && nodes.size()<8192 && records.size()<4096)
            {
                auto item=std::move(work.back());work.pop_back();const int transformId=Id(item.transform);
                if(!transformId || !visited.insert(transformId).second || Covered(item.path))continue;
                auto* object=api.owner(item.transform,nullptr);if(!object)continue;
                Node node;node.transform=transformId;node.object=Id(object);node.objectToken=object;node.path=item.path;node.forbidden=Forbidden(item.path);
                node.match=Classify(item.path,regions);
                int current=transformId;
                for(unsigned depth=0;current && current!=rootTransformId && depth<48;++depth)
                {const int parent=Parent(current);node.chain.push_back({current,parent});current=parent;}
                if(current!=rootTransformId)continue;
                nodes[transformId]=std::move(node);const auto& cached=nodes.at(transformId);
                if(cached.forbidden || !cached.match.presets)continue;
                auto* graphic=Component(object,"Graphic");auto* canvas=graphic?Component(object,"CanvasRenderer"):nullptr;
                bool expected=false;if(graphic)for(const char* type:{"Image","RawImage","Text","SimpleText"})if(Component(object,type)){expected=true;break;}
                void* component=expected?canvas:nullptr;const char* kind="CanvasRenderer";bool particle=false;
                // Skill graphics and typed UI effects share the same hide state.
                if(!component && cached.match.particle)
                    for(const char* type:{"ParticleSystemRenderer","MeshRenderer"})if(auto* renderer=Component(object,type))
                    {if(EffectOwner(item.transform,anchorTransform)){component=renderer;kind=type;particle=true;}break;}
                if(component)
                {
                    const int id=Id(component);auto old=records.find(id);
                    if(old==records.end() || old->second.token!=component || old->second.object!=cached.object || old->second.path!=item.path)
                    {
                        if(old!=records.end())Restore(old->second);
                        Record r;r.id=id;r.object=cached.object;r.transform=transformId;r.graphic=Id(graphic);r.graphicToken=graphic;
                        r.anchor=rootId;r.anchorTransform=rootTransformId;r.path=item.path;r.kind=kind;r.match=cached.match;
                        r.particle=particle;r.token=component;r.chain=cached.chain;
                        records[id]=std::move(r);added.insert(id);
                    }
                }
                const int count=api.count(item.transform,nullptr);if(item.depth>=32 || count<0 || count>1024)continue;
                for(int i=0;i<count;++i){auto* child=api.child(item.transform,i,nullptr);if(child)work.push_back({child,item.path+"/"+Name(child),item.depth+1});}
            }
            if(work.empty())covered.insert(region.path);
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
        void SelectLayoutTargets()
        {
            // Input metadata is reviewed once, and only for a region whose layout
            // is actually requested. Right alignment never reviews the skill/map trees.
            std::unordered_set<int> blocked,candidates;
            for(auto& pair:nodes)
            {
                auto& n=pair.second;
                if(!n.inputChecked && !n.forbidden && LayoutNeeded(n.match))
                {auto* object=Resolve(n.object);n.input=!object || object!=n.objectToken || HasInput(object);n.inputChecked=true;}
                if(n.forbidden || n.input)
                {blocked.insert(n.transform);for(const auto& edge:n.chain)blocked.insert(edge.second);}
            }
            for(auto& pair:records)
            {
                auto& r=pair.second;
                if(LayoutNeeded(r.match) && !blocked.count(r.transform))candidates.insert(r.transform);
            }
            for(auto& pair:records)
            {
                auto& r=pair.second;bool target=candidates.count(r.transform)!=0;
                if(target)for(const auto& edge:r.chain)if(candidates.count(edge.second)){target=false;break;}
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
                if(!Active(r.transform)){RestoreLayout(r);continue;}
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
                if(binding.rect){Rect rect{};api.rect(t,&rect,nullptr);unit.center=Point(MatrixOf(unit.transform),{rect.x+rect.width*.5f,rect.y+rect.height*.5f});}
                else unit.center=Point(MatrixOf(unit.transform),{});
                unit.active=Active(unit.transform) && (!binding.button || (Resolve(binding.button)==binding.buttonToken && Active(binding.button)));
            }
            for(const auto& pair:records)
            {const auto& r=pair.second;auto it=result.find(r.match.unit);if(it!=result.end() && !it->second.visible && !HiddenNow(r))it->second.visible=Active(r.transform);}
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
                if(!Active(r.transform))continue;
                const int parent=Parent(r.transform);const auto parentMatrix=MatrixOf(parent);if(invalidMatrices.count(parent))continue;
                const auto unit=units.find(r.match.unit);if(unit==units.end() || !unit->second.transform)continue;
                Rotation rotation{};api.rotation(transform,&rotation,nullptr);Rect rect{};if(r.rect)api.rect(transform,&rect,nullptr);
                V2 local{r.local.x+r.layout.position.x-r.currentPosition.x,r.local.y+r.layout.position.y-r.currentPosition.y};
                V2 shift{unit->second.alignment,0};
                if(horizontal){if(r.match.edges&Left)shift.x+=settings->marginLeft;if(r.match.edges&Right)shift.x-=settings->marginRight;}
                if(vertical){if(r.match.edges&Top)shift.y-=settings->marginTop;if(r.match.edges&Bottom)shift.y+=settings->marginBottom;}
                V3 desiredScale{};V2 delta{};
                if(!Geometry(parentMatrix,local,r.layout.scale,rotation,rect,unit->second.center,shift,settings->scale,desiredScale,delta))continue;
                const auto desiredPosition=Add(r.layout.position,delta);
                if(!Identity(r,true))continue;
                V3 currentScale{};api.scale(transform,&currentScale,nullptr);
                if(!Same(currentScale,desiredScale)){api.setScale(transform,&desiredScale,nullptr);r.layout.changed=true;}
                if(!Same(r.currentPosition,desiredPosition))
                {
                    if(r.rect)api.setPosition(transform,&desiredPosition,nullptr);
                    else {V3 localPosition=r.local;localPosition.x=desiredPosition.x;localPosition.y=desiredPosition.y;api.setLocal(transform,&localPosition,nullptr);}
                    r.layout.changed=true;
                }
                api.scale(transform,&r.layout.appliedScale,nullptr);
                if(r.rect)api.position(transform,&r.layout.appliedPosition,nullptr);
                else {V3 current{};api.local(transform,&current,nullptr);r.layout.appliedPosition={current.x,current.y};}
                r.layout.applied=true;
            }
        }
        void ApplyVisibility()
        {
            for(auto& pair:records)
            {
                auto& r=pair.second;if(!visibilityDirty.count(r.id))continue;
                if(!HiddenNow(r) && !r.alphaChanged && !r.rendererChanged)continue;
                if(!Identity(r)){Guard([&]{Restore(r);});continue;}
                const bool hidden=HiddenNow(r) && Active(r.transform);auto* component=Resolve(r.id);
                if(r.particle)
                {
                    const bool current=api.rendererEnabled(component,nullptr);
                    if(hidden && current && Identity(r,true)){r.enabled=current;r.rendererChanged=true;api.setRendererEnabled(component,false,nullptr);}
                    else if(!hidden && r.rendererChanged){if(!current && Identity(r,true))api.setRendererEnabled(component,r.enabled,nullptr);r.rendererChanged=false;}
                }
                else
                {
                    Color color{};api.color(component,&color,nullptr);if(!std::isfinite(color.a))continue;
                    if(hidden && !Near(color.a,0,.00001f) && Identity(r,true))
                    {r.alpha=color.a;r.alphaChanged=true;api.setAlpha(component,0.f,nullptr);}
                    else if(!hidden && r.alphaChanged)
                    {if(Near(color.a,0,.00001f) && Identity(r,true))api.setAlpha(component,r.alpha,nullptr);r.alphaChanged=false;}
                }
            }
        }
        bool Key(int key,bool& held)
        {DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);const bool down=key && pid==GetCurrentProcessId() && (GetAsyncKeyState(key)&0x8000);const bool pressed=down && !held;held=down;return pressed;}
        void Export()
        {
            std::ofstream file(Support::PluginDirectory()/L"ui-tree.txt",std::ios::binary|std::ios::trunc);if(!file)return;
            file<<"# MainUICustomizer: activeSelf<TAB>relative path; approved drawing components annotated; no text contents.\n";
            struct Item{void* transform;std::string path;unsigned depth;};
            std::vector<Item> work{{Resolve(rootTransformId),"",0}};size_t count=0;
            while(!work.empty() && count++<20000)
            {
                auto item=std::move(work.back());work.pop_back();if(!item.transform)continue;
                auto* object=api.owner(item.transform,nullptr);const auto match=Classify(item.path,regions);
                file<<(api.active(object,nullptr)?1:0)<<'\t'<<UI::MainPagePath<<(item.path.empty()?"":"/"+item.path)<<"\tapproved_region="<<(match.presets!=0)<<'\n';
                const int children=api.count(item.transform,nullptr);if(item.depth>=48 || children<0 || children>4096)continue;
                for(int i=children-1;i>=0;--i){auto* child=api.child(item.transform,i,nullptr);if(child)work.push_back({child,item.path+(item.path.empty()?"":"/")+Name(child),item.depth+1});}
            }
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
            if(next==settings && !captured && !command)return;
            const auto previous=settings;settings=next;
            if(previous && SameUi(*previous,*settings) && !captured && !command)return;
            ClearFrame();visibilityDirty.clear();layoutDirty.clear();added.clear();
            bool force=!previous || captured!=0;
            if(command&1){restored=!restored;force=true;}
            bool replaced=false;
            if(captured && !DiscoverRoot(captured,replaced))return;
            if(!Demand() && !(command&2)){RestoreAll();return;}
            if(!captured && !DiscoverRoot(0,replaced))return;force=force || replaced;
            if(command&2){ResetCache();LoadRegions(true);Export();force=true;}
            effectiveMask=HiddenMask(*settings);
            if(restored || faulted.load()){RestoreAll();return;}
            EnsureRegions();
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
                    (LayoutNeeded(r.match) || r.layout.changed))layoutDirty.insert(r.id);
            }
            if(topVisibility && (old.alignRightButtons || settings->alignRightButtons))
                for(const auto& pair:records)if(pair.second.match.topRight)layoutDirty.insert(pair.first);
            if(!layoutDirty.empty())
            {
                SelectLayoutTargets();
                for(int id:layoutDirty){auto& r=records.at(id);if(!r.layoutTarget && r.layout.changed)RestoreLayout(r);}
                if(LayoutDemand())ApplyLayouts();
            }
            ApplyVisibility();
        }

        void __fastcall OnAwake(void* page,void* method)
        {
            originalAwake(page,method);
            if(!ready.load(std::memory_order_acquire))return;
            Guard([&]{pendingPage.store(Id(page),std::memory_order_release);});
        }
        void __fastcall OnRender(void* method)
        {
            originalRender(method);if(!ready.load(std::memory_order_acquire) || applying)return;
            DWORD empty=0;uiThread.compare_exchange_strong(empty,GetCurrentThreadId());if(uiThread.load()!=GetCurrentThreadId())return;
            applying=true;bool ok=false;try{ok=Guard([]{Update();});}catch(...){}
            if(!ok){faulted.store(true);ready.store(false);try{RestoreAll();}catch(...){}}
            applying=false;
        }
        bool Initialize()
        {
            const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));bool compatible=false;
            Guard([&]{const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000)return;
                const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);compatible=nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 && nt->FileHeader.TimeDateStamp==Native::Timestamp && nt->OptionalHeader.SizeOfImage==Native::ImageSize;});
            if(!compatible)return false;bool valid=true;
            auto address=[&](const Native::Method& method)->void*{auto* p=reinterpret_cast<void*>(base+method.rva);const bool match=Support::MatchesEntryBytes(p,method.bytes);if(!match)valid=false;return match?p:nullptr;};
#define LOAD(field,method) api.field=reinterpret_cast<decltype(api.field)>(address(Native::method))
            LOAD(stringNew,StringNew);LOAD(find,FindObject);LOAD(transform,ObjectTransform);LOAD(owner,ComponentObject);LOAD(parent,Parent);LOAD(name,Name);
            LOAD(id,InstanceId);LOAD(resolve,FromId);LOAD(count,ChildCount);LOAD(component,ComponentByName);LOAD(childPath,FindTransform);LOAD(child,Child);LOAD(active,ActiveSelf);
            LOAD(rendererEnabled,ReadRendererEnabled);LOAD(setRendererEnabled,SetRendererEnabled);LOAD(scale,ReadScale);LOAD(local,ReadLocalPosition);LOAD(setScale,SetScale);
            LOAD(position,ReadPosition);LOAD(setPosition,SetPosition);LOAD(rect,ReadRect);LOAD(rotation,ReadRotation);
#undef LOAD
            api.color=reinterpret_cast<ColorGet>(address(Native::ReadCanvasColor));api.setAlpha=reinterpret_cast<FloatSet>(address(Native::SetCanvasAlpha));
            api.setLocal=reinterpret_cast<V3Set>(address(Native::SetLocalPosition));auto* target=address(Native::WillRender);auto* awake=address(Native::MainAwake);
            if(!valid)return false;Support::HookSetupGuard setup;if(!setup || MH_Initialize()!=MH_OK)return false;
            if(MH_CreateHook(target,reinterpret_cast<void*>(&OnRender),reinterpret_cast<void**>(&originalRender))!=MH_OK)return false;
            if(MH_CreateHook(awake,reinterpret_cast<void*>(&OnAwake),reinterpret_cast<void**>(&originalAwake))!=MH_OK){MH_RemoveHook(target);return false;}
            if(MH_EnableHook(target)!=MH_OK || MH_EnableHook(awake)!=MH_OK){MH_DisableHook(target);MH_DisableHook(awake);MH_RemoveHook(target);MH_RemoveHook(awake);return false;}
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
    void Run()
    {
        const auto directory=Support::PluginDirectory();if(directory.empty())return;Reload();if(!Initialize())return;
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
