#pragma once
#include <windows.h>
#include "../ui/LayeredD2DRenderer.h"
#include "../core/MetricsSnapshot.h"
#include "ProOverviewVisuals.h"
#include <vector>
#include <functional>
#include <algorithm>

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
        int themeMode = 1;           // 0: 亮色玻璃, 1: 纯白 (默认), 2: 深色, 3: 跟随系统
        int layoutMode = 0;          // 0: 完整长条 (FullRows), 1: 紧凑胶囊 (Compact)
        float glassOpacity = 0.40f;  // 40% (背景透明度 60%)
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
    void SetAppearance(int theme,int layout,float opacity) {m_settings.themeMode=std::clamp(theme,0,3);m_settings.layoutMode=std::clamp(layout,0,1);m_settings.glassOpacity=std::clamp(opacity,.1f,1.f);}
    void SetAppearanceSaveFailed(bool failed) {m_appearanceSaveFailed=failed;}
    AccordionModule GetExpandedModule() const { return static_cast<AccordionModule>(m_expandedModule); }
    HWND GetHwnd() const { return m_hwnd; }

    void UpdateSnapshot(const MetricsSnapshot& snapshot);
    bool IsPinned() const { return m_isPinned; }
    void SetPinned(bool pin);
    void ExpandModule(AccordionModule mod);
    void SetOnSettingsChanged(std::function<void(const SettingsState&)> cb) { m_onSettingsChanged = cb; }

    // 主题解析与判定
    // themeMode: 0: 亮色玻璃, 1: 纯白, 2: 深色, 3: 跟随系统 (系统亮色解析为纯白，暗色解析为深色)
    bool IsPureWhite() const {
        if (m_settings.themeMode == 1) return true;
        if (m_settings.themeMode == 3) {
            return !SystemUsesDarkTheme();
        }
        return false;
    }
    static bool SystemUsesDarkTheme() {
        DWORD light=1,size=sizeof(light);
        return RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size)==ERROR_SUCCESS && light==0;
    }

    // 分层材质透明度映射 (Material Layering Hierarchy)
    // t: 0.10f ~ 1.00f (默认 0.40f)
    float GetMiniBarBgAlpha() const {
        if (IsPureWhite()) return 1.0f; // 纯白模式: 不透明 1.0
        if (m_settings.reduceTransparency) return 0.95f;
        float t = std::clamp(m_settings.glassOpacity, 0.10f, 1.00f);
        return std::clamp(0.20f + t * 0.60f, 0.25f, 0.85f); // 默认 40% 滑块对应 0.44f
    }
    float GetDashboardBgAlpha() const {
        if (IsPureWhite()) return 1.0f; // 纯白模式: 不透明 1.0
        if (m_settings.reduceTransparency) return 0.96f;
        float t = std::clamp(m_settings.glassOpacity, 0.10f, 1.00f);
        return std::clamp(0.60f + t * 0.35f, 0.60f, 0.96f); // 默认 40% 滑块对应 0.74f (扎实半透)
    }
    float GetTooltipBgAlpha() const {
        if (IsPureWhite()) return 1.0f; // 纯白模式: 不透明 1.0
        if (m_settings.reduceTransparency) return 0.95f;
        float t = std::clamp(m_settings.glassOpacity, 0.10f, 1.00f);
        return std::clamp(0.70f + t * 0.25f, 0.70f, 0.95f); // 默认 40% 滑块对应 0.80f (轻灵透澈)
    }

private:
    friend struct ProOverviewRenderTest;
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render();
    void UpdateDimensionsAndPosition();

    static float GetDetailCardHeight(int mod);

    // Section Renderers
    void DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width);
    void DrawWhiteOverview(ID2D1DCRenderTarget* rt);
    void DrawWhiteCpuDetails(ID2D1DCRenderTarget* rt, bool standalone = false);
    void DrawWhiteCpuDirect(ID2D1DCRenderTarget* rt);
    void DrawWhiteTempDirect(ID2D1DCRenderTarget* rt);
    void DrawSettingsAppearance(ID2D1DCRenderTarget* rt);
    bool m_appearanceSaveFailed=false;
    void DrawCpuCorePeek(ID2D1DCRenderTarget* rt);
    bool IsWhiteCpu() const { return IsPureWhite() && (m_mode == Mode_DirectCpu || (m_mode == Mode_FullAccordion && m_expandedModule == Module_CPU)); }
    int HitCpuCore(float x, float y) const;
    bool OnCpuKey(UINT key, bool shift);
    void ScrollCpu(float delta);
    void DrawWhiteGpuDetails(ID2D1DCRenderTarget* rt);
    void DrawWhiteMemoryDetails(ID2D1DCRenderTarget* rt, bool standalone = false);
    void DrawWhiteMemoryDirect(ID2D1DCRenderTarget* rt);
    void DrawWhiteDiskDetails(ID2D1DCRenderTarget* rt);
    const PhysicalDiskInfo* SelectedDisk() const;
    void ReconcileDiskSelection();
    bool OnDiskKey(UINT key);
    std::wstring m_selectedDiskId;
    bool m_diskMenuOpen=false;
    int m_diskMenuFocus=0, m_diskMenuFirst=0, m_diskMenuHover=-1;
    D2D1_RECT_F m_diskTriggerRect{},m_diskMenuRect{};
    std::vector<D2D1_RECT_F> m_diskOptionRects;
    std::vector<std::wstring> m_diskOptionIds;
    void DrawWhiteSensorDetails(ID2D1DCRenderTarget* rt);
    void DrawWhiteNetworkDetails(ID2D1DCRenderTarget* rt, bool standalone = false);
    void DrawWhiteNetworkDirect(ID2D1DCRenderTarget* rt);
    float CpuCoreExtraHeight() const {
        const size_t rows = (m_snapshot.cpuCoreUsage.size() + 7) / 8;
        return rows > 2 ? static_cast<float>((rows - 2) * 34) : 0;
    }
    bool IsWhiteOverview() const {
        return IsPureWhite() && m_mode == Mode_FullAccordion &&
            (m_expandedModule >= Module_None && m_expandedModule < Module_Count) &&
            (m_settings.layoutMode == 0 || (m_expandedModule >= Module_CPU && m_expandedModule < Module_Count)) &&
            (!m_isPinned || (m_expandedModule >= Module_CPU && m_expandedModule < Module_Count));
    }
    void DrawModuleCard(ID2D1DCRenderTarget* rt, AccordionModule mod, float& curY, float width);
    void DrawCompactCapsule(ID2D1DCRenderTarget* rt, AccordionModule mod, float x, float y, float w, float h);
    void DrawCompactGrid(ID2D1DCRenderTarget* rt, const std::vector<AccordionModule>& mods, float& curY, float contentX, float contentW);
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
    ProOverviewVisuals m_overviewVisuals;
    float m_shadowApron = 0;
    MetricsSnapshot m_snapshot;

    bool m_visible = false;
    bool m_isPinned = false;
    int m_expandedModule = Module_None; // Default Overview state matching Figure 01

    float m_dipWidth = 360.0f;
    float m_dipHeight = 294.0f;
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 360;
    int m_pixelHeight = 294;

    RECT m_anchorBarRectDip = { 0, 0, 0, 0 };

    WindowMode m_mode = Mode_Accordion;
    int m_alertTempThreshold = 85;

    // Hover & Interactive Probe
    int m_hoveredModule = -1;
    std::vector<D2D1_RECT_F> m_cpuCoreRects;
    int m_hoveredCpuCore = -1, m_focusedCpuCore = -1;
    bool m_cpuKeyboardFocus = false;
    float m_cpuFullHeight = 0, m_cpuScroll = 0;
    bool m_cpuPlacementValid = false, m_cpuAbove = false;
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
    uint64_t m_netSampleUnixSeconds = 0;
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
    D2D1_RECT_F m_themeRects[4]{};
    D2D1_RECT_F m_layoutOptionRects[2]{};
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
