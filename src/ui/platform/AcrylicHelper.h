/*
 * 模块说明：Windows 磨砂背景适配
 * 尝试动态获取窗口合成属性接口，不可用时走 DWM 模糊回退。
 * 该文件封装平台效果而非硬件采集；最终外观依赖系统版本和合成支持。
 */
#pragma once
#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// 合成材质枚举，数值与 Windows 接口约定对应，不能随意重新编号。
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5
};

// pvData 指向的策略结构：材质、标志、背景色及保留动画 ID。
struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

// 把属性编号、策略地址和字节长度一起交给系统；PVOID 是不带具体类型的地址。
struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

// hwnd 是目标窗口，gradientColor 使用 AABBGGRR 顺序（不是常见的 AARRGGBB）。
// 动态查询接口存在时尝试设置并返回；当前实现不检查其 BOOL 返回值，
// 因此 DWM 回退只在接口不可用时发生，不涵盖接口调用失败的情况。
inline void EnableWindowAcrylic(HWND hwnd, DWORD gradientColor = 0xCC1F1F1F) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        pfnSetWindowCompositionAttribute setWindowCompositionAttribute =
            (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
        if (setWindowCompositionAttribute) {
            ACCENT_POLICY policy = {};
            policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
            policy.AccentFlags = 2; // 边框/阴影标志
            policy.GradientColor = gradientColor; // AABBGGRR

            WINDOWCOMPOSITIONATTRIBDATA data = {};
            data.Attrib = 19; // WCA_ACCENT_POLICY
            data.pvData = &policy;
            data.cbData = sizeof(policy);

            setWindowCompositionAttribute(hwnd, &data);
            return;
        }
    }

    // 合成属性接口不可用时，尝试 DWM 背景模糊
    DWM_BLURBEHIND bb = { 0 };
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd, &bb);
}
