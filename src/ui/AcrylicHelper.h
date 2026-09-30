#pragma once
#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

inline void EnableWindowAcrylic(HWND hwnd, DWORD gradientColor = 0xCC1F1F1F) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        pfnSetWindowCompositionAttribute setWindowCompositionAttribute =
            (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (setWindowCompositionAttribute) {
            ACCENT_POLICY policy = {};
            policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
            policy.AccentFlags = 2; // draw border/shadow
            policy.GradientColor = gradientColor; // AABBGGRR

            WINDOWCOMPOSITIONATTRIBDATA data = {};
            data.Attrib = 19; // WCA_ACCENT_POLICY
            data.pvData = &policy;
            data.cbData = sizeof(policy);

            setWindowCompositionAttribute(hwnd, &data);
            return;
        }
    }

    // Fallback DWM Blur
    DWM_BLURBEHIND bb = { 0 };
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);
}
