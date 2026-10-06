#pragma once
#include <windows.h>
#include <string>
#include "ui/rendering/LayeredD2DRenderer.h"
#include "core/metrics/MetricsSnapshot.h"
#include "pro/rendering/ProOverviewVisuals.h"

class ProMiniBarTooltipWindow {
public:
    ProMiniBarTooltipWindow();
    ~ProMiniBarTooltipWindow();

    bool Create(HWND hOwner);
    void Show(int segmentIndex, RECT barPixels, const MetricsSnapshot& snapshot, float dpiScale);
    void SetTempUnit(int unit) { m_fahrenheit = unit == 1; }
    void SetAlwaysOnTop(bool value){m_alwaysOnTop=value;if(m_hwnd)SetWindowPos(m_hwnd,value?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
    void Hide();
    bool IsVisible() const { return m_visible; }

    void SetOpacity(float opacity) {
        m_opacity = opacity;
        ReleaseBrushes();
        if (m_visible) Render();
    }
    void SetPureWhite(bool pw) {
        if (m_isPureWhite != pw) Hide();
        m_isPureWhite = pw;
        ReleaseBrushes();
        if (m_visible) Render();
    }

private:
    friend struct ProOverviewRenderTest;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void DrawWhitePeek(ID2D1DCRenderTarget* rt);
    void FormatValues(wchar_t (&left)[64], wchar_t (&right)[64]) const;
    void ConfigureSize(int segment);
    static POINT Place(RECT bar, RECT work, float scale, float offset, float gap, float width, float height);

private:
    HWND m_hwnd = nullptr;
    HWND m_hOwner = nullptr;
    LayeredD2DRenderer m_renderer;
    ProOverviewVisuals m_visuals;
    static constexpr float kShadowApron = 24;
    bool m_fahrenheit = false;
    bool m_alwaysOnTop=true;

    float m_dipWidth = 200.0f;
    float m_dipHeight = 56.0f;
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 200;
    int m_pixelHeight = 56;

    bool m_visible = false;
    int m_segmentIndex = -1;
    MetricsSnapshot m_snapshot;
    float m_opacity = 0.86f;
    bool m_isPureWhite = false;

    ID2D1SolidColorBrush* m_pCardBg = nullptr;
    ID2D1SolidColorBrush* m_pCardBorder = nullptr;
    ID2D1SolidColorBrush* m_pSepBrush = nullptr;

    void ReleaseBrushes();
    void EnsureBrushes(ID2D1DCRenderTarget* rt);
};
