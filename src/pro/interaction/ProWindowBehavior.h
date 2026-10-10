/*
 * 模块说明：Pro 窗口定位与快捷键工具
 * 工作区限制支持多显示器和负坐标；根据可用空间决定详情在悬浮条上方或下方。
 * 快捷键先注册新组合再撤销旧组合，避免冲突失败导致已有快捷键失效。
 */
#pragma once
#include "core/config/ProGeneralSettings.h"
#include <algorithm>
#include <string>
#include <cmath>

namespace ProWindowBehavior {
/**
 * 把像素坐标限制在显示器工作区内，保留边距并支持负坐标。
 */
inline POINT ClampVisiblePosition(POINT origin,SIZE size,RECT work,int margin);
struct PanelPlacement {int x=0,y=0;float height=0;bool above=false;};
/**
 * 结合工作区可用空间选择上/下展开，并限制位置和高度。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * @param dpi DPI 缩放比例，1 表示 96 DPI。
 */
inline PanelPlacement PlacePanel(RECT bar,RECT work,float naturalHeight,float width,float dpi,bool locked,bool above) {
    // bar/work 为屏幕像素矩形，naturalHeight/width 为 DIP。间距先乘 DPI 转成像素。
    // 空间转回 DIP 后限制内容高度，最终位置再按像素取整。
    const float gap=16*dpi,margin=8*dpi;
    const float below=std::max(0.f,(work.bottom-margin-bar.bottom-gap)/dpi);
    const float over=std::max(0.f,(bar.top-gap-work.top-margin)/dpi);
    if(!locked)above=below<std::min(naturalHeight,284.f)&&over>below;
    const float available=above ? over : below;
    const float height=std::min(naturalHeight,std::max(std::min(96.f,(work.bottom-work.top)/dpi-16),available));
    const int x=static_cast<int>(std::lround((bar.left+bar.right-width*dpi)/2));
    const int y=static_cast<int>(std::lround(above ? bar.top-gap-height*dpi : bar.bottom+gap));
    const auto p=ClampVisiblePosition({x,y},{static_cast<LONG>(std::lround(width*dpi)),static_cast<LONG>(std::lround(height*dpi))},work,static_cast<int>(margin));
    return {p.x,p.y,height,above};
}
/**
 * 把修饰键和虚拟键码转换为显示文字，不负责注册快捷键。
 * @param key 虚拟键码，用于键盘导航或快捷键。
 */
inline std::wstring HotkeyText(UINT modifiers,UINT key) {
    if(!key)return L"未设置";
    std::wstring value;
    if(modifiers&MOD_CONTROL)value+=L"Ctrl + ";if(modifiers&MOD_ALT)value+=L"Alt + ";
    if(modifiers&MOD_SHIFT)value+=L"Shift + ";if(modifiers&MOD_WIN)value+=L"Win + ";
    if(key>=VK_F1&&key<=VK_F24)value+=L"F"+std::to_wstring(key-VK_F1+1);else value+=static_cast<wchar_t>(key);
    return value;
}

// 坐标是屏幕物理像素；副显示器可位于主屏左边/上边，因此不能把坐标限定为非负。
/**
 * 把像素坐标限制在显示器工作区内，保留边距并支持负坐标。
 */
inline POINT ClampVisiblePosition(POINT origin,SIZE size,RECT work,int margin) {
    return {std::clamp<LONG>(origin.x,work.left+margin,std::max(work.left+margin,work.right-margin-size.cx)),
        std::clamp<LONG>(origin.y,work.top+margin,std::max(work.top+margin,work.bottom-margin-size.cy))};
}
/**
 * 带少量像素容差判断窗口是否覆盖显示器，用于全屏隐藏检测。
 */
inline bool CoversMonitor(RECT window,RECT monitor) {
    constexpr int tolerance=2;
    return window.left<=monitor.left+tolerance&&window.top<=monitor.top+tolerance&&
        window.right>=monitor.right-tolerance&&window.bottom>=monitor.bottom-tolerance;
}

class HotkeyBinding {
public:
    ~HotkeyBinding(){Clear();}
    HotkeyBinding()=default;
    HotkeyBinding(const HotkeyBinding&)=delete;
    HotkeyBinding& operator=(const HotkeyBinding&)=delete;
    /**
     * 验证并替换全局快捷键；先注册新组合，成功后才解除旧组合。
     * @param owner 所属窗口，用于焦点、消息或窗口关系。
     * @param key 虚拟键码，用于键盘导航或快捷键。
     */
    bool Set(HWND owner,UINT modifiers,UINT key,std::wstring& error) {
        if(!IsValidProHotkey(modifiers,key)){error=L"请使用 Ctrl 或 Alt 加字母、数字或 F 键。";return false;}
        if(m_owner==owner&&m_id&&m_modifiers==modifiers&&m_key==key){error.clear();return true;}
        if(!key){Clear();error.clear();return true;}
        const int candidate=m_id==0x5101?0x5102:0x5101;
        // 先注册替代组合，若冲突则返回错误，保留原来仍可工作的快捷键。
        if(!owner||!RegisterHotKey(owner,candidate,modifiers|MOD_NOREPEAT,key)) {
            error=GetLastError()==ERROR_HOTKEY_ALREADY_REGISTERED?L"快捷键已被占用，请换一个组合。":L"快捷键注册失败，请换一个组合。";
            return false;
        }
        Clear();m_owner=owner;m_id=candidate;m_modifiers=modifiers;m_key=key;error.clear();return true;
    }
    /**
     * 解除已注册的快捷键并清空绑定状态。
     */
    void Clear(){if(m_id&&m_owner)UnregisterHotKey(m_owner,m_id);m_owner=nullptr;m_id=0;m_modifiers=0;m_key=0;}
    /**
     * 判断 WM_HOTKEY 的 ID 是否属于当前绑定。
     */
    bool Matches(WPARAM id) const {return m_id&&id==static_cast<WPARAM>(m_id);}
    UINT Key() const {return m_key;}
private:
    HWND m_owner=nullptr;int m_id=0;UINT m_modifiers=0,m_key=0;
};
}
