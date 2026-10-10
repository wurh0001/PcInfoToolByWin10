/*
 * 模块说明：面板布局动画计算
 * 只插值位置、尺寸、圆角和内容透明度，硬件数据和设备选择不属于动画状态。
 * 时间按毫秒归一化到 0..1；贝塞尔缓动控制布局变化，文字本身不缩放。
 */
#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// 动画只保存 DIP 几何和透明度，采样历史和设备选择不随过渡复制。
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
/**
 * 把线性时间转换为贝塞尔缓动进度；先求 X 对应的曲线参数再求 Y。
 */
inline float Ease(float time) {
    time=std::clamp(time,0.f,1.f);
    if(time==0||time==1)return time;
    // 时间是贝塞尔曲线的 X 坐标，先用二分法反求曲线参数 u，再用 u 求 Y（进度）。
    // 20 次二分给出足够精度；固定迭代次数也让单次计算开销可控。
    float low=0,high=1;
    for(int i=0;i<20;++i){float u=(low+high)*.5f,v=1-u;
        float x=3*v*v*u*.22f+3*v*u*u*.36f+u*u*u;
        if(x<time)low=u;else high=u;}
    float u=(low+high)*.5f,v=1-u;
    return 3*v*v*u+3*v*u*u+u*u*u;
}
/**
 * 在源/目标布局间插值尺寸和位置，内容透明度按分段时间过渡。
 */
inline Layout Mix(const Layout& a,const Layout& b,float progress) {
    float t=Ease(progress);auto lerp=[&](float x,float y){return x+(y-x)*t;};
    Layout out;out.height=lerp(a.height,b.height);out.expanded=lerp(a.expanded,b.expanded);
    for(size_t i=0;i<out.items.size();++i){auto& r=out.items[i];const auto& x=a.items[i];const auto& y=b.items[i];
        r.x=lerp(x.x,y.x);r.y=lerp(x.y,y.y);r.w=lerp(x.w,y.w);r.h=lerp(x.h,y.h);
        r.radius=lerp(x.radius,y.radius);r.background=lerp(x.background,y.background);
        // 内容淡出/淡入是实现中的分段策略，并非从 Figma 导出的关键帧：
        // 旧内容在 0..75ms 淡出，新内容在 75..225ms 淡入。
        // 位置和尺寸仍走完整 300ms 缓动；通过移动和透明度过渡，文字不缩放。
        for(int k=0;k<3;++k){float blend=y.content[k]<x.content[k]?std::clamp(progress/.25f,0.f,1.f):std::clamp((progress-.25f)/.5f,0.f,1.f);
            r.content[k]=x.content[k]+(y.content[k]-x.content[k])*blend;}}
    return out;
}
struct Transition {
    bool active=false;
    unsigned long long started=0;
    Layout from{},to{},current{};
    void Start(const Layout& source,const Layout& target,unsigned long long now){from=source;to=target;current=source;started=now;active=true;}
    /**
     * 根据当前毫秒时间推进过渡，结束时精确设置为目标布局。
     * @param now 当前毫秒时间，用于计算经过时间。
     */
    void Tick(unsigned long long now){if(!active)return;float t=std::clamp(float(now-started)/DurationMs,0.f,1.f);current=Mix(from,to,t);if(t>=1){current=to;active=false;}}
};
}
