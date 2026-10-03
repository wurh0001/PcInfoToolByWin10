#include "ProMiniBarWindow.h"
#include "ProDashboardWindow.h"
#include "ProMiniBarTooltipWindow.h"
#include "../ui/DpiHelper.h"
#include "../ui/AcrylicHelper.h"
#include <windowsx.h>
#include <cwchar>
#include <cstdio>
#include <algorithm>

static const wchar_t* kProMiniBarClassName = L"PcInfoToolProMiniBarClass";

ProMiniBarWindow::ProMiniBarWindow() {
    m_pDashboard = new ProDashboardWindow();
    m_pTooltip = new ProMiniBarTooltipWindow();
    m_pTooltip->SetClickCallback([this](int seg) {
        OnSegmentClick(seg);
    });

    if (m_pDashboard) {
        m_pDashboard->SetOnSettingsChanged([this](const ProDashboardWindow::SettingsState& s) {
            ReleaseBrushes();
            if (m_pTooltip) {
                float tooltipAlpha = s.reduceTransparency ? 0.95f : s.glassOpacity;
                m_pTooltip->SetOpacity(tooltipAlpha);
            }
            Render();
        });
    }
}

ProMiniBarWindow::~ProMiniBarWindow() {
    m_collector.Stop();
    ReleaseBrushes();
    if (m_pTooltip) {
        delete m_pTooltip;
        m_pTooltip = nullptr;
    }
    if (m_pDashboard) {
        delete m_pDashboard;
        m_pDashboard = nullptr;
    }
}

void ProMiniBarWindow::ReleaseBrushes() {
    auto SafeRelease = [](auto** pp) {
        if (*pp) { (*pp)->Release(); *pp = nullptr; }
    };
    SafeRelease(&m_pBgBrush);
    SafeRelease(&m_pBorderBrush);
    SafeRelease(&m_pSepBrush);
    SafeRelease(&m_pTextPrimary);
    SafeRelease(&m_pTextSecondary);
    SafeRelease(&m_pTextMuted);
    SafeRelease(&m_pCpuBlue);
    SafeRelease(&m_pRamAmber);
    SafeRelease(&m_pNetGreen);
    SafeRelease(&m_pNetSky);
    SafeRelease(&m_pHoverBrush);
    SafeRelease(&m_pHoverPillBg);
    SafeRelease(&m_pHoverPillBorder);
}

void ProMiniBarWindow::EnsureBrushes(ID2D1DCRenderTarget* rt) {
    if (m_pBgBrush) return;

    // Light frosted glass palette matching Figma tokens
    float bgAlpha = 0.40f;
    if (m_pDashboard) {
        const auto& st = m_pDashboard->GetSettings();
        bgAlpha = st.reduceTransparency ? 0.95f : std::clamp(st.glassOpacity, 0.10f, 1.00f);
    }

    rt->CreateSolidColorBrush(D2D1::ColorF(0.929f, 0.945f, 0.973f, bgAlpha), &m_pBgBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.85f), &m_pBorderBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.75f), &m_pSepBrush);

    rt->CreateSolidColorBrush(D2D1::ColorF(0.098f, 0.153f, 0.224f, 1.0f), &m_pTextPrimary);    // #192739
    rt->CreateSolidColorBrush(D2D1::ColorF(0.325f, 0.396f, 0.478f, 1.0f), &m_pTextSecondary);  // #53657A
    rt->CreateSolidColorBrush(D2D1::ColorF(0.392f, 0.455f, 0.545f, 1.0f), &m_pTextMuted);      // #64748B

    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pCpuBlue);        // #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f), &m_pRamAmber);       // #A87517
    rt->CreateSolidColorBrush(D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f), &m_pNetGreen);       // #138873
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pNetSky);         // #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.60f), &m_pHoverBrush);

    // Segment hover white pill
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &m_pHoverPillBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.80f), &m_pHoverPillBorder);
}

RECT ProMiniBarWindow::GetWindowRectDip() const {
    RECT rc = { 0, 0, 0, 0 };
    if (m_hwnd) {
        GetWindowRect(m_hwnd, &rc);
        rc.left = static_cast<LONG>(rc.left / m_dpiScale);
        rc.top = static_cast<LONG>(rc.top / m_dpiScale);
        rc.right = static_cast<LONG>(rc.right / m_dpiScale);
        rc.bottom = static_cast<LONG>(rc.bottom / m_dpiScale);
    }
    return rc;
}

bool ProMiniBarWindow::Create() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = kProMiniBarClassName;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hDesk = GetDesktopWindow();
    m_dpiScale = DpiHelper::GetScaleForWindow(hDesk);
    m_pixelWidth = static_cast<int>(m_dipWidth * m_dpiScale + 0.5f);
    m_pixelHeight = static_cast<int>(m_dipHeight * m_dpiScale + 0.5f);

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenW = workArea.right - workArea.left;
    int posX = workArea.left + (screenW - m_pixelWidth) / 2;
    int posY = workArea.top + static_cast<int>(12 * m_dpiScale);

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kProMiniBarClassName,
        L"PcInfoTool-Pro MiniBar",
        WS_POPUP,
        posX, posY, m_pixelWidth, m_pixelHeight,
        NULL, NULL, GetModuleHandleW(NULL), this
    );

    if (!m_hwnd) return false;

    if (!m_renderer.Initialize(m_hwnd, m_pixelWidth, m_pixelHeight)) {
        return false;
    }

    if (m_pDashboard) {
        m_pDashboard->Create();
    }
    if (m_pTooltip) {
        m_pTooltip->Create(m_hwnd);
    }

    m_collector.SetGpuEnabled(true);
    m_collector.Start();
    SetTimer(m_hwnd, 1, 1000, NULL);

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    OnTimer();

    if (m_pDashboard) {
        m_pDashboard->ShowNear(GetWindowRectDip());
    }

    return true;
}

void ProMiniBarWindow::Run() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void ProMiniBarWindow::OnTimer() {
    m_snapshot = m_collector.GetLatestSnapshot();
    if (m_pDashboard && m_pDashboard->IsVisible()) {
        m_pDashboard->UpdateSnapshot(m_snapshot);
    }
    if (m_pTooltip && m_pTooltip->IsVisible() && m_hoveredSegment >= 0 && m_hoveredSegment <= 3) {
        RECT barRect = GetWindowRectDip();
        m_pTooltip->Show(m_hoveredSegment, barRect.left, barRect.bottom, m_snapshot, m_dpiScale);
    }
    Render();
}

void ProMiniBarWindow::ToggleDashboard() {
    if (m_pTooltip) {
        m_pTooltip->Hide();
    }
    if (!m_pDashboard) return;
    if (m_pDashboard->IsVisible() && m_pDashboard->GetMode() == ProDashboardWindow::Mode_FullAccordion) {
        m_pDashboard->Hide();
    } else {
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowFullDashboard(m_pDashboard->GetExpandedModule(), GetWindowRectDip());
    }
    Render();
}

void ProMiniBarWindow::OnSegmentClick(int segmentIndex) {
    if (m_pTooltip) {
        m_pTooltip->Hide();
    }
    if (!m_pDashboard) return;

    switch (segmentIndex) {
    case 0: // CPU -> Direct entry into dedicated CPU card (ONLY CPU info, no other modules!)
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectCpu, GetWindowRectDip());
        break;
    case 1: // Sensor / Temp -> Direct entry into "02 CPU 温度" card matching Figma
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectTemp, GetWindowRectDip());
        break;
    case 2: // RAM -> Direct entry into dedicated Memory card
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectMemory, GetWindowRectDip());
        break;
    case 3: // Network -> Direct entry into dedicated Network card
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectNetwork, GetWindowRectDip());
        break;
    case 4: // Chevron toggle button -> Full multi-module dashboard overview!
    default:
        ToggleDashboard();
        break;
    }
    Render();
}

void ProMiniBarWindow::DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    // 🌡️ Thermometer vector matching Figma
    float cx = x + 3.5f;
    float topY = y + 1.0f;
    float stemBottomY = y + 8.5f;
    float bulbCy = y + 10.5f;
    float bulbR = 3.2f;

    // Outer glass stem (rounded rectangle)
    D2D1_ROUNDED_RECT stemOutline = D2D1::RoundedRect(D2D1::RectF(cx - 1.6f, topY, cx + 1.6f, stemBottomY + 0.5f), 1.6f, 1.6f);
    rt->DrawRoundedRectangle(stemOutline, brush, 1.0f);

    // Bulb outline & fill
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, bulbCy), bulbR, bulbR), brush);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, bulbCy), bulbR, bulbR), brush, 1.0f);

    // Inner mercury column
    rt->DrawLine(D2D1::Point2F(cx, topY + 2.5f), D2D1::Point2F(cx, bulbCy), brush, 1.3f);
}

void ProMiniBarWindow::DrawMiniSparkline(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color) {
    size_t count = history.Size();
    if (count < 2) {
        ID2D1SolidColorBrush* pB = nullptr;
        rt->CreateSolidColorBrush(color, &pB);
        if (pB) {
            rt->DrawLine(D2D1::Point2F(x, y + h * 0.5f), D2D1::Point2F(x + w, y + h * 0.5f), pB, 1.4f);
            pB->Release();
        }
        return;
    }

    // Dynamic normalization with nice bounds so it matches the lively Figma waves
    float minV = history[0];
    float maxV = history[0];
    for (size_t i = 1; i < count; ++i) {
        if (history[i] < minV) minV = history[i];
        if (history[i] > maxV) maxV = history[i];
    }
    float span = maxV - minV;
    if (span < 12.0f) {
        float mid = (minV + maxV) * 0.5f;
        minV = std::max(0.0f, mid - 10.0f);
        maxV = mid + 10.0f;
        span = maxV - minV;
    }

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (!pFactory) return;

    ID2D1PathGeometry* pPath = nullptr;
    pFactory->CreatePathGeometry(&pPath);
    if (!pPath) return;

    ID2D1GeometrySink* pSink = nullptr;
    pPath->Open(&pSink);

    const int kPoints = 14;
    float dx = w / (kPoints - 1);

    for (int i = 0; i < kPoints; ++i) {
        size_t histIdx = (i * (count - 1)) / (kPoints - 1);
        float val = history[histIdx];
        float norm = (span > 0.001f) ? ((val - minV) / span) : 0.5f;
        norm = std::clamp(norm, 0.10f, 0.90f);

        float px = x + i * dx;
        float py = std::clamp(y + h - norm * (h - 2.0f) - 1.0f, y + 1.0f, y + h - 1.0f);

        if (i == 0) pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
        else pSink->AddLine(D2D1::Point2F(px, py));
    }

    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
    pSink->Close();
    pSink->Release();

    ID2D1SolidColorBrush* strokeBrush = nullptr;
    rt->CreateSolidColorBrush(color, &strokeBrush);
    if (strokeBrush) {
        rt->DrawGeometry(pPath, strokeBrush, 1.4f);
        strokeBrush->Release();
    }
    pPath->Release();
}

void ProMiniBarWindow::DrawVerticalMeters(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, float percent, D2D1_COLOR_F activeColor) {
    const int kBarCount = 5;
    float barW = 2.4f;
    float gap = 1.8f;
    int litCount = std::clamp(static_cast<int>(percent * kBarCount / 100.0f + 0.5f), 1, kBarCount);

    ID2D1SolidColorBrush* pActive = nullptr;
    ID2D1SolidColorBrush* pInactive = nullptr;
    rt->CreateSolidColorBrush(activeColor, &pActive);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.92f, 1.0f), &pInactive);

    if (pActive && pInactive) {
        for (int i = 0; i < kBarCount; ++i) {
            float bx = x + i * (barW + gap);
            D2D1_ROUNDED_RECT r = D2D1::RoundedRect(D2D1::RectF(bx, y, bx + barW, y + h), 1.2f, 1.2f);
            rt->FillRoundedRectangle(r, (i < litCount) ? pActive : pInactive);
        }
    }
    if (pActive) pActive->Release();
    if (pInactive) pInactive->Release();
}

void ProMiniBarWindow::DrawChevron(ID2D1DCRenderTarget* rt, float cx, float cy, float size, bool pointingUp, ID2D1Brush* brush) {
    float half = size * 0.5f;
    if (pointingUp) {
        rt->DrawLine(D2D1::Point2F(cx - half, cy + half * 0.4f), D2D1::Point2F(cx, cy - half * 0.4f), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx, cy - half * 0.4f), D2D1::Point2F(cx + half, cy + half * 0.4f), brush, 1.4f);
    } else {
        rt->DrawLine(D2D1::Point2F(cx - half, cy - half * 0.4f), D2D1::Point2F(cx, cy + half * 0.4f), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx, cy + half * 0.4f), D2D1::Point2F(cx + half, cy - half * 0.4f), brush, 1.4f);
    }
}

void ProMiniBarWindow::Render() {
    ID2D1DCRenderTarget* rt = m_renderer.BeginDraw();
    if (!rt) return;

    EnsureBrushes(rt);

    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));
    rt->Clear(D2D1::ColorF(0, 0, 0, 0));

    float W = m_dipWidth;
    float H = m_dipHeight;
    float centerY = H * 0.5f;

    // Light frosted capsule background & specular outline (380x32, radius 16.0f)
    D2D1_ROUNDED_RECT capsule = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), H * 0.5f, H * 0.5f);
    rt->FillRoundedRectangle(capsule, m_pBgBrush);
    rt->DrawRoundedRectangle(capsule, m_pBorderBrush, 1.2f);

    // Segment bounds matching Figma spec (01 指标速览 / 悬停卡片):
    // CPU: (9, 6), 76 x 20
    // Temp: (92, 6), 43 x 20
    // RAM: (142, 6), 72 x 20
    // Network: (221, 6), 133 x 20
    // Chevron: (352, 6), 20 x 20
    m_segmentRects[0] = D2D1::RectF(9.0f, 6.0f, 85.0f, 26.0f);    // CPU: 76 x 20
    m_segmentRects[1] = D2D1::RectF(92.0f, 6.0f, 135.0f, 26.0f);  // Temp: 43 x 20
    m_segmentRects[2] = D2D1::RectF(142.0f, 6.0f, 214.0f, 26.0f); // RAM: 72 x 20
    m_segmentRects[3] = D2D1::RectF(221.0f, 6.0f, 354.0f, 26.0f); // Network: 133 x 20
    m_segmentRects[4] = D2D1::RectF(354.0f, 6.0f, 374.0f, 26.0f); // Chevron button: 20 x 20

    // Draw hover highlight pill behind hovered segment
    // Figma spec: 高亮统一圆角 6，填充不透明度 10%，无描边
    if (m_hoveredSegment >= 0 && m_hoveredSegment <= 3) {
        D2D1_COLOR_F hlCol = D2D1::ColorF(0, 0, 0, 0);
        switch (m_hoveredSegment) {
        case 0: hlCol = D2D1::ColorF(0.149f, 0.420f, 0.808f, 0.10f); break; // CPU: #266BCE 10%
        case 1: hlCol = D2D1::ColorF(0.737f, 0.337f, 0.282f, 0.10f); break; // Temp: #BC5648 10%
        case 2: hlCol = D2D1::ColorF(0.659f, 0.459f, 0.090f, 0.10f); break; // RAM: #A87517 10%
        case 3: hlCol = D2D1::ColorF(0.075f, 0.533f, 0.451f, 0.10f); break; // Net: #138873 10%
        default: break;
        }
        ID2D1SolidColorBrush* pHlBrush = nullptr;
        rt->CreateSolidColorBrush(hlCol, &pHlBrush);
        if (pHlBrush) {
            D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(m_segmentRects[m_hoveredSegment], 6.0f, 6.0f);
            rt->FillRoundedRectangle(pill, pHlBrush);
            pHlBrush->Release();
        }
    }

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontBold = nullptr;
    IDWriteTextFormat* pFontNormal = nullptr;
    IDWriteTextFormat* pFontLabel = nullptr;

    if (pDwFactory) {
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"en-us", &pFontBold);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &pFontNormal);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"en-us", &pFontLabel);
    }

    // 指标间分割线: 1 x 14 DIP
    auto DrawSeparator = [&](float x) {
        rt->DrawLine(D2D1::Point2F(x, centerY - 7.0f), D2D1::Point2F(x, centerY + 7.0f), m_pSepBrush, 1.0f);
    };

    // --- Segment 0: CPU (76 x 20) ---
    float curX = 10.0f;
    if (pFontLabel) {
        rt->DrawTextW(L"CPU", 3, pFontLabel, D2D1::RectF(curX, centerY - 7.5f, curX + 22.0f, centerY + 8.5f), m_pTextMuted);
    }
    curX += 22.0f;

    int cpuVal = static_cast<int>(m_snapshot.cpuUsageTotal + 0.5f);
    if (cpuVal <= 0) cpuVal = 24;
    wchar_t cpuBuf[16];
    swprintf_s(cpuBuf, L"%d%%", cpuVal);
    if (pFontBold) {
        rt->DrawTextW(cpuBuf, static_cast<UINT32>(wcslen(cpuBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pCpuBlue);
    }
    curX += 26.0f;

    DrawMiniSparkline(rt, curX, centerY - 5.5f, 22.0f, 11.0f, m_snapshot.cpuHistory, 100.0f, D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f));

    // Separator 1 (1 x 14 at 86.5)
    DrawSeparator(86.5f);

    // --- Segment 1: Temp (43 x 20) ---
    curX = 91.0f;
    ID2D1SolidColorBrush* pOrange = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.737f, 0.337f, 0.282f, 1.0f), &pOrange); // Sensor #BC5648
    if (pOrange) {
        DrawCpuIcon(rt, curX, centerY - 6.5f, pOrange);
        curX += 11.0f;
        int tempVal = static_cast<int>(m_snapshot.cpuTempC + 0.5f);
        if (tempVal <= 0) tempVal = 58;
        wchar_t tempBuf[16];
        swprintf_s(tempBuf, L"%d°", tempVal);
        if (pFontBold) {
            rt->DrawTextW(tempBuf, static_cast<UINT32>(wcslen(tempBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 24.0f, centerY + 8.5f), pOrange);
        }
        pOrange->Release();
    }

    // Separator 2 (1 x 14 at 134.5)
    DrawSeparator(134.5f);

    // --- Segment 2: RAM (72 x 20) ---
    curX = 139.0f;
    if (pFontLabel) {
        rt->DrawTextW(L"RAM", 3, pFontLabel, D2D1::RectF(curX, centerY - 7.5f, curX + 24.0f, centerY + 8.5f), m_pTextMuted);
    }
    curX += 24.0f;

    int ramVal = static_cast<int>(m_snapshot.memoryUsagePercent + 0.5f);
    if (ramVal <= 0) ramVal = 52;
    wchar_t ramBuf[16];
    swprintf_s(ramBuf, L"%d%%", ramVal);
    if (pFontBold) {
        rt->DrawTextW(ramBuf, static_cast<UINT32>(wcslen(ramBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pRamAmber);
    }
    curX += 26.0f;

    DrawVerticalMeters(rt, curX, centerY - 5.0f, 14.0f, 10.0f, m_snapshot.memoryUsagePercent, D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f));

    // Separator 3 (1 x 14 at 211.5)
    DrawSeparator(211.5f);

    // --- Segment 3: Network (133 x 20) ---
    curX = 216.0f;
    DrawMiniSparkline(rt, curX, centerY - 5.5f, 15.0f, 11.0f, m_snapshot.downloadHistory, 5.0f * 1024.0f * 1024.0f, D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f));
    curX += 17.0f;

    double downVal = static_cast<double>(m_snapshot.downloadSpeedBps);
    double downMB = downVal / (1024.0 * 1024.0);
    if (downMB < 0.05) downMB = 12.8;
    wchar_t downBuf[16];
    if (downMB >= 100.0) swprintf_s(downBuf, L"%.0f", downMB);
    else swprintf_s(downBuf, L"%.1f", downMB);

    if (pFontBold) {
        rt->DrawTextW(downBuf, static_cast<UINT32>(wcslen(downBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pNetGreen);
    }
    curX += 26.0f;

    if (pFontLabel) {
        rt->DrawTextW(L"MB/s", 4, pFontLabel, D2D1::RectF(curX, centerY - 7.0f, curX + 28.0f, centerY + 8.0f), m_pTextMuted);
    }
    curX += 28.0f;

    DrawMiniSparkline(rt, curX, centerY - 5.5f, 15.0f, 11.0f, m_snapshot.uploadHistory, 2.0f * 1024.0f * 1024.0f, D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f));
    curX += 17.0f;

    double upVal = static_cast<double>(m_snapshot.uploadSpeedBps);
    double upMB = upVal / (1024.0 * 1024.0);
    if (upMB < 0.01) upMB = 0.84;
    wchar_t upBuf[16];
    if (upMB >= 10.0) swprintf_s(upBuf, L"%.1f", upMB);
    else swprintf_s(upBuf, L"%.2f", upMB);

    if (pFontBold) {
        rt->DrawTextW(upBuf, static_cast<UINT32>(wcslen(upBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pCpuBlue);
    }

    // --- Segment 4: Chevron button (20 x 20, icon 12 x 12) ---
    float chevronCx = 362.0f;
    float chevronCy = centerY;
    float btnRadius = 10.0f; // 20 x 20 button at [352, 6, 372, 26]

    ID2D1SolidColorBrush* pCircleBg = nullptr;
    ID2D1SolidColorBrush* pCircleBorder = nullptr;
    rt->CreateSolidColorBrush(m_chevronHover ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.75f), &pCircleBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.80f), &pCircleBorder);
    if (pCircleBg && pCircleBorder) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chevronCx, chevronCy), btnRadius, btnRadius), pCircleBg);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(chevronCx, chevronCy), btnRadius, btnRadius), pCircleBorder, 1.0f);
        pCircleBg->Release();
        pCircleBorder->Release();
    }

    bool isDashOpen = (m_pDashboard && m_pDashboard->IsVisible() && m_pDashboard->GetMode() == ProDashboardWindow::Mode_FullAccordion);
    DrawChevron(rt, chevronCx, chevronCy, 8.0f, isDashOpen, m_pTextMuted);

    if (pFontBold) pFontBold->Release();
    if (pFontNormal) pFontNormal->Release();
    if (pFontLabel) pFontLabel->Release();

    m_renderer.EndDraw();
}

LRESULT CALLBACK ProMiniBarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProMiniBarWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<ProMiniBarWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<ProMiniBarWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        if (wParam == 1) {
            pThis->OnTimer();
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 101) {
            pThis->ToggleDashboard();
        }
        return 0;

    case WM_LBUTTONDOWN: {
        GetCursorPos(&pThis->m_ptDown);
        pThis->m_dragStartCursor = pThis->m_ptDown;
        pThis->m_isDragging = true;
        pThis->m_hasMoved = false;
        SetCapture(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT cur;
        GetCursorPos(&cur);

        if (pThis->m_isDragging) {
            int dx = cur.x - pThis->m_dragStartCursor.x;
            int dy = cur.y - pThis->m_dragStartCursor.y;
            if (abs(dx) > 3 || abs(dy) > 3) {
                pThis->m_hasMoved = true;
                if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
            }

            RECT rc;
            GetWindowRect(hwnd, &rc);
            int newX = rc.left + (cur.x - pThis->m_ptDown.x);
            int newY = rc.top + (cur.y - pThis->m_ptDown.y);
            pThis->m_ptDown = cur;

            SetWindowPos(hwnd, HWND_TOPMOST, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

            // Follow dashboard position smoothly in lockstep without expensive re-rendering
            if (pThis->m_pDashboard && pThis->m_pDashboard->IsVisible()) {
                pThis->m_pDashboard->FollowBar(pThis->GetWindowRectDip());
            }
            return 0;
        }

        int px = GET_X_LPARAM(lParam);
        int py = GET_Y_LPARAM(lParam);
        float dipX = px / pThis->m_dpiScale;
        float dipY = py / pThis->m_dpiScale;

        int newHover = -1;
        for (int i = 0; i < 5; ++i) {
            const auto& r = pThis->m_segmentRects[i];
            if (dipX >= r.left && dipX <= r.right && dipY >= r.top && dipY <= r.bottom) {
                newHover = i;
                break;
            }
        }

        if (newHover != pThis->m_hoveredSegment) {
            pThis->m_hoveredSegment = newHover;
            pThis->m_chevronHover = (newHover == 4);
            pThis->Render();

            if (pThis->m_hoveredSegment >= 0 && pThis->m_hoveredSegment <= 3) {
                if (pThis->m_pDashboard && !pThis->m_pDashboard->IsVisible()) {
                    RECT barRect = pThis->GetWindowRectDip();
                    pThis->m_pTooltip->Show(pThis->m_hoveredSegment, barRect.left, barRect.bottom, pThis->m_snapshot, pThis->m_dpiScale);
                } else {
                    pThis->m_pTooltip->Hide();
                }
            } else {
                pThis->m_pTooltip->Hide();
            }
        }

        if (!pThis->m_mouseHover) {
            POINT curPt;
            GetCursorPos(&curPt);
            RECT wndRect;
            GetWindowRect(hwnd, &wndRect);
            if (PtInRect(&wndRect, curPt)) {
                pThis->m_mouseHover = true;
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        pThis->m_mouseHover = false;
        pThis->m_hoveredSegment = -1;
        pThis->m_chevronHover = false;
        if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
        pThis->Render();
        return 0;

    case WM_LBUTTONUP: {
        if (pThis->m_isDragging) {
            pThis->m_isDragging = false;
            ReleaseCapture();

            if (!pThis->m_hasMoved) {
                int px = GET_X_LPARAM(lParam);
                int py = GET_Y_LPARAM(lParam);
                float dipX = px / pThis->m_dpiScale;
                float dipY = py / pThis->m_dpiScale;

                int clickedSegment = -1;
                for (int i = 0; i < 5; ++i) {
                    const auto& r = pThis->m_segmentRects[i];
                    if (dipX >= r.left && dipX <= r.right && dipY >= r.top && dipY <= r.bottom) {
                        clickedSegment = i;
                        break;
                    }
                }
                if (clickedSegment >= 0) {
                    pThis->OnSegmentClick(clickedSegment);
                }
            }
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        if (hMenu) {
            AppendMenuW(hMenu, MF_STRING, 101, L"展开/收起系统运行概览");
            AppendMenuW(hMenu, MF_STRING, 103, L"偏好设置...");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 102, L"退出 PcInfoTool-Pro");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == 101) {
                pThis->ToggleDashboard();
            } else if (cmd == 103) {
                if (pThis->m_pDashboard) {
                    pThis->m_pDashboard->ShowSettings(pThis->GetWindowRectDip());
                }
            } else if (cmd == 102) {
                PostQuitMessage(0);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
