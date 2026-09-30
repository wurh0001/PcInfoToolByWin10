#pragma once
#include <windows.h>

class DpiHelper {
public:
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

    static int DipToPx(float dip, float scale) {
        return static_cast<int>(dip * scale + 0.5f);
    }
};
