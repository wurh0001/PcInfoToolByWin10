#pragma once
#include <windows.h>
#include "LayeredD2DRenderer.h"
#include "../core/MetricsSnapshot.h"
#include "Theme.h"

class FlyoutWindow {
public:
    FlyoutWindow();
    ~FlyoutWindow();

    bool Create();
    void ShowNear(const RECT& anchorRect);
    void Hide();
    bool IsVisible() const { return m_visible; }
    HWND GetHwnd() const { return m_hwnd; }

    void UpdateSnapshot(const MetricsSnapshot& snapshot);
    void SetLightMode(bool lightMode);
    void SetThemePreset(Theme::ThemePreset preset);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    void DrawCpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    void DrawMemorySection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    void DrawNetworkSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    void DrawDiskSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);

private:
    HWND m_hwnd = nullptr;
    LayeredD2DRenderer m_renderer;
    bool m_visible = false;
    bool m_isLightMode = false;
    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass;
    MetricsSnapshot m_snapshot;

    // Compact DIP dimensions
    const float m_dipWidth = 330.0f;
    const float m_dipHeight = 440.0f;

    int m_pixelWidth = 330;
    int m_pixelHeight = 440;
    float m_dpiScale = 1.0f;
};
