#pragma once
#include <windows.h>
#include "LayeredD2DRenderer.h"
#include "FlyoutWindow.h"
#include "../core/MetricsCollector.h"

enum class DockEdge {
    None,
    Top,
    Bottom,
    Left,
    Right
};

enum class ThemeStyle {
    RingGlass,  // Style: Liquid Frosted Glass with circular ring gauges (User reference image)
    BarDark     // Style: Obsidian Slate with horizontal progress bars
};

class MiniBarWindow {
public:
    MiniBarWindow();
    ~MiniBarWindow();

    bool Create();
    void Run();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void RenderBarDarkStyle(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, D2D1_COLOR_F cpuStatusCol, float W, float H);
    void RenderRingGlassStyle(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, D2D1_COLOR_F cpuStatusCol, float W, float H);

    void OnTimer();
    void ToggleFlyout();
    void UpdateDockState();
    void UndockFromEdge();
    void SetLightMode(bool light);
    void SetThemeStyle(ThemeStyle style);
    void SetThemePreset(Theme::ThemePreset preset);
    Theme::ThemePreset GetThemePreset() const { return m_preset; }
    void SetAutoHideOnHover(bool enable);
    bool IsAutoHideOnHover() const { return m_autoHideOnHover; }
    void SaveSettings();
    void LoadSettings();
    D2D1_RECT_F GetCollapsedStripeRectDip() const;

    void DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawRamIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawGpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawChevron(ID2D1DCRenderTarget* rt, float centerX, float centerY, ID2D1Brush* brush);
    void DrawRingGauge(ID2D1DCRenderTarget* rt, float cx, float cy, float radius, float strokeWidth, float percent, ID2D1Brush* trackBrush, ID2D1Brush* activeBrush);
    void RenderCollapsedTriBar(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, float W, float H);

private:
    HWND m_hwnd = nullptr;
    LayeredD2DRenderer m_renderer;
    MetricsCollector m_collector;
    MetricsSnapshot m_snapshot;
    FlyoutWindow m_flyout;

    DockEdge m_dockEdge = DockEdge::None;
    DockEdge m_initialDockEdge = DockEdge::None;
    ThemeStyle m_style = ThemeStyle::RingGlass; // Default to new liquid glass ring style!
    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass; // Default to new translucent glass style!
    bool m_isCollapsed = false;
    bool m_wasCollapsedBeforeDrag = false;
    bool m_mouseTracking = false;
    bool m_isDragging = false;
    bool m_isLightMode = true; // Default light mode
    bool m_autoHideOnHover = false;
    bool m_isHoverHidden = false;

    // Proportional dimensions matching the exact UI designs: 345x40
    const float m_dipWidth = 345.0f;
    const float m_dipHeight = 40.0f;

    int m_pixelWidth = 345;
    int m_pixelHeight = 40;
    float m_dpiScale = 1.0f;

    // Instant click & smooth native drag tracking
    POINT m_ptDown = { 0, 0 };
    POINT m_dragStartCursor = { 0, 0 };
    POINT m_lastDragMousePos = { 0, 0 };
    ULONGLONG m_timeDown = 0;
    bool m_hasMovedDuringDrag = false;
};
