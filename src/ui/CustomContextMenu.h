#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "Theme.h"
#include "LayeredD2DRenderer.h"

class CustomContextMenu {
public:
    struct MenuItem {
        int id = 0;
        std::wstring text;
        bool isSeparator = false;
        bool isChecked = false;
        bool isSubmenu = false;
        std::vector<MenuItem> subItems;
    };

    CustomContextMenu();
    ~CustomContextMenu();

    void SetTheme(Theme::ThemePreset preset);
    void AddItem(int id, const std::wstring& text, bool isChecked = false);
    void AddSeparator();
    void AddSubMenu(const std::wstring& text, const std::vector<MenuItem>& subItems);

    // Track popup menu at screen position, returns chosen cmd id (0 if canceled)
    int Track(HWND hParent, int screenX, int screenY);

private:
    static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SubmenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RenderMenu(HWND hwnd, const std::vector<MenuItem>& items, int hoveredIndex);
    void MeasureMenu(const std::vector<MenuItem>& items, int& outWidth, int& outHeight);

    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass;
    std::vector<MenuItem> m_items;

    HWND m_hMenuWnd = nullptr;
    HWND m_hSubmenuWnd = nullptr;
    LayeredD2DRenderer m_renderer;
    LayeredD2DRenderer m_subRenderer;

    int m_hoveredIndex = -1;
    int m_selectedCmd = 0;
    bool m_looping = false;
    float m_dpiScale = 1.0f;

    // Submenu state
    int m_openSubmenuIndex = -1;
    int m_subHoveredIndex = -1;
    RECT m_submenuRect = {};
};
