/*
 * 模块说明：Pro 只读提示卡接口
 * 根据悬停指标选择宽度、文本和值，借用主窗口数据，不启动新的硬件采集器。
 * 提示窗口不激活、不接收点击；主题变化会影响画笔与透明度。
 */
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

    /**
     * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
     */
    bool Create(HWND hOwner);
    void SetOwner(HWND owner) { m_hOwner=owner; }
    // segmentIndex 选择指标，barPixels 是屏幕像素锚点，snapshot 为数据源，dpiScale 为缩放。
    void Show(int segmentIndex, RECT barPixels, const MetricsSnapshot& snapshot, float dpiScale);
    // 只改变温度显示单位；采集和历史仍保存摄氏度。
    void SetTempUnit(int unit) { m_fahrenheit = unit == 1; }
    void SetAlwaysOnTop(bool value){m_alwaysOnTop=value;if(m_hwnd)SetWindowPos(m_hwnd,value?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
    /**
     * 隐藏当前窗口并更新交互状态；隐藏本身并不等于销毁所有 C++ 对象。
     */
    void Hide();
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsVisible() const { return m_visible; }

    // 旧画笔包含原透明度，修改后先释放，下次绘制再按新值创建。
    void SetOpacity(float opacity) {
        m_opacity = opacity;
        ReleaseBrushes();
        if (m_visible) Render();
    }
    // 主题改变可能影响布局与资源，先隐藏提示并清理画笔，避免显示旧材质。
    void SetPureWhite(bool pw) {
        if (m_isPureWhite != pw) Hide();
        m_isPureWhite = pw;
        ReleaseBrushes();
        if (m_visible) Render();
    }

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
    void DrawWhitePeek(ID2D1DCRenderTarget* rt);
    void FormatValues(wchar_t (&left)[64], wchar_t (&right)[64]) const;
    void ConfigureSize(int segment);
    static POINT Place(RECT bar, RECT work, float scale, float offset, float gap, float width, float height);

private:
    // HWND 是系统窗口句柄；本 C++ 对象通过窗口用户数据接收消息。
    HWND m_hwnd = nullptr;
    HWND m_hOwner = nullptr;
    // 持有画布、DC 和绘图接口；析构会清理自身资源。
    LayeredD2DRenderer m_renderer;
    ProOverviewVisuals m_visuals;
    static constexpr float kShadowApron = 24;
    bool m_fahrenheit = false;
    bool m_alwaysOnTop=true;

    float m_dipWidth = 200.0f;
    float m_dipHeight = 56.0f;
    // DIP 到像素的缩放，96 DPI 为 1；跨显示器移动时需更新。
    float m_dpiScale = 1.0f;
    int m_pixelWidth = 200;
    int m_pixelHeight = 56;

    // 应用维护的显示状态，控制是否需要绘图及更新交互。
    bool m_visible = false;
    int m_segmentIndex = -1;
    // UI 私有快照，与后台数据分离；绘图从这里读取。
    MetricsSnapshot m_snapshot;
    float m_opacity = 0.86f;
    bool m_isPureWhite = false;

    ID2D1SolidColorBrush* m_pCardBg = nullptr;
    ID2D1SolidColorBrush* m_pCardBorder = nullptr;
    ID2D1SolidColorBrush* m_pSepBrush = nullptr;

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
