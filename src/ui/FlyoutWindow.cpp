// 详情弹出窗口的声明。
#include "FlyoutWindow.h"
// DPI 换算工具：在 DIP 和实际像素之间转换。
#include "DpiHelper.h"
// 主题颜色和主题配置。
#include "Theme.h"
// 提供 std::clamp，用于限制进度条和图表数值范围。
#include <algorithm>

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
                HWND hOther = reinterpret_cast<HWND>(lParam);
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

// 绘制详情窗口背景，并依次绘制标题、CPU、内存、网络和磁盘区域。
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

    DrawHeader(rt, curY, contentW, pal);
    DrawCpuSection(rt, curY, contentW, pal);
    DrawMemorySection(rt, curY, contentW, pal);
    DrawNetworkSection(rt, curY, contentW, pal);
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

    curY += 28.0f;
}

// 绘制 CPU 当前使用率和最近历史曲线。
void FlyoutWindow::DrawCpuSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 100.0f;

    D2D1_COLOR_F cpuStatusCol = Theme::GetCpuStatusColor(m_snapshot.cpuUsageTotal);

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pCpuLine = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;
    ID2D1SolidColorBrush* pGridLine = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(cpuStatusCol, &pCpuLine);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);
    rt->CreateSolidColorBrush(pal.GridLine, &pGridLine);

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
    if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

    // Section Header: Label "CPU 总体负载"
    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        const wchar_t* cpuLabel = L"CPU 总体负载";
        rt->DrawTextW(cpuLabel, static_cast<UINT32>(wcslen(cpuLabel)), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 10.0f, curY + 6.0f, startX + width - 85.0f, curY + 26.0f), pTextSecondary);
    }

    // Value right-aligned
    wchar_t valText[32];
    swprintf_s(valText, L"%.1f%%", m_snapshot.cpuUsageTotal);
    if (pCpuLine && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(valText, (UINT32)wcslen(valText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 90.0f, curY + 6.0f, startX + width - 10.0f, curY + 25.0f), pCpuLine);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // Chart Area
    float chartX = startX + 10.0f;
    float chartY = curY + 29.0f;
    float chartW = width - 20.0f;
    float chartH = 61.0f;

    D2D1_ROUNDED_RECT chartRc = D2D1::RoundedRect(D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH), 5.0f, 5.0f);
    if (pTrackBg) rt->FillRoundedRectangle(chartRc, pTrackBg);
    if (pCardBorder) rt->DrawRoundedRectangle(chartRc, pCardBorder, 1.0f);

    // Subtle grid line (50%)
    if (pGridLine) {
        float midY = chartY + chartH * 0.5f;
        rt->DrawLine(D2D1::Point2F(chartX, midY), D2D1::Point2F(chartX + chartW, midY), pGridLine, 1.0f);
    }

    // Waveform Area Gradient + Stroke
    const auto& hist = m_snapshot.cpuHistory;
    size_t count = hist.size();
    if (count > 1 && m_renderer.GetD2DFactory()) {
        ID2D1PathGeometry* pAreaGeom = nullptr;
        m_renderer.GetD2DFactory()->CreatePathGeometry(&pAreaGeom);

        float step = chartW / 59.0f;
        float startHistX = chartX + (59 - (count - 1)) * step;

        if (pAreaGeom) {
            ID2D1GeometrySink* pSink = nullptr;
            if (SUCCEEDED(pAreaGeom->Open(&pSink))) {
                pSink->BeginFigure(D2D1::Point2F(startHistX, chartY + chartH), D2D1_FIGURE_BEGIN_FILLED);

                for (size_t i = 0; i < count; ++i) {
                    float px = startHistX + i * step;
                    float pct = std::clamp(hist[i], 0.0f, 100.0f) / 100.0f;
                    float py = chartY + chartH - (pct * (chartH - 4.0f)) - 2.0f;
                    pSink->AddLine(D2D1::Point2F(px, py));
                }

                float endPx = startHistX + (count - 1) * step;
                pSink->AddLine(D2D1::Point2F(endPx, chartY + chartH));
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                pSink->Release();

                ID2D1GradientStopCollection* pStops = nullptr;
                D2D1_GRADIENT_STOP stops[2];
                stops[0].color = D2D1::ColorF(cpuStatusCol.r, cpuStatusCol.g, cpuStatusCol.b, m_isLightMode ? 0.35f : 0.28f);
                stops[0].position = 0.0f;
                stops[1].color = D2D1::ColorF(cpuStatusCol.r, cpuStatusCol.g, cpuStatusCol.b, 0.0f);
                stops[1].position = 1.0f;

                rt->CreateGradientStopCollection(stops, 2, &pStops);
                if (pStops) {
                    ID2D1LinearGradientBrush* pAreaBrush = nullptr;
                    rt->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(
                            D2D1::Point2F(chartX, chartY),
                            D2D1::Point2F(chartX, chartY + chartH)
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

        // Draw crisp stroke line over area
        for (size_t i = 1; i < count; ++i) {
            float x1 = startHistX + (i - 1) * step;
            float y1 = chartY + chartH - (std::clamp(hist[i - 1], 0.0f, 100.0f) / 100.0f * (chartH - 4.0f)) - 2.0f;
            float x2 = startHistX + i * step;
            float y2 = chartY + chartH - (std::clamp(hist[i], 0.0f, 100.0f) / 100.0f * (chartH - 4.0f)) - 2.0f;

            if (pCpuLine) {
                rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), pCpuLine, 1.8f);
            }
        }

        // Current point beacon
        float lastPct = std::clamp(hist.back(), 0.0f, 100.0f) / 100.0f;
        float lastX = startHistX + (count - 1) * step;
        float lastY = chartY + chartH - (lastPct * (chartH - 4.0f)) - 2.0f;

        ID2D1SolidColorBrush* pGlowBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pGlowBrush);
        if (pGlowBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(lastX, lastY), 2.2f, 2.2f), pGlowBrush);
            pGlowBrush->Release();
        }
    }

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pCpuLine) pCpuLine->Release();
    if (pTrackBg) pTrackBg->Release();
    if (pGridLine) pGridLine->Release();

    curY += cardH + 8.0f;
}

// 绘制物理内存使用量、百分比和进度条。
void FlyoutWindow::DrawMemorySection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 64.0f;

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextPrimary = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pRamBrush = nullptr;
    ID2D1SolidColorBrush* pTrackBg = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextPrimary, &pTextPrimary);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(pal.RamPurple, &pRamBrush);
    rt->CreateSolidColorBrush(pal.TrackBg, &pTrackBg);

    D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + width, curY + cardH), 9.0f, 9.0f);
    if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
    if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

    double usedGB = static_cast<double>(m_snapshot.usedPhysMem) / (1024.0 * 1024.0 * 1024.0);
    double totalGB = static_cast<double>(m_snapshot.totalPhysMem) / (1024.0 * 1024.0 * 1024.0);

    wchar_t ramSub[64];
    swprintf_s(ramSub, L"物理内存 (%.1f / %.1f GB)", usedGB, totalGB);
    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        rt->DrawTextW(ramSub, (UINT32)wcslen(ramSub), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 10.0f, curY + 7.0f, startX + width - 75.0f, curY + 25.0f), pTextSecondary);
    }

    wchar_t pctText[16];
    swprintf_s(pctText, L"%.0f%%", m_snapshot.memoryUsagePercent);
    if (pRamBrush && m_renderer.GetFormatValue()) {
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(pctText, (UINT32)wcslen(pctText), m_renderer.GetFormatValue(),
            D2D1::RectF(startX + width - 70.0f, curY + 6.0f, startX + width - 10.0f, curY + 25.0f), pRamBrush);
        m_renderer.GetFormatValue()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    // Modern Pill Progress Bar
    float barX = startX + 10.0f;
    float barY = curY + 34.0f;
    float barW = width - 20.0f;
    float barH = 16.0f;

    D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(barX, barY, barX + barW, barY + barH), 8.0f, 8.0f);
    if (pTrackBg) rt->FillRoundedRectangle(trackRc, pTrackBg);
    if (pCardBorder) rt->DrawRoundedRectangle(trackRc, pCardBorder, 1.0f);

    float fillW = (barW - 4.0f) * (std::clamp(m_snapshot.memoryUsagePercent, 0.0f, 100.0f) / 100.0f);
    if (fillW > 4.0f && pRamBrush) {
        D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(
            D2D1::RectF(barX + 2.0f, barY + 2.0f, barX + 2.0f + fillW, barY + barH - 2.0f),
            6.0f, 6.0f
        );
        rt->FillRoundedRectangle(fillRc, pRamBrush);
    }

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pRamBrush) pRamBrush->Release();
    if (pTrackBg) pTrackBg->Release();

    curY += cardH + 8.0f;
}

// 绘制下载和上传速度两个网络信息卡片。
void FlyoutWindow::DrawNetworkSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    float startX = 12.0f;
    float cardH = 64.0f;
    float gap = 8.0f;
    float halfW = (width - gap) / 2.0f;

    ID2D1SolidColorBrush* pCardBg = nullptr;
    ID2D1SolidColorBrush* pCardBorder = nullptr;
    ID2D1SolidColorBrush* pTextSecondary = nullptr;
    ID2D1SolidColorBrush* pDownBrush = nullptr;
    ID2D1SolidColorBrush* pUpBrush = nullptr;

    rt->CreateSolidColorBrush(pal.CardBg, &pCardBg);
    rt->CreateSolidColorBrush(pal.CardBorder, &pCardBorder);
    rt->CreateSolidColorBrush(pal.TextSecondary, &pTextSecondary);
    rt->CreateSolidColorBrush(pal.NetDownGreen, &pDownBrush);
    rt->CreateSolidColorBrush(pal.NetUpBlue, &pUpBrush);

    // Left Card: Download
    {
        D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(startX, curY, startX + halfW, curY + cardH), 9.0f, 9.0f);
        if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
        if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

        if (pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"↓ 下载流速", 6, m_renderer.GetFormatLabel(),
                D2D1::RectF(startX + 10.0f, curY + 8.0f, startX + halfW - 8.0f, curY + 24.0f), pTextSecondary);
        }

        std::wstring downText = FormatSpeed(m_snapshot.downloadSpeedBps);
        if (pDownBrush && m_renderer.GetFormatValue()) {
            rt->DrawTextW(downText.c_str(), (UINT32)downText.length(), m_renderer.GetFormatValue(),
                D2D1::RectF(startX + 10.0f, curY + 31.0f, startX + halfW - 8.0f, curY + 55.0f), pDownBrush);
        }
    }

    // Right Card: Upload
    {
        float rightX = startX + halfW + gap;
        D2D1_ROUNDED_RECT cardRc = D2D1::RoundedRect(D2D1::RectF(rightX, curY, rightX + halfW, curY + cardH), 9.0f, 9.0f);
        if (pCardBg) rt->FillRoundedRectangle(cardRc, pCardBg);
        if (pCardBorder) rt->DrawRoundedRectangle(cardRc, pCardBorder, 1.0f);

        if (pTextSecondary && m_renderer.GetFormatLabel()) {
            rt->DrawTextW(L"↑ 上传流速", 6, m_renderer.GetFormatLabel(),
                D2D1::RectF(rightX + 10.0f, curY + 8.0f, rightX + halfW - 8.0f, curY + 24.0f), pTextSecondary);
        }

        std::wstring upText = FormatSpeed(m_snapshot.uploadSpeedBps);
        if (pUpBrush && m_renderer.GetFormatValue()) {
            rt->DrawTextW(upText.c_str(), (UINT32)upText.length(), m_renderer.GetFormatValue(),
                D2D1::RectF(rightX + 10.0f, curY + 31.0f, rightX + halfW - 8.0f, curY + 55.0f), pUpBrush);
        }
    }

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pDownBrush) pDownBrush->Release();
    if (pUpBrush) pUpBrush->Release();

    curY += cardH + 8.0f;
}

// 绘制每个本地磁盘的容量、使用率和容量进度条。
void FlyoutWindow::DrawDiskSection(ID2D1DCRenderTarget* rt, float& curY, float width, const Theme::Palette& pal) {
    if (m_snapshot.disks.empty()) return;

    float startX = 12.0f;
    float cardH = 30.0f + static_cast<float>(m_snapshot.disks.size()) * 30.0f;

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

    if (pTextSecondary && m_renderer.GetFormatLabel()) {
        std::wstring header = L"本地磁盘存储";
        rt->DrawTextW(header.c_str(), (UINT32)header.length(), m_renderer.GetFormatLabel(),
            D2D1::RectF(startX + 10.0f, curY + 7.0f, startX + width - 10.0f, curY + 23.0f), pTextSecondary);
    }

    float diskY = curY + 27.0f;
    for (const auto& disk : m_snapshot.disks) {
        double used = static_cast<double>(disk.totalBytes - disk.freeBytes) / (1024.0 * 1024.0 * 1024.0);
        double total = static_cast<double>(disk.totalBytes) / (1024.0 * 1024.0 * 1024.0);
        float pct = (disk.totalBytes > 0) ? (static_cast<float>(disk.totalBytes - disk.freeBytes) / disk.totalBytes) : 0.0f;

        // Drive tag
        ID2D1SolidColorBrush* pPillBg = nullptr;
        rt->CreateSolidColorBrush(pal.PillBg, &pPillBg);
        if (pPillBg) {
            D2D1_ROUNDED_RECT driveRc = D2D1::RoundedRect(D2D1::RectF(startX + 10.0f, diskY, startX + 34.0f, diskY + 20.0f), 4.0f, 4.0f);
            rt->FillRoundedRectangle(driveRc, pPillBg);
            pPillBg->Release();
        }

        if (pTextPrimary && m_renderer.GetFormatValue()) {
            rt->DrawTextW(disk.drive.c_str(), (UINT32)disk.drive.length(), m_renderer.GetFormatValue(),
                D2D1::RectF(startX + 12.0f, diskY + 1.0f, startX + 34.0f, diskY + 20.0f), pTextPrimary);
        }

        // Capacity text
        wchar_t lineText[64];
        swprintf_s(lineText, L"%.0f / %.0f GB (%.0f%%)", used, total, pct * 100.0f);
        if (pTextSecondary && m_renderer.GetFormatSmall()) {
            rt->DrawTextW(lineText, (UINT32)wcslen(lineText), m_renderer.GetFormatSmall(),
                D2D1::RectF(startX + 40.0f, diskY + 3.0f, startX + 175.0f, diskY + 20.0f), pTextSecondary);
        }

        // Mini bar
        float barX = startX + 178.0f;
        float barW = width - 188.0f;
        float barH = 10.0f;

        D2D1_ROUNDED_RECT trackRc = D2D1::RoundedRect(D2D1::RectF(barX, diskY + 5.0f, barX + barW, diskY + 5.0f + barH), 5.0f, 5.0f);
        if (pTrackBg) rt->FillRoundedRectangle(trackRc, pTrackBg);
        if (pCardBorder) rt->DrawRoundedRectangle(trackRc, pCardBorder, 1.0f);

        float fillW = barW * std::clamp(pct, 0.0f, 1.0f);
        if (fillW > 3.0f && pDiskAmber) {
            D2D1_ROUNDED_RECT fillRc = D2D1::RoundedRect(D2D1::RectF(barX, diskY + 5.0f, barX + fillW, diskY + 5.0f + barH), 5.0f, 5.0f);
            rt->FillRoundedRectangle(fillRc, pDiskAmber);
        }

        diskY += 28.0f;
    }

    if (pCardBg) pCardBg->Release();
    if (pCardBorder) pCardBorder->Release();
    if (pTextPrimary) pTextPrimary->Release();
    if (pTextSecondary) pTextSecondary->Release();
    if (pDiskAmber) pDiskAmber->Release();
    if (pTrackBg) pTrackBg->Release();

    curY += cardH + 8.0f;
}
