/*
 * 模块说明：Lite 展开窗口接口
 * 保存一份指标快照，按各模块绘制详情；隐藏回调用于通知悬浮条同步状态。
 * 窗口类负责自身绘图资源，传入的数据和回调上下文需遵守调用方生命周期。
 */
#pragma once
#include <windows.h>
#include "ui/rendering/LayeredD2DRenderer.h"
#include "core/metrics/MetricsSnapshot.h"
#include "ui/theme/Theme.h"

class FlyoutWindow {
public:
    FlyoutWindow();
    ~FlyoutWindow();

    /**
     * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
     */
    bool Create();
    /**
     * 依据锚点矩形显示窗口；具体屏幕定位与 DPI 转换在实现内完成。
     */
    void ShowNear(const RECT& anchorRect);
    /**
     * 隐藏当前窗口并更新交互状态；隐藏本身并不等于销毁所有 C++ 对象。
     */
    void Hide();
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsVisible() const { return m_visible; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    HWND GetHwnd() const { return m_hwnd; }

    /**
     * 复制采集快照到窗口状态；调用方负责提供有效数据，不在这里执行硬件查询。
     * @param snapshot 采集结果；窗口保存副本，避免直接依赖采集线程可变数据。
     */
    void UpdateSnapshot(const MetricsSnapshot& snapshot);
    /**
     * 修改亮色模式并同步相关页面或重新绘制。
     */
    void SetLightMode(bool lightMode);
    /**
     * 切换主题预设，使下一帧使用对应调色板。
     */
    void SetThemePreset(Theme::ThemePreset preset);
    void SetHideCallback(void (*callback)(void*), void* context) {
        m_hideCallback = callback;
        m_hideContext = context;
    }

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
     * 绘制页头与操作按钮，并推进后续内容的纵向起点。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    /**
     * 绘制CPU模块；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    /**
     * 绘制内存模块；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawMemorySection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    /**
     * 绘制网络模块；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawNetworkSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    /**
     * 绘制GPU模块；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawGpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);
    /**
     * 绘制磁盘模块；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawDiskSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal);

    // Vector Icon Helpers
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
     * 绘制磁盘图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
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

        /**
         * 释放本对象持有的绘图接口，为销毁或重新初始化准备。
         */
        void ReleaseAll();
        /**
         * 首次使用或重建目标后创建画笔，已有可用资源时复用。
         */
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
    // HWND 是系统窗口句柄；本 C++ 对象通过窗口用户数据接收消息。
    HWND m_hwnd = nullptr;
    // 持有画布、DC 和绘图接口；析构会清理自身资源。
    LayeredD2DRenderer m_renderer;
    // 应用维护的显示状态，控制是否需要绘图及更新交互。
    bool m_visible = false;
    bool m_isLightMode = false;
    Theme::ThemePreset m_preset = Theme::ThemePreset::TranslucentGlass;
    // UI 私有快照，与后台数据分离；绘图从这里读取。
    MetricsSnapshot m_snapshot;
    FlyoutBrushPool m_brushes;
    int m_renderCount = 0;

    // DIP dimensions matching the MiniBar width (345 DIP) for pixel-perfect alignment
    const float m_dipWidth = 345.0f;
    const float m_dipHeight = 540.0f;

    int m_pixelWidth = 345;
    int m_pixelHeight = 540;
    // DIP 到像素的缩放，96 DPI 为 1；跨显示器移动时需更新。
    float m_dpiScale = 1.0f;

    void (*m_hideCallback)(void*) = nullptr;
    void* m_hideContext = nullptr;
};
