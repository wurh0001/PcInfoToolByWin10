/*
 * 模块说明：DIP 与像素转换
 * 96 DPI 对应缩放 1.0；优先查询窗口 DPI，无法获取时退回系统 DPI 或 1.0。
 * 动态查找 API 避免直接依赖导出；DipToPx 用于非负尺寸的四舍五入。
 */
#pragma once
#include <windows.h>

class DpiHelper {
public:
    /**
     * 返回窗口 DPI/96；窗口 API 不可用时回退系统 DPI。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     */
    static float GetScaleForWindow(HWND hwnd) {
        UINT dpi = 96;
        HMODULE hUser = GetModuleHandleW(L"user32.dll");
        if (hUser) {
            typedef UINT(WINAPI* pfnGetDpiForWindow)(HWND);
            pfnGetDpiForWindow getDpiForWindow = (pfnGetDpiForWindow)GetProcAddress(hUser, "GetDpiForWindow");
            if (getDpiForWindow && hwnd) {
                dpi = getDpiForWindow(hwnd);
                return static_cast<float>(dpi) / 96.0f;
            }

            typedef UINT(WINAPI* pfnGetDpiForSystem)();
            pfnGetDpiForSystem getDpiForSystem = (pfnGetDpiForSystem)GetProcAddress(hUser, "GetDpiForSystem");
            if (getDpiForSystem) {
                dpi = getDpiForSystem();
                return static_cast<float>(dpi) / 96.0f;
            }
        }
        return 1.0f;
    }

    /**
     * 把非负 DIP 尺寸按缩放换算为整数像素，加入 0.5 再截断以四舍五入。
     */
    static int DipToPx(float dip, float scale) {
        return static_cast<int>(dip * scale + 0.5f);
    }
};
