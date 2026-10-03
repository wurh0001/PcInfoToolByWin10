#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "../ui/LayeredD2DRenderer.h"
#include "../core/MetricsSnapshot.h"

class ProMiniBarTooltipWindow {
public:
    ProMiniBarTooltipWindow();
    ~ProMiniBarTooltipWindow();

    bool Create(HWND hOwner);
    void Show(int segmentIndex, int barLeftDip, int barBottomDip, const MetricsSnapshot& snapshot, float dpiScale);
    void Hide();
    bool IsVisible() const { return m_visible; }

    void SetClickCallback(std::function<void(int segmentIndex)> cb) {
        m_onClick = cb;
    }
    void SetOpacity(float opacity) {
        m_opacity = opacity;
        ReleaseBrushes();
        if (m_visible) Render();
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();

private:
    HWND m_hwnd = nullptr;
    HWND m_hOwner = nullptr;
    LayeredD2DRenderer m_renderer;

    float m_dipWidth = 200.0f;
    float m_dipHeight = 56.0f;
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 200;
    int m_pixelHeight = 56;

    bool m_visible = false;
    int m_segmentIndex = -1;
    MetricsSnapshot m_snapshot;
    std::function<void(int segmentIndex)> m_onClick;
    float m_opacity = 0.86f;

    ID2D1SolidColorBrush* m_pCardBg = nullptr;
    ID2D1SolidColorBrush* m_pCardBorder = nullptr;
    ID2D1SolidColorBrush* m_pCardShadow = nullptr;
    ID2D1SolidColorBrush* m_pSepBrush = nullptr;
    ID2D1SolidColorBrush* m_pTextLabel = nullptr;
    ID2D1SolidColorBrush* m_pTextVal = nullptr;
    ID2D1SolidColorBrush* m_pCpuBlue = nullptr;
    ID2D1SolidColorBrush* m_pTempCoral = nullptr;
    ID2D1SolidColorBrush* m_pRamAmber = nullptr;
    ID2D1SolidColorBrush* m_pNetGreen = nullptr;

    void ReleaseBrushes();
    void EnsureBrushes(ID2D1DCRenderTarget* rt);
};
