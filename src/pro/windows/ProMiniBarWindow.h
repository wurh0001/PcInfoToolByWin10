/*
 * 模块说明：Pro 主悬浮条接口与状态
 * 主窗口连接采集、设置、详情页、提示卡和右键菜单，是 Pro 交互的入口。
 * 详情和提示的 C++ 对象与它们的 HWND/画布并非同一生命周期，资源按需准备。
 */
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

    /**
     * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
     */
    bool Create();
    /**
     * 进入 GetMessage/DispatchMessage 消息循环；收到退出消息后返回。
     */
    void Run();

    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    HWND GetHwnd() const { return m_hwnd; }
    /**
     * 返回用于详情定位的悬浮条锚点矩形；使用前注意实现中的屏幕坐标约定。
     */
    RECT GetWindowRectDip() const;

private:
    friend struct ProOverviewRenderTest;
    /**
     * Win32 窗口过程：按消息类型分发交互；用窗口用户数据关联 C++ 实例。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     * @param msg 窗口消息编号，决定后续分支。
     * @param wParam 消息附加参数，含义随 msg 改变。
     * @param lParam 消息附加参数，鼠标坐标等需按消息格式解码。
     */
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    /**
     * 按当前状态绘制并提交一帧；数据来自已保存快照，绘图不等于重新采集硬件。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void Render();
    /**
     * 绘制纯白悬浮条的指标、数值和概览入口。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteBar(ID2D1DCRenderTarget* rt);
    /**
     * 定时读取最新数据并更新可见窗口，同时处理该窗口的周期性状态。
     */
    void OnTimer();
    /**
     * 依据悬停指标显示只读提示卡，拖动或详情显示时按规则隐藏。
     */
    void ShowHoverPeek();
    /**
     * 依据当前显示模式展开或收起概览，保持悬浮条状态同步。
     */
    void ToggleDashboard();
    /**
     * 把悬浮条分段索引映射为独立详情或概览的打开操作。
     */
    void OnSegmentClick(int segmentIndex);
    /**
     * 应用窗口行为和快捷键等常规设置，把失败原因交回设置页面。
     */
    bool ApplyGeneralChange(ProGeneralSettings& value,std::wstring& error);
    /**
     * 记录窗口当前位置和 DPI 到设置结构，供以后恢复。
     */
    void CapturePosition(ProGeneralSettings& value) const;
    /**
     * 依据记忆位置开关保存窗口位置，避免无条件覆盖用户配置。
     */
    void SavePosition();
    /**
     * 检查前台窗口是否覆盖显示器，更新全屏隐藏状态。
     */
    void PollFullscreen();
    /**
     * 根据应用可见状态和全屏隐藏规则更新实际窗口显示。
     */
    void ApplyVisibility();
    /**
     * 切换应用可见状态，用于全局快捷键入口。
     */
    void ToggleAppVisibility();
    bool m_userHidden=false,m_fullscreenHidden=false,m_restoreDashboard=false,m_positionDirty=false;
    HWND m_fullscreenOverride=nullptr;
    ProWindowBehavior::HotkeyBinding m_hotkeyBinding;

    // Drawing helpers
    /**
     * 绘制CPU图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    /**
     * 把历史点映射到指定矩形，按最大值缩放并处理无效点。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawMiniSparkline(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color);
    /**
     * 把利用率映射为竖向填充高度，背景表示未使用部分。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param percent 比例数值，绘图前需限制到有效范围。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawVerticalMeters(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, float percent, D2D1_COLOR_F activeColor);
    /**
     * 用线段/几何绘制展开箭头，方向由参数或状态决定。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawChevron(ID2D1DCRenderTarget* rt, float cx, float cy, float size, bool pointingUp, ID2D1Brush* brush);

private:
    // HWND 是系统窗口句柄；本 C++ 对象通过窗口用户数据接收消息。
    HWND m_hwnd = nullptr;
    // 持有画布、DC 和绘图接口；析构会清理自身资源。
    LayeredD2DRenderer m_renderer;
    ProOverviewVisuals m_overviewVisuals;
    static constexpr float kShadowApron = 18;
    // 本窗口拥有采集器；退出前需 Stop，确保后台线程不再访问成员。
    MetricsCollector m_collector;
    // UI 私有快照，与后台数据分离；绘图从这里读取。
    MetricsSnapshot m_snapshot;
    // 由主窗口 new/delete 管理的详情对象；其 HWND 和画布可以延迟创建。
    ProDashboardWindow* m_pDashboard = nullptr;
    ProMiniBarTooltipWindow* m_pTooltip = nullptr;

    const float m_dipWidth = 380.0f;
    const float m_dipHeight = 32.0f;
    // DIP 到像素的缩放，96 DPI 为 1；跨显示器移动时需更新。
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 380;
    int m_pixelHeight = 32;

    // Mouse tracking & dragging
    POINT m_ptDown = { 0, 0 };
    POINT m_dragStartCursor = { 0, 0 };
    /**
     * 根据屏幕鼠标位移移动悬浮条，避免位置变化触发完整重绘。
     */
    void MoveDragTo(POINT cursor);
    bool m_isDragging = false;
    bool m_hasMoved = false;
    bool m_mouseHover = false;
    bool m_chevronHover = false;

    // Segment bounds: 0: CPU, 1: Temp, 2: RAM, 3: Network, 4: Chevron
    int m_hoveredSegment = -1;
    D2D1_RECT_F m_segmentRects[5]{};

    // Cache brushes
    // 画笔为 COM 接口，需 Release；它们与创建它们的绘图目标绑定。
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

    /**
     * 释放当前绘图目标关联的画笔和相应资源，清空指针以便下次重新创建。
     */
    void ReleaseBrushes();
    /**
     * 首次使用或重建目标后创建画笔，已有可用资源时复用。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     */
    void EnsureBrushes(ID2D1DCRenderTarget* rt);
};
