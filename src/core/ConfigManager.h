#pragma once
#include <windows.h>
#include <string>
#include "../ui/Theme.h"

namespace ConfigManager {

    struct AppConfig {
        int posX = -1;              // -1: default calculation
        int posY = -1;
        int dockEdge = 0;           // 0: None, 1: Top, 2: Bottom, 3: Left, 4: Right
        int themePreset = 0;        // 0: TranslucentGlass, 1: GlacierWhite, 2: ObsidianDark, 3: AuroraGlass, 4: CyberNeon
        int themeStyle = 0;         // 0: RingGlass, 1: BarDark
        bool autoHideOnHover = false;
        bool autoStartWithWindows = false;
    };

    inline std::wstring GetConfigFilePath() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring path(exePath);
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            path = path.substr(0, lastSlash + 1);
        }
        path += L"config.ini";
        return path;
    }

    inline AppConfig LoadConfig() {
        AppConfig cfg;
        std::wstring ini = GetConfigFilePath();

        cfg.posX = GetPrivateProfileIntW(L"Window", L"PosX", -1, ini.c_str());
        cfg.posY = GetPrivateProfileIntW(L"Window", L"PosY", -1, ini.c_str());
        cfg.dockEdge = GetPrivateProfileIntW(L"Window", L"DockEdge", 0, ini.c_str());
        cfg.themePreset = GetPrivateProfileIntW(L"Appearance", L"ThemePreset", 0, ini.c_str());
        cfg.themeStyle = GetPrivateProfileIntW(L"Appearance", L"ThemeStyle", 0, ini.c_str());
        cfg.autoHideOnHover = (GetPrivateProfileIntW(L"Behavior", L"AutoHideOnHover", 0, ini.c_str()) != 0);

        // Check if autorun registry exists
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[MAX_PATH] = {};
            DWORD len = sizeof(val);
            if (RegQueryValueExW(hKey, L"PcInfoTool", NULL, NULL, (LPBYTE)val, &len) == ERROR_SUCCESS) {
                cfg.autoStartWithWindows = true;
            }
            RegCloseKey(hKey);
        }

        return cfg;
    }

    inline void SaveConfig(const AppConfig& cfg) {
        std::wstring ini = GetConfigFilePath();

        WritePrivateProfileStringW(L"Window", L"PosX", std::to_wstring(cfg.posX).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Window", L"PosY", std::to_wstring(cfg.posY).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Window", L"DockEdge", std::to_wstring(cfg.dockEdge).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Appearance", L"ThemePreset", std::to_wstring(cfg.themePreset).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Appearance", L"ThemeStyle", std::to_wstring(cfg.themeStyle).c_str(), ini.c_str());
        WritePrivateProfileStringW(L"Behavior", L"AutoHideOnHover", cfg.autoHideOnHover ? L"1" : L"0", ini.c_str());
    }

    inline bool SetAutoStart(bool enable) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, L"PcInfoTool", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"PcInfoTool");
        }
        RegCloseKey(hKey);
        return true;
    }

    inline bool IsAutoStartEnabled() {
        HKEY hKey;
        bool enabled = false;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t val[MAX_PATH] = {};
            DWORD len = sizeof(val);
            if (RegQueryValueExW(hKey, L"PcInfoTool", NULL, NULL, (LPBYTE)val, &len) == ERROR_SUCCESS) {
                enabled = true;
            }
            RegCloseKey(hKey);
        }
        return enabled;
    }
}
