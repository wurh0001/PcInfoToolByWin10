#pragma once
#include <windows.h>
#include "ui/rendering/LayeredD2DRenderer.h"
#include "core/collectors/MetricsCollector.h"
#include "ui/controls/CustomContextMenu.h"
#include "pro/rendering/ProOverviewVisuals.h"
#include "pro/interaction/ProWindowBehavior.h"

class ProDashboardWindow;
class ProMiniBarTooltipWindow;

class ProMiniBarWindow {
public:
    ProMiniBarWindow();
    ~ProMiniBarWindow();

    bool Create();
    void Run();

    HWND GetHwnd() const { return m_hwnd; }
    RECT GetWindowRectDip() const;

private:
    friend struct ProOverviewRenderTest;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void DrawWhiteBar(ID2D1DCRenderTarget* rt);
    void OnTimer();
    void ShowHoverPeek();
    void ToggleDashboard();
    void OnSegmentClick(int segmentIndex);
    bool ApplyGeneralChange(ProGeneralSettings& value,std::wstring& error);
    void CapturePosition(ProGeneralSettings& value) const;
    void SavePosition();
    void PollFullscreen();
    void ApplyVisibility();
    void ToggleAppVisibility();
    bool m_userHidden=false,m_fullscreenHidden=false,m_restoreDashboard=false,m_positionDirty=false;
    HWND m_fullscreenOverride=nullptr;
    ProWindowBehavior::HotkeyBinding m_hotkeyBinding;

    // Drawing helpers
    void DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    void DrawMiniSparkline(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color);
    void DrawVerticalMeters(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, float percent, D2D1_COLOR_F activeColor);
    void DrawChevron(ID2D1DCRenderTarget* rt, float cx, float cy, float size, bool pointingUp, ID2D1Brush* brush);

private:
    HWND m_hwnd = nullptr;
    LayeredD2DRenderer m_renderer;
    ProOverviewVisuals m_overviewVisuals;
    static constexpr float kShadowApron = 18;
    MetricsCollector m_collector;
    MetricsSnapshot m_snapshot;
    ProDashboardWindow* m_pDashboard = nullptr;
    ProMiniBarTooltipWindow* m_pTooltip = nullptr;

    const float m_dipWidth = 380.0f;
    const float m_dipHeight = 32.0f;
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 380;
    int m_pixelHeight = 32;

    // Mouse tracking & dragging
    POINT m_ptDown = { 0, 0 };
    POINT m_dragStartCursor = { 0, 0 };
    bool m_isDragging = false;
    bool m_hasMoved = false;
    bool m_mouseHover = false;
    bool m_chevronHover = false;

    // Segment bounds: 0: CPU, 1: Temp, 2: RAM, 3: Network, 4: Chevron
    int m_hoveredSegment = -1;
    D2D1_RECT_F m_segmentRects[5]{};

    // Cache brushes
    ID2D1SolidColorBrush* m_pBgBrush = nullptr;
    ID2D1SolidColorBrush* m_pBorderBrush = nullptr;
    ID2D1SolidColorBrush* m_pSepBrush = nullptr;
    ID2D1SolidColorBrush* m_pTextPrimary = nullptr;
    ID2D1SolidColorBrush* m_pTextSecondary = nullptr;
    ID2D1SolidColorBrush* m_pTextMuted = nullptr;
    ID2D1SolidColorBrush* m_pCpuBlue = nullptr;
    ID2D1SolidColorBrush* m_pRamAmber = nullptr;
    ID2D1SolidColorBrush* m_pNetGreen = nullptr;
    ID2D1SolidColorBrush* m_pNetSky = nullptr;
    ID2D1SolidColorBrush* m_pHoverBrush = nullptr;
    ID2D1SolidColorBrush* m_pHoverPillBg = nullptr;
    ID2D1SolidColorBrush* m_pHoverPillBorder = nullptr;

    void ReleaseBrushes();
    void EnsureBrushes(ID2D1DCRenderTarget* rt);
};
