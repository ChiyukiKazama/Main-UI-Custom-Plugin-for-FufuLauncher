#pragma once
#include <cmath>
namespace HUD
{
    struct V2 {float x=0,y=0;};
    struct V3 {float x=1,y=1,z=1;};
    struct Rect {float x=0,y=0,width=0,height=0;};
    struct Rotation {float x=0,y=0,z=0,w=1;};
    struct Matrix {float a=1,b=0,c=0,d=1,x=0,y=0;};
    inline V2 Add(V2 a,V2 b){return {a.x+b.x,a.y+b.y};}
    inline V2 Sub(V2 a,V2 b){return {a.x-b.x,a.y-b.y};}
    inline V2 Mul(V2 a,float factor){return {a.x*factor,a.y*factor};}
    inline V2 Point(const Matrix& m,V2 p){return {m.a*p.x+m.c*p.y+m.x,m.b*p.x+m.d*p.y+m.y};}
    inline V2 Vector(const Matrix& m,V2 p){return {m.a*p.x+m.c*p.y,m.b*p.x+m.d*p.y};}
    inline Matrix Product(const Matrix& p,const Matrix& q)
    {return {p.a*q.a+p.c*q.b,p.b*q.a+p.d*q.b,p.a*q.c+p.c*q.d,p.b*q.c+p.d*q.d,p.a*q.x+p.c*q.y+p.x,p.b*q.x+p.d*q.y+p.y};}
    inline Matrix Transform(V2 position,V3 scale,Rotation rotation)
    {const float c=1-2*rotation.z*rotation.z,s=2*rotation.w*rotation.z;return {c*scale.x,s*scale.x,-s*scale.y,c*scale.y,position.x,position.y};}
    inline bool InverseVector(const Matrix& m,V2 value,V2& result)
    {const float determinant=m.a*m.d-m.b*m.c;if(!std::isfinite(determinant) || std::abs(determinant)<.000001f)return false;
        result={(m.d*value.x-m.c*value.y)/determinant,(-m.b*value.x+m.a*value.y)/determinant};return std::isfinite(result.x) && std::isfinite(result.y);}
    inline bool Near(float a,float b,float epsilon=.001f){return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<epsilon;}
    inline bool Same(V2 a,V2 b){return Near(a.x,b.x) && Near(a.y,b.y);}
    inline bool Same(V3 a,V3 b){return Near(a.x,b.x,.00001f) && Near(a.y,b.y,.00001f) && Near(a.z,b.z,.00001f);}
    struct Layout {V3 scale{},appliedScale{};V2 position{},appliedPosition{};bool applied=false,changed=false;};
    inline void Observe(Layout& state,V3 scale,V2 position)
    {
        if(!state.applied){state.scale=scale;state.position=position;return;}
        if(!Near(scale.x,state.appliedScale.x,.00001f))state.scale.x=scale.x;
        if(!Near(scale.y,state.appliedScale.y,.00001f))state.scale.y=scale.y;
        if(!Near(scale.z,state.appliedScale.z,.00001f))state.scale.z=scale.z;
        if(!Near(position.x,state.appliedPosition.x))state.position.x=position.x;
        if(!Near(position.y,state.appliedPosition.y))state.position.y=position.y;
    }
    inline bool Geometry(const Matrix& parent,V2 local,V3 scale,Rotation rotation,Rect rect,V2 unitCenter,V2 shift,float factor,V3& newScale,V2& anchoredDelta)
    {
        if(std::abs(rotation.x)+std::abs(rotation.y)>.0001f)return false;
        const auto localMatrix=Transform(local,scale,rotation);
        const V2 rectCenter{rect.x+rect.width*.5f,rect.y+rect.height*.5f};
        const auto offset=Vector(localMatrix,rectCenter);
        const auto center=Point(parent,Add(local,offset));
        V2 correction{};
        if(!InverseVector(parent,Add(Mul(Sub(unitCenter,center),1-factor),shift),correction))return false;
        anchoredDelta=Add(Mul(offset,1-factor),correction);newScale={scale.x*factor,scale.y*factor,scale.z};
        return std::isfinite(anchoredDelta.x) && std::isfinite(anchoredDelta.y) && std::isfinite(newScale.x) && std::isfinite(newScale.y);
    }
}
