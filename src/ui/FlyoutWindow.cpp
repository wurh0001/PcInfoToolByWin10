// 详情弹出窗口的声明。
#include "FlyoutWindow.h"
// DPI 换算工具：在 DIP 和实际像素之间转换。
#include "DpiHelper.h"
// 主题颜色和主题配置。
#include "Theme.h"
// 提供 std::clamp，用于限制进度条和图表数值范围。
#include <algorithm>
#include <cmath>

// 构造详情弹窗对象；窗口和渲染资源在 Create 中创建。
FlyoutWindow::FlyoutWindow() {}

// 销毁详情窗口，渲染器成员随后自动释放图形资源。
FlyoutWindow::~FlyoutWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

// 注册并创建隐藏的详情窗口，同时初始化透明 Direct2D 渲染器。
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
void FlyoutWindow::ShowNear(const RECT& anchorRect) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int x = anchorRect.left;
    if (x + m_pixelWidth > screenW - 10) {
        x = screenW - m_pixelWidth - 10;
    }
    if (x < 10) x = 10;

    int y = anchorRect.bottom + 8;
    if (y + m_pixelHeight > screenH - 10) {
        y = anchorRect.top - m_pixelHeight - 8;
    }

    m_visible = true;
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_pixelWidth, m_pixelHeight, SWP_SHOWWINDOW);
    SetForegroundWindow(m_hwnd);
    SetActiveWindow(m_hwnd);
    Render();
}

// 隐藏详情窗口，但保留窗口和渲染资源以便下次快速显示。
void FlyoutWindow::Hide() {
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);
}

// 设置浅色或深色模式，并在弹窗可见时立即重绘。
void FlyoutWindow::SetLightMode(bool lightMode) {
    m_isLightMode = lightMode;
    m_preset = lightMode ? Theme::ThemePreset::GlacierWhite : Theme::ThemePreset::ObsidianDark;
    if (m_visible && m_hwnd) {
        Render();
    }
}

// 更新详情窗口的主题预设。
void FlyoutWindow::SetThemePreset(Theme::ThemePreset preset) {
    m_preset = preset;
    m_isLightMode = (preset == Theme::ThemePreset::GlacierWhite || preset == Theme::ThemePreset::AuroraGlass);
    if (m_visible && m_hwnd) {
        Render();
    }
}

// 接收主窗口传来的最新系统指标，并在可见时刷新界面。
void FlyoutWindow::UpdateSnapshot(const MetricsSnapshot& snapshot) {
    m_snapshot = snapshot;
    if (m_visible && m_hwnd) {
        Render();
    }
}

// 处理详情窗口的激活、关闭按钮和销毁消息。
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

void FlyoutWindow::DrawGpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    D2D1_ROUNDED_RECT shroudRc = D2D1::RoundedRect(D2D1::RectF(x + 0.5f, y + 1.0f, x + 10.5f, y + 9.5f), 1.2f, 1.2f);
    rt->DrawRoundedRectangle(shroudRc, brush, 1.0f);

    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x + 5.5f, y + 5.2f), 2.5f, 2.5f), brush, 0.9f);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 5.5f, y + 5.2f), 0.9f, 0.9f), brush);
    rt->DrawLine(D2D1::Point2F(x + 10.5f, y + 0.5f), D2D1::Point2F(x + 10.5f, y + 10.0f), brush, 1.1f);
}

void FlyoutWindow::DrawDiskIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    if (!rt || !brush) return;
    D2D1_ROUNDED_RECT body = D2D1::RoundedRect(D2D1::RectF(x + 1.0f, y + 1.5f, x + 13.0f, y + 10.5f), 1.8f, 1.8f);
    rt->DrawRoundedRectangle(body, brush, 1.1f);
    rt->DrawLine(D2D1::Point2F(x + 1.0f, y + 7.5f), D2D1::Point2F(x + 13.0f, y + 7.5f), brush, 1.0f);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 10.5f, y + 9.0f), 0.8f, 0.8f), brush);
}

// -------------------------------------------------------------
// Reusable High-Fidelity Waveform Plotter
// -------------------------------------------------------------
void FlyoutWindow::DrawWaveform(
    ID2D1DCRenderTarget* rt,
    float chartX, float chartY, float chartW, float chartH,
    const std::vector<float>& history,
    float maxVal,
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

    // 2. Waveform Geometry & Fill
    size_t count = history.size();
    if (count < 2 || !m_renderer.GetD2DFactory()) return;

    float step = plotW / 59.0f;
    float startHistX = plotX + (59 - (count - 1)) * step;

    ID2D1PathGeometry* pAreaGeom = nullptr;
    m_renderer.GetD2DFactory()->CreatePathGeometry(&pAreaGeom);

    if (pAreaGeom) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pAreaGeom->Open(&pSink))) {
            pSink->BeginFigure(D2D1::Point2F(startHistX, chartY + chartH), D2D1_FIGURE_BEGIN_FILLED);

            for (size_t i = 0; i < count; ++i) {
                float px = startHistX + i * step;
                float pct = std::clamp(history[i] / maxVal, 0.0f, 1.0f);
                float py = chartY + chartH - (pct * (chartH - 6.0f)) - 3.0f;
                pSink->AddLine(D2D1::Point2F(px, py));
            }

            float endPx = startHistX + (count - 1) * step;
            pSink->AddLine(D2D1::Point2F(endPx, chartY + chartH));
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            pSink->Release();

            // Gradient fill (top vibrant, fading to pure transparent at base)
            ID2D1GradientStopCollection* pStops = nullptr;
            D2D1_GRADIENT_STOP stops[2];
            stops[0].color = D2D1::ColorF(strokeCol.r, strokeCol.g, strokeCol.b, m_isLightMode ? 0.35f : 0.28f);
            stops[0].position = 0.0f;
            stops[1].color = D2D1::ColorF(strokeCol.r, strokeCol.g, strokeCol.b, 0.0f);
            stops[1].position = 1.0f;

            rt->CreateGradientStopCollection(stops, 2, &pStops);
            if (pStops) {
                ID2D1LinearGradientBrush* pAreaBrush = nullptr;
                rt->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(plotX, chartY),
                        D2D1::Point2F(plotX, chartY + chartH)
                    ),
                    pStops,
                    &pAreaBrush
                );
                if (pAreaBrush) {
                    rt->FillGeometry(pAreaGeom, pAreaBrush);
                    pAreaBrush->Release();
                }
                pStops->Release();
            }
        }
        pAreaGeom->Release();
    }

    // 3. Crisp stroke line with round joins
    ID2D1SolidColorBrush* pStrokeBrush = nullptr;
    rt->CreateSolidColorBrush(strokeCol, &pStrokeBrush);
    if (pStrokeBrush) {
        for (size_t i = 1; i < count; ++i) {
            float x1 = startHistX + (i - 1) * step;
            float pct1 = std::clamp(history[i - 1] / maxVal, 0.0f, 1.0f);
            float y1 = chartY + chartH - (pct1 * (chartH - 6.0f)) - 3.0f;

            float x2 = startHistX + i * step;
            float pct2 = std::clamp(history[i] / maxVal, 0.0f, 1.0f);
            float y2 = chartY + chartH - (pct2 * (chartH - 6.0f)) - 3.0f;

            rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), pStrokeBrush, 1.6f);
        }

        // 4. Live Current Beacon Point with white core & glowing halo
        float lastPct = std::clamp(history.back() / maxVal, 0.0f, 1.0f);
        float lastX = startHistX + (count - 1) * step;
        float lastY = chartY + chartH - (lastPct * (chartH - 6.0f)) - 3.0f;

        ID2D1SolidColorBrush* pHaloBrush = nullptr;
        ID2D1SolidColorBrush* pCoreBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(strokeCol.r, strokeCol.g, strokeCol.b, 0.40f), &pHaloBrush);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pCoreBrush);

        if (pHaloBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(lastX, lastY), 4.0f, 4.0f), pHaloBrush);
            pHaloBrush->Release();
        }
        if (pCoreBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(lastX, lastY), 1.8f, 1.8f), pCoreBrush);
            pCoreBrush->Release();
        }

        pStrokeBrush->Release();
    }
}

// -------------------------------------------------------------
// Main Render Entry
// -------------------------------------------------------------
void FlyoutWindow::Render() {
    auto rt = m_renderer.BeginDraw();
    if (!rt) return;

    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));

    float W = m_dipWidth;
    float H = m_dipHeight;
    float contentW = W - 24.0f;
    float curY = 12.0f;

    auto pal = Theme::GetPresetPalette(m_preset);

    // 1. Frosted Translucent Card with rounded corners
    ID2D1SolidColorBrush* pWindowBg = nullptr;
    ID2D1SolidColorBrush* pWindowBorder = nullptr;
    rt->CreateSolidColorBrush(pal.WindowBg, &pWindowBg);
    rt->CreateSolidColorBrush(pal.WindowBorder, &pWindowBorder);

    D2D1_ROUNDED_RECT rrc = D2D1::RoundedRect(
        D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), 16.0f, 16.0f
    );
    if (pWindowBg) rt->FillRoundedRectangle(rrc, pWindowBg);

    // Specular Rim Gradient Highlight for Translucent styles
    if (pal.isTranslucent) {
        ID2D1GradientStopCollection* pGlassStops = nullptr;
        D2D1_GRADIENT_STOP glassStops[3];
        glassStops[0].color = pal.WindowBorder;
        glassStops[0].position = 0.0f;
        glassStops[1].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.45f);
        glassStops[1].position = 0.40f;
        glassStops[2].color = D2D1::ColorF(pal.WindowBorder.r, pal.WindowBorder.g, pal.WindowBorder.b, pal.WindowBorder.a * 0.20f);
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
                rt->DrawRoundedRectangle(rrc, pGlassBorder, 1.4f);
                pGlassBorder->Release();
            }
            pGlassStops->Release();
        }
    } else {
        if (pWindowBorder) rt->DrawRoundedRectangle(rrc, pWindowBorder, 1.2f);
    }

    if (pWindowBg) pWindowBg->Release();
    if (pWindowBorder) pWindowBorder->Release();

    // Render All Dashboard Modules:
    // Header -> CPU Waveform -> Memory Waveform -> Net Dual Waveform -> GPU Waveform -> Disks
    DrawHeader(rt, curY, contentW, pal);
    DrawCpuSection(rt, curY, contentW, pal);
    DrawMemorySection(rt, curY, contentW, pal);
    DrawNetworkSection(rt, curY, contentW, pal);
    DrawGpuSection(rt, curY, contentW, pal);
    DrawDiskSection(rt, curY, contentW, pal);

    m_renderer.EndDraw();
}

// 绘制详情窗口标题、在线状态指示灯和关闭按钮。
void FlyoutWindow::DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;

    // Glowing green live status dot
    ID2D1SolidColorBrush* pDotBrush = nullptr;
    ID2D1SolidColorBrush* pDotGlow = nullptr;
    rt->CreateSolidColorBrush(pal.NetDownGreen, &pDotBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(pal.NetDownGreen.r, pal.NetDownGreen.g, pal.NetDownGreen.b, 0.25f), &pDotGlow);

    if (pDotGlow) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(startX + 8.0f, curY + 9.5f), 5.0f, 5.0f), pDotGlow);
        pDotGlow->Release();
    }
    if (pDotBrush) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(startX + 8.0f, curY + 9.5f), 2.8f, 2.8f), pDotBrush);
        pDotBrush->Release();
    }

    // Title
    ID2D1SolidColorBrush* pTextBrush = nullptr;
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextBrush);
    if (pTextBrush && m_renderer.GetFormatTitle()) {
        std::wstring title = L"系统运行概览";
        rt->DrawTextW(title.c_str(), (UINT32)title.length(), m_renderer.GetFormatTitle(),
            D2D1::RectF(startX + 18.0f, curY, startX + width - 26.0f, curY + 22.0f), pTextBrush);
    }

    // Close button "✕"
    ID2D1SolidColorBrush* pMutedBrush = nullptr;
    rt->CreateSolidColorBrush(pal.TextMuted, &pMutedBrush);
    if (pMutedBrush && m_renderer.GetFormatValue()) {
        rt->DrawTextW(L"✕", 1, m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 16.0f, curY, startX + width, curY + 22.0f), pMutedBrush);
        pMutedBrush->Release();
    }
    if (pTextBrush) pTextBrush->Release();

    curY += 26.0f;
}

// -------------------------------------------------------------
// SECTION 1: CPU Waveform Card
// -------------------------------------------------------------
void FlyoutWindow::DrawCpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;

    D2D1_COLOR_F cpuStatusCol = Theme::GetCpuStatusColor(m_snapshot.cpuUsageTotal);

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pCpuLine = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;
    ID2D1SolidColorBrush* pGridLine = nullptr;
    ID2D1SolidColorBrush* pTextMuted = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(cpuStatusCol, &pCpuLine);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);
    rt->CreateSolidColorBrush(pal.GridLine, &pGridLine);
    rt->CreateSolidColorBrush(pal.TextMuted, &pTextMuted);

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
    if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

    // Vector CPU Icon
    DrawCpuIcon(rt, startX + 10.0f, curY + 5.5f, pTextSecondary);

    // Label "CPU 总体负载"
    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        const wchar_t* cpuLabel = L"CPU 总体负载";
        rt->DrawTextW(cpuLabel, static_cast<UINT32>(wcslen(cpuLabel)), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 28.0f, curY + 4.5f, startX + width - 75.0f, curY + 22.0f), pTextSecondary);
    }

    // Value right-aligned
    wchar_t valText[32];
    swprintf_s(valText, L"%.1f%%", m_snapshot.cpuUsageTotal);
    if (pCpuLine && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(valText, (UINT32)wcslen(valText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 85.0f, curY + 3.5f, startX + width - 10.0f, curY + 22.0f), pCpuLine);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // Waveform Plot
    float chartX = startX + 8.0f;
    float chartY = curY + 24.0f;
    float chartW = width - 16.0f;
    float chartH = 50.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
    if (pTrackBg) rt->FillRoundedRectangle(chartRc, pTrackBg);
    if (pCardBorder) rt->DrawRoundedRectangle(chartRc, pCardBorder, 0.8f);

    DrawWaveform(rt, chartX, chartY, chartW, chartH, m_snapshot.cpuHistory, 100.0f, cpuStatusCol, pGridLine, pTextMuted, L"100%", L"50%");

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pCpuLine) pCpuLine->Release();
    if (pTrackBg) pTrackBg->Release();
    if (pGridLine) pGridLine->Release();
    if (pTextMuted) pTextMuted->Release();

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 2: Memory Waveform Card
// -------------------------------------------------------------
void FlyoutWindow::DrawMemorySection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pRamBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;
    ID2D1SolidColorBrush* pGridLine = nullptr;
    ID2D1SolidColorBrush* pTextMuted = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(pal.RamPurple, &pRamBrush);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);
    rt->CreateSolidColorBrush(pal.GridLine, &pGridLine);
    rt->CreateSolidColorBrush(pal.TextMuted, &pTextMuted);

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
    if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

    // Vector RAM Icon
    DrawRamIcon(rt, startX + 10.0f, curY + 5.5f, pTextSecondary);

    double usedGB = static_cast<double>(m_snapshot.usedPhysMem) / (1024.0 * 1024.0 * 1024.0);
    double totalGB = static_cast<double>(m_snapshot.totalPhysMem) / (1024.0 * 1024.0 * 1024.0);

    wchar_t ramSub[64];
    swprintf_s(ramSub, L"物理内存 (%.1f / %.1f GB)", usedGB, totalGB);
    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        rt->DrawTextW(ramSub, (UINT32)wcslen(ramSub), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 28.0f, curY + 4.5f, startX + width - 65.0f, curY + 22.0f), pTextSecondary);
    }

    wchar_t pctText[16];
    swprintf_s(pctText, L"%.0f%%", m_snapshot.memoryUsagePercent);
    if (pRamBrush && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(pctText, (UINT32)wcslen(pctText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 75.0f, curY + 3.5f, startX + width - 10.0f, curY + 22.0f), pRamBrush);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // Memory Waveform Plot
    float chartX = startX + 8.0f;
    float chartY = curY + 24.0f;
    float chartW = width - 16.0f;
    float chartH = 50.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
    if (pTrackBg) rt->FillRoundedRectangle(chartRc, pTrackBg);
    if (pCardBorder) rt->DrawRoundedRectangle(chartRc, pCardBorder, 0.8f);

    DrawWaveform(rt, chartX, chartY, chartW, chartH, m_snapshot.memoryHistory, 100.0f, pal.RamPurple, pGridLine, pTextMuted, L"100%", L"50%");

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pRamBrush) pRamBrush->Release();
    if (pTrackBg) pTrackBg->Release();
    if (pGridLine) pGridLine->Release();
    if (pTextMuted) pTextMuted->Release();

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 3: Network Dual Waveform Cards (Download & Upload)
// -------------------------------------------------------------
void FlyoutWindow::DrawNetworkSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;
    float gap = 8.0f;
    float halfW = (width - gap) / 2.0f;

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pDownBrush = nullptr;
    ID2D1SolidColorBrush* pUpBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;
    ID2D1SolidColorBrush* pGridLine = nullptr;
    ID2D1SolidColorBrush* pTextMuted = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(pal.NetDownGreen, &pDownBrush);
    rt->CreateSolidColorBrush(pal.NetUpBlue, &pUpBrush);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);
    rt->CreateSolidColorBrush(pal.GridLine, &pGridLine);
    rt->CreateSolidColorBrush(pal.TextMuted, &pTextMuted);

    // Calculate dynamic auto-scaling upper bound for Download & Upload
    float maxDown = 1024.0f * 1024.0f; // Minimum scale: 1 MB/s
    for (float val : m_snapshot.downloadHistory) {
        if (val > maxDown) maxDown = val;
    }
    float maxUp = 512.0f * 1024.0f; // Minimum scale: 512 KB/s
    for (float val : m_snapshot.uploadHistory) {
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
        if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
        if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

        if (pDownBrush && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"↓", 1, m_renderer.GetFormatLabel(),
                D2D1::RectF(startX + 8.0f, curY + 4.5f, startX + 20.0f, curY + 22.0f), pDownBrush);
        }
        if (pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"下载流速", 4, m_renderer.GetFormatLabel(),
                D2D1::RectF(startX + 20.0f, curY + 4.5f, startX + halfW - 65.0f, curY + 22.0f), pTextSecondary);
        }

        std::wstring downText = FormatSpeed(m_snapshot.downloadSpeedBps);
        if (pDownBrush && m_renderer.GetFormatSmall()) {
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            rt->DrawTextW(downText.c_str(), (UINT32)downText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(startX + halfW - 85.0f, curY + 4.5f, startX + halfW - 8.0f, curY + 22.0f), pDownBrush);
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        float chartX = startX + 6.0f;
        float chartY = curY + 24.0f;
        float chartW = halfW - 12.0f;
        float chartH = 50.0f;

        D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
        if (pTrackBg) rt->FillRoundedRectangle(chartRc, pTrackBg);
        if (pCardBorder) rt->DrawRoundedRectangle(chartRc, pCardBorder, 0.8f);

        DrawWaveform(rt, chartX, chartY, chartW, chartH, m_snapshot.downloadHistory, maxDown, pal.NetDownGreen, pGridLine, pTextMuted, downTopScale, downMidScale);
    }

    // Right Card: Upload
    {
        float rightX = startX + halfW + gap;
        D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(rightX, curY, rightX + halfW, curY + cardH), 9.0f, 9.0f);
        if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
        if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

        if (pUpBrush && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"↑", 1, m_renderer.GetFormatLabel(),
                D2D1::RectF(rightX + 8.0f, curY + 4.5f, rightX + 20.0f, curY + 22.0f), pUpBrush);
        }
        if (pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"上传流速", 4, m_renderer.GetFormatLabel(),
                D2D1::RectF(rightX + 20.0f, curY + 4.5f, rightX + halfW - 65.0f, curY + 22.0f), pTextSecondary);
        }

        std::wstring upText = FormatSpeed(m_snapshot.uploadSpeedBps);
        if (pUpBrush && m_renderer.GetFormatSmall()) {
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            rt->DrawTextW(upText.c_str(), (UINT32)upText.length(), m_renderer.GetFormatSmall(),
                D2D1::RectF(rightX + halfW - 85.0f, curY + 4.5f, rightX + halfW - 8.0f, curY + 22.0f), pUpBrush);
            m_renderer.GetFormatSmall()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        float chartX = rightX + 6.0f;
        float chartY = curY + 24.0f;
        float chartW = halfW - 12.0f;
        float chartH = 50.0f;

        D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
        if (pTrackBg) rt->FillRoundedRectangle(chartRc, pTrackBg);
        if (pCardBorder) rt->DrawRoundedRectangle(chartRc, pCardBorder, 0.8f);

        DrawWaveform(rt, chartX, chartY, chartW, chartH, m_snapshot.uploadHistory, maxUp, pal.NetUpBlue, pGridLine, pTextMuted, upTopScale, upMidScale);
    }

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pDownBrush) pDownBrush->Release();
    if (pUpBrush) pUpBrush->Release();
    if (pTrackBg) pTrackBg->Release();
    if (pGridLine) pGridLine->Release();
    if (pTextMuted) pTextMuted->Release();

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 4: GPU Waveform Card
// -------------------------------------------------------------
void FlyoutWindow::DrawGpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 82.0f;

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pGpuBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;
    ID2D1SolidColorBrush* pGridLine = nullptr;
    ID2D1SolidColorBrush* pTextMuted = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(pal.GpuCyan, &pGpuBrush);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);
    rt->CreateSolidColorBrush(pal.GridLine, &pGridLine);
    rt->CreateSolidColorBrush(pal.TextMuted, &pTextMuted);

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
    if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

    // Vector GPU Icon
    DrawGpuIcon(rt, startX + 10.0f, curY + 5.5f, pTextSecondary);

    // Label "GPU 使用率"
    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        const wchar_t* gpuLabel = L"GPU 使用率";
        rt->DrawTextW(gpuLabel, static_cast<UINT32>(wcslen(gpuLabel)), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 26.0f, curY + 4.5f, startX + width - 65.0f, curY + 22.0f), pTextSecondary);
    }

    // Value right-aligned
    wchar_t valText[32];
    swprintf_s(valText, L"%.0f%%", m_snapshot.gpuUsagePercent);
    if (pGpuBrush && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(valText, (UINT32)wcslen(valText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 75.0f, curY + 3.5f, startX + width - 10.0f, curY + 22.0f), pGpuBrush);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // GPU Waveform Plot
    float chartX = startX + 8.0f;
    float chartY = curY + 24.0f;
    float chartW = width - 16.0f;
    float chartH = 50.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 4.0f, 4.0f);
    if (pTrackBg) rt->FillRoundedRectangle(chartRc, pTrackBg);
    if (pCardBorder) rt->DrawRoundedRectangle(chartRc, pCardBorder, 0.8f);

    DrawWaveform(rt, chartX, chartY, chartW, chartH, m_snapshot.gpuHistory, 100.0f, pal.GpuCyan, pGridLine, pTextMuted, L"100%", L"50%");

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pGpuBrush) pGpuBrush->Release();
    if (pTrackBg) pTrackBg->Release();
    if (pGridLine) pGridLine->Release();
    if (pTextMuted) pTextMuted->Release();

    curY += cardH + 7.0f;
}

// -------------------------------------------------------------
// SECTION 5: Disk Storage Module
// -------------------------------------------------------------
void FlyoutWindow::DrawDiskSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    if (m_snapshot.disks.empty()) return;

    float startX = 12.0f;
    float cardH = 26.0f + static_cast<float>(m_snapshot.disks.size()) * 26.0f;

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pDiskAmber = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(pal.DiskAmber, &pDiskAmber);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
    if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

    // Vector Disk Icon
    DrawDiskIcon(rt, startX + 10.0f, curY + 6.0f, pTextSecondary);

    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        std::wstring header = L"本地磁盘存储";
        rt->DrawTextW(header.c_str(), (UINT32)header.length(), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 28.0f, curY + 4.5f, startX + width - 10.0f, curY + 20.0f), pTextSecondary);
    }

    float diskY = curY + 24.0f;
    for (const auto& disk : m_snapshot.disks) {
        double used = static_cast<double>(disk.totalBytes - disk.freeBytes) / (1024.0 * 1024.0 * 1024.0);
        double total = static_cast<double>(disk.totalBytes) / (1024.0 * 1024.0 * 1024.0);
        float pct = (disk.totalBytes > 0) ? (static_cast<float>(disk.totalBytes - disk.freeBytes) / disk.totalBytes) : 0.0f;

        // Drive tag badge (e.g. "C:\")
        ID2D1SolidColorBrush* pPillBg = nullptr;
        rt->CreateSolidColorBrush(pal.PillBg, &pPillBg);
        if (pPillBg) {
            D2D1_ROUNDED_RECT driveRc = D2D1::RoundedRect(D2D1::RectF(startX + 10.0f, diskY, startX + 32.0f, diskY + 18.0f), 3.5f, 3.5f);
            rt->FillRoundedRectangle(driveRc, pPillBg);
            pPillBg->Release();
        }

        if (pTextPrimary && m_renderer.GetFormatBoldTiny()) {
            rt->DrawTextW(disk.drive.c_str(), (UINT32)disk.drive.length(), m_renderer.GetFormatBoldTiny(),
                D2D1::RectF(startX + 10.0f, diskY - 1.0f, startX + 32.0f, diskY + 18.0f), pTextPrimary);
        }

        // Capacity text (e.g. "106 / 376 GB (28%)")
        wchar_t lineText[64];
        swprintf_s(lineText, L"%.0f / %.0f GB (%.0f%%)", used, total, pct * 100.0f);
        if (pTextSecondary && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(lineText, (UINT32)wcslen(lineText), m_renderer.GetFormatSmall(),
                D2D1::RectF(startX + 38.0f, diskY + 1.0f, startX + 172.0f, diskY + 18.0f), pTextSecondary);
        }

        // Mini pill progress bar with warm amber fill
        float barX = startX + 174.0f;
        float barW = width - 184.0f;
        float barH = 10.0f;

        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(barX, diskY + 4.0f, barX + barW, diskY + 4.0f + barH), 5.0f, 5.0f);
        if (pTrackBg) rt->FillRoundedRectangle(trackRc, pTrackBg);
        if (pCardBorder) rt->DrawRoundedRectangle(trackRc, pCardBorder, 0.8f);

        float fillW = barW * std::clamp(pct, 0.0f, 1.0f);
        if (fillW > 3.0f && pDiskAmber) {
            D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(D2D1::RectF(barX, diskY + 4.0f, barX + fillW, diskY + 4.0f + barH), 5.0f, 5.0f);
            rt->FillRoundedRectangle(fillRc, pDiskAmber);
        }

        diskY += 24.0f;
    }

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pDiskAmber) pDiskAmber->Release();
    if (pTrackBg) pTrackBg->Release();

    curY += cardH + 7.0f;
}
