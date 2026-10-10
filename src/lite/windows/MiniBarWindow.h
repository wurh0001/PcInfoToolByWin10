/*
 * 模块说明：Lite 主悬浮条接口与状态
 * 主窗口持有共享采集器、渲染器和展开窗口，管理主题、拖动、贴边与配置。
 * 成员变量记录交互状态；WndProc 从 HWND 找到对象后分发消息。
 */
#pragma once
#include <windows.h>
#include "ui/rendering/LayeredD2DRenderer.h"
#include "lite/windows/FlyoutWindow.h"
#include "core/collectors/MetricsCollector.h"

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

    /**
     * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
     */
    bool Create();
    /**
     * 进入 GetMessage/DispatchMessage 消息循环；收到退出消息后返回。
     */
    void Run();

private:
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
     * 绘制 Lite 条形样式，使用当前主题颜色和采样快照。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void RenderBarDarkStyle(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, D2D1_COLOR_F cpuStatusCol, float W, float H);
    /**
     * 绘制 Lite 圆环玻璃样式，使用当前主题颜色和采样快照。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void RenderRingGlassStyle(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, D2D1_COLOR_F cpuStatusCol, float W, float H);

    /**
     * 定时读取最新数据并更新可见窗口，同时处理该窗口的周期性状态。
     */
    void OnTimer();
    /**
     * 切换 Lite 展开窗口的可见状态，并按悬浮条位置更新展开位置。
     */
    void ToggleFlyout();
    /**
     * 检查窗口与显示器工作区的关系，更新贴边方向和显示状态。
     */
    void UpdateDockState();
    /**
     * 退出贴边状态，调整窗口位置以恢复可见主体。
     */
    void UndockFromEdge();
    /**
     * 修改亮色模式并同步相关页面或重新绘制。
     */
    void SetLightMode(bool light);
    /**
     * 切换 Lite 展示样式并保存选择。
     */
    void SetThemeStyle(ThemeStyle style);
    /**
     * 切换主题预设，使下一帧使用对应调色板。
     */
    void SetThemePreset(Theme::ThemePreset preset);
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    Theme::ThemePreset GetThemePreset() const { return m_preset; }
    /**
     * 设置鼠标悬停时的自动隐藏行为，并同步显示。
     */
    void SetAutoHideOnHover(bool enable);
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsAutoHideOnHover() const { return m_autoHideOnHover; }
    /**
     * 收集当前窗口位置与外观选项并写入配置。
     */
    void SaveSettings();
    /**
     * 读取保存的窗口和主题设置，应用到当前窗口状态。
     */
    void LoadSettings();
    /**
     * 返回折叠贴边条的 DIP 命中区域，供绘制和交互共用。
     */
    D2D1_RECT_F GetCollapsedStripeRectDip() const;

    /**
     * 绘制CPU图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    /**
     * 绘制内存图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawRamIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    /**
     * 绘制GPU图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawGpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush);
    /**
     * 用线段/几何绘制展开箭头，方向由参数或状态决定。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawChevron(ID2D1DCRenderTarget* rt, float centerX, float centerY, ID2D1Brush* brush);
    /**
     * 按百分比绘制圆环轨道和有效弧段，单位和颜色由调用方提供。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param percent 比例数值，绘图前需限制到有效范围。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawRingGauge(ID2D1DCRenderTarget* rt, float cx, float cy, float radius, float strokeWidth, float percent, ID2D1Brush* trackBrush, ID2D1Brush* activeBrush);
    /**
     * 绘制 Lite 折叠条中的三个指标。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void RenderCollapsedTriBar(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, float W, float H);

private:
    // HWND 是系统窗口句柄；本 C++ 对象通过窗口用户数据接收消息。
    HWND m_hwnd = nullptr;
    // 持有画布、DC 和绘图接口；析构会清理自身资源。
    LayeredD2DRenderer m_renderer;
    // 本窗口拥有采集器；退出前需 Stop，确保后台线程不再访问成员。
    MetricsCollector m_collector;
    // UI 私有快照，与后台数据分离；绘图从这里读取。
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
    // DIP 到像素的缩放，96 DPI 为 1；跨显示器移动时需更新。
    float m_dpiScale = 1.0f;

    // Instant click & smooth native drag tracking
    POINT m_ptDown = { 0, 0 };
    POINT m_dragStartCursor = { 0, 0 };
    POINT m_lastDragMousePos = { 0, 0 };
    ULONGLONG m_timeDown = 0;
    bool m_hasMovedDuringDrag = false;
};
