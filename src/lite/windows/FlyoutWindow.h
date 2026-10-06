#pragma once
#include <windows.h>
#include "ui/rendering/LayeredD2DRenderer.h"
#include "core/metrics/MetricsSnapshot.h"
#include "ui/theme/Theme.h"

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
    void SetHideCallback(void (*callback)(void*), void* context) {
        m_hideCallback = callback;
        m_hideContext = context;
    }

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
        const MetricHistory& history,
        float maxVal,
        ID2D1SolidColorBrush* strokeBrush,
        ID2D1LinearGradientBrush* areaBrush,
        D2D1_COLOR_F strokeCol,
        ID2D1Brush* gridBrush,
        ID2D1Brush* textBrush,
        const wchar_t* scaleTopText = nullptr,
        const wchar_t* scaleMidText = nullptr
    );

    struct FlyoutBrushPool {
        ID2D1SolidColorBrush* pWindowBg = nullptr;
        ID2D1SolidColorBrush* pWindowBorder = nullptr;
        ID2D1SolidColorBrush* pCardBg = nullptr;
        ID2D1SolidColorBrush* pCardBorder = nullptr;
        ID2D1SolidColorBrush* pPillBg = nullptr;
        ID2D1SolidColorBrush* pTextPrimary = nullptr;
        ID2D1SolidColorBrush* pTextSecondary = nullptr;
        ID2D1SolidColorBrush* pTextMuted = nullptr;
        ID2D1SolidColorBrush* pTrackBg = nullptr;
        ID2D1SolidColorBrush* pGridLine = nullptr;
        ID2D1SolidColorBrush* pRamPurple = nullptr;
        ID2D1SolidColorBrush* pNetDownGreen = nullptr;
        ID2D1SolidColorBrush* pNetUpBlue = nullptr;
        ID2D1SolidColorBrush* pGpuCyan = nullptr;
        ID2D1SolidColorBrush* pDiskAmber = nullptr;
        ID2D1SolidColorBrush* pWhiteCore = nullptr;
        ID2D1SolidColorBrush* pGreenDotGlow = nullptr;
        ID2D1SolidColorBrush* pHaloBrush = nullptr;

        // Dynamic or color-varying brushes
        ID2D1SolidColorBrush* pCpuLine = nullptr;
        D2D1_COLOR_F lastCpuColor = { 0, 0, 0, 0 };

        // Specular Rim Gradient Brush
        ID2D1LinearGradientBrush* pGlassRim = nullptr;
        ID2D1GradientStopCollection* pGlassStops = nullptr;

        // Waveform Area Gradient Brushes & Stops
        ID2D1LinearGradientBrush* pCpuGradBrush = nullptr;
        ID2D1GradientStopCollection* pCpuGradStops = nullptr;
        ID2D1LinearGradientBrush* pRamGradBrush = nullptr;
        ID2D1GradientStopCollection* pRamGradStops = nullptr;
        ID2D1LinearGradientBrush* pNetDownGradBrush = nullptr;
        ID2D1GradientStopCollection* pNetDownGradStops = nullptr;
        ID2D1LinearGradientBrush* pNetUpGradBrush = nullptr;
        ID2D1GradientStopCollection* pNetUpGradStops = nullptr;
        ID2D1LinearGradientBrush* pGpuGradBrush = nullptr;
        ID2D1GradientStopCollection* pGpuGradStops = nullptr;

        Theme::ThemePreset lastPreset = static_cast<Theme::ThemePreset>(-1);
        bool lastLightMode = false;

        void ReleaseAll();
        void EnsureBrushes(
            ID2D1DCRenderTarget* rt,
            const Theme::Palette& pal,
            float H,
            bool isLightMode,
            Theme::ThemePreset preset,
            D2D1_COLOR_F cpuColor
        );
    };

private:
    HWND m_hwnd = nullptr;
    LayeredD2DRenderer m_renderer;
    bool m_visible = false;
    bool m_isLightMode = false;
    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass;
    MetricsSnapshot m_snapshot;
    FlyoutBrushPool m_brushes;
    int m_renderCount = 0;

    // DIP dimensions matching the MiniBar width (345 DIP) for pixel-perfect alignment
    const float m_dipWidth = 345.0f;
    const float m_dipHeight = 540.0f;

    int m_pixelWidth = 345;
    int m_pixelHeight = 540;
    float m_dpiScale = 1.0f;

    void (*m_hideCallback)(void*) = nullptr;
    void* m_hideContext = nullptr;
};
