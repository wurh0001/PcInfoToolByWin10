#pragma once
#include <windows.h>
#include "../ui/LayeredD2DRenderer.h"
#include "../core/MetricsSnapshot.h"
#include <vector>
#include <functional>

class ProDashboardWindow {
public:
    enum AccordionModule {
        Module_None = -1,
        Module_CPU = 0,
        Module_GPU = 1,
        Module_Memory = 2,
        Module_Disk = 3,
        Module_Network = 4,
        Module_Sensor = 5,
        Module_Count = 6
    };

    enum WindowMode {
        Mode_FullAccordion = 0,
        Mode_Accordion = 0,
        Mode_DirectCpu = 1,
        Mode_DirectTemp = 2,
        Mode_CpuTempDirect = 2,
        Mode_DirectMemory = 3,
        Mode_DirectNetwork = 4,
        Mode_DirectGpu = 5,
        Mode_DirectDisk = 6,
        Mode_Settings = 7
    };

    struct SettingsState {
        int activeTab = 0;           // 0: 外观, 1: 监控, 2: 常规
        int themeMode = 0;           // 0: 亮色玻璃, 1: 深色, 2: 跟随系统
        float glassOpacity = 0.40f;  // 40%
        bool showCpu = true;
        bool showTemp = true;
        bool showRam = true;
        bool showNet = true;
        int updateIntervalSec = 1;   // 1s
        int tempUnit = 0;            // 0: °C, 1: °F
        int highTempAlert = 85;      // 85°C
        bool reduceTransparency = false;
    };

    ProDashboardWindow();
    ~ProDashboardWindow();

    bool Create();
    void ShowNear(const RECT& barRectDip);
    void ShowDirectModule(WindowMode directMode, const RECT& barRectDip);
    void ShowCpuTempDirect(const RECT& barRectDip);
    void ShowSettings(const RECT& barRectDip);
    void ShowFullDashboard(AccordionModule mod, const RECT& barRectDip);
    void ToggleDashboard(const RECT& barRectDip);
    void FollowBar(const RECT& barRectDip);
    void Hide();
    bool IsVisible() const { return m_visible; }
    WindowMode GetMode() const { return m_mode; }
    const SettingsState& GetSettings() const { return m_settings; }
    AccordionModule GetExpandedModule() const { return static_cast<AccordionModule>(m_expandedModule); }
    HWND GetHwnd() const { return m_hwnd; }

    void UpdateSnapshot(const MetricsSnapshot& snapshot);
    bool IsPinned() const { return m_isPinned; }
    void SetPinned(bool pin);
    void ExpandModule(AccordionModule mod);
    void SetOnSettingsChanged(std::function<void(const SettingsState&)> cb) { m_onSettingsChanged = cb; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void UpdateDimensionsAndPosition();

    static float GetDetailCardHeight(int mod);

    // Section Renderers
    void DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width);
    void DrawModuleCard(ID2D1DCRenderTarget* rt, AccordionModule mod, float& curY, float width);
    void DrawCpuExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawGpuExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawMemoryExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawDiskExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawNetworkExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawSensorExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawCpuTempDirect(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawCpuDirect(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawMemoryDirect(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawNetworkDirect(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawGpuDirect(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawDiskDirect(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawSettings(ID2D1DCRenderTarget* rt, float width, float height);
    void DrawFooter(ID2D1DCRenderTarget* rt, float& curY, float width);

    // Graphic Helpers
    void DrawMiniSparkline(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color);
    void DrawSmoothWaveform(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F strokeCol, D2D1_COLOR_F fillCol);
    void DrawDualNetworkWaveform(ID2D1DCRenderTarget* rt, float x, float y, float w, float h);
    void DrawChevron(ID2D1DCRenderTarget* rt, float cx, float cy, float size, bool isExpanded, ID2D1Brush* brush);

    // Hit testing
    void OnClick(float x, float y);
    void OnMouseMove(float x, float y);

private:
    HWND m_hwnd = nullptr;
    LayeredD2DRenderer m_renderer;
    MetricsSnapshot m_snapshot;

    bool m_visible = false;
    bool m_isPinned = false;
    int m_expandedModule = Module_None; // Default Overview state matching Figure 01

    float m_dipWidth = 420.0f;
    float m_dipHeight = 355.0f;
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 420;
    int m_pixelHeight = 355;

    RECT m_anchorBarRectDip = { 0, 0, 0, 0 };

    WindowMode m_mode = Mode_Accordion;
    int m_alertTempThreshold = 85;

    // Hover & Interactive Probe
    int m_hoveredModule = -1;
    bool m_pinHover = false;
    bool m_closeHover = false;
    bool m_settingsHover = false;

    // CPU Temp direct card inspection & interaction
    bool m_tempProbeActive = false;
    int m_tempProbeSecAgo = 28;
    float m_tempProbeX = 0.0f;
    D2D1_RECT_F m_tempChartRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_tempTargetRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_tempAlertRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_allSensorsRect = { 0, 0, 0, 0 };
    bool m_allSensorsHover = false;
    D2D1_RECT_F m_overviewLinkRect = { 0, 0, 0, 0 };
    bool m_overviewLinkHover = false;

    // Network chart probe
    bool m_netProbeActive = false;
    float m_netProbeX = 0.0f;
    float m_netProbeY = 0.0f;
    D2D1_RECT_F m_netChartRect = { 0, 0, 0, 0 };

    // Module row bounding boxes for hit testing
    D2D1_RECT_F m_moduleRects[Module_Count]{};
    D2D1_RECT_F m_pinButtonRect{};
    D2D1_RECT_F m_closeButtonRect{};
    D2D1_RECT_F m_settingsButtonRect{};
    D2D1_RECT_F m_settingsHeaderBtnRect{};
    D2D1_RECT_F m_settingsFooterBtnRect{};

    // Settings page state & hit rects
    SettingsState m_settings;
    bool m_sliderDragging = false;
    D2D1_RECT_F m_settingsDoneRect = { 0, 0, 0, 0 };
    bool m_settingsDoneHover = false;
    D2D1_RECT_F m_tabRects[3]{};
    D2D1_RECT_F m_themeRects[3]{};
    D2D1_RECT_F m_sliderTrackRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_metricToggleRects[4]{};
    D2D1_RECT_F m_intervalRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_tempUnitRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_highTempAlertRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_reduceTransRect = { 0, 0, 0, 0 };
    std::function<void(const SettingsState&)> m_onSettingsChanged;

    // D2D Resources
    ID2D1SolidColorBrush* m_pBgBrush = nullptr;
    ID2D1SolidColorBrush* m_pBorderBrush = nullptr;
    ID2D1SolidColorBrush* m_pCardBgBrush = nullptr;
    ID2D1SolidColorBrush* m_pCardBorderBrush = nullptr;
    ID2D1SolidColorBrush* m_pCardHoverBrush = nullptr;
    ID2D1SolidColorBrush* m_pTextPrimary = nullptr;
    ID2D1SolidColorBrush* m_pTextSecondary = nullptr;
    ID2D1SolidColorBrush* m_pTextMuted = nullptr;
    ID2D1SolidColorBrush* m_pCpuBlue = nullptr;
    ID2D1SolidColorBrush* m_pGpuPurple = nullptr;
    ID2D1SolidColorBrush* m_pRamAmber = nullptr;
    ID2D1SolidColorBrush* m_pDiskCyan = nullptr;
    ID2D1SolidColorBrush* m_pNetGreen = nullptr;
    ID2D1SolidColorBrush* m_pNetSky = nullptr;
    ID2D1SolidColorBrush* m_pSensorCoral = nullptr;
    ID2D1SolidColorBrush* m_pTrackBrush = nullptr;
    ID2D1SolidColorBrush* m_pGridBrush = nullptr;

    void ReleaseBrushes();
    void EnsureBrushes(ID2D1DCRenderTarget* rt);
};
