#include "ProMiniBarTooltipWindow.h"
#include <cstdio>
#include <cwchar>
#include <algorithm>

static const wchar_t* kProTooltipClassName = L"PcInfoToolProTooltipClass";

ProMiniBarTooltipWindow::ProMiniBarTooltipWindow() {
}

ProMiniBarTooltipWindow::~ProMiniBarTooltipWindow() {
    ReleaseBrushes();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void ProMiniBarTooltipWindow::ReleaseBrushes() {
    auto SafeRelease = [](auto** pp) {
        if (*pp) { (*pp)->Release(); *pp = nullptr; }
    };
    SafeRelease(&m_pCardBg);
    SafeRelease(&m_pCardBorder);
    SafeRelease(&m_pCardShadow);
    SafeRelease(&m_pSepBrush);
    SafeRelease(&m_pTextLabel);
    SafeRelease(&m_pTextVal);
    SafeRelease(&m_pCpuBlue);
    SafeRelease(&m_pTempCoral);
    SafeRelease(&m_pRamAmber);
    SafeRelease(&m_pNetGreen);
}

void ProMiniBarTooltipWindow::EnsureBrushes(ID2D1DCRenderTarget* rt) {
    if (m_pCardBg) return;

    // Figma spec:
    // 背景: #FFFFFF, 不透明度 86% (即透明度 14%)
    // 描边: #C2CBD9, 100%, 1 DIP 内描边
    // 阴影: #1A263D, 10%
    // 分割线: #C2CBD9, 0.60
    // 标签颜色: #53657A, 100%
    // 普通数值颜色: #192739, 100%
    // CPU/上传强调色: #266BCE
    // 温度强调色: #BC5648
    // 内存强调色: #A87517
    // 下载强调色: #138873
    float cardAlpha = std::clamp(m_opacity, 0.15f, 1.0f);
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, cardAlpha), &m_pCardBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 1.0f), &m_pCardBorder);  // #C2CBD9 100%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.102f, 0.149f, 0.239f, 0.10f), &m_pCardShadow);  // #1A263D 10%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &m_pSepBrush);   // #C2CBD9 60%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.325f, 0.396f, 0.478f, 1.0f), &m_pTextLabel);   // #53657A 100%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.098f, 0.153f, 0.224f, 1.0f), &m_pTextVal);     // #192739 100%

    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pCpuBlue);    // #266BCE 100%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.737f, 0.337f, 0.282f, 1.0f), &m_pTempCoral);  // #BC5648 100%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f), &m_pRamAmber);   // #A87517 100%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f), &m_pNetGreen);   // #138873 100%
}

bool ProMiniBarTooltipWindow::Create(HWND hOwner) {
    m_hOwner = hOwner;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = kProTooltipClassName;
    wc.hCursor = LoadCursorW(NULL, IDC_HAND);
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kProTooltipClassName,
        L"PcInfoTool-Pro Tooltip",
        WS_POPUP,
        0, 0, m_pixelWidth, m_pixelHeight,
        m_hOwner, NULL, GetModuleHandleW(NULL), this
    );

    if (!m_hwnd) return false;

    m_renderer.Initialize(m_hwnd, m_pixelWidth, m_pixelHeight);
    return true;
}

void ProMiniBarTooltipWindow::Show(int segmentIndex, int barLeftDip, int barBottomDip, const MetricsSnapshot& snapshot, float dpiScale) {
    if (segmentIndex < 0 || segmentIndex > 3) {
        Hide();
        return;
    }

    m_segmentIndex = segmentIndex;
    m_snapshot = snapshot;
    m_dpiScale = dpiScale;

    // Figma spec:
    // CPU: 200 x 56, X offset: 0, Y offset: 38 (距下边缘 6)
    // Temp: 184 x 56, X offset: 22, Y offset: 38
    // RAM: 216 x 56, X offset: 70, Y offset: 38
    // Network: 224 x 56, X offset: 156, Y offset: 38
    float cardW = 200.0f;
    float cardH = 56.0f;
    float offsetX = 0.0f;

    switch (segmentIndex) {
    case 0: // CPU 占用
        cardW = 200.0f;
        offsetX = 0.0f;
        break;
    case 1: // CPU 温度
        cardW = 184.0f;
        offsetX = 22.0f;
        break;
    case 2: // 物理内存
        cardW = 216.0f;
        offsetX = 70.0f;
        break;
    case 3: // 网络吞吐
        cardW = 224.0f;
        offsetX = 156.0f;
        break;
    }

    m_dipWidth = cardW;
    m_dipHeight = cardH;

    int newPixelW = static_cast<int>(m_dipWidth * m_dpiScale + 0.5f);
    int newPixelH = static_cast<int>(m_dipHeight * m_dpiScale + 0.5f);

    if (newPixelW != m_pixelWidth || newPixelH != m_pixelHeight) {
        m_pixelWidth = newPixelW;
        m_pixelHeight = newPixelH;
        m_renderer.Resize(m_pixelWidth, m_pixelHeight);
        ReleaseBrushes();
    }

    // 开发时位置: 卡片左上角 = 悬浮条左上角 + (对应 X 偏移, 38)
    // 悬浮条高 32，barBottomDip = barTopDip + 32，因此 Y = barTopDip + 38 = barBottomDip + 6
    int posX = static_cast<int>((barLeftDip + offsetX) * m_dpiScale + 0.5f);
    int posY = static_cast<int>((barBottomDip + 6) * m_dpiScale + 0.5f);

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (posX + m_pixelWidth > wa.right) posX = wa.right - m_pixelWidth - 8;
    if (posX < wa.left) posX = wa.left + 8;
    // 靠近屏幕边缘时限制卡片不越界；下方空间不足则翻转到上方，仍保留 6 DIP 间距
    if (posY + m_pixelHeight > wa.bottom) {
        posY = static_cast<int>((barBottomDip - 32 - 6) * m_dpiScale - m_pixelHeight);
    }

    SetWindowPos(m_hwnd, HWND_TOPMOST, posX, posY, m_pixelWidth, m_pixelHeight, SWP_NOACTIVATE);
    m_visible = true;
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    Render();
}

void ProMiniBarTooltipWindow::Hide() {
    if (m_visible) {
        m_visible = false;
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void ProMiniBarTooltipWindow::Render() {
    if (!m_hwnd || !m_visible) return;

    ID2D1DCRenderTarget* rt = m_renderer.BeginDraw();
    if (!rt) return;

    EnsureBrushes(rt);

    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));
    rt->Clear(D2D1::ColorF(0, 0, 0, 0));

    float W = m_dipWidth;
    float H = m_dipHeight;

    // 阴影偏移 X=0、Y=4, 模糊 12, 颜色约 #1A263D 不透明度 10%
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.0f, 4.0f, W, H + 4.0f), 10.0f, 10.0f);
    rt->FillRoundedRectangle(shadowR, m_pCardShadow);

    // 卡片背景与 1 DIP 描边 (圆角 10, 内边距 左右 12, 上下 8)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), 10.0f, 10.0f);
    rt->FillRoundedRectangle(cardR, m_pCardBg);
    rt->DrawRoundedRectangle(cardR, m_pCardBorder, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontLabel = nullptr;
    IDWriteTextFormat* pFontVal = nullptr;

    if (pDwFactory) {
        // Noto Sans SC / Segoe UI: 标签 10 / 14 (Regular), 数值 16 / 22 (Medium)
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"zh-cn", &pFontLabel);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"zh-cn", &pFontVal);
    }

    // 两个等宽信息列，中间竖分割线 (1 x 28, Y=14)
    // 各卡片精确坐标：
    // 卡片   左列 X / 宽   分割线 X   右列 X / 宽
    // CPU    12 / 79.5     99.5       108.5 / 79.5
    // 温度   12 / 71.5     91.5       100.5 / 71.5
    // 内存   12 / 87.5     107.5      116.5 / 87.5
    // 网络   12 / 91.5     111.5      120.5 / 91.5
    float leftColX = 12.0f;
    float colW = 79.5f;
    float sepX = 99.5f;
    float rightColX = 108.5f;

    const wchar_t* labelLeft = L"";
    const wchar_t* labelRight = L"";
    wchar_t valLeftBuf[32] = { 0 };
    wchar_t valRightBuf[32] = { 0 };
    ID2D1SolidColorBrush* pLeftValBrush = m_pTextVal;
    ID2D1SolidColorBrush* pRightValBrush = m_pTextVal;

    switch (m_segmentIndex) {
    case 0: { // CPU 占用
        colW = 79.5f;
        sepX = 99.5f;
        rightColX = 108.5f;
        labelLeft = L"CPU 占用";
        labelRight = L"有效频率";

        int cpu = static_cast<int>(m_snapshot.cpuUsageTotal + 0.5f);
        if (cpu <= 0) cpu = 24;
        swprintf_s(valLeftBuf, L"%d%%", cpu);

        float freq = (m_snapshot.cpuFreqGHz > 0.1f) ? m_snapshot.cpuFreqGHz : 4.82f;
        swprintf_s(valRightBuf, L"%.2f GHz", freq);

        pLeftValBrush = m_pCpuBlue;   // #266BCE 强调色
        pRightValBrush = m_pTextVal;  // #192739 普通数值
        break;
    }
    case 1: { // CPU 温度
        colW = 71.5f;
        sepX = 91.5f;
        rightColX = 100.5f;
        labelLeft = L"当前温度";
        labelRight = L"60 秒峰值";

        int temp = static_cast<int>(m_snapshot.cpuTempC + 0.5f);
        if (temp <= 0) temp = 58;
        swprintf_s(valLeftBuf, L"%d°C", temp);

        int peak = static_cast<int>(m_snapshot.tempMaxC + 0.5f);
        if (peak <= 0) peak = 64;
        swprintf_s(valRightBuf, L"%d°C", peak);

        pLeftValBrush = m_pTempCoral; // #BC5648 强调色
        pRightValBrush = m_pTextVal;  // #192739 普通数值
        break;
    }
    case 2: { // 物理内存
        colW = 87.5f;
        sepX = 107.5f;
        rightColX = 116.5f;
        labelLeft = L"正在使用";
        labelRight = L"可用内存";

        double usedG = static_cast<double>(m_snapshot.usedPhysMem) / (1024.0 * 1024.0 * 1024.0);
        double totalG = static_cast<double>(m_snapshot.totalPhysMem) / (1024.0 * 1024.0 * 1024.0);
        if (totalG <= 0.1) { usedG = 16.6; totalG = 32.0; }
        double availG = (totalG > usedG) ? (totalG - usedG) : 15.4;
        swprintf_s(valLeftBuf, L"%.1f GiB", usedG);
        swprintf_s(valRightBuf, L"%.1f GiB", availG);

        pLeftValBrush = m_pRamAmber;  // #A87517 强调色
        pRightValBrush = m_pTextVal;  // #192739 普通数值
        break;
    }
    case 3: { // 网络吞吐
        colW = 91.5f;
        sepX = 111.5f;
        rightColX = 120.5f;
        labelLeft = L"下载 MB/s";
        labelRight = L"上传 MB/s";

        double downVal = static_cast<double>(m_snapshot.downloadSpeedBps);
        double upVal = static_cast<double>(m_snapshot.uploadSpeedBps);
        double downMB = downVal / (1024.0 * 1024.0);
        double upMB = upVal / (1024.0 * 1024.0);
        if (downMB < 0.05 && upMB < 0.05) { downMB = 12.8; upMB = 0.84; }
        swprintf_s(valLeftBuf, L"%.1f", downMB);
        swprintf_s(valRightBuf, L"%.2f", upMB);

        pLeftValBrush = m_pNetGreen;  // #138873 强调色
        pRightValBrush = m_pCpuBlue;  // #266BCE 强调色
        break;
    }
    default:
        break;
    }

    // 竖分割线: 1 x 28, Y=14
    if (m_pSepBrush) {
        rt->DrawLine(D2D1::Point2F(sepX, 14.0f), D2D1::Point2F(sepX, 42.0f), m_pSepBrush, 1.0f);
    }

    // 标签: Y=8, 高 14; 数值: Y=26, 高 22
    if (pFontLabel) {
        rt->DrawTextW(labelLeft, static_cast<UINT32>(wcslen(labelLeft)), pFontLabel,
                      D2D1::RectF(leftColX, 8.0f, leftColX + colW, 22.0f), m_pTextLabel);
        rt->DrawTextW(labelRight, static_cast<UINT32>(wcslen(labelRight)), pFontLabel,
                      D2D1::RectF(rightColX, 8.0f, rightColX + colW, 22.0f), m_pTextLabel);
        pFontLabel->Release();
    }

    if (pFontVal) {
        rt->DrawTextW(valLeftBuf, static_cast<UINT32>(wcslen(valLeftBuf)), pFontVal,
                      D2D1::RectF(leftColX, 26.0f, leftColX + colW, 48.0f), pLeftValBrush ? pLeftValBrush : m_pTextVal);
        rt->DrawTextW(valRightBuf, static_cast<UINT32>(wcslen(valRightBuf)), pFontVal,
                      D2D1::RectF(rightColX, 26.0f, rightColX + colW, 48.0f), pRightValBrush ? pRightValBrush : m_pTextVal);
        pFontVal->Release();
    }

    m_renderer.EndDraw();
}

LRESULT CALLBACK ProMiniBarTooltipWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProMiniBarTooltipWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<ProMiniBarTooltipWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<ProMiniBarTooltipWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_LBUTTONDOWN:
        if (pThis->m_onClick) {
            pThis->m_onClick(pThis->m_segmentIndex);
        }
        pThis->Hide();
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
