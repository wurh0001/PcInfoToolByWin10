// 主监控条窗口的声明。
#include "MiniBarWindow.h"
// DPI 换算工具：处理不同显示器缩放比例。
#include "DpiHelper.h"
// 主题颜色、主题预设和绘制配置。
#include "Theme.h"
// 自定义右键菜单的创建和处理。
#include "CustomContextMenu.h"
// 配置文件的读取和保存。
#include "../core/ConfigManager.h"
// 提供 std::clamp、std::min 和 std::max 等工具。
#include <algorithm>
// 数学计算函数，用于尺寸、动画和指标计算。
#include <cmath>

// 构造主监控条对象；具体窗口和渲染资源在 Create 中初始化。
MiniBarWindow::MiniBarWindow() {}

// 停止后台采集线程，再销毁主窗口。
MiniBarWindow::~MiniBarWindow() {
    m_collector.Stop();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

// 注册窗口类、创建主窗口、初始化渲染器，并启动数据采集和刷新定时器。
bool MiniBarWindow::Create() {
    // 创建并清零一个窗口类描述结构，sizeof(WNDCLASSEX) 用于告诉 Windows 当前结构版本。
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    // 指定窗口收到消息时使用的静态窗口过程。
    wc.lpfnWndProc = MiniBarWindow::WndProc;
    // 获取当前可执行文件的实例句柄，用于注册窗口类和创建窗口。
    wc.hInstance = GetModuleHandle(NULL);
    // 设置窗口类名称，后面 CreateWindowEx 会通过这个名称找到它。
    wc.lpszClassName = L"PcInfoTool_MiniBarWindowClass";
    // 使用系统默认箭头光标。
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // 向 Windows 注册窗口类，失败时通常表示类已经注册或系统资源不足。
    RegisterClassEx(&wc);

    // 获取当前显示环境的 DPI 缩放比例。
    m_dpiScale = DpiHelper::GetScaleForWindow(NULL);
    // 将设计尺寸 DIP 转换为窗口实际需要的物理像素宽度。
    m_pixelWidth = DpiHelper::DipToPx(m_dipWidth, m_dpiScale);
    // 将设计尺寸 DIP 转换为窗口实际需要的物理像素高度。
    m_pixelHeight = DpiHelper::DipToPx(m_dipHeight, m_dpiScale);

    // 先读取上一次保存的主题、位置和交互设置。
    LoadSettings();

    // 默认把监控条放在主屏幕右上方；如果配置中有有效位置，则使用配置位置。
    // 获取主显示器的屏幕宽度，单位是物理像素。
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    // 计算默认 X 坐标：屏幕右边缘向左留出窗口宽度和 30 像素边距。
    int posX = screenW - m_pixelWidth - 30;
    // 默认 Y 坐标为 0，即贴近屏幕顶部。
    int posY = 0;

    // 读取配置文件中的窗口位置和其他持久化设置。
    auto cfg = ConfigManager::LoadConfig();
    // 只有两个坐标都非负时才使用保存的位置，避免配置损坏导致窗口出现在屏幕外。
    if (cfg.posX >= 0 && cfg.posY >= 0) {
        // 使用上一次关闭程序时保存的水平位置。
        posX = cfg.posX;
        // 使用上一次关闭程序时保存的垂直位置。
        posY = cfg.posY;
    }

    // 创建无边框的 Windows 分层窗口。
    m_hwnd = CreateWindowEx(
        // TOOLWINDOW 不在任务栏显示，TOPMOST 保持窗口置顶，LAYERED 支持逐像素透明。
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        // 使用前面注册的窗口类。
        wc.lpszClassName,
        // 设置窗口标题，主要用于调试和系统识别。
        L"PcInfoTool_MiniBar",
        // POPUP 创建无标题栏、无边框窗口。
        WS_POPUP,
        // 设置窗口左上角位置和实际像素尺寸。
        posX, posY, m_pixelWidth, m_pixelHeight,
        // 主监控条是顶层窗口，不设置父窗口。
        NULL, NULL, wc.hInstance, this
    );

    // 窗口句柄为空表示创建失败，直接返回 false 让主程序显示错误提示。
    if (!m_hwnd) return false;

    // 初始化分层窗口渲染器：支持逐像素透明、圆角和无传统窗口边框绘制。
    // 渲染器会创建 Direct2D 工厂、字体、内存位图和分层窗口渲染目标。
    if (!m_renderer.Initialize(m_hwnd, m_pixelWidth, m_pixelHeight)) {
        // Direct2D 或位图资源初始化失败时，当前窗口无法正常显示。
        return false;
    }

    // 提前创建详情窗口，真正显示时只需要移动并显示它。
    // 详情窗口初始保持隐藏状态。
    m_flyout.Create();
    // 让详情窗口使用与主监控条相同的主题预设。
    m_flyout.SetThemePreset(m_preset);
    // 当详情窗口因点击叉号或失焦自动收起时，联动关闭 GPU 采集并回收工作集
    m_flyout.SetHideCallback([](void* ctx) {
        auto pThis = reinterpret_cast<MiniBarWindow*>(ctx);
        if (pThis) {
            pThis->m_collector.SetGpuEnabled(false);
        }
    }, this);

    // 根据初始位置判断窗口是否贴靠在屏幕边缘。
    UpdateDockState();
    // 程序首次启动时强制使用展开状态，即使窗口位于屏幕边缘。
    m_isCollapsed = false;

    // 在窗口显示前先绘制第一帧，避免刚出现时内容为空。
    Render();

    // 将创建好的窗口显示到屏幕上。
    ShowWindow(m_hwnd, SW_SHOW);

    // 启动后台线程，开始采集 CPU、内存、GPU、网络和磁盘信息。
    m_collector.Start();

    // 主线程每秒读取一次快照并重绘 UI。
    // 定时器消息会在 WndProc 中转发到 OnTimer。
    SetTimer(m_hwnd, 1, 1000, NULL);

    // 请求接收鼠标离开窗口的消息，用于自动收起和自动隐藏逻辑。
    // TME_NONCLIENT 同时追踪窗口非客户区，适配无边框窗口的拖动区域。
    TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_NONCLIENT | TME_LEAVE, m_hwnd, 0 };
    // 向 Windows 注册鼠标离开追踪请求。
    TrackMouseEvent(&tme);
    // 标记当前已经开启鼠标追踪，避免重复注册。
    m_mouseTracking = true;

    // 请求 Windows 回收暂时不用的工作集页面，减少初始化后的常驻内存。
    // -1 表示让系统根据需要裁剪进程工作集，而不是指定固定大小。
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));

    // 到这里所有核心资源都已准备完成，通知 wWinMain 可以进入消息循环。
    return true;
}

// 运行 Windows 消息循环，负责分发定时器、鼠标和窗口生命周期消息。
void MiniBarWindow::Run() {
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// 在详情弹窗的显示和隐藏状态之间切换。
void MiniBarWindow::ToggleFlyout() {
    if (m_flyout.IsVisible()) {
        // 详情已打开：关闭详情；如果主条贴边且鼠标已经离开，则同时收起主条。
        m_flyout.Hide();
        m_collector.SetGpuEnabled(false);
        // If docked and mouse is not over window, collapse back
        if (m_dockEdge != DockEdge::None && !m_isDragging) {
            POINT pt;
            GetCursorPos(&pt);
            RECT rc;
            GetWindowRect(m_hwnd, &rc);
            if (!PtInRect(&rc, pt)) {
                m_isCollapsed = true;
                Render();
            }
        }
    } else {
        // 详情未打开：以主监控条为锚点，在其下方或上方显示详情窗口。
        m_collector.SetGpuEnabled(true);
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        m_flyout.ShowNear(rc);
    }
}

// 将贴靠在屏幕边缘的窗口移回工作区，并清除贴靠状态。
void MiniBarWindow::UndockFromEdge() {
    if (!m_hwnd) return;
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    HMONITOR hMon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    if (hMon) {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(hMon, &mi)) {
            int newX = rc.left;
            int newY = rc.top;
            if (m_dockEdge == DockEdge::Top) {
                newY = mi.rcWork.top + 50;
            } else if (m_dockEdge == DockEdge::Bottom) {
                newY = mi.rcWork.bottom - m_pixelHeight - 50;
            } else if (m_dockEdge == DockEdge::Left) {
                newX = mi.rcWork.left + 50;
            } else if (m_dockEdge == DockEdge::Right) {
                newX = mi.rcWork.right - m_pixelWidth - 50;
            }
            SetWindowPos(m_hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }
    m_dockEdge = DockEdge::None;
    m_initialDockEdge = DockEdge::None;
    m_isCollapsed = false;
    Render();
}

// 从配置文件加载主题、位置、自动隐藏等用户设置。
void MiniBarWindow::LoadSettings() {
    auto cfg = ConfigManager::LoadConfig();
    // 限制主题编号范围，避免配置文件损坏导致非法枚举值。
    m_preset = static_cast<Theme::ThemePreset>(std::clamp(cfg.themePreset, 0, 4));
    m_style = (cfg.themeStyle == 1) ? ThemeStyle::BarDark : ThemeStyle::RingGlass;
    m_autoHideOnHover = cfg.autoHideOnHover;
    m_isLightMode = (m_preset == Theme::ThemePreset::GlacierWhite || m_preset == Theme::ThemePreset::AuroraGlass);
}

// 将当前窗口位置、主题和交互设置保存到配置文件。
void MiniBarWindow::SaveSettings() {
    ConfigManager::AppConfig cfg;
    if (m_hwnd) {
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        cfg.posX = rc.left;
        cfg.posY = rc.top;
    }
    cfg.dockEdge = static_cast<int>(m_dockEdge);
    cfg.themePreset = static_cast<int>(m_preset);
    cfg.themeStyle = (m_style == ThemeStyle::BarDark) ? 1 : 0;
    cfg.autoHideOnHover = m_autoHideOnHover;
    ConfigManager::SaveConfig(cfg);
}

// 切换浅色/深色主题，并立即更新详情窗口和主窗口。
void MiniBarWindow::SetLightMode(bool light) {
    m_isLightMode = light;
    m_preset = light ? Theme::ThemePreset::GlacierWhite : Theme::ThemePreset::ObsidianDark;
    m_flyout.SetThemePreset(m_preset);
    SaveSettings();
    Render();
}

// 设置完整主题预设，例如深色、白色或玻璃效果主题。
void MiniBarWindow::SetThemePreset(Theme::ThemePreset preset) {
    m_preset = preset;
    m_isLightMode = (preset == Theme::ThemePreset::GlacierWhite || preset == Theme::ThemePreset::AuroraGlass);
    m_flyout.SetThemePreset(preset);
    SaveSettings();
    Render();
}

// 设置监控条的显示样式并保存配置。
void MiniBarWindow::SetThemeStyle(ThemeStyle style) {
    m_style = style;
    SaveSettings();
    Render();
}

// 设置鼠标悬停时是否自动隐藏监控条。
void MiniBarWindow::SetAutoHideOnHover(bool enable) {
    m_autoHideOnHover = enable;
    if (!m_autoHideOnHover) {
        m_isHoverHidden = false;
    }
    SaveSettings();
    Render();
}

// 根据贴靠方向计算收起状态下的细条区域，坐标使用 DIP 单位。
D2D1_RECT_F MiniBarWindow::GetCollapsedStripeRectDip() const {
    float W = m_dipWidth;
    float H = m_dipHeight;
    float stripeLength = 300.0f; // Multi-bar container width
    float stripeThick = 14.0f;   // Slim yet comfortable dark capsule height

    if (m_dockEdge == DockEdge::Top) {
        float startX = (W - stripeLength) / 2.0f;
        return D2D1::RectF(startX, 0.0f, startX + stripeLength, stripeThick);
    } else if (m_dockEdge == DockEdge::Bottom) {
        float startX = (W - stripeLength) / 2.0f;
        return D2D1::RectF(startX, H - stripeThick, startX + stripeLength, H);
    } else if (m_dockEdge == DockEdge::Left) {
        float startY = 4.0f;
        return D2D1::RectF(0.0f, startY, stripeThick, H - 4.0f);
    } else if (m_dockEdge == DockEdge::Right) {
        float startY = 4.0f;
        return D2D1::RectF(W - stripeThick, startY, W, H - 4.0f);
    }
    return D2D1::RectF(0, 0, 0, 0);
}

// 根据窗口与显示器工作区边缘的距离判断当前贴靠方向。
void MiniBarWindow::UpdateDockState() {
    if (!m_hwnd) return;
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    HMONITOR hMon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    if (hMon) {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfo(hMon, &mi)) {
            const RECT& work = mi.rcWork;
            // 窗口边缘在 4 个像素以内时认为已经贴靠。
            const int DOCK_TOLERANCE = 4;

            if (abs(rc.top - work.top) <= DOCK_TOLERANCE) {
                m_dockEdge = DockEdge::Top;
            } else if (abs(rc.bottom - work.bottom) <= DOCK_TOLERANCE) {
                m_dockEdge = DockEdge::Bottom;
            } else if (abs(rc.left - work.left) <= DOCK_TOLERANCE) {
                m_dockEdge = DockEdge::Left;
            } else if (abs(rc.right - work.right) <= DOCK_TOLERANCE) {
                m_dockEdge = DockEdge::Right;
            } else {
                m_dockEdge = DockEdge::None;
            }
        }
    }
}

// 绘制收起状态下使用的 CPU 芯片图标。
void MiniBarWindow::DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    // Central chip package square (9x9)
    D2D1_ROUNDED_RECT pkg = D2D1::RoundedRect(D2D1::RectF(x + 2.5f, y + 2.5f, x + 11.5f, y + 11.5f), 1.5f, 1.5f);
    rt->DrawRoundedRectangle(pkg, brush, 1.2f);

    // Silicon core square
    rt->FillRectangle(D2D1::RectF(x + 5.0f, y + 5.0f, x + 9.0f, y + 9.0f), brush);

    // 2 pins on each side
    // Top pins
    rt->DrawLine(D2D1::Point2F(x + 4.5f, y + 0.5f), D2D1::Point2F(x + 4.5f, y + 2.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 9.5f, y + 0.5f), D2D1::Point2F(x + 9.5f, y + 2.5f), brush, 1.2f);
    // Bottom pins
    rt->DrawLine(D2D1::Point2F(x + 4.5f, y + 11.5f), D2D1::Point2F(x + 4.5f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 9.5f, y + 11.5f), D2D1::Point2F(x + 9.5f, y + 13.5f), brush, 1.2f);
    // Left pins
    rt->DrawLine(D2D1::Point2F(x + 0.5f, y + 4.5f), D2D1::Point2F(x + 2.5f, y + 4.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 0.5f, y + 9.5f), D2D1::Point2F(x + 2.5f, y + 9.5f), brush, 1.2f);
    // Right pins
    rt->DrawLine(D2D1::Point2F(x + 11.5f, y + 4.5f), D2D1::Point2F(x + 13.5f, y + 4.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 11.5f, y + 9.5f), D2D1::Point2F(x + 13.5f, y + 9.5f), brush, 1.2f);
}

// 绘制收起状态下使用的内存条图标。
void MiniBarWindow::DrawRamIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    // Outer memory module rectangle
    D2D1_ROUNDED_RECT modRc = D2D1::RoundedRect(D2D1::RectF(x + 0.5f, y + 2.0f, x + 14.5f, y + 11.5f), 1.5f, 1.5f);
    rt->DrawRoundedRectangle(modRc, brush, 1.2f);

    // Inner chip slots
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 3.2f, y + 4.5f, x + 5.8f, y + 8.5f), 0.8f, 0.8f), brush);
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 9.2f, y + 4.5f, x + 11.8f, y + 8.5f), 0.8f, 0.8f), brush);

    // 3 connector pins on bottom edge
    rt->DrawLine(D2D1::Point2F(x + 3.0f, y + 11.5f), D2D1::Point2F(x + 3.0f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 7.5f, y + 11.5f), D2D1::Point2F(x + 7.5f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 12.0f, y + 11.5f), D2D1::Point2F(x + 12.0f, y + 13.5f), brush, 1.2f);
}

// 绘制收起状态下使用的 GPU 显卡图标。
void MiniBarWindow::DrawGpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    // GPU graphic card outer shroud rectangle (approx 12x9)
    D2D1_ROUNDED_RECT shroudRc = D2D1::RoundedRect(D2D1::RectF(x + 0.5f, y + 1.0f, x + 10.5f, y + 9.5f), 1.2f, 1.2f);
    rt->DrawRoundedRectangle(shroudRc, brush, 1.0f);

    // Fan circle inside shroud
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x + 5.5f, y + 5.2f), 2.5f, 2.5f), brush, 0.9f);
    // Fan center hub
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 5.5f, y + 5.2f), 0.9f, 0.9f), brush);

    // PCI-e bracket on right side
    rt->DrawLine(D2D1::Point2F(x + 10.5f, y + 0.5f), D2D1::Point2F(x + 10.5f, y + 10.0f), brush, 1.1f);
}

// 绘制窗口贴靠并收起后的三段式 CPU/RAM/GPU 指标条。
void MiniBarWindow::RenderCollapsedTriBar(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, float W, float H) {
    D2D1_RECT_F sRc = GetCollapsedStripeRectDip();
    float sW = sRc.right - sRc.left;
    float sH = sRc.bottom - sRc.top;

    // --- Option D: Refined Dark Obsidian Glass Capsule ---
    // High-contrast, deeply legible on pure white/bright backgrounds like a Dynamic Island pill
    D2D1_ROUNDED_RECT pillRc = D2D1::RoundedRect(sRc, sH / 2.0f, sH / 2.0f);

    // Refined medium-deep slate gray capsule background (~88% opacity)
    ID2D1SolidColorBrush* pBgBrush = nullptr;
    ID2D1SolidColorBrush* pBorderBrush = nullptr;
    ID2D1SolidColorBrush* pDividerBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBorderBrush = nullptr;

    // Outer Capsule: Elegant medium-deep slate gray
    rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.24f, 0.27f, 0.90f), &pBgBrush);
    // Outer border: crisp soft outline
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f), &pBorderBrush);
    // Divider line between C, M, G: subtle contrast separator
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f), &pDividerBrush);
    // Inner Track: Deep sunken pitch black for maximum contrast against progress bar
    rt->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.05f, 0.06f, 0.92f), &pTrackBrush);
    // Track micro-border: subtle bevel outline
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &pTrackBorderBrush);

    if (pBgBrush) rt->FillRoundedRectangle(pillRc, pBgBrush);

    // Specular top rim highlight (crystal glass top edge)
    {
        ID2D1GradientStopCollection* pStops = nullptr;
        D2D1_GRADIENT_STOP stops[3];
        stops[0].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.38f);
        stops[0].position = 0.0f;
        stops[1].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f);
        stops[1].position = 0.50f;
        stops[2].color = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.25f);
        stops[2].position = 1.0f;

        rt->CreateGradientStopCollection(stops, 3, &pStops);
        if (pStops) {
            ID2D1LinearGradientBrush* pRim = nullptr;
            rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(sRc.left, sRc.top), D2D1::Point2F(sRc.left, sRc.bottom)),
                pStops, &pRim
            );
            if (pRim) {
                rt->DrawRoundedRectangle(pillRc, pRim, 1.0f);
                pRim->Release();
            }
            pStops->Release();
        }
    }

    // Sophisticated Low-Saturation Palette (Morandi / Nordic Minimalist):
    // C: Low-saturation Sage Mint Green (rgb 104, 194, 150)
    // M: Low-saturation Dusty Lavender / Soft Iris (rgb 168, 142, 218)
    // G: Low-saturation Soft Glacier Slate Blue (rgb 102, 175, 214)
    D2D1_COLOR_F cpuCol;
    if (m_snapshot.cpuUsageTotal >= 85.0f) {
        cpuCol = D2D1::ColorF(0.88f, 0.45f, 0.45f, 1.0f); // Soft muted coral red for alert
    } else if (m_snapshot.cpuUsageTotal >= 70.0f) {
        cpuCol = D2D1::ColorF(0.88f, 0.70f, 0.42f, 1.0f); // Soft muted amber for warm
    } else {
        cpuCol = D2D1::ColorF(0.41f, 0.76f, 0.59f, 1.0f); // Muted Sage Mint
    }

    D2D1_COLOR_F ramCol = D2D1::ColorF(0.66f, 0.56f, 0.85f, 1.0f); // Dusty Lavender
    D2D1_COLOR_F gpuCol = D2D1::ColorF(0.40f, 0.69f, 0.84f, 1.0f); // Soft Glacier Slate Blue

    ID2D1SolidColorBrush* pCpuBrush = nullptr;
    ID2D1SolidColorBrush* pRamBrush = nullptr;
    ID2D1SolidColorBrush* pGpuBrush = nullptr;
    rt->CreateSolidColorBrush(cpuCol, &pCpuBrush);
    rt->CreateSolidColorBrush(ramCol, &pRamBrush);
    rt->CreateSolidColorBrush(gpuCol, &pGpuBrush);

    // 3 sections layout: total 300 width
    // Section width = 94.0f
    float sectionW = 94.0f;
    float startX = sRc.left + 8.0f;
    float midY = sRc.top + sH / 2.0f;
    float barH = 3.6f; // Clean visible progress bar
    float barW = 63.0f;
    float iconW = 12.0f;

    // --- Section 1: CPU ---
    {
        float secX = startX;
        // Bold letter 'C'
        if (pCpuBrush && m_renderer.GetFormatBoldTiny()) {
            rt->DrawTextW(L"C", 1, m_renderer.GetFormatBoldTiny(),
                D2D1::RectF(secX, midY - 6.5f, secX + iconW, midY + 6.5f), pCpuBrush);
        }
        // Track
        float tX = secX + iconW + 3.0f;
        float tY = midY - barH / 2.0f;
        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(tX, tY, tX + barW, tY + barH), barH / 2.0f, barH / 2.0f);
        if (pTrackBrush) rt->FillRoundedRectangle(trackRc, pTrackBrush);
        if (pTrackBorderBrush) rt->DrawRoundedRectangle(trackRc, pTrackBorderBrush, 0.6f);

        // Progress Fill
        float fillW = barW * (std::clamp(m_snapshot.cpuUsageTotal, 0.0f, 100.0f) / 100.0f);
        if (fillW > 1.5f && pCpuBrush) {
            D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(D2D1::RectF(tX, tY, tX + fillW, tY + barH), barH / 2.0f, barH / 2.0f);
            rt->FillRoundedRectangle(fillRc, pCpuBrush);
        }
    }

    // Divider 1
    float div1X = startX + sectionW + 1.0f;
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(div1X, sRc.top + 3.0f), D2D1::Point2F(div1X, sRc.bottom - 3.0f), pDividerBrush, 1.0f);
    }

    // --- Section 2: RAM ---
    {
        float secX = div1X + 5.0f;
        // Bold letter 'M' (Memory)
        if (pRamBrush && m_renderer.GetFormatBoldTiny()) {
            rt->DrawTextW(L"M", 1, m_renderer.GetFormatBoldTiny(),
                D2D1::RectF(secX, midY - 6.5f, secX + iconW, midY + 6.5f), pRamBrush);
        }
        // Track
        float tX = secX + iconW + 3.0f;
        float tY = midY - barH / 2.0f;
        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(tX, tY, tX + barW, tY + barH), barH / 2.0f, barH / 2.0f);
        if (pTrackBrush) rt->FillRoundedRectangle(trackRc, pTrackBrush);
        if (pTrackBorderBrush) rt->DrawRoundedRectangle(trackRc, pTrackBorderBrush, 0.6f);

        // Progress Fill
        float fillW = barW * (std::clamp(m_snapshot.memoryUsagePercent, 0.0f, 100.0f) / 100.0f);
        if (fillW > 1.5f && pRamBrush) {
            D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(D2D1::RectF(tX, tY, tX + fillW, tY + barH), barH / 2.0f, barH / 2.0f);
            rt->FillRoundedRectangle(fillRc, pRamBrush);
        }
    }

    // Divider 2
    float div2X = div1X + sectionW + 5.0f;
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(div2X, sRc.top + 3.0f), D2D1::Point2F(div2X, sRc.bottom - 3.0f), pDividerBrush, 1.0f);
    }

    // --- Section 3: GPU ---
    {
        float secX = div2X + 5.0f;
        // Bold letter 'G' (GPU)
        if (pGpuBrush && m_renderer.GetFormatBoldTiny()) {
            rt->DrawTextW(L"G", 1, m_renderer.GetFormatBoldTiny(),
                D2D1::RectF(secX, midY - 6.5f, secX + iconW, midY + 6.5f), pGpuBrush);
        }
        // Track
        float tX = secX + iconW + 3.0f;
        float tY = midY - barH / 2.0f;
        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(tX, tY, tX + barW, tY + barH), barH / 2.0f, barH / 2.0f);
        if (pTrackBrush) rt->FillRoundedRectangle(trackRc, pTrackBrush);
        if (pTrackBorderBrush) rt->DrawRoundedRectangle(trackRc, pTrackBorderBrush, 0.6f);

        // Progress Fill
        float fillW = barW * (std::clamp(m_snapshot.gpuUsagePercent, 0.0f, 100.0f) / 100.0f);
        if (fillW > 1.5f && pGpuBrush) {
            D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(D2D1::RectF(tX, tY, tX + fillW, tY + barH), barH / 2.0f, barH / 2.0f);
            rt->FillRoundedRectangle(fillRc, pGpuBrush);
        }
    }

    if (pBgBrush) pBgBrush->Release();
    if (pBorderBrush) pBorderBrush->Release();
    if (pDividerBrush) pDividerBrush->Release();
    if (pTrackBrush) pTrackBrush->Release();
    if (pTrackBorderBrush) pTrackBorderBrush->Release();
    if (pCpuBrush) pCpuBrush->Release();
    if (pRamBrush) pRamBrush->Release();
    if (pGpuBrush) pGpuBrush->Release();
}

void MiniBarWindow::DrawChevron(ID2D1DCRenderTarget* rt, float centerX, float centerY, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    rt->DrawLine(D2D1::Point2F(centerX - 3.8f, centerY - 1.5f), D2D1::Point2F(centerX, centerY + 2.5f), brush, 1.6f);
    rt->DrawLine(D2D1::Point2F(centerX, centerY + 2.5f), D2D1::Point2F(centerX + 3.8f, centerY - 1.5f), brush, 1.6f);
}

void MiniBarWindow::DrawRingGauge(ID2D1DCRenderTarget* rt, float cx, float cy, float radius, float strokeWidth, float percent, ID2D1Brush* trackBrush, ID2D1Brush* activeBrush) {
    if (!rt) return;

    // 先绘制完整的浅色背景环，表示指标的 0% 到 100% 范围。
    if (trackBrush) {
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), trackBrush, strokeWidth);
    }

    // 数据太小或没有前景画刷时不绘制进度弧，避免出现几乎不可见的噪点。
    if (!activeBrush || percent <= 0.5f) return;

    float p = std::clamp(percent, 0.0f, 100.0f);
    if (p >= 99.5f) {
        // 接近 100% 时直接绘制完整圆，避免 Direct2D 绘制接近整圆的弧线出现接缝。
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), activeBrush, strokeWidth);
        return;
    }

    // 将百分比转换成从顶部开始顺时针绘制的角度。
    float sweepAngle = 360.0f * (p / 100.0f);
    if (sweepAngle < 2.0f) sweepAngle = 2.0f;

    float startRad = -90.0f * (3.14159265358979323846f / 180.0f);
    float endRad = (-90.0f + sweepAngle) * (3.14159265358979323846f / 180.0f);

    D2D1_POINT_2F startPt = D2D1::Point2F(cx + radius * cosf(startRad), cy + radius * sinf(startRad));
    D2D1_POINT_2F endPt = D2D1::Point2F(cx + radius * cosf(endRad), cy + radius * sinf(endRad));

    ID2D1PathGeometry* pArcGeom = nullptr;
    if (m_renderer.GetD2DFactory() && SUCCEEDED(m_renderer.GetD2DFactory()->CreatePathGeometry(&pArcGeom))) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pArcGeom->Open(&pSink))) {
            pSink->BeginFigure(startPt, D2D1_FIGURE_BEGIN_HOLLOW);
            pSink->AddArc(D2D1::ArcSegment(
                endPt,
                D2D1::SizeF(radius, radius),
                0.0f,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                (sweepAngle > 180.0f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
            ));
            pSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pSink->Close();
            pSink->Release();

            rt->DrawGeometry(pArcGeom, activeBrush, strokeWidth, m_renderer.GetRoundStrokeStyle());
        }
        pArcGeom->Release();
    }
}

// 主窗口过程：Windows 的定时器、鼠标、拖动、贴边和右键菜单都从这里进入。
LRESULT CALLBACK MiniBarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MiniBarWindow* pThis = nullptr;
    // 创建窗口时把 this 指针保存到窗口实例，后续静态窗口过程才能访问成员变量。
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MiniBarWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<MiniBarWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
        case WM_TIMER:
            // 只处理本程序创建的 1 秒刷新定时器。
            if (wParam == 1) {
                pThis->OnTimer();
            }
            return 0;

        case WM_NCHITTEST: {
            // 根据鼠标所在区域决定：允许拖动、允许点击，还是让鼠标事件穿透窗口。
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            RECT rc;
            GetWindowRect(hwnd, &rc);

            if (pThis->m_isCollapsed && pThis->m_dockEdge != DockEdge::None) {
                // 收起状态只保留一条窄触发区域，其他透明区域不拦截桌面操作。
                D2D1_RECT_F sRc = pThis->GetCollapsedStripeRectDip();
                // Generous trigger zone for easy grabbing or hovering
                int pxLeft = rc.left + static_cast<int>((sRc.left - 8.0f) * pThis->m_dpiScale);
                int pxRight = rc.left + static_cast<int>((sRc.right + 8.0f) * pThis->m_dpiScale);
                int pxTop = rc.top + static_cast<int>((sRc.top - 4.0f) * pThis->m_dpiScale);
                int pxBottom = rc.top + static_cast<int>((sRc.bottom + 8.0f) * pThis->m_dpiScale);

                if (pt.x >= pxLeft && pt.x <= pxRight && pt.y >= pxTop && pt.y <= pxBottom) {
                    return HTCAPTION; // Allows immediate dragging on press down!
                }
                // Click-through anywhere else in empty space
                return HTTRANSPARENT;
            }

            // 展开状态下，右侧圆形箭头是点击区域，其余胶囊区域用于拖动。
            int btnWidth = static_cast<int>(42.0f * pThis->m_dpiScale);
            bool inChevronBtn = (pt.x >= rc.right - btnWidth);

            // 自动避让：鼠标移到主体时隐藏主体，只保留右侧箭头作为操作锚点。
            if (pThis->m_autoHideOnHover && !pThis->m_isDragging && !pThis->m_flyout.IsVisible()) {
                if (inChevronBtn) {
                    // Hovering the chevron button: chevron remains visible and can be dragged or clicked!
                    if (pThis->m_isHoverHidden) {
                        pThis->m_isHoverHidden = false;
                        pThis->Render();
                    }
                    if (!pThis->m_mouseTracking) {
                        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_NONCLIENT | TME_LEAVE, hwnd, 0 };
                        TrackMouseEvent(&tme);
                        pThis->m_mouseTracking = true;
                    }
                    return HTCAPTION; // Allow drag and click
                } else {
                    // Hovering over the monitoring body: hide the body, pass clicks through to underlying windows!
                    if (!pThis->m_isHoverHidden) {
                        pThis->m_isHoverHidden = true;
                        pThis->Render();
                    }
                    // Return HTTRANSPARENT so mouse events pass directly to windows/desktop beneath
                    return HTTRANSPARENT;
                }
            }

            if (inChevronBtn) {
                return HTCLIENT;
            }
            // Entire capsule is draggable
            return HTCAPTION;
        }

        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE: {
            // 鼠标重新进入窗口时恢复自动隐藏的主体，并持续追踪离开事件。
            if (pThis->m_autoHideOnHover && pThis->m_isHoverHidden) {
                pThis->m_isHoverHidden = false;
                pThis->Render();
            }
            if (!pThis->m_mouseTracking) {
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_NONCLIENT | TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                pThis->m_mouseTracking = true;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_NCMOUSELEAVE:
        case WM_MOUSELEAVE: {
            // 鼠标离开后恢复正常显示，并在贴边状态下按条件收起主条。
            pThis->m_mouseTracking = false;
            // Restore visibility if hidden by hover
            if (pThis->m_autoHideOnHover && pThis->m_isHoverHidden) {
                pThis->m_isHoverHidden = false;
                pThis->Render();
            }
            // Only collapse if docked, not actively dragging, and flyout is closed
            if (pThis->m_dockEdge != DockEdge::None && !pThis->m_isDragging && !pThis->m_flyout.IsVisible()) {
                POINT pt;
                GetCursorPos(&pt);
                RECT rc;
                GetWindowRect(hwnd, &rc);
                if (!PtInRect(&rc, pt)) {
                    pThis->m_isCollapsed = true;
                    pThis->Render();
                }
            }
            return 0;
        }

        case WM_NCLBUTTONDOWN: {
            // 非客户区点击通常来自 HTCAPTION，用于原生窗口拖动。
            pThis->m_ptDown = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            pThis->m_timeDown = GetTickCount64();
            pThis->m_dragStartCursor = pThis->m_ptDown;
            pThis->m_lastDragMousePos = pThis->m_ptDown;
            pThis->m_initialDockEdge = pThis->m_dockEdge;
            pThis->m_wasCollapsedBeforeDrag = pThis->m_isCollapsed;
            pThis->m_isDragging = true;
            pThis->m_isCollapsed = false;
            pThis->m_hasMovedDuringDrag = false;
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_LBUTTONDOWN: {
            // 客户区点击通常来自右侧箭头，先记录位置和时间，稍后区分点击与拖动。
            pThis->m_timeDown = GetTickCount64();
            GetCursorPos(&pThis->m_ptDown);
            pThis->m_dragStartCursor = pThis->m_ptDown;
            pThis->m_lastDragMousePos = pThis->m_ptDown;
            pThis->m_initialDockEdge = pThis->m_dockEdge;
            pThis->m_wasCollapsedBeforeDrag = pThis->m_isCollapsed;
            pThis->m_isDragging = false;
            pThis->m_hasMovedDuringDrag = false;
            return 0;
        }

        case WM_MOVING: {
            // 拖动过程中实时判断是否接近屏幕边缘，并进行方向敏感的吸附。
            pThis->m_isDragging = true;
            pThis->m_hasMovedDuringDrag = true;
            if (pThis->m_isCollapsed) {
                pThis->m_isCollapsed = false;
                pThis->Render();
            }
            RECT* pRect = reinterpret_cast<RECT*>(lParam);
            if (pRect) {
                HMONITOR hMon = MonitorFromRect(pRect, MONITOR_DEFAULTTONEAREST);
                if (hMon) {
                    MONITORINFO mi = { sizeof(MONITORINFO) };
                    if (GetMonitorInfo(hMon, &mi)) {
                        const RECT& work = mi.rcWork;
                        // 距离边缘 20 像素以内才尝试吸附。
                        const int SNAP_DIST = 20;

                        POINT curMouse;
                        GetCursorPos(&curMouse);
                        int mouseDx = curMouse.x - pThis->m_lastDragMousePos.x;
                        int mouseDy = curMouse.y - pThis->m_lastDragMousePos.y;
                        pThis->m_lastDragMousePos = curMouse;

                        int width = pRect->right - pRect->left;
                        int height = pRect->bottom - pRect->top;

                        // 只有鼠标正朝某条边移动时才吸附，避免用户从边缘拖开时又被吸回去。

                        // Snap Top: ONLY if moving UP (mouseDy <= 0)
                        if (mouseDy <= 0 && abs(pRect->top - work.top) <= SNAP_DIST) {
                            pRect->top = work.top;
                            pRect->bottom = pRect->top + height;
                        }
                        // Snap Bottom: ONLY if moving DOWN (mouseDy >= 0)
                        else if (mouseDy >= 0 && abs(pRect->bottom - work.bottom) <= SNAP_DIST) {
                            pRect->bottom = work.bottom;
                            pRect->top = pRect->bottom - height;
                        }

                        // Snap Left: ONLY if moving LEFT (mouseDx <= 0)
                        if (mouseDx <= 0 && abs(pRect->left - work.left) <= SNAP_DIST) {
                            pRect->left = work.left;
                            pRect->right = pRect->left + width;
                        }
                        // Snap Right: ONLY if moving RIGHT (mouseDx >= 0)
                        else if (mouseDx >= 0 && abs(pRect->right - work.right) <= SNAP_DIST) {
                            pRect->right = work.right;
                            pRect->left = pRect->right - width;
                        }
                    }
                }
            }
            return TRUE;
        }

        case WM_EXITSIZEMOVE: {
            // 拖动结束：保存位置、更新贴边状态，并区分“点击”与“真正移动”。
            pThis->m_isDragging = false;
            pThis->UpdateDockState();
            pThis->m_initialDockEdge = pThis->m_dockEdge;
            pThis->SaveSettings();

            // 鼠标位移很小且持续时间很短时，将本次操作视为点击而不是拖动。
            POINT pt;
            GetCursorPos(&pt);
            int dx = abs(pt.x - pThis->m_dragStartCursor.x);
            int dy = abs(pt.y - pThis->m_dragStartCursor.y);
            ULONGLONG duration = GetTickCount64() - pThis->m_timeDown;

            if (!pThis->m_hasMovedDuringDrag && dx <= 5 && dy <= 5 && duration < 350) {
                // User simply clicked the capsule without dragging
                if (pThis->m_initialDockEdge != DockEdge::None && pThis->m_wasCollapsedBeforeDrag) {
                    pThis->m_isCollapsed = false;
                    pThis->Render();
                } else {
                    pThis->ToggleFlyout();
                }
            } else {
                if (pThis->m_dockEdge != DockEdge::None) {
                    // If docked, check if mouse is still hovering
                    RECT rc;
                    GetWindowRect(hwnd, &rc);
                    if (!PtInRect(&rc, pt)) {
                        pThis->m_isCollapsed = true;
                    } else {
                        pThis->m_isCollapsed = false;
                        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_NONCLIENT | TME_LEAVE, hwnd, 0 };
                        TrackMouseEvent(&tme);
                        pThis->m_mouseTracking = true;
                    }
                } else {
                    pThis->m_isCollapsed = false; // Floating freely on desktop!
                }
                pThis->Render();
            }
            return 0;
        }

        case WM_NCLBUTTONUP: {
            // 非客户区拖动结束后的点击判定，逻辑与普通客户区点击保持一致。
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int dx = abs(pt.x - pThis->m_ptDown.x);
            int dy = abs(pt.y - pThis->m_ptDown.y);
            if (!pThis->m_hasMovedDuringDrag && dx <= 5 && dy <= 5 && (GetTickCount64() - pThis->m_timeDown < 350)) {
                if (pThis->m_wasCollapsedBeforeDrag) {
                    pThis->m_isCollapsed = false;
                    pThis->Render();
                } else {
                    pThis->ToggleFlyout();
                }
                return 0;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        case WM_LBUTTONUP: {
            // 右侧按钮的客户区点击：展开详情，或从收起状态恢复主条。
            POINT pt;
            GetCursorPos(&pt);
            int dx = abs(pt.x - pThis->m_ptDown.x);
            int dy = abs(pt.y - pThis->m_ptDown.y);
            if (dx <= 5 && dy <= 5 && (GetTickCount64() - pThis->m_timeDown < 350)) {
                if (pThis->m_wasCollapsedBeforeDrag) {
                    pThis->m_isCollapsed = false;
                    pThis->Render();
                } else {
                    pThis->ToggleFlyout();
                }
            }
            return 0;
        }

        case WM_RBUTTONUP:
        case WM_NCRBUTTONUP: {
            // 右键菜单同时支持主题、显示样式、自动隐藏、开机启动和退出。
            POINT pt;
            GetCursorPos(&pt);

            CustomContextMenu menu;
            menu.SetTheme(pThis->m_preset);

            if (pThis->m_dockEdge != DockEdge::None) {
                menu.AddItem(1004, L"脱离贴边吸附 ⎋");
                menu.AddSeparator();
            }

            // Submenu for Theme Presets
            std::vector<CustomContextMenu::MenuItem> themeItems;
            themeItems.push_back({ 2000, L"雾透冰晶 🫧", false, pThis->m_preset == Theme::ThemePreset::TranslucentGlass, false });
            themeItems.push_back({ 2001, L"冰川凝白 ❄️", false, pThis->m_preset == Theme::ThemePreset::GlacierWhite, false });
            themeItems.push_back({ 2002, L"极光晨曦 🔮", false, pThis->m_preset == Theme::ThemePreset::AuroraGlass, false });
            themeItems.push_back({ 2003, L"深邃暗夜 🌙", false, pThis->m_preset == Theme::ThemePreset::ObsidianDark, false });
            themeItems.push_back({ 2004, L"赛博赛道 ⚡", false, pThis->m_preset == Theme::ThemePreset::CyberNeon, false });
            menu.AddSubMenu(L"主题配色预设 🎨", themeItems);

            menu.AddItem(1005, pThis->m_style == ThemeStyle::RingGlass ? L"胶囊形态：暗夜条形 📊" : L"胶囊形态：毛玻璃圆环 🔮");
            menu.AddItem(1001, L"展开/收起详情面板");
            menu.AddSeparator();

            menu.AddItem(1006, L"鼠标移入自动避让 👻", pThis->m_autoHideOnHover);
            bool isAutoStart = ConfigManager::IsAutoStartEnabled();
            menu.AddItem(1007, L"开机自动启动 🚀", isAutoStart);
            menu.AddSeparator();

            menu.AddItem(1002, L"退出部件 ✕");

            int cmd = menu.Track(hwnd, pt.x, pt.y);

            if (cmd == 1004) {
                pThis->UndockFromEdge();
                pThis->SaveSettings();
            } else if (cmd == 1005) {
                pThis->SetThemeStyle(pThis->m_style == ThemeStyle::RingGlass ? ThemeStyle::BarDark : ThemeStyle::RingGlass);
            } else if (cmd == 1001) {
                pThis->ToggleFlyout();
            } else if (cmd == 2000) {
                pThis->SetThemePreset(Theme::ThemePreset::TranslucentGlass);
            } else if (cmd == 2001) {
                pThis->SetThemePreset(Theme::ThemePreset::GlacierWhite);
            } else if (cmd == 2002) {
                pThis->SetThemePreset(Theme::ThemePreset::AuroraGlass);
            } else if (cmd == 2003) {
                pThis->SetThemePreset(Theme::ThemePreset::ObsidianDark);
            } else if (cmd == 2004) {
                pThis->SetThemePreset(Theme::ThemePreset::CyberNeon);
            } else if (cmd == 1006) {
                pThis->SetAutoHideOnHover(!pThis->m_autoHideOnHover);
            } else if (cmd == 1007) {
                ConfigManager::SetAutoStart(!isAutoStart);
            } else if (cmd == 1002) {
                pThis->SaveSettings();
                PostQuitMessage(0);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MiniBarWindow::OnTimer() {
    // 定时器运行在 UI 线程：只复制采集快照，不直接执行耗时的系统查询。
    m_snapshot = m_collector.GetLatestSnapshot();
    m_flyout.UpdateSnapshot(m_snapshot);

    // 贴边、未拖动且详情关闭时，确认鼠标是否已经离开并决定是否收起。
    if (m_dockEdge != DockEdge::None && !m_isDragging && !m_flyout.IsVisible() && !m_isCollapsed) {
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        if (!PtInRect(&rc, pt)) {
            m_isCollapsed = true;
        }
    }

    // 自动隐藏状态下，确认鼠标离开后恢复主体显示。
    if (m_autoHideOnHover && m_isHoverHidden && !m_isDragging) {
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        if (!PtInRect(&rc, pt)) {
            m_isHoverHidden = false;
        }
    }

    Render();

    static int tickCount = 0;
    if (++tickCount % 5 == 0) {
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    }
}

void MiniBarWindow::Render() {
    // 所有模式共用一帧 Direct2D 绘制：先绑定渲染目标，再按当前状态选择绘制分支。
    auto rt = m_renderer.BeginDraw();
    if (!rt) return;

    // 使用 DIP 坐标绘制，渲染器负责根据 DPI 缩放到实际像素。
    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));

    auto pal = Theme::GetPresetPalette(m_preset);
    D2D1_COLOR_F cpuStatusCol = Theme::GetCpuStatusColor(m_snapshot.cpuUsageTotal);

    float W = m_dipWidth;
    float H = m_dipHeight;

    // 模式一：贴边收起，只绘制 CPU/RAM/GPU 三段窄指标条。
    if (m_isCollapsed && m_dockEdge != DockEdge::None) {
        RenderCollapsedTriBar(rt, pal, W, H);
        m_renderer.EndDraw();
        return;
    }

    // 模式二：鼠标自动避让，隐藏主体，仅保留右侧圆形箭头作为拖动和点击入口。
    if (m_autoHideOnHover && m_isHoverHidden) {
        float centerX = 320.0f;
        float centerY = H / 2.0f;
        float radius = 12.5f;

        ID2D1SolidColorBrush* pBtnBgBrush = nullptr;
        ID2D1SolidColorBrush* pBtnBorderBrush = nullptr;
        ID2D1SolidColorBrush* pBtnIconBrush = nullptr;

        rt->CreateSolidColorBrush(pal.ChevronBtnBg, &pBtnBgBrush);
        rt->CreateSolidColorBrush(pal.ChevronBtnBorder, &pBtnBorderBrush);
        rt->CreateSolidColorBrush(pal.ChevronIcon, &pBtnIconBrush);

        D2D1_ELLIPSE btnEll = D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radius, radius);
        if (pBtnBgBrush) rt->FillEllipse(btnEll, pBtnBgBrush);
        if (pBtnBorderBrush) rt->DrawEllipse(btnEll, pBtnBorderBrush, 1.0f);

        DrawChevron(rt, centerX, centerY, pBtnIconBrush);

        if (pBtnBgBrush) pBtnBgBrush->Release();
        if (pBtnBorderBrush) pBtnBorderBrush->Release();
        if (pBtnIconBrush) pBtnIconBrush->Release();

        m_renderer.EndDraw();
        return;
    }

    // 模式三：正常展开，根据用户选择绘制毛玻璃圆环或深色条形样式。
    if (m_style == ThemeStyle::RingGlass) {
        RenderRingGlassStyle(rt, pal, cpuStatusCol, W, H);
    } else {
        RenderBarDarkStyle(rt, pal, cpuStatusCol, W, H);
    }

    m_renderer.EndDraw();
}

void MiniBarWindow::RenderRingGlassStyle(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, D2D1_COLOR_F cpuStatusCol, float W, float H) {
    // Frosted Translucent Liquid Glass Background
    D2D1_ROUNDED_RECT pillRc = D2D1::RoundedRect(
        D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f),
        H / 2.0f, H / 2.0f
    );

    // Glass Base Tint
    ID2D1SolidColorBrush* pGlassBg = nullptr;
    rt->CreateSolidColorBrush(pal.WindowBg, &pGlassBg);
    if (pGlassBg) {
        rt->FillRoundedRectangle(pillRc, pGlassBg);
        pGlassBg->Release();
    }

    // Glass Specular Rim Highlight (Gradient from top white glow to bottom subtle shadow)
    ID2D1GradientStopCollection* pGlassStops = nullptr;
    D2D1_GRADIENT_STOP glassStops[3];
    glassStops[0].color = pal.WindowBorder;
    glassStops[0].position = 0.0f;
    glassStops[1].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.4f);
    glassStops[1].position = 0.45f;
    glassStops[2].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.15f);
    glassStops[2].position = 1.0f;

    rt->CreateGradientStopCollection(glassStops, 3, &pGlassStops);
    if (pGlassStops) {
        ID2D1LinearGradientBrush* pGlassBorder = nullptr;
        rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.5f), D2D1::Point2F(0.0f, H - 0.5f)),
            pGlassStops,
            &pGlassBorder
        );
        if (pGlassBorder) {
            rt->DrawRoundedRectangle(pillRc, pGlassBorder, 1.2f);
            pGlassBorder->Release();
        }
        pGlassStops->Release();
    }

    // Common Brushes mapped to Theme::Palette
    ID2D1SolidColorBrush* pWhiteIconBrush = nullptr;
    ID2D1SolidColorBrush* pLabelBrush = nullptr;
    ID2D1SolidColorBrush* pBoldWhiteText = nullptr;
    ID2D1SolidColorBrush* pRingTrackBrush = nullptr;
    ID2D1SolidColorBrush* pCpuArcBrush = nullptr;
    ID2D1SolidColorBrush* pRamArcBrush = nullptr;
    ID2D1SolidColorBrush* pNetDownBrush = nullptr;
    ID2D1SolidColorBrush* pNetUpBrush = nullptr;
    ID2D1SolidColorBrush* pDividerBrush = nullptr;
    ID2D1SolidColorBrush* pBtnBgBrush = nullptr;
    ID2D1SolidColorBrush* pBtnBorderBrush = nullptr;
    ID2D1SolidColorBrush* pBtnIconBrush = nullptr;

    D2D1_COLOR_F iconColor = pal.isLight ? D2D1::ColorF(0.20f, 0.25f, 0.35f, 0.90f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.88f);

    rt->CreateSolidColorBrush(iconColor, &pWhiteIconBrush);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pLabelBrush);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pBoldWhiteText);
    rt->CreateSolidColorBrush(pal.TrackBorder, &pRingTrackBrush);
    rt->CreateSolidColorBrush(cpuStatusCol, &pCpuArcBrush);
    rt->CreateSolidColorBrush(pal.RamPurple, &pRamArcBrush);
    rt->CreateSolidColorBrush(pal.NetDownGreen, &pNetDownBrush);
    rt->CreateSolidColorBrush(pal.NetUpBlue, &pNetUpBrush);
    rt->CreateSolidColorBrush(pal.DividerLine, &pDividerBrush);
    rt->CreateSolidColorBrush(pal.ChevronBtnBg, &pBtnBgBrush);
    rt->CreateSolidColorBrush(pal.ChevronBtnBorder, &pBtnBorderBrush);
    rt->CreateSolidColorBrush(pal.ChevronIcon, &pBtnIconBrush);

    // ==========================================
    // SECTION 1: CPU (Icon + Text Column + Ring Gauge)
    // ==========================================
    {
        float secX = 14.0f;
        // Microchip Icon
        DrawCpuIcon(rt, secX, 13.0f, pWhiteIconBrush);

        // Label "CPU" (top)
        if (pLabelBrush && m_renderer.GetFormatTiny()) {
            rt->DrawTextW(L"CPU", 3, m_renderer.GetFormatTiny(),
                D2D1::RectF(secX + 18.0f, 7.5f, secX + 48.0f, 18.5f), pLabelBrush);
        }

        // Value (bottom bold)
        wchar_t valBuf[16];
        swprintf_s(valBuf, L"%.0f%%", m_snapshot.cpuUsageTotal);
        if (pBoldWhiteText && m_renderer.GetFormatValue()) {
            rt->DrawTextW(valBuf, (UINT32)wcslen(valBuf), m_renderer.GetFormatValue(),
                D2D1::RectF(secX + 18.0f, 16.5f, secX + 64.0f, 33.5f), pBoldWhiteText);
        }

        // Circular Ring Gauge
        DrawRingGauge(rt, secX + 68.0f, H / 2.0f, 10.0f, 3.6f, m_snapshot.cpuUsageTotal, pRingTrackBrush, pCpuArcBrush);
    }

    // Divider 1
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(104.0f, 11.0f), D2D1::Point2F(104.0f, H - 11.0f), pDividerBrush, 1.0f);
    }

    // ==========================================
    // SECTION 2: RAM (Icon + Text Column + Ring Gauge)
    // ==========================================
    {
        float secX = 118.0f;
        // RAM Icon
        DrawRamIcon(rt, secX, 13.0f, pWhiteIconBrush);

        // Label "RAM" (top)
        if (pLabelBrush && m_renderer.GetFormatTiny()) {
            rt->DrawTextW(L"RAM", 3, m_renderer.GetFormatTiny(),
                D2D1::RectF(secX + 19.0f, 7.0f, secX + 48.0f, 18.5f), pLabelBrush);
        }

        // Value (bottom bold)
        wchar_t valBuf[16];
        swprintf_s(valBuf, L"%.0f%%", m_snapshot.memoryUsagePercent);
        if (pBoldWhiteText && m_renderer.GetFormatValue()) {
            rt->DrawTextW(valBuf, (UINT32)wcslen(valBuf), m_renderer.GetFormatValue(),
                D2D1::RectF(secX + 19.0f, 16.5f, secX + 65.0f, 33.5f), pBoldWhiteText);
        }

        // Circular Ring Gauge
        DrawRingGauge(rt, secX + 69.0f, H / 2.0f, 10.0f, 3.6f, m_snapshot.memoryUsagePercent, pRingTrackBrush, pRamArcBrush);
    }

    // Divider 2
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(210.0f, 11.0f), D2D1::Point2F(210.0f, H - 11.0f), pDividerBrush, 1.0f);
    }

    // ==========================================
    // SECTION 3: NETWORK (Two Rows with Colored Arrows)
    // ==========================================
    {
        float secX = 222.0f;
        float textX = secX + 13.0f;
        float textW = 60.0f;

        // Row 1: Download
        if (pNetDownBrush && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(L"↓", 1, m_renderer.GetFormatSmall(),
                D2D1::RectF(secX, 4.5f, secX + 12.0f, 18.5f), pNetDownBrush);
        }
        std::wstring downText = FormatSpeed(m_snapshot.downloadSpeedBps);
        if (pBoldWhiteText && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(downText.c_str(), (UINT32)downText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(textX, 4.0f, textX + textW, 18.5f), pBoldWhiteText);
        }

        // Row 2: Upload
        if (pNetUpBrush && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(L"↑", 1, m_renderer.GetFormatSmall(),
                D2D1::RectF(secX, 19.5f, secX + 12.0f, 33.5f), pNetUpBrush);
        }
        std::wstring upText = FormatSpeed(m_snapshot.uploadSpeedBps);
        if (pBoldWhiteText && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(upText.c_str(), (UINT32)upText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(textX, 19.0f, textX + textW, 33.5f), pBoldWhiteText);
        }
    }

    // Divider 3
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(296.0f, 11.0f), D2D1::Point2F(296.0f, H - 11.0f), pDividerBrush, 1.0f);
    }

    // ==========================================
    // SECTION 4: ACTION BUTTON (Circular Chevron Pill)
    // ==========================================
    {
        float centerX = 320.0f;
        float centerY = H / 2.0f;
        float radius = 12.5f;

        D2D1_ELLIPSE btnEll = D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radius, radius);
        if (pBtnBgBrush) rt->FillEllipse(btnEll, pBtnBgBrush);
        if (pBtnBorderBrush) rt->DrawEllipse(btnEll, pBtnBorderBrush, 1.0f);

        DrawChevron(rt, centerX, centerY, pBtnIconBrush);
    }

    // Release brushes
    if (pWhiteIconBrush) pWhiteIconBrush->Release();
    if (pLabelBrush) pLabelBrush->Release();
    if (pBoldWhiteText) pBoldWhiteText->Release();
    if (pRingTrackBrush) pRingTrackBrush->Release();
    if (pCpuArcBrush) pCpuArcBrush->Release();
    if (pRamArcBrush) pRamArcBrush->Release();
    if (pNetDownBrush) pNetDownBrush->Release();
    if (pNetUpBrush) pNetUpBrush->Release();
    if (pDividerBrush) pDividerBrush->Release();
    if (pBtnBgBrush) pBtnBgBrush->Release();
    if (pBtnBorderBrush) pBtnBorderBrush->Release();
    if (pBtnIconBrush) pBtnIconBrush->Release();
}

void MiniBarWindow::RenderBarDarkStyle(ID2D1DCRenderTarget* rt, const Theme::Palette& pal, D2D1_COLOR_F cpuStatusCol, float W, float H) {
    ID2D1SolidColorBrush* pWindowBg = nullptr;
    ID2D1SolidColorBrush* pWindowBorder = nullptr;
    ID2D1SolidColorBrush* pDividerBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBgBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBorderBrush = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pCpuTextBrush = nullptr;
    ID2D1SolidColorBrush* pRamTextBrush = nullptr;
    ID2D1SolidColorBrush* pNetDownBrush = nullptr;
    ID2D1SolidColorBrush* pNetUpBrush = nullptr;
    ID2D1SolidColorBrush* pBtnBgBrush = nullptr;
    ID2D1SolidColorBrush* pBtnBorderBrush = nullptr;
    ID2D1SolidColorBrush* pBtnIconBrush = nullptr;

    rt->CreateSolidColorBrush(pal.WindowBg, &pWindowBg);
    rt->CreateSolidColorBrush(pal.WindowBorder, &pWindowBorder);
    rt->CreateSolidColorBrush(pal.DividerLine, &pDividerBrush);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBgBrush);
    rt->CreateSolidColorBrush(pal.TrackBorder, &pTrackBorderBrush);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(cpuStatusCol, &pCpuTextBrush);
    rt->CreateSolidColorBrush(pal.RamPurple, &pRamTextBrush);
    rt->CreateSolidColorBrush(pal.NetDownGreen, &pNetDownBrush);
    rt->CreateSolidColorBrush(pal.NetUpBlue, &pNetUpBrush);
    rt->CreateSolidColorBrush(pal.ChevronBtnBg, &pBtnBgBrush);
    rt->CreateSolidColorBrush(pal.ChevronBtnBorder, &pBtnBorderBrush);
    rt->CreateSolidColorBrush(pal.ChevronIcon, &pBtnIconBrush);

    // Pill Capsule Background
    D2D1_ROUNDED_RECT pillRc = D2D1::RoundedRect(
        D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f),
        H / 2.0f, H / 2.0f
    );
    if (pWindowBg) rt->FillRoundedRectangle(pillRc, pWindowBg);
    if (pWindowBorder) rt->DrawRoundedRectangle(pillRc, pWindowBorder, 1.2f);

    // Dynamic Snapped Edge Status Stripe when docked (Top/Bottom/Left/Right)
    if (m_dockEdge != DockEdge::None && pCpuTextBrush) {
        D2D1_RECT_F sRc = GetCollapsedStripeRectDip();
        D2D1_ROUNDED_RECT stripeRrc = D2D1::RoundedRect(sRc, 2.0f, 2.0f);
        rt->FillRoundedRectangle(stripeRrc, pCpuTextBrush);
    }

    // SECTION 1: CPU (Icon + Label + Value + Gradient Bar)
    {
        float secX = 14.0f;
        float barW = 76.0f;

        DrawCpuIcon(rt, secX, 8.5f, pTextSecondary);

        if (pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"CPU", 3, m_renderer.GetFormatLabel(),
                D2D1::RectF(secX + 18.0f, 7.0f, secX + 44.0f, 22.0f), pTextSecondary);
        }

        wchar_t valBuf[16];
        swprintf_s(valBuf, L"%.0f%%", m_snapshot.cpuUsageTotal);
        if (pCpuTextBrush && m_renderer.GetFormatValue()) {
            rt->DrawTextW(valBuf, (UINT32)wcslen(valBuf), m_renderer.GetFormatValue(),
                D2D1::RectF(secX + 44.0f, 5.5f, secX + barW + 2.0f, 22.5f), pCpuTextBrush);
        }

        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(
            D2D1::RectF(secX, 24.5f, secX + barW, 29.0f), 2.25f, 2.25f
        );
        if (pTrackBgBrush) rt->FillRoundedRectangle(trackRc, pTrackBgBrush);
        if (pTrackBorderBrush) rt->DrawRoundedRectangle(trackRc, pTrackBorderBrush, 0.8f);

        float fillW = barW * std::clamp(m_snapshot.cpuUsageTotal / 100.0f, 0.05f, 1.0f);
        ID2D1GradientStopCollection* pStops = nullptr;
        D2D1_GRADIENT_STOP stops[2];
        stops[0].color = pal.CpuMint1;
        stops[0].position = 0.0f;
        stops[1].color = cpuStatusCol;
        stops[1].position = 1.0f;

        rt->CreateGradientStopCollection(stops, 2, &pStops);
        if (pStops) {
            ID2D1LinearGradientBrush* pGradBrush = nullptr;
            rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(secX, 24.5f),
                    D2D1::Point2F(secX + barW, 24.5f)
                ),
                pStops,
                &pGradBrush
            );
            if (pGradBrush) {
                D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(
                    D2D1::RectF(secX, 24.5f, secX + fillW, 29.0f), 2.25f, 2.25f
                );
                rt->FillRoundedRectangle(fillRc, pGradBrush);
                pGradBrush->Release();
            }
            pStops->Release();
        }
    }

    // Divider 1
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(102.0f, 10.0f), D2D1::Point2F(102.0f, H - 10.0f), pDividerBrush, 1.0f);
    }

    // SECTION 2: RAM (Icon + Label + Value + Gradient Bar)
    {
        float secX = 114.0f;
        float barW = 76.0f;

        DrawRamIcon(rt, secX, 8.5f, pTextSecondary);

        if (pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"RAM", 3, m_renderer.GetFormatLabel(),
                D2D1::RectF(secX + 19.0f, 7.0f, secX + 46.0f, 22.0f), pTextSecondary);
        }

        wchar_t valBuf[16];
        swprintf_s(valBuf, L"%.0f%%", m_snapshot.memoryUsagePercent);
        if (pRamTextBrush && m_renderer.GetFormatValue()) {
            rt->DrawTextW(valBuf, (UINT32)wcslen(valBuf), m_renderer.GetFormatValue(),
                D2D1::RectF(secX + 46.0f, 5.5f, secX + barW + 2.0f, 22.5f), pRamTextBrush);
        }

        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(
            D2D1::RectF(secX, 24.5f, secX + barW, 29.0f), 2.25f, 2.25f
        );
        if (pTrackBgBrush) rt->FillRoundedRectangle(trackRc, pTrackBgBrush);
        if (pTrackBorderBrush) rt->DrawRoundedRectangle(trackRc, pTrackBorderBrush, 0.8f);

        float fillW = barW * std::clamp(m_snapshot.memoryUsagePercent / 100.0f, 0.05f, 1.0f);
        ID2D1GradientStopCollection* pStops = nullptr;
        D2D1_GRADIENT_STOP stops[2];
        stops[0].color = pal.RamIndigo;
        stops[0].position = 0.0f;
        stops[1].color = pal.RamPurple;
        stops[1].position = 1.0f;

        rt->CreateGradientStopCollection(stops, 2, &pStops);
        if (pStops) {
            ID2D1LinearGradientBrush* pGradBrush = nullptr;
            rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(secX, 24.5f),
                    D2D1::Point2F(secX + barW, 24.5f)
                ),
                pStops,
                &pGradBrush
            );
            if (pGradBrush) {
                D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(
                    D2D1::RectF(secX, 24.5f, secX + fillW, 29.0f), 2.25f, 2.25f
                );
                rt->FillRoundedRectangle(fillRc, pGradBrush);
                pGradBrush->Release();
            }
            pStops->Release();
        }
    }

    // Divider 2
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(202.0f, 10.0f), D2D1::Point2F(202.0f, H - 10.0f), pDividerBrush, 1.0f);
    }

    // SECTION 3: NETWORK
    {
        float secX = 214.0f;
        float textX = secX + 13.0f;
        float textW = 68.0f;

        if (pNetDownBrush && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(L"↓", 1, m_renderer.GetFormatSmall(),
                D2D1::RectF(secX, 5.0f, secX + 12.0f, 18.0f), pNetDownBrush);
        }
        std::wstring downText = FormatSpeed(m_snapshot.downloadSpeedBps);
        if (pTextPrimary && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(downText.c_str(), (UINT32)downText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(textX, 5.0f, textX + textW, 18.0f), pTextPrimary);
        }

        if (pNetUpBrush && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(L"↑", 1, m_renderer.GetFormatSmall(),
                D2D1::RectF(secX, 20.0f, secX + 12.0f, 33.0f), pNetUpBrush);
        }
        std::wstring upText = FormatSpeed(m_snapshot.uploadSpeedBps);
        if (pTextPrimary && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(upText.c_str(), (UINT32)upText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(textX, 20.0f, textX + textW, 33.0f), pTextPrimary);
        }
    }

    // Divider 3
    if (pDividerBrush) {
        rt->DrawLine(D2D1::Point2F(296.0f, 10.0f), D2D1::Point2F(296.0f, H - 10.0f), pDividerBrush, 1.0f);
    }

    // SECTION 4: Circular Action Button
    {
        float centerX = 320.0f;
        float centerY = H / 2.0f;
        float radius = 12.5f;

        D2D1_ELLIPSE btnEll = D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radius, radius);
        if (pBtnBgBrush) rt->FillEllipse(btnEll, pBtnBgBrush);
        if (pBtnBorderBrush) rt->DrawEllipse(btnEll, pBtnBorderBrush, 1.0f);

        DrawChevron(rt, centerX, centerY, pBtnIconBrush);
    }

    // Release brushes
    if (pWindowBg) pWindowBg->Release();
    if (pWindowBorder) pWindowBorder->Release();
    if (pDividerBrush) pDividerBrush->Release();
    if (pTrackBgBrush) pTrackBgBrush->Release();
    if (pTrackBorderBrush) pTrackBorderBrush->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pCpuTextBrush) pCpuTextBrush->Release();
    if (pRamTextBrush) pRamTextBrush->Release();
    if (pNetDownBrush) pNetDownBrush->Release();
    if (pNetUpBrush) pNetUpBrush->Release();
    if (pBtnBgBrush) pBtnBgBrush->Release();
    if (pBtnBorderBrush) pBtnBorderBrush->Release();
    if (pBtnIconBrush) pBtnIconBrush->Release();
}
