#pragma once
#include "core/config/ProGeneralSettings.h"
#include <algorithm>
#include <string>
#include <cmath>

namespace ProWindowBehavior {
inline POINT ClampVisiblePosition(POINT origin,SIZE size,RECT work,int margin);
struct PanelPlacement {int x=0,y=0;float height=0;bool above=false;};
inline PanelPlacement PlacePanel(RECT bar,RECT work,float naturalHeight,float width,float dpi,bool locked,bool above) {
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
inline std::wstring HotkeyText(UINT modifiers,UINT key) {
    if(!key)return L"未设置";
    std::wstring value;
    if(modifiers&MOD_CONTROL)value+=L"Ctrl + ";if(modifiers&MOD_ALT)value+=L"Alt + ";
    if(modifiers&MOD_SHIFT)value+=L"Shift + ";if(modifiers&MOD_WIN)value+=L"Win + ";
    if(key>=VK_F1&&key<=VK_F24)value+=L"F"+std::to_wstring(key-VK_F1+1);else value+=static_cast<wchar_t>(key);
    return value;
}

// Coordinates are physical pixels, including negative monitor origins.
inline POINT ClampVisiblePosition(POINT origin,SIZE size,RECT work,int margin) {
    return {std::clamp<LONG>(origin.x,work.left+margin,std::max(work.left+margin,work.right-margin-size.cx)),
        std::clamp<LONG>(origin.y,work.top+margin,std::max(work.top+margin,work.bottom-margin-size.cy))};
}
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
    bool Set(HWND owner,UINT modifiers,UINT key,std::wstring& error) {
        if(!IsValidProHotkey(modifiers,key)){error=L"请使用 Ctrl 或 Alt 加字母、数字或 F 键。";return false;}
        if(m_owner==owner&&m_id&&m_modifiers==modifiers&&m_key==key){error.clear();return true;}
        if(!key){Clear();error.clear();return true;}
        const int candidate=m_id==0x5101?0x5102:0x5101;
        // Register the replacement first: a conflict never destroys the working shortcut.
        if(!owner||!RegisterHotKey(owner,candidate,modifiers|MOD_NOREPEAT,key)) {
            error=GetLastError()==ERROR_HOTKEY_ALREADY_REGISTERED?L"快捷键已被占用，请换一个组合。":L"快捷键注册失败，请换一个组合。";
            return false;
        }
        Clear();m_owner=owner;m_id=candidate;m_modifiers=modifiers;m_key=key;error.clear();return true;
    }
    void Clear(){if(m_id&&m_owner)UnregisterHotKey(m_owner,m_id);m_owner=nullptr;m_id=0;m_modifiers=0;m_key=0;}
    bool Matches(WPARAM id) const {return m_id&&id==static_cast<WPARAM>(m_id);}
    UINT Key() const {return m_key;}
private:
    HWND m_owner=nullptr;int m_id=0;UINT m_modifiers=0,m_key=0;
};
}
