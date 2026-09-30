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
    void DrawGpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    void DrawDiskSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);

    // Vector Icon Helpers
    void DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawRamIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawGpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawDiskIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);

    // Reusable Waveform Plotter with grid, smooth curve, gradient fill, and beacon
    void DrawWaveform(
        ID2D1DCRenderTarget* rt,
        float chartX, float chartY, float chartW, float chartH,
        const std::vector<float>& history,
        float maxVal,
        D2D1_COLOR_F strokeCol,
        ID2D1Brush* gridBrush,
        ID2D1Brush* textBrush,
        const wchar_t* scaleTopText = nullptr,
        const wchar_t* scaleMidText = nullptr
    );

private:
    HWND m_hwnd = nullptr;
    LayeredD2DRenderer m_renderer;
    bool m_visible = false;
    bool m_isLightMode = false;
    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass;
    MetricsSnapshot m_snapshot;

    // DIP dimensions matching the full-featured dashboard with GPU + Net charts
    const float m_dipWidth = 330.0f;
    const float m_dipHeight = 540.0f;

    int m_pixelWidth = 330;
    int m_pixelHeight = 540;
    float m_dpiScale = 1.0f;
};
