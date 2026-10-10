/*
 * 模块说明：Lite 详情页面实现
 * ShowNear 依据悬浮条定位；各 Draw 函数按顺序绘制模块，Render 负责一帧的开始和提交。
 * 原始 COM 指针需要 Release，临时画笔和几何对象也必须在用完后释放。
 */
// 详情弹出窗口的声明。
#include "lite/windows/FlyoutWindow.h"
// DPI 换算工具：在 DIP 和实际像素之间转换。
#include "ui/platform/DpiHelper.h"
// 主题颜色和主题配置。
#include "ui/theme/Theme.h"
// 提供 std::clamp，用于限制进度条和图表数值范围。
#include <algorithm>
#include <cmath>

// 构造详情弹窗对象；窗口和渲染资源在 Create 中创建。
FlyoutWindow::FlyoutWindow() {}

// 销毁详情窗口，释放画刷池，渲染器成员随后自动释放图形资源。
FlyoutWindow::~FlyoutWindow() {
    m_brushes.ReleaseAll();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

// 注册并创建隐藏的详情窗口，同时初始化透明 Direct2D 渲染器。
/**
 * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
 */
bool FlyoutWindow::Create() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = FlyoutWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"PcInfoTool_FlyoutWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    m_dpiScale = DpiHelper::GetScaleForWindow(NULL);
    m_pixelWidth = DpiHelper::DipToPx(m_dipWidth, m_dpiScale);
    m_pixelHeight = DpiHelper::DipToPx(m_dipHeight, m_dpiScale);

    m_hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
        wc.lpszClassName,
        L"PcInfoTool_Flyout",
        WS_POPUP,
        0, 0, m_pixelWidth, m_pixelHeight,
        NULL, NULL, wc.hInstance, this
    );

    if (!m_hwnd) return false;

    // Initialize Layered Direct2D renderer in software mode for per-pixel alpha & ultra light footprint
    if (!m_renderer.Initialize(m_hwnd, m_pixelWidth, m_pixelHeight)) {
        return false;
    }

    return true;
}

// 根据主监控条位置显示详情窗口，并在屏幕边缘自动调整位置。
/**
 * 依据锚点矩形显示窗口；具体屏幕定位与 DPI 转换在实现内完成。
 */
void FlyoutWindow::ShowNear(const RECT& anchorRect) {
    if (!m_hwnd) return;

    // 根据锚点窗口所在显示器动态校准 DPI 与像素尺寸
    float newScale = DpiHelper::GetScaleForWindow(m_hwnd);
    if (newScale > 0.1f && (abs(newScale - m_dpiScale) > 0.01f || m_pixelWidth != DpiHelper::DipToPx(m_dipWidth, newScale))) {
        m_dpiScale = newScale;
        m_pixelWidth = DpiHelper::DipToPx(m_dipWidth, m_dpiScale);
        m_pixelHeight = DpiHelper::DipToPx(m_dipHeight, m_dpiScale);
        m_renderer.Resize(m_pixelWidth, m_pixelHeight);
        m_brushes.ReleaseAll();
    }

    HMONITOR hMon = MonitorFromRect(&anchorRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    RECT workArea = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (hMon && GetMonitorInfo(hMon, &mi)) {
        workArea = mi.rcWork;
    }

    // 水平对齐：由于宽度统一为 345 DIP，直接与 MiniBar 左侧对齐；若超出屏幕右侧则向右侧边缘对齐
    int x = anchorRect.left;
    if (x + m_pixelWidth > workArea.right - 10) {
        x = anchorRect.right - m_pixelWidth;
    }
    if (x + m_pixelWidth > workArea.right - 8) {
        x = workArea.right - m_pixelWidth - 8;
    }
    if (x < workArea.left + 8) {
        x = workArea.left + 8;
    }

    // 垂直对齐：优先显示在 MiniBar 下方，空间不足则显示在上方
    int y = anchorRect.bottom + 8;
    if (y + m_pixelHeight > workArea.bottom - 8) {
        y = anchorRect.top - m_pixelHeight - 8;
    }
    if (y < workArea.top + 8) {
        y = workArea.top + 8;
    }

    m_visible = true;
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_pixelWidth, m_pixelHeight, SWP_SHOWWINDOW);
    SetForegroundWindow(m_hwnd);
    SetActiveWindow(m_hwnd);
    Render();

    // 展开首帧立即触发工作集修剪，抹平创建窗口表面和字形表的首帧瞬时波峰
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
}

// 隐藏详情窗口，释放工作集并保留窗口和渲染资源以便下次快速显示。
/**
 * 隐藏当前窗口并更新交互状态；隐藏本身并不等于销毁所有 C++ 对象。
 */
void FlyoutWindow::Hide() {
    if (!m_visible) return;
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);
    // 详情卡片收起后释放所有缓存的画刷，彻底还给 D2D 表面，并触发工作集整理
    m_brushes.ReleaseAll();
    m_renderCount = 0;
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));

    if (m_hideCallback) {
        m_hideCallback(m_hideContext);
    }
}

// 设置浅色或深色模式，并在弹窗可见时立即重绘。
/**
 * 修改亮色模式并同步相关页面或重新绘制。
 */
void FlyoutWindow::SetLightMode(bool lightMode) {
    m_isLightMode = lightMode;
    m_preset = lightMode ? Theme::ThemePreset::GlacierWhite : Theme::ThemePreset::ObsidianDark;
    m_brushes.ReleaseAll();
    if (m_visible && m_hwnd) {
        Render();
    }
}

// 更新详情窗口的主题预设。
/**
 * 切换主题预设，使下一帧使用对应调色板。
 */
void FlyoutWindow::SetThemePreset(Theme::ThemePreset preset) {
    m_preset = preset;
    m_isLightMode = (preset == Theme::ThemePreset::GlacierWhite || preset == Theme::ThemePreset::AuroraGlass);
    m_brushes.ReleaseAll();
    if (m_visible && m_hwnd) {
        Render();
    }
}

// 接收主窗口传来的最新系统指标，并在可见时刷新界面。
/**
 * 复制采集快照到窗口状态；调用方负责提供有效数据，不在这里执行硬件查询。
 * @param snapshot 采集结果；窗口保存副本，避免直接依赖采集线程可变数据。
 */
void FlyoutWindow::UpdateSnapshot(const MetricsSnapshot& snapshot) {
    m_snapshot = snapshot;
    if (m_visible && m_hwnd) {
        Render();
    }
}

// 处理详情窗口的激活、关闭按钮和销毁消息。
/**
 * Win32 窗口过程：按消息类型分发交互；用窗口用户数据关联 C++ 实例。
 * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
 * @param msg 窗口消息编号，决定后续分支。
 * @param wParam 消息附加参数，含义随 msg 改变。
 * @param lParam 消息附加参数，鼠标坐标等需按消息格式解码。
 */
LRESULT CALLBACK FlyoutWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FlyoutWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<FlyoutWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<FlyoutWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) {
                // If focus moved elsewhere outside the app, close the flyout
                pThis->Hide();
                return 0;
            }
            break;
        // 左键松开：完成拖动或确认点击，并结束鼠标捕获状态。
        case WM_LBUTTONUP: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            // Click top-right close icon
            int btnSize = static_cast<int>(32.0f * pThis->m_dpiScale);
            if (x >= pThis->m_pixelWidth - btnSize && y <= btnSize) {
                pThis->Hide();
                return 0;
            }
            break;
        }
        // 窗口销毁：按该窗口角色清理状态；只有应用主窗口应结束应用循环。
        case WM_DESTROY:
            pThis->m_hwnd = nullptr;
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// -------------------------------------------------------------
// Vector Icon Helpers
// -------------------------------------------------------------
/**
 * 绘制CPU图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param brush 借用的颜色画笔，不转移所有权。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    D2D1_ROUNDED_RECT pkg = D2D1::RoundedRect(D2D1::RectF(x + 2.5f, y + 2.5f, x + 11.5f, y + 11.5f), 1.5f, 1.5f);
    rt->DrawRoundedRectangle(pkg, brush, 1.2f);
    rt->FillRectangle(D2D1::RectF(x + 5.0f, y + 5.0f, x + 9.0f, y + 9.0f), brush);

    rt->DrawLine(D2D1::Point2F(x + 4.5f, y + 0.5f), D2D1::Point2F(x + 4.5f, y + 2.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 9.5f, y + 0.5f), D2D1::Point2F(x + 9.5f, y + 2.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 4.5f, y + 11.5f), D2D1::Point2F(x + 4.5f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 9.5f, y + 11.5f), D2D1::Point2F(x + 9.5f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 0.5f, y + 4.5f), D2D1::Point2F(x + 2.5f, y + 4.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 0.5f, y + 9.5f), D2D1::Point2F(x + 2.5f, y + 9.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 11.5f, y + 4.5f), D2D1::Point2F(x + 13.5f, y + 4.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 11.5f, y + 9.5f), D2D1::Point2F(x + 13.5f, y + 9.5f), brush, 1.2f);
}

/**
 * 绘制内存图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param brush 借用的颜色画笔，不转移所有权。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawRamIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    D2D1_ROUNDED_RECT modRc = D2D1::RoundedRect(D2D1::RectF(x + 0.5f, y + 2.0f, x + 14.5f, y + 11.5f), 1.5f, 1.5f);
    rt->DrawRoundedRectangle(modRc, brush, 1.2f);

    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 3.2f, y + 4.5f, x + 5.8f, y + 8.5f), 0.8f, 0.8f), brush);
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x + 9.2f, y + 4.5f, x + 11.8f, y + 8.5f), 0.8f, 0.8f), brush);

    rt->DrawLine(D2D1::Point2F(x + 3.0f, y + 11.5f), D2D1::Point2F(x + 3.0f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 7.5f, y + 11.5f), D2D1::Point2F(x + 7.5f, y + 13.5f), brush, 1.2f);
    rt->DrawLine(D2D1::Point2F(x + 12.0f, y + 11.5f), D2D1::Point2F(x + 12.0f, y + 13.5f), brush, 1.2f);
}

/**
 * 绘制GPU图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param brush 借用的颜色画笔，不转移所有权。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawGpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    D2D1_ROUNDED_RECT shroudRc = D2D1::RoundedRect(D2D1::RectF(x + 0.5f, y + 1.0f, x + 10.5f, y + 9.5f), 1.2f, 1.2f);
    rt->DrawRoundedRectangle(shroudRc, brush, 1.0f);

    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x + 5.5f, y + 5.2f), 2.5f, 2.5f), brush, 0.9f);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 5.5f, y + 5.2f), 0.9f, 0.9f), brush);
    rt->DrawLine(D2D1::Point2F(x + 10.5f, y + 0.5f), D2D1::Point2F(x + 10.5f, y + 10.0f), brush, 1.1f);
}

/**
 * 绘制磁盘图标；x/y 指定局部位置，brush 提供颜色，几何图形组合成符号。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param brush 借用的颜色画笔，不转移所有权。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawDiskIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    D2D1_ROUNDED_RECT body = D2D1::RoundedRect(D2D1::RectF(x + 1.0f, y + 1.5f, x + 13.0f, y + 10.5f), 1.8f, 1.8f);
    rt->DrawRoundedRectangle(body, brush, 1.1f);
    rt->DrawLine(D2D1::Point2F(x + 1.0f, y + 7.5f), D2D1::Point2F(x + 13.0f, y + 7.5f), brush, 1.0f);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 10.5f, y + 9.0f), 0.8f, 0.8f), brush);
}

// -------------------------------------------------------------
// COM 安全释放工具与波形渐变构建
// -------------------------------------------------------------
template <typename T>
static inline void SafeReleaseCOM(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

static void CreateWaveformGrad(
    ID2D1DCRenderTarget* rt,
    D2D1_COLOR_F color,
    float topAlpha,
    ID2D1GradientStopCollection** ppStops,
    ID2D1LinearGradientBrush** ppBrush
) {
    if (!rt) return;
    SafeReleaseCOM(*ppBrush);
    SafeReleaseCOM(*ppStops);

    D2D1_GRADIENT_STOP stops[2];
    stops[0].color = D2D1::ColorF(color.r, color.g, color.b, topAlpha);
    stops[0].position = 0.0f;
    stops[1].color = D2D1::ColorF(color.r, color.g, color.b, 0.0f);
    stops[1].position = 1.0f;

    rt->CreateGradientStopCollection(stops, 2, ppStops);
    if (*ppStops) {
        rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 50.0f)),
            *ppStops,
            ppBrush
        );
    }
}

/**
 * 释放本对象持有的绘图接口，为销毁或重新初始化准备。
 */
void FlyoutWindow::FlyoutBrushPool::ReleaseAll() {
    SafeReleaseCOM(pWindowBg);
    SafeReleaseCOM(pWindowBorder);
    SafeReleaseCOM(pCardBg);
    SafeReleaseCOM(pCardBorder);
    SafeReleaseCOM(pPillBg);
    SafeReleaseCOM(pTextPrimary);
    SafeReleaseCOM(pTextSecondary);
    SafeReleaseCOM(pTextMuted);
    SafeReleaseCOM(pTrackBg);
    SafeReleaseCOM(pGridLine);
    SafeReleaseCOM(pRamPurple);
    SafeReleaseCOM(pNetDownGreen);
    SafeReleaseCOM(pNetUpBlue);
    SafeReleaseCOM(pGpuCyan);
    SafeReleaseCOM(pDiskAmber);
    SafeReleaseCOM(pWhiteCore);
    SafeReleaseCOM(pGreenDotGlow);
    SafeReleaseCOM(pHaloBrush);

    SafeReleaseCOM(pCpuLine);

    SafeReleaseCOM(pGlassRim);
    SafeReleaseCOM(pGlassStops);

    SafeReleaseCOM(pCpuGradBrush);
    SafeReleaseCOM(pCpuGradStops);
    SafeReleaseCOM(pRamGradBrush);
    SafeReleaseCOM(pRamGradStops);
    SafeReleaseCOM(pNetDownGradBrush);
    SafeReleaseCOM(pNetDownGradStops);
    SafeReleaseCOM(pNetUpGradBrush);
    SafeReleaseCOM(pNetUpGradStops);
    SafeReleaseCOM(pGpuGradBrush);
    SafeReleaseCOM(pGpuGradStops);

    lastPreset = static_cast<Theme::ThemePreset>(-1);
    lastCpuColor = { 0, 0, 0, 0 };
}

/**
 * 首次使用或重建目标后创建画笔，已有可用资源时复用。
 */
void FlyoutWindow::FlyoutBrushPool::EnsureBrushes(
    ID2D1DCRenderTarget* rt,
    const Theme::Palette& pal,
    float H,
    bool isLightMode,
    Theme::ThemePreset preset,
    D2D1_COLOR_F cpuColor
) {
    if (!rt) return;

    bool themeChanged = (!pWindowBg || lastPreset != preset || lastLightMode != isLightMode);

    if (themeChanged) {
        ReleaseAll();

        lastPreset = preset;
        lastLightMode = isLightMode;

        rt->CreateSolidColorBrush(pal.WindowBg, &pWindowBg);
        rt->CreateSolidColorBrush(pal.WindowBorder, &pWindowBorder);
        rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
        rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
        rt->CreateSolidColorBrush(pal.PillBg, &pPillBg);
        rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
        rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
        rt->CreateSolidColorBrush(pal.TextMuted, &pTextMuted);
        rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);
        rt->CreateSolidColorBrush(pal.GridLine, &pGridLine);
        rt->CreateSolidColorBrush(pal.RamPurple, &pRamPurple);
        rt->CreateSolidColorBrush(pal.NetDownGreen, &pNetDownGreen);
        rt->CreateSolidColorBrush(pal.NetUpBlue, &pNetUpBlue);
        rt->CreateSolidColorBrush(pal.GpuCyan, &pGpuCyan);
        rt->CreateSolidColorBrush(pal.DiskAmber, &pDiskAmber);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pWhiteCore);
        rt->CreateSolidColorBrush(D2D1::ColorF(pal.NetDownGreen.r, pal.NetDownGreen.g, pal.NetDownGreen.b, 0.25f), &pGreenDotGlow);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f), &pHaloBrush);

        if (pal.isTranslucent) {
            D2D1_GRADIENT_STOP glassStops[3];
            glassStops[0].color = pal.WindowBorder;
            glassStops[0].position = 0.0f;
            glassStops[1].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.45f);
            glassStops[1].position = 0.40f;
            glassStops[2].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.20f);
            glassStops[2].position = 1.0f;

            rt->CreateGradientStopCollection(glassStops, 3, &pGlassStops);
            if (pGlassStops) {
                rt->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.5f), D2D1::Point2F(0.0f, H - 0.5f)),
                    pGlassStops,
                    &pGlassRim
                );
            }
        }

        float topAlpha = isLightMode ? 0.35f : 0.28f;
        CreateWaveformGrad(rt, pal.RamPurple, topAlpha, &pRamGradStops, &pRamGradBrush);
        CreateWaveformGrad(rt, pal.NetDownGreen, topAlpha, &pNetDownGradStops, &pNetDownGradBrush);
        CreateWaveformGrad(rt, pal.NetUpBlue, topAlpha, &pNetUpGradStops, &pNetUpGradBrush);
        CreateWaveformGrad(rt, pal.GpuCyan, topAlpha, &pGpuGradStops, &pGpuGradBrush);
    }

    // 检查 CPU 状态颜色是否有跨阈值变化（绿/橙/红）
    bool cpuColorChanged = (!pCpuLine ||
        lastCpuColor.r != cpuColor.r ||
        lastCpuColor.g != cpuColor.g ||
        lastCpuColor.b != cpuColor.b ||
        lastCpuColor.a != cpuColor.a);

    if (cpuColorChanged) {
        lastCpuColor = cpuColor;
        if (pCpuLine) {
            pCpuLine->SetColor(cpuColor);
        } else {
            rt->CreateSolidColorBrush(cpuColor, &pCpuLine);
        }
        float topAlpha = isLightMode ? 0.35f : 0.28f;
        CreateWaveformGrad(rt, cpuColor, topAlpha, &pCpuGradStops, &pCpuGradBrush);
    }
}

// -------------------------------------------------------------
// Reusable High-Fidelity Waveform Plotter
// -------------------------------------------------------------
// 将历史采样转换为线条/面积几何，绘制网格、曲线和尾点；坐标基于指定区域。
void FlyoutWindow::DrawWaveform(
    ID2D1DCRenderTarget* rt,
    float chartX, float chartY, float chartW, float chartH,
    const MetricHistory& history,
    float maxVal,
    ID2D1SolidColorBrush* strokeBrush,
    ID2D1LinearGradientBrush* areaBrush,
    D2D1_COLOR_F strokeCol,
    ID2D1Brush* gridBrush,
    ID2D1Brush* textBrush,
    const wchar_t* scaleTopText,
    const wchar_t* scaleMidText
) {
    if (!rt) return;

    // 1. Grid Background
    float labelColW = scaleTopText ? 30.0f : 0.0f;
    float plotX = chartX + labelColW;
    float plotW = chartW - labelColW;

    // Labels: Top, Mid, Bottom
    if (textBrush && m_renderer.GetFormatTiny()) {
        if (scaleTopText) {
            rt->DrawTextW(scaleTopText, (UINT32)wcslen(scaleTopText), m_renderer.GetFormatTiny(),
                D2D1::RectF(chartX + 2.0f, chartY - 1.0f, plotX - 2.0f, chartY + 12.0f), textBrush);
        }
        if (scaleMidText) {
            float midY = chartY + chartH * 0.5f;
            rt->DrawTextW(scaleMidText, (UINT32)wcslen(scaleMidText), m_renderer.GetFormatTiny(),
                D2D1::RectF(chartX + 2.0f, midY - 6.0f, plotX - 2.0f, midY + 8.0f), textBrush);
        }
        rt->DrawTextW(L"0", 1, m_renderer.GetFormatTiny(),
            D2D1::RectF(chartX + 2.0f, chartY + chartH - 12.0f, plotX - 2.0f, chartY + chartH), textBrush);
    }

    // Horizontal grid lines (100%, 50%, 0%)
    if (gridBrush) {
        float y100 = chartY + 1.0f;
        float y50 = chartY + chartH * 0.5f;
        float y0 = chartY + chartH - 1.0f;
        rt->DrawLine(D2D1::Point2F(plotX, y100), D2D1::Point2F(plotX + plotW, y100), gridBrush, 0.8f);
        rt->DrawLine(D2D1::Point2F(plotX, y50), D2D1::Point2F(plotX + plotW, y50), gridBrush, 0.8f);
        rt->DrawLine(D2D1::Point2F(plotX, y0), D2D1::Point2F(plotX + plotW, y0), gridBrush, 0.8f);

        // Vertical subtle dotted grid lines (every 15 points)
        for (int vi = 1; vi <= 3; ++vi) {
            float vx = plotX + vi * (plotW / 4.0f);
            rt->DrawLine(D2D1::Point2F(vx, chartY + 1.0f), D2D1::Point2F(vx, chartY + chartH - 1.0f), gridBrush, 0.6f);
        }
    }

    // 2. Waveform Geometry (Fill Area & Crisp Stroke Polyline)
    size_t count = history.Size();
    if (count < 2 || !m_renderer.GetD2DFactory()) return;

    float step = plotW / 59.0f;
    float startHistX = plotX + (59 - (count - 1)) * step;

    ID2D1PathGeometry* pAreaGeom = nullptr;
    ID2D1PathGeometry* pStrokeGeom = nullptr;
    m_renderer.GetD2DFactory()->CreatePathGeometry(&pAreaGeom);
    m_renderer.GetD2DFactory()->CreatePathGeometry(&pStrokeGeom);

    if (pAreaGeom) {
        ID2D1GeometrySink* pAreaSink = nullptr;
        if (SUCCEEDED(pAreaGeom->Open(&pAreaSink))) {
            pAreaSink->BeginFigure(D2D1::Point2F(startHistX, chartY + chartH), D2D1_FIGURE_BEGIN_FILLED);

            for (size_t i = 0; i < count; ++i) {
                float px = startHistX + i * step;
                float pct = std::clamp(history[i] / maxVal, 0.0f, 1.0f);
                float py = chartY + chartH - (pct * (chartH - 6.0f)) - 3.0f;
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            }

            float endPx = startHistX + (count - 1) * step;
            pAreaSink->AddLine(D2D1::Point2F(endPx, chartY + chartH));
            pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pAreaSink->Close();
            pAreaSink->Release();

            // 复用池化线性渐变画刷填充波形曲面，无需每帧分配
            if (areaBrush) {
                areaBrush->SetStartPoint(D2D1::Point2F(plotX, chartY));
                areaBrush->SetEndPoint(D2D1::Point2F(plotX, chartY + chartH));
                rt->FillGeometry(pAreaGeom, areaBrush);
            }
        }
        pAreaGeom->Release();
    }

    // 3. Crisp stroke line: 单次提交连续折线，避免循环调用数百次 DrawLine
    if (pStrokeGeom && strokeBrush) {
        ID2D1GeometrySink* pStrokeSink = nullptr;
        if (SUCCEEDED(pStrokeGeom->Open(&pStrokeSink))) {
            float firstPct = std::clamp(history[0] / maxVal, 0.0f, 1.0f);
            float firstPy = chartY + chartH - (firstPct * (chartH - 6.0f)) - 3.0f;
            pStrokeSink->BeginFigure(D2D1::Point2F(startHistX, firstPy), D2D1_FIGURE_BEGIN_HOLLOW);

            for (size_t i = 1; i < count; ++i) {
                float px = startHistX + i * step;
                float pct = std::clamp(history[i] / maxVal, 0.0f, 1.0f);
                float py = chartY + chartH - (pct * (chartH - 6.0f)) - 3.0f;
                pStrokeSink->AddLine(D2D1::Point2F(px, py));
            }

            pStrokeSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pStrokeSink->Close();
            pStrokeSink->Release();

            rt->DrawGeometry(pStrokeGeom, strokeBrush, 1.6f, m_renderer.GetRoundStrokeStyle());
        }
        pStrokeGeom->Release();
    }

    // 4. Live Current Beacon Point with white core & glowing halo
    float lastPct = std::clamp(history.Back() / maxVal, 0.0f, 1.0f);
    float lastX = startHistX + (count - 1) * step;
    float lastY = chartY + chartH - (lastPct * (chartH - 6.0f)) - 3.0f;

    if (m_brushes.pHaloBrush) {
        m_brushes.pHaloBrush->SetColor(D2D1::ColorF(strokeCol.r, strokeCol.g, strokeCol.b, 0.40f));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(lastX, lastY), 4.0f, 4.0f), m_brushes.pHaloBrush);
    }
    if (m_brushes.pWhiteCore) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(lastX, lastY), 1.8f, 1.8f), m_brushes.pWhiteCore);
    }
}

// -------------------------------------------------------------
// Main Render Entry
// -------------------------------------------------------------
/**
 * 按当前状态绘制并提交一帧；数据来自已保存快照，绘图不等于重新采集硬件。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::Render() {
    auto rt = m_renderer.BeginDraw();
    if (!rt) return;

    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));

    float W = m_dipWidth;
    float H = m_dipHeight;
    float contentW = W - 24.0f;
    float curY = 12.0f;

    auto pal = Theme::GetPresetPalette(m_preset);
    D2D1_COLOR_F cpuStatusCol = Theme::GetCpuStatusColor(m_snapshot.cpuUsageTotal);

    // 0. Ensure pooled brushes are ready for current theme & DPI
    m_brushes.EnsureBrushes(rt, pal, H, m_isLightMode, m_preset, cpuStatusCol);

    // 1. Frosted Translucent Card with rounded corners
    D2D1_ROUNDED_RECT rrc = D2D1::RoundedRect(
        D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), 16.0f, 16.0f
    );
    if (m_brushes.pWindowBg) rt->FillRoundedRectangle(rrc, m_brushes.pWindowBg);

    // Specular Rim Gradient Highlight for Translucent styles
    if (pal.isTranslucent) {
        if (m_brushes.pGlassRim) {
            rt->DrawRoundedRectangle(rrc, m_brushes.pGlassRim, 1.4f);
        }
    } else {
        if (m_brushes.pWindowBorder) {
            rt->DrawRoundedRectangle(rrc, m_brushes.pWindowBorder, 1.2f);
        }
    }

    // Render All Dashboard Modules:
    // Header -> CPU Waveform -> Memory Waveform -> Net Dual Waveform -> GPU Waveform -> Disks
    DrawHeader(rt, curY, contentW, pal);
    DrawCpuSection(rt, curY, contentW, pal);
    DrawMemorySection(rt, curY, contentW, pal);
    DrawNetworkSection(rt, curY, contentW, pal);
    DrawGpuSection(rt, curY, contentW, pal);
    DrawDiskSection(rt, curY, contentW, pal);

    m_renderer.EndDraw();

    // 周期性主动整理工作集，保持展开状态下的内存处于极致轻量范围 (8~10 MB)
    m_renderCount++;
    if (m_renderCount % 3 == 0) {
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    }
}

// 绘制详情窗口标题、在线状态指示灯和关闭按钮。
/**
 * 绘制页头与操作按钮，并推进后续内容的纵向起点。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;

    // Glowing green live status dot
    if (m_brushes.pGreenDotGlow) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(startX + 8.0f, curY + 9.5f), 5.0f, 5.0f), m_brushes.pGreenDotGlow);
    }
    if (m_brushes.pNetDownGreen) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(startX + 8.0f, curY + 9.5f), 2.8f, 2.8f), m_brushes.pNetDownGreen);
    }

    // Title
    if (m_brushes.pTextPrimary && m_renderer.GetFormatTitle()) {
        std::wstring title = L"系统运行概览";
        rt->DrawTextW(title.c_str(), (UINT32)title.length(), m_renderer.GetFormatTitle(),
            D2D1::RectF(startX + 18.0f, curY, startX + width - 26.0f, curY + 22.0f), m_brushes.pTextPrimary);
    }

    // Close button "✕"
    if (m_brushes.pTextMuted && m_renderer.GetFormatValue()) {
        rt->DrawTextW(L"✕", 1, m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 16.0f, curY, startX + width, curY + 22.0f), m_brushes.pTextMuted);
    }

    curY += 26.0f;
}

// -------------------------------------------------------------
// SECTION 1: CPU Waveform Card
// -------------------------------------------------------------
/**
 * 绘制CPU模块；从窗口快照和设置取值，调用绘图库生成画面。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawCpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;

    D2D1_COLOR_F cpuStatusCol = m_brushes.lastCpuColor;

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (m_brushes.pCardBg) rt->FillRoundedRectangle(cardRc, m_brushes.pCardBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(cardRc, m_brushes.pCardBorder, 1.0f);

    // Vector CPU Icon
    DrawCpuIcon(rt, startX + 10.0f, curY + 5.5f, m_brushes.pTextSecondary);

    // Label "CPU 总体负载"
    if (m_brushes.pTextSecondary && m_renderer.GetFormatLabel()) {
        const wchar_t* cpuLabel = L"CPU 总体负载";
        rt->DrawTextW(cpuLabel, static_cast<UINT32>(wcslen(cpuLabel)), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 28.0f, curY + 4.5f, startX + width - 75.0f, curY + 22.0f), m_brushes.pTextSecondary);
    }

    // Value right-aligned
    wchar_t valText[32];
    swprintf_s(valText, L"%.1f%%", m_snapshot.cpuUsageTotal);
    if (m_brushes.pCpuLine && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(valText, (UINT32)wcslen(valText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 85.0f, curY + 3.5f, startX + width - 10.0f, curY + 22.0f), m_brushes.pCpuLine);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // Waveform Plot
    float chartX = startX + 8.0f;
    float chartY = curY + 24.0f;
    float chartW = width - 16.0f;
    float chartH = 50.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
    if (m_brushes.pTrackBg) rt->FillRoundedRectangle(chartRc, m_brushes.pTrackBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(chartRc, m_brushes.pCardBorder, 0.8f);

    DrawWaveform(
        rt, chartX, chartY, chartW, chartH,
        m_snapshot.cpuHistory, 100.0f,
        m_brushes.pCpuLine, m_brushes.pCpuGradBrush,
        cpuStatusCol, m_brushes.pGridLine, m_brushes.pTextMuted,
        L"100%", L"50%"
    );

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 2: Memory Waveform Card
// -------------------------------------------------------------
/**
 * 绘制内存模块；从窗口快照和设置取值，调用绘图库生成画面。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawMemorySection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (m_brushes.pCardBg) rt->FillRoundedRectangle(cardRc, m_brushes.pCardBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(cardRc, m_brushes.pCardBorder, 1.0f);

    // Vector RAM Icon
    DrawRamIcon(rt, startX + 10.0f, curY + 5.5f, m_brushes.pTextSecondary);

    double usedGB = static_cast<double>(m_snapshot.usedPhysMem) / (1024.0 * 1024.0 * 1024.0);
    double totalGB = static_cast<double>(m_snapshot.totalPhysMem) / (1024.0 * 1024.0 * 1024.0);

    wchar_t ramSub[64];
    swprintf_s(ramSub, L"物理内存 (%.1f / %.1f GB)", usedGB, totalGB);
    if (m_brushes.pTextSecondary && m_renderer.GetFormatLabel()) {
        rt->DrawTextW(ramSub, (UINT32)wcslen(ramSub), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 28.0f, curY + 4.5f, startX + width - 65.0f, curY + 22.0f), m_brushes.pTextSecondary);
    }

    wchar_t pctText[16];
    swprintf_s(pctText, L"%.0f%%", m_snapshot.memoryUsagePercent);
    if (m_brushes.pRamPurple && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(pctText, (UINT32)wcslen(pctText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 75.0f, curY + 3.5f, startX + width - 10.0f, curY + 22.0f), m_brushes.pRamPurple);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // Memory Waveform Plot
    float chartX = startX + 8.0f;
    float chartY = curY + 24.0f;
    float chartW = width - 16.0f;
    float chartH = 50.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
    if (m_brushes.pTrackBg) rt->FillRoundedRectangle(chartRc, m_brushes.pTrackBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(chartRc, m_brushes.pCardBorder, 0.8f);

    DrawWaveform(
        rt, chartX, chartY, chartW, chartH,
        m_snapshot.memoryHistory, 100.0f,
        m_brushes.pRamPurple, m_brushes.pRamGradBrush,
        pal.RamPurple, m_brushes.pGridLine, m_brushes.pTextMuted,
        L"100%", L"50%"
    );

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 3: Network Dual Waveform Cards (Download & Upload)
// -------------------------------------------------------------
/**
 * 绘制网络模块；从窗口快照和设置取值，调用绘图库生成画面。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawNetworkSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;
    float gap = 8.0f;
    float halfW = (width - gap) / 2.0f;

    // Calculate dynamic auto-scaling upper bound for Download & Upload
    float maxDown = 1024.0f * 1024.0f; // Minimum scale: 1 MB/s
    for (size_t i = 0; i < m_snapshot.downloadHistory.Size(); ++i) {
        float val = m_snapshot.downloadHistory[i];
        if (val > maxDown) maxDown = val;
    }
    float maxUp = 512.0f * 1024.0f; // Minimum scale: 512 KB/s
    for (size_t i = 0; i < m_snapshot.uploadHistory.Size(); ++i) {
        float val = m_snapshot.uploadHistory[i];
        if (val > maxUp) maxUp = val;
    }

    wchar_t downTopScale[16];
    wchar_t downMidScale[16];
    swprintf_s(downTopScale, L"%.0fM", maxDown / (1024.0f * 1024.0f));
    swprintf_s(downMidScale, L"%.0fM", (maxDown * 0.5f) / (1024.0f * 1024.0f));

    wchar_t upTopScale[16];
    wchar_t upMidScale[16];
    swprintf_s(upTopScale, L"%.0fM", maxUp / (1024.0f * 1024.0f));
    swprintf_s(upMidScale, L"%.0fM", (maxUp * 0.5f) / (1024.0f * 1024.0f));

    // Left Card: Download
    {
        D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + halfW, curY + cardH), 9.0f, 9.0f);
        if (m_brushes.pCardBg) rt->FillRoundedRectangle(cardRc, m_brushes.pCardBg);
        if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(cardRc, m_brushes.pCardBorder, 1.0f);

        if (m_brushes.pNetDownGreen && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"↓", 1, m_renderer.GetFormatLabel(),
                D2D1::RectF(startX + 8.0f, curY + 4.5f, startX + 20.0f, curY + 22.0f), m_brushes.pNetDownGreen);
        }
        if (m_brushes.pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"下载流速", 4, m_renderer.GetFormatLabel(),
                D2D1::RectF(startX + 20.0f, curY + 4.5f, startX + halfW - 65.0f, curY + 22.0f), m_brushes.pTextSecondary);
        }

        std::wstring downText = FormatSpeed(m_snapshot.downloadSpeedBps);
        if (m_brushes.pNetDownGreen && m_renderer.GetFormatSmall()) {
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            rt->DrawTextW(downText.c_str(), (UINT32)downText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(startX + halfW - 85.0f, curY + 4.5f, startX + halfW - 8.0f, curY + 22.0f), m_brushes.pNetDownGreen);
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        float chartX = startX + 6.0f;
        float chartY = curY + 24.0f;
        float chartW = halfW - 12.0f;
        float chartH = 50.0f;

        D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
        if (m_brushes.pTrackBg) rt->FillRoundedRectangle(chartRc, m_brushes.pTrackBg);
        if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(chartRc, m_brushes.pCardBorder, 0.8f);

        DrawWaveform(
            rt, chartX, chartY, chartW, chartH,
            m_snapshot.downloadHistory, maxDown,
            m_brushes.pNetDownGreen, m_brushes.pNetDownGradBrush,
            pal.NetDownGreen, m_brushes.pGridLine, m_brushes.pTextMuted,
            downTopScale, downMidScale
        );
    }

    // Right Card: Upload
    {
        float rightX = startX + halfW + gap;
        D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(rightX, curY, rightX + halfW, curY + cardH), 9.0f, 9.0f);
        if (m_brushes.pCardBg) rt->FillRoundedRectangle(cardRc, m_brushes.pCardBg);
        if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(cardRc, m_brushes.pCardBorder, 1.0f);

        if (m_brushes.pNetUpBlue && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"↑", 1, m_renderer.GetFormatLabel(),
                D2D1::RectF(rightX + 8.0f, curY + 4.5f, rightX + 20.0f, curY + 22.0f), m_brushes.pNetUpBlue);
        }
        if (m_brushes.pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"上传流速", 4, m_renderer.GetFormatLabel(),
                D2D1::RectF(rightX + 20.0f, curY + 4.5f, rightX + halfW - 65.0f, curY + 22.0f), m_brushes.pTextSecondary);
        }

        std::wstring upText = FormatSpeed(m_snapshot.uploadSpeedBps);
        if (m_brushes.pNetUpBlue && m_renderer.GetFormatSmall()) {
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            rt->DrawTextW(upText.c_str(), (UINT32)upText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(rightX + halfW - 85.0f, curY + 4.5f, rightX + halfW - 8.0f, curY + 22.0f), m_brushes.pNetUpBlue);
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        float chartX = rightX + 6.0f;
        float chartY = curY + 24.0f;
        float chartW = halfW - 12.0f;
        float chartH = 50.0f;

        D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
        if (m_brushes.pTrackBg) rt->FillRoundedRectangle(chartRc, m_brushes.pTrackBg);
        if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(chartRc, m_brushes.pCardBorder, 0.8f);

        DrawWaveform(
            rt, chartX, chartY, chartW, chartH,
            m_snapshot.uploadHistory, maxUp,
            m_brushes.pNetUpBlue, m_brushes.pNetUpGradBrush,
            pal.NetUpBlue, m_brushes.pGridLine, m_brushes.pTextMuted,
            upTopScale, upMidScale
        );
    }

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 4: GPU Waveform Card
// -------------------------------------------------------------
/**
 * 绘制GPU模块；从窗口快照和设置取值，调用绘图库生成画面。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawGpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (m_brushes.pCardBg) rt->FillRoundedRectangle(cardRc, m_brushes.pCardBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(cardRc, m_brushes.pCardBorder, 1.0f);

    // Vector GPU Icon
    DrawGpuIcon(rt, startX + 10.0f, curY + 5.5f, m_brushes.pTextSecondary);

    // Label "GPU 使用率"
    if (m_brushes.pTextSecondary && m_renderer.GetFormatLabel()) {
        const wchar_t* gpuLabel = L"GPU 使用率";
        rt->DrawTextW(gpuLabel, static_cast<UINT32>(wcslen(gpuLabel)), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 26.0f, curY + 4.5f, startX + width - 65.0f, curY + 22.0f), m_brushes.pTextSecondary);
    }

    // Value right-aligned
    wchar_t valText[32];
    swprintf_s(valText, L"%.0f%%", m_snapshot.gpuUsagePercent);
    if (m_brushes.pGpuCyan && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(valText, (UINT32)wcslen(valText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 75.0f, curY + 3.5f, startX + width - 10.0f, curY + 22.0f), m_brushes.pGpuCyan);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // GPU Waveform Plot
    float chartX = startX + 8.0f;
    float chartY = curY + 24.0f;
    float chartW = width - 16.0f;
    float chartH = 50.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
    if (m_brushes.pTrackBg) rt->FillRoundedRectangle(chartRc, m_brushes.pTrackBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(chartRc, m_brushes.pCardBorder, 0.8f);

    DrawWaveform(
        rt, chartX, chartY, chartW, chartH,
        m_snapshot.gpuHistory, 100.0f,
        m_brushes.pGpuCyan, m_brushes.pGpuGradBrush,
        pal.GpuCyan, m_brushes.pGridLine, m_brushes.pTextMuted,
        L"100%", L"50%"
    );

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 5: Disk Storage Module
// -------------------------------------------------------------
/**
 * 绘制磁盘模块；从窗口快照和设置取值，调用绘图库生成画面。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param curY 通过引用传入的纵向游标，绘完后可供下一模块继续排列。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void FlyoutWindow::DrawDiskSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    if (m_snapshot.disks.empty()) return;

    float startX = 12.0f;
    float cardH = 26.0f + static_cast<float>(m_snapshot.disks.size()) * 26.0f;

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (m_brushes.pCardBg) rt->FillRoundedRectangle(cardRc, m_brushes.pCardBg);
    if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(cardRc, m_brushes.pCardBorder, 1.0f);

    // Vector Disk Icon
    DrawDiskIcon(rt, startX + 10.0f, curY + 6.0f, m_brushes.pTextSecondary);

    if (m_brushes.pTextSecondary && m_renderer.GetFormatLabel()) {
        std::wstring header = L"本地磁盘存储";
        rt->DrawTextW(header.c_str(), (UINT32)header.length(), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 28.0f, curY + 4.5f, startX + width - 10.0f, curY + 20.0f), m_brushes.pTextSecondary);
    }

    float diskY = curY + 24.0f;
    for (const auto& disk : m_snapshot.disks) {
        double used = static_cast<double>(disk.totalBytes - disk.freeBytes) / (1024.0 * 1024.0 * 1024.0);
        double total = static_cast<double>(disk.totalBytes) / (1024.0 * 1024.0 * 1024.0);
        float pct = (disk.totalBytes > 0) ? (static_cast<float>(disk.totalBytes - disk.freeBytes) / disk.totalBytes) : 0.0f;

        // Drive tag badge (e.g. "C:\")
        if (m_brushes.pPillBg) {
            D2D1_ROUNDED_RECT driveRc = D2D1::RoundedRect(D2D1::RectF(startX + 10.0f, diskY, startX + 32.0f, diskY + 18.0f), 3.5f, 3.5f);
            rt->FillRoundedRectangle(driveRc, m_brushes.pPillBg);
        }

        if (m_brushes.pTextPrimary && m_renderer.GetFormatBoldTiny()) {
            rt->DrawTextW(disk.drive.c_str(), (UINT32)disk.drive.length(), m_renderer.GetFormatBoldTiny(),
                D2D1::RectF(startX + 10.0f, diskY - 1.0f, startX + 32.0f, diskY + 18.0f), m_brushes.pTextPrimary);
        }

        // Capacity text (e.g. "106 / 376 GB (28%)")
        wchar_t lineText[64];
        swprintf_s(lineText, L"%.0f / %.0f GB (%.0f%%)", used, total, pct * 100.0f);
        if (m_brushes.pTextSecondary && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(lineText, (UINT32)wcslen(lineText), m_renderer.GetFormatSmall(),
                D2D1::RectF(startX + 38.0f, diskY + 1.0f, startX + 172.0f, diskY + 18.0f), m_brushes.pTextSecondary);
        }

        // Mini pill progress bar with warm amber fill
        float barX = startX + 174.0f;
        float barW = width - 184.0f;
        float barH = 10.0f;

        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(barX, diskY + 4.0f, barX + barW, diskY + 4.0f + barH), 5.0f, 5.0f);
        if (m_brushes.pTrackBg) rt->FillRoundedRectangle(trackRc, m_brushes.pTrackBg);
        if (m_brushes.pCardBorder) rt->DrawRoundedRectangle(trackRc, m_brushes.pCardBorder, 0.8f);

        float fillW = barW * std::clamp(pct, 0.0f, 1.0f);
        if (fillW > 3.0f && m_brushes.pDiskAmber) {
            D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(D2D1::RectF(barX, diskY + 4.0f, barX + fillW, diskY + 4.0f + barH), 5.0f, 5.0f);
            rt->FillRoundedRectangle(fillRc, m_brushes.pDiskAmber);
        }

        diskY += 24.0f;
    }

    curY += cardH + 7.0f;
}
