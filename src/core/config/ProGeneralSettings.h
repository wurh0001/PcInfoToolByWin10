#pragma once
#include <windows.h>

// Pro-only preferences. Pinning belongs to the panel session, not these settings.
struct ProGeneralSettings {
    bool autoStart=false;
    bool alwaysOnTop=true;
    bool lockPosition=false;
    bool hideInFullscreen=true;
    bool rememberPosition=true;
    bool reduceMotion=false;
    UINT hotkeyModifiers=MOD_CONTROL|MOD_ALT;
    UINT hotkeyKey='I';
    bool positionValid=false;
    int positionX=0,positionY=0,savedDpi=96;
};

inline bool IsValidProHotkey(UINT modifiers,UINT key) {
    if(key==0)return modifiers==0;
    const UINT allowed=MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN;
    return !(modifiers&~allowed)&&(modifiers&(MOD_CONTROL|MOD_ALT))&&
        ((key>='A'&&key<='Z')||(key>='0'&&key<='9')||(key>=VK_F1&&key<=VK_F24));
}
