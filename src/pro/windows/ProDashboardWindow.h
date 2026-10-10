/*
 * 模块说明：Pro 详情与设置窗口的状态模型
 * WindowMode 选择独立详情、概览或设置；AccordionModule 选择概览中展开的模块。
 * SettingsState 保存界面设置，命中矩形和焦点索引保存交互状态；两者职责不同。
 * 绘图函数的 rt 是借用的目标指针，所有权仍在 renderer；读取函数返回的引用也不是副本。
 */
#pragma once
#include <windows.h>
#include "ui/rendering/LayeredD2DRenderer.h"
#include "core/metrics/MetricsSnapshot.h"
#include "pro/rendering/ProOverviewVisuals.h"
#include "pro/interaction/ProPanelMotion.h"
#include "core/config/ProGeneralSettings.h"
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
        ProGeneralSettings general;
        int activeTab = 0;           // 0: 外观, 1: 监控, 2: 常规
        int themeMode = 1;           // 0: 亮色玻璃, 1: 纯白 (默认), 2: 深色, 3: 跟随系统
        int layoutMode = 0;          // 0: 完整长条 (FullRows), 1: 紧凑胶囊 (Compact)
        float glassOpacity = 0.40f;  // 40% (背景透明度 60%)
        bool showCpu = true;
        bool showTemp = true;
        bool showRam = true;
        bool showNet = true;
        std::array<int,4> indicatorOrder{0,1,2,3};
        int updateIntervalSec = 1;   // 1s
        int tempUnit = 0;            // 0: °C, 1: °F
        int highTempAlert = 85;      // 85°C
        bool reduceTransparency = false;
    };

    ProDashboardWindow();
    ~ProDashboardWindow();

    /**
     * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
     */
    bool Create();
    /**
     * 依据锚点矩形显示窗口；具体屏幕定位与 DPI 转换在实现内完成。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     */
    void ShowNear(const RECT& barRectDip);
    /**
     * 选择独立指标卡片模式，再定位并显示，避免重复保留概览标题条。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     */
    void ShowDirectModule(WindowMode directMode, const RECT& barRectDip);
    /**
     * 打开独立 CPU 温度卡片，复用详情窗口而不创建新的采集线程。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     */
    void ShowCpuTempDirect(const RECT& barRectDip);
    /**
     * 进入设置页面，保留返回所需的页面状态。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     */
    void ShowSettings(const RECT& barRectDip);
    /**
     * 进入概览模式并指定展开模块，计算布局后显示。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     */
    void ShowFullDashboard(AccordionModule mod, const RECT& barRectDip);
    /**
     * 依据当前显示模式展开或收起概览，保持悬浮条状态同步。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     */
    void ToggleDashboard(const RECT& barRectDip);
    /**
     * 悬浮条移动时同步详情位置；尺寸不变时尽量复用现有画面。
     * @param barRectDip 悬浮条锚点；名称含 Dip，实际屏幕坐标转换以实现为准。
     * @param dpi DPI 缩放比例，1 表示 96 DPI。
     */
    void FollowBar(const RECT& barRectDip,float dpi=0);
    void SetAnchorDpi(float dpi) {m_dpiScale=dpi;}
    void SetBarOwner(HWND owner) {m_barOwner=owner;}
    /**
     * 按层级关闭当前浮层、编辑状态或页面，不把所有关闭操作都当退出应用。
     */
    void CloseTopLayer();
    /**
     * 隐藏当前窗口并更新交互状态；隐藏本身并不等于销毁所有 C++ 对象。
     */
    void Hide();
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsVisible() const { return m_visible; }
    void SetExternalMenuActive(bool active) { m_settingsMenuActive=active; }
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    WindowMode GetMode() const { return m_mode; }
    // 返回内部对象的借用引用，所属窗口更新或销毁后不可继续依赖它。
    const SettingsState& GetSettings() const { return m_settings; }
    void SetAppearance(int theme,int layout,float opacity) {m_settings.themeMode=std::clamp(theme,0,3);m_settings.layoutMode=std::clamp(layout,0,1);m_settings.glassOpacity=std::clamp(opacity,.1f,1.f);}
    void SetAppearanceSaveFailed(bool failed) {m_appearanceSaveFailed=failed;}
    void SetGeneral(const ProGeneralSettings& value){m_settings.general=value;if(value.reduceMotion&&m_panelMotion.active){CancelPanelMotion();UpdateDimensionsAndPosition();if(m_visible)Render();}}
    void SetGeneralError(const std::wstring& error){m_generalError=error;}
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsHotkeyCapturing() const {return m_hotkeyCapturing;}
    bool ConfirmCurrentHotkey(){return CommitGeneralHotkey(m_settings.general.hotkeyModifiers,m_settings.general.hotkeyKey);}
    void SetOnGeneralChange(std::function<bool(ProGeneralSettings&,std::wstring&)> cb){m_onGeneralChange=cb;}
    // 返回内部对象的借用引用，所属窗口更新或销毁后不可继续依赖它。
    const std::wstring& GetSelectedDiskId() const {return m_selectedDiskId;}
    void SetMonitoring(const std::array<bool,4>& visible,const std::array<int,4>& order,int interval,int unit,int alert,const std::wstring& disk) {
        m_settings.showCpu=visible[0];m_settings.showTemp=visible[1];m_settings.showRam=visible[2];m_settings.showNet=visible[3];
        m_settings.indicatorOrder=order;m_settings.updateIntervalSec=interval;m_settings.tempUnit=unit;m_settings.highTempAlert=alert;m_alertTempThreshold=alert;m_selectedDiskId=disk;
    }
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    AccordionModule GetExpandedModule() const { return static_cast<AccordionModule>(m_expandedModule); }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    HWND GetHwnd() const { return m_hwnd; }

    /**
     * 复制采集快照到窗口状态；调用方负责提供有效数据，不在这里执行硬件查询。
     * @param snapshot 采集结果；窗口保存副本，避免直接依赖采集线程可变数据。
     */
    void UpdateSnapshot(const MetricsSnapshot& snapshot);
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsPinned() const { return m_isPinned; }
    /**
     * 更新固定状态，影响外部点击是否收起窗口。
     */
    void SetPinned(bool pin);
    /**
     * 请求展开或收起对应模块，具体过渡由 TransitionModule 处理。
     */
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
     * 把密集交互合并为一次后续重绘请求，降低重复绘图成本。
     */
    void RequestInteractionRender();
    /**
     * 提交滑块积累的设置变化，避免每个鼠标位置都重复持久化。
     */
    void CommitSliderSettings();
    // 已安排延迟重绘时不重复安排，合并一次交互中的密集事件。
    bool m_interactionRenderPending = false;
    // 滑块值已变更但尚未提交，用于合并保存操作。
    bool m_sliderSettingsDirty = false;
    /**
     * 依据模式、布局和动画计算窗口尺寸/位置，必要时同步 Win32 窗口。
     */
    void UpdateDimensionsAndPosition(bool applyWindow=true, bool preserveZOrder=false);

    /**
     * 根据模块返回详情卡片的逻辑高度，供布局计算使用。
     */
    static float GetDetailCardHeight(int mod);

    // Section Renderers
    /**
     * 绘制页头与操作按钮，并推进后续内容的纵向起点。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawHeader(ID2D1RenderTarget* rt, float& curY, float width);
    /**
     * 组合纯白概览的头部、概要与展开详情，布局状态决定模块位置。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteOverview(ID2D1RenderTarget* rt);
    /**
     * 绘制CPU详情内容；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param standalone 是否按独立卡片布局绘制。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteCpuDetails(ID2D1RenderTarget* rt, bool standalone = false);
    /**
     * 绘制CPU独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteCpuDirect(ID2D1RenderTarget* rt);
    /**
     * 绘制温度独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteTempDirect(ID2D1RenderTarget* rt);
    /**
     * 为内容设置滚动变换和裁剪，结束时恢复绘图状态。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawScrolledSurface(ID2D1RenderTarget* rt,const std::function<void()>& draw);
    /**
     * 绘制外观设置控件并记录命中区域。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawSettingsAppearance(ID2D1RenderTarget* rt);
    /**
     * 绘制监控设置控件并记录命中区域。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawSettingsMonitoring(ID2D1RenderTarget* rt);
    /**
     * 绘制常规设置、快捷键与错误提示，记录交互区域。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawSettingsGeneral(ID2D1RenderTarget* rt);
    /**
     * 依据点击位置匹配常规设置控件，触发切换或快捷键捕获。
     */
    bool OnGeneralClick(float x,float y);
    /**
     * 处理常规设置的键盘导航、开关操作与快捷键捕获。
     * @param key 虚拟键码，用于键盘导航或快捷键。
     */
    bool OnGeneralKey(UINT key,bool shift,bool alt);
    /**
     * 通过回调验证并应用常规设置，记录错误以便界面显示。
     */
    bool CommitGeneral(ProGeneralSettings value);
    /**
     * 组装新的快捷键设置交给常规设置提交路径，失败时保留错误提示。
     * @param key 虚拟键码，用于键盘导航或快捷键。
     */
    bool CommitGeneralHotkey(UINT modifiers,UINT key);
    // 绘制时记录控件矩形，点击时用相同坐标空间做命中测试。
    D2D1_RECT_F m_generalToggleRects[6]{},m_generalHotkeyRect{};
    int m_generalFocus=-1;
    bool m_hotkeyCapturing=false;
    std::wstring m_generalError;
    // 回调把系统行为应用交给主窗口；引用参数允许返回修正设置和错误文本。
    std::function<bool(ProGeneralSettings&,std::wstring&)> m_onGeneralChange;
    /**
     * 处理监控页开关、下拉选项及指标排列的鼠标操作。
     */
    bool OnMonitoringClick(float x,float y);
    /**
     * 把控件选项转换为监控设置字段，并通知保存/刷新。
     */
    void ApplyMonitoringChoice(int control,int value);
    /**
     * 拖动时根据纵向位置调整悬浮条指标顺序。
     */
    void MoveMonitoringIndicator(float y);
    /**
     * 处理监控页键盘焦点、选择和导航。
     * @param key 虚拟键码，用于键盘导航或快捷键。
     */
    bool OnMonitoringKey(UINT key,bool shift,bool alt);
    /**
     * 返回当前监控控件焦点矩形，用于绘制焦点和保持可见。
     */
    D2D1_RECT_F MonitoringFocusRect() const;
    int m_monitoringFocus=-1;
    int m_monitoringGroup=-1,m_indicatorDragging=-1;
    bool m_settingsMenuActive=false;
    D2D1_RECT_F m_monitoringHeaders[2]{},m_indicatorGripRects[4]{},m_monitoringDeviceRects[3]{};
    bool m_appearanceSaveFailed=false;
    /**
     * 绘制当前悬停或选中的逻辑核心提示信息。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuCorePeek(ID2D1RenderTarget* rt);
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsWhiteCpu() const { return IsPureWhite() && (m_mode == Mode_DirectCpu || (m_mode == Mode_FullAccordion && m_expandedModule == Module_CPU)); }
    /**
     * 用绘图时记录的核心矩形命中鼠标，返回核心索引或未命中。
     */
    int HitCpuCore(float x, float y) const;
    /**
     * 处理核心列表的键盘选择、焦点移动和相关交互。
     * @param key 虚拟键码，用于键盘导航或快捷键。
     */
    bool OnCpuKey(UINT key, bool shift);
    /**
     * 调整 CPU 详情的滚动偏移，并限制在内容范围内。
     */
    void ScrollCpu(float delta);
    /**
     * 绘制GPU详情内容；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteGpuDetails(ID2D1RenderTarget* rt);
    /**
     * 绘制内存详情内容；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param standalone 是否按独立卡片布局绘制。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteMemoryDetails(ID2D1RenderTarget* rt, bool standalone = false);
    /**
     * 绘制内存独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteMemoryDirect(ID2D1RenderTarget* rt);
    /**
     * 绘制磁盘详情内容；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteDiskDetails(ID2D1RenderTarget* rt);
    /**
     * 绘制所选磁盘断开时的说明，避免把旧设备数据当实时读数。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteDisconnectedDisk(ID2D1RenderTarget* rt);
    // 返回当前状态值的副本，此查询本身不触发硬件采样。
    bool IsDisconnectedDisk() const {return IsWhiteOverview() && m_expandedModule==Module_Disk && m_diskDisconnected;}
    bool m_diskDisconnected=false;
    std::wstring m_lastDiskModel;
    std::wstring m_lastDiskId;
    /**
     * 在当前快照中查找所选设备；返回借用指针，快照变化后不能长期保留。
     */
    const PhysicalDiskInfo* SelectedDisk() const;
    /**
     * 对照新快照校正设备选择，处理设备缺失及重新出现。
     */
    void ReconcileDiskSelection();
    /**
     * 处理磁盘选择菜单的键盘导航与确认/取消。
     * @param key 虚拟键码，用于键盘导航或快捷键。
     */
    bool OnDiskKey(UINT key);
    // 保存稳定设备 ID 而非列表索引，设备重新排序后仍能匹配。
    std::wstring m_selectedDiskId;
    bool m_diskMenuOpen=false;
    int m_diskMenuFocus=0, m_diskMenuFirst=0, m_diskMenuHover=-1;
    int m_diskMenuRows=5;
    D2D1_RECT_F m_diskTriggerRect{},m_diskMenuRect{};
    std::vector<D2D1_RECT_F> m_diskOptionRects;
    std::vector<std::wstring> m_diskOptionIds;
    /**
     * 绘制传感器详情内容；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteSensorDetails(ID2D1RenderTarget* rt);
    /**
     * 绘制网络详情内容；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param standalone 是否按独立卡片布局绘制。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteNetworkDetails(ID2D1RenderTarget* rt, bool standalone = false);
    /**
     * 绘制网络独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawWhiteNetworkDirect(ID2D1RenderTarget* rt);
    float CpuCoreExtraHeight() const {
        const size_t rows = (m_snapshot.cpuCoreUsage.size() + 7) / 8;
        return rows > 2 ? static_cast<float>((rows - 2) * 34) : 0;
    }
    bool IsWhiteOverview() const {
        return IsPureWhite() && m_mode == Mode_FullAccordion &&
            (m_expandedModule >= Module_None && m_expandedModule < Module_Count) &&
            (m_settings.layoutMode == 0 || m_settings.layoutMode == 1);
    }
    /**
     * 绘制概览模块卡片，按展开状态选择概要或详情内容。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawModuleCard(ID2D1RenderTarget* rt, AccordionModule mod, float& curY, float width);
    /**
     * 绘制紧凑布局的一枚指标胶囊，复用当前快照。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCompactCapsule(ID2D1RenderTarget* rt, AccordionModule mod, float x, float y, float w, float h);
    /**
     * 排列并绘制紧凑胶囊网格，计算后续内容起点。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCompactGrid(ID2D1RenderTarget* rt, const std::vector<AccordionModule>& mods, float& curY, float contentX, float contentW);
    /**
     * 绘制CPU展开详情；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuExpanded(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 绘制GPU展开详情；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawGpuExpanded(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 绘制内存展开详情；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawMemoryExpanded(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 绘制磁盘展开详情；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawDiskExpanded(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 绘制网络展开详情；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawNetworkExpanded(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 绘制传感器展开详情；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawSensorExpanded(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 绘制CPU 温度独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuTempDirect(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 绘制CPU独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawCpuDirect(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 绘制内存独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawMemoryDirect(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 绘制网络独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawNetworkDirect(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 绘制GPU独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawGpuDirect(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 绘制磁盘独立详情页；从窗口快照和设置取值，调用绘图库生成画面。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawDiskDirect(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 组合设置页面的导航、当前页内容和反馈状态。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawSettings(ID2D1RenderTarget* rt, float width, float height);
    /**
     * 绘制页脚信息与提示。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawFooter(ID2D1RenderTarget* rt, float& curY, float width);

    // Graphic Helpers
    /**
     * 把历史点映射到指定矩形，按最大值缩放并处理无效点。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawMiniSparkline(ID2D1RenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color);
    /**
     * 把历史点转换为平滑曲线与填充区域，控制点决定线条形状。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawSmoothWaveform(ID2D1RenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F strokeCol, D2D1_COLOR_F fillCol);
    /**
     * 在同一区域绘制上/下行历史曲线，用颜色区分方向。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawDualNetworkWaveform(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    /**
     * 用线段/几何绘制展开箭头，方向由参数或状态决定。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param brush 借用的颜色画笔，不转移所有权。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawChevron(ID2D1RenderTarget* rt, float cx, float cy, float size, bool isExpanded, ID2D1Brush* brush);

    // Hit testing
    /**
     * 按当前页面及命中区域分发点击；绘图记录的矩形与这里的坐标要一致。
     */
    void OnClick(float x, float y);
    /**
     * 更新悬停、拖动或滑块状态，必要时请求重绘。
     */
    void OnMouseMove(float x, float y);

private:
    // HWND 是系统窗口句柄；本 C++ 对象通过窗口用户数据接收消息。
    HWND m_hwnd = nullptr;
    // 持有画布、DC 和绘图接口；析构会清理自身资源。
    LayeredD2DRenderer m_renderer;
    ProOverviewVisuals m_overviewVisuals;
    float m_shadowApron = 0;
    // UI 私有快照，与后台数据分离；绘图从这里读取。
    MetricsSnapshot m_snapshot;

    // 应用维护的显示状态，控制是否需要绘图及更新交互。
    bool m_visible = false;
    bool m_memoryRenderLogged = false;
    bool m_graphicsSuspended = false;
    bool m_isPinned = false;
    int m_expandedModule = Module_None; // Default Overview state matching Figure 01

    float m_dipWidth = 360.0f;
    float m_dipHeight = 294.0f;
    // DIP 到像素的缩放，96 DPI 为 1；跨显示器移动时需更新。
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
    RECT m_placementWork{};
    bool m_settingsReturnValid=false;
    WindowMode m_settingsReturnMode=Mode_FullAccordion;
    int m_settingsReturnModule=Module_None;
    int m_settingsReturnLayoutMode=0;
    ProPanelMotion::Layout m_settingsReturnGeometry{};
    float m_settingsReturnScroll=0;
    /**
     * 恢复进入设置前的页面状态并重新显示。
     */
    void ReturnFromSettings();
    /**
     * 调整滚动位置，让键盘焦点所在设置控件出现在可视区域。
     */
    void EnsureSettingsFocusVisible(D2D1_RECT_F rect);
    ProPanelMotion::Transition m_panelMotion;
    bool m_motionContents=false;
    /**
     * 把当前页面状态转换为动画布局数据，不复制硬件采样历史。
     */
    ProPanelMotion::Layout MotionLayout() const;
    /**
     * 从现有布局启动到目标模块的过渡，或直接切换到最终布局。
     */
    void TransitionModule(AccordionModule module);
    /**
     * 判断当前设置和页面是否允许动画，例如减少动态效果时禁用。
     */
    bool PanelMotionAllowed() const;
    /**
     * 停止正在进行的布局动画，清理动画驱动状态。
     */
    void CancelPanelMotion();
    /**
     * 推进一帧动画，更新几何布局并在结束时停止驱动。
     */
    void TickPanelMotion();
    /**
     * 按动画插值布局绘制模块，透明度和裁剪控制内容过渡。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void DrawMotionBody(ID2D1RenderTarget* rt);
    /**
     * 处理窗口外点击；是否收起取决于固定和当前交互状态。
     */
    void OnOutsideClick();
    /**
     * 安装或撤销外部点击监听，隐藏窗口时避免留下无用钩子。
     */
    void WatchOutsideClicks(bool watch);
    /**
     * 鼠标钩子回调：检测窗口外点击，并把后续消息继续传给钩子链。
     * @param wParam 消息附加参数，含义随 msg 改变。
     * @param lParam 消息附加参数，鼠标坐标等需按消息格式解码。
     */
    static LRESULT CALLBACK OutsideMouseProc(int code,WPARAM wParam,LPARAM lParam);
    inline static ProDashboardWindow* s_mouseWatcher=nullptr;
    HHOOK m_mouseHook=nullptr;
    HWND m_barOwner=nullptr;
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
    // 画笔为 COM 接口，需 Release；它们与创建它们的绘图目标绑定。
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

    /**
     * 释放当前绘图目标关联的画笔和相应资源，清空指针以便下次重新创建。
     */
    void ReleaseBrushes();
    /**
     * 持续隐藏后释放可重建的绘图资源；重新打开时由渲染路径恢复。
     */
    void ReleaseDormantGraphics();
    /**
     * 首次使用或重建目标后创建画笔，已有可用资源时复用。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     */
    void EnsureBrushes(ID2D1RenderTarget* rt);
};
