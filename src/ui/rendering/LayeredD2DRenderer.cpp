// 分层窗口 Direct2D 渲染器的声明。
#include "ui/rendering/LayeredD2DRenderer.h"
// 提供标准算法工具。
#include <algorithm>

// Direct2D：二维图形绘制；DirectWrite：文字排版；分层窗口使用下面的位图和 Windows API。
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// 统一释放 Direct2D/DirectWrite 的 COM 接口对象。
template <typename T>
static void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

LayeredD2DRenderer::LayeredD2DRenderer() {}

LayeredD2DRenderer::~LayeredD2DRenderer() {
    Cleanup();
}

// 创建详情窗口和监控条所需的多种文字格式。
void LayeredD2DRenderer::InitTypography() {
    if (!m_pDWriteFactory) return;

    // Use installed MiSans with fallbacks
    const wchar_t* fontFamily = L"MiSans";

    // Panel Header Title (系统运行概览): 13.5f Medium
    m_pDWriteFactory->CreateTextFormat(
        fontFamily, NULL, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 13.5f, L"zh-cn", &m_pFormatTitle
    );

    // Key Metric Values (e.g. 9.1%, 31%, 144 / 466 GB): 13.5f SemiBold (Numbers stand out)
    m_pDWriteFactory->CreateTextFormat(
        fontFamily, NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 13.5f, L"zh-cn", &m_pFormatValue
    );

    // Section Labels (e.g. CPU 总体负载, 物理内存, 本地磁盘存储, 下载/上传流速): 11.5f Regular/Normal (Slender and elegant)
    m_pDWriteFactory->CreateTextFormat(
        fontFamily, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"zh-cn", &m_pFormatLabel
    );

    // Secondary / Disk detail text (e.g. 144 / 466 GB (31%)): 11.0f Regular/Normal
    m_pDWriteFactory->CreateTextFormat(
        fontFamily, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"zh-cn", &m_pFormatSmall
    );

    // MiniBar Small Category Labels (CPU, RAM): 9.5f Regular/Normal (Clean and not heavy)
    m_pDWriteFactory->CreateTextFormat(
        fontFamily, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"zh-cn", &m_pFormatTiny
    );

    // Collapsed MiniBar Section Indicator Letters (C, M, G): 9.0f Bold / SemiBold
    m_pDWriteFactory->CreateTextFormat(
        fontFamily, NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 9.0f, L"zh-cn", &m_pFormatBoldTiny
    );
    if (m_pFormatBoldTiny) {
        m_pFormatBoldTiny->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFormatBoldTiny->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

static ID2D1Factory* s_sharedD2DFactory = nullptr;
static IDWriteFactory* s_sharedDWriteFactory = nullptr;
static LONG s_rendererCount = 0;

bool LayeredD2DRenderer::Initialize(HWND hwnd, int width, int height) {
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    InterlockedIncrement(&s_rendererCount);

    if (!s_sharedD2DFactory) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &s_sharedD2DFactory);
    }
    m_pD2DFactory = s_sharedD2DFactory;
    if (m_pD2DFactory) m_pD2DFactory->AddRef();

    if (!s_sharedDWriteFactory) {
        DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&s_sharedDWriteFactory)
        );
    }
    m_pDWriteFactory = s_sharedDWriteFactory;
    if (m_pDWriteFactory) m_pDWriteFactory->AddRef();

    if (!m_pD2DFactory || !m_pDWriteFactory) return false;

    InitTypography();

    // Create Round Cap stroke style for smooth circular ring gauges
    D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND
    );
    m_pD2DFactory->CreateStrokeStyle(strokeProps, NULL, 0, &m_pRoundStrokeStyle);

    // Create 32-bit ARGB DIB
    m_memDC = CreateCompatibleDC(NULL);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
    if (!m_hBitmap) return false;
    m_hOldBitmap = (HBITMAP)SelectObject(m_memDC, m_hBitmap);

    // Create Direct2D DC Render Target in SOFTWARE mode to avoid GPU driver footprint (e.g. 75MB NV driver)
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    HRESULT hr = m_pD2DFactory->CreateDCRenderTarget(&props, &m_pDCRenderTarget);
    if (FAILED(hr) || !m_pDCRenderTarget) {
        return false;
    }

    m_pDCRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_pDCRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    return true;
}

// 重新创建与新窗口尺寸匹配的 DIB 位图。
void LayeredD2DRenderer::Resize(int width, int height) {
    if (width == m_width && height == m_height) return;
    m_width = width;
    m_height = height;

    if (m_hOldBitmap && m_memDC) {
        SelectObject(m_memDC, m_hOldBitmap);
        m_hOldBitmap = nullptr;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
    if (m_hBitmap) {
        m_hOldBitmap = (HBITMAP)SelectObject(m_memDC, m_hBitmap);
    }
}

// 开始一帧绘制，并将 Direct2D 渲染目标绑定到内存 DC。
ID2D1DCRenderTarget* LayeredD2DRenderer::BeginDraw() {
    if (!m_pDCRenderTarget || !m_memDC) return nullptr;

    RECT rc = { 0, 0, m_width, m_height };
    m_pDCRenderTarget->BindDC(m_memDC, &rc);
    m_pDCRenderTarget->BeginDraw();
    m_pDCRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0.0f)); // 100% transparent clear!
    return m_pDCRenderTarget;
}

// 结束绘制，并通过 UpdateLayeredWindow 将带透明度的位图提交到屏幕。
void LayeredD2DRenderer::EndDraw() {
    if (!m_pDCRenderTarget || !m_memDC || !m_hwnd) return;

    m_pDCRenderTarget->EndDraw();

    HDC screenDC = GetDC(NULL);
    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { m_width, m_height };
    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA; // Per-pixel alpha

    UpdateLayeredWindow(m_hwnd, screenDC, NULL, &sizeWnd, m_memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screenDC);
}

// 释放字体、渲染目标、位图和内存 DC。
void LayeredD2DRenderer::Cleanup() {
    SafeRelease(m_pFormatBoldTiny);
    SafeRelease(m_pFormatTiny);
    SafeRelease(m_pFormatTitle);
    SafeRelease(m_pFormatValue);
    SafeRelease(m_pFormatLabel);
    SafeRelease(m_pFormatSmall);
    SafeRelease(m_pRoundStrokeStyle);
    SafeRelease(m_pDCRenderTarget);
    SafeRelease(m_pDWriteFactory);
    SafeRelease(m_pD2DFactory);

    if (InterlockedDecrement(&s_rendererCount) == 0) {
        SafeRelease(s_sharedDWriteFactory);
        SafeRelease(s_sharedD2DFactory);
    }

    if (m_hOldBitmap && m_memDC) {
        SelectObject(m_memDC, m_hOldBitmap);
        m_hOldBitmap = nullptr;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
}
