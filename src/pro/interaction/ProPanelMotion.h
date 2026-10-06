#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// DIP geometry only. Data/history and device selection never belong to a transition.
namespace ProPanelMotion {
constexpr float DurationMs=300.f;
struct Item {
    float x=11,y=42,w=338,h=30,radius=10,background=0;
    std::array<float,3> content{1,0,0}; // full row, capsule, detail
};
struct Layout {
    float height=284,expanded=0;
    std::array<Item,6> items{};
};
inline float Ease(float time) {
    time=std::clamp(time,0.f,1.f);
    if(time==0||time==1)return time;
    // Solve bezier X first; applying the Y polynomial directly to time is wrong.
    float low=0,high=1;
    for(int i=0;i<20;++i){float u=(low+high)*.5f,v=1-u;
        float x=3*v*v*u*.22f+3*v*u*u*.36f+u*u*u;
        if(x<time)low=u;else high=u;}
    float u=(low+high)*.5f,v=1-u;
    return 3*v*v*u+3*v*u*u+u*u*u;
}
inline Layout Mix(const Layout& a,const Layout& b,float progress) {
    float t=Ease(progress);auto lerp=[&](float x,float y){return x+(y-x)*t;};
    Layout out;out.height=lerp(a.height,b.height);out.expanded=lerp(a.expanded,b.expanded);
    for(size_t i=0;i<out.items.size();++i){auto& r=out.items[i];const auto& x=a.items[i];const auto& y=b.items[i];
        r.x=lerp(x.x,y.x);r.y=lerp(x.y,y.y);r.w=lerp(x.w,y.w);r.h=lerp(x.h,y.h);
        r.radius=lerp(x.radius,y.radius);r.background=lerp(x.background,y.background);
        // Approved implementation staging, not exported Figma keyframes:
        // old content leaves in 0..75ms; new content enters in 75..225ms.
        // Position/size retain the documented 300ms bezier. Text never scales.
        for(int k=0;k<3;++k){float blend=y.content[k]<x.content[k]?std::clamp(progress/.25f,0.f,1.f):std::clamp((progress-.25f)/.5f,0.f,1.f);
            r.content[k]=x.content[k]+(y.content[k]-x.content[k])*blend;}}
    return out;
}
struct Transition {
    bool active=false;
    unsigned long long started=0;
    Layout from{},to{},current{};
    void Start(const Layout& source,const Layout& target,unsigned long long now){from=source;to=target;current=source;started=now;active=true;}
    void Tick(unsigned long long now){if(!active)return;float t=std::clamp(float(now-started)/DurationMs,0.f,1.f);current=Mix(from,to,t);if(t>=1){current=to;active=false;}}
};
}
