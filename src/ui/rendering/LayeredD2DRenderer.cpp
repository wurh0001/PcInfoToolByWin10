/*
 * 模块说明：DIB 与 Direct2D 资源管理
 * 共享工厂创建字体和图形资源；每个渲染器持有自己的 DC、位图与绘图目标。
 * 预乘 Alpha 的 BGRA 位图经 UpdateLayeredWindow 提交；位图容量与窗口可见高度可不同。
 * Cleanup 可重复调用；COM Release、GDI DeleteObject 和 DeleteDC 对应不同资源类型。
 */
#include "core/diagnostics/PerformanceTrace.h"
// 分层窗口 Direct2D 渲染器的声明。
#include "ui/rendering/LayeredD2DRenderer.h"
// 提供标准算法工具。
#include <algorithm>

// Direct2D：二维图形绘制；DirectWrite：文字排版；分层窗口使用下面的位图和 Windows API。
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// 统一释放 Direct2D/DirectWrite 的 COM 接口对象。
template <typename T>
/**
 * 对非空 COM 指针调用 Release 后置空；适用于接口指针，不用于普通 new 对象。
 */
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
/**
 * 准备常用字号和字重的文字格式，后续绘制可复用而非每次重新创建。
 */
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

/**
 * 建立本类所需的系统资源；参数指定初始化环境，失败路径由返回值通知调用方。
 * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
 */
bool LayeredD2DRenderer::Initialize(HWND hwnd, int width, int height) {
    pcinfo::diagnostics::MemoryEvent("renderer.initialize.begin");
    if(m_registeredRenderer) Cleanup();
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;
    m_surfaceHeight = m_reuseSurface ? ((height+127)/128)*128 : height;

    InterlockedIncrement(&s_rendererCount);
    m_registeredRenderer = true;

    if (!s_sharedD2DFactory) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &s_sharedD2DFactory);
    }
    // COM 接口采用引用计数：AddRef 表示本渲染器也持有它，Cleanup 时对应 Release。
    // 共享工厂仍被悬浮条持有时，关闭详情页不一定释放工厂内部的全部缓存。
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
    pcinfo::diagnostics::MemoryEvent("renderer.typography.ready");

    // Create Round Cap stroke style for smooth circular ring gauges
    D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND
    );
    m_pD2DFactory->CreateStrokeStyle(strokeProps, NULL, 0, &m_pRoundStrokeStyle);

    // 画布每像素 4 字节：像素宽 × 容量高 × 4，是位图数据量，不是进程总占用。
    // DPI 增大会同时增加像素宽高，因此相同 DIP 尺寸的位图占用约随缩放平方增长。
    // Create 32-bit ARGB DIB
    m_memDC = CreateCompatibleDC(NULL);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_surfaceHeight; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
    if (!m_hBitmap) return false;
    m_hOldBitmap = (HBITMAP)SelectObject(m_memDC, m_hBitmap);

    // 使用软件绘制以减少硬件绘图驱动的开销；软件绘图库自身仍会申请工作内存。
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

    pcinfo::diagnostics::MemoryEvent("renderer.initialize.ready",static_cast<size_t>(width)*m_surfaceHeight*4,1);
    return true;
}

// 尺寸超出容量才重建画布；复用模式下缩小窗口保留容量，以内存换取动画流畅度。
/**
 * 同步绘图表面尺寸；是否复用容量由具体渲染器策略决定。
 * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
 * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
 */
void LayeredD2DRenderer::Resize(int width, int height) {
    pcinfo::diagnostics::Scope perfScope("LayeredD2DRenderer::Resize");
    if (width == m_width && height == m_height) return;
    const bool sameWidth = width == m_width;
    m_width = width;
    m_height = height;
    if(m_reuseSurface && sameWidth && height<=m_surfaceHeight)return;
    m_surfaceHeight=m_reuseSurface ? ((height+127)/128)*128 : height;
    pcinfo::diagnostics::MemoryEvent("renderer.resize",static_cast<size_t>(width)*m_surfaceHeight*4,1);

    if (m_hOldBitmap && m_memDC) {
        // GDI 位图还被 DC 选中时不能直接删除，先恢复原来的位图。
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
    bmi.bmiHeader.biHeight = -m_surfaceHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
    if (m_hBitmap) {
        m_hOldBitmap = (HBITMAP)SelectObject(m_memDC, m_hBitmap);
    }
}

// 开始一帧绘制，并将 Direct2D 渲染目标绑定到内存 DC。
/**
 * 开始画布的一帧绘制，返回借用的绘图目标；不可由调用方自行释放目标。
 */
ID2D1DCRenderTarget* LayeredD2DRenderer::BeginDraw() {
    if (!m_pDCRenderTarget || !m_memDC) return nullptr;

    // 始终绑定画布容量，避免动画每帧改变绑定区域，触发绘图库内部资源调整。
    RECT rc = { 0, 0, m_width, m_surfaceHeight };
    m_pDCRenderTarget->BindDC(m_memDC, &rc);
    m_pDCRenderTarget->BeginDraw();
    m_pDCRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0.0f)); // 100% transparent clear!
    return m_pDCRenderTarget;
}

// 结束绘制，并通过 UpdateLayeredWindow 将带透明度的位图提交到屏幕。
/**
 * 结束当前绘制并把画布提交到透明窗口；与 BeginDraw 配对。
 */
void LayeredD2DRenderer::EndDraw() {
    pcinfo::diagnostics::Scope perfScope("LayeredD2DRenderer::EndDraw");
    if (!m_pDCRenderTarget || !m_memDC || !m_hwnd) return;

    m_pDCRenderTarget->EndDraw();

    HDC screenDC = GetDC(NULL);
    POINT ptSrc = { 0, 0 };
    // 提交的是实际显示尺寸，画布预留的额外高度不显示出来。
    SIZE sizeWnd = { m_width, m_height };
    BLENDFUNCTION bf = {};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA; // Per-pixel alpha

    UpdateLayeredWindow(m_hwnd, screenDC, NULL, &sizeWnd, m_memDC, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screenDC);
}

// 释放字体、渲染目标、位图和内存 DC。
/**
 * 成对释放本对象持有的系统与绘图资源；指针清空避免重复释放。
 */
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

    // 只在最后一个渲染器退出时释放共享工厂；注册标志防止重复 Cleanup 重复减计数。
    if (m_registeredRenderer && InterlockedDecrement(&s_rendererCount) == 0) {
        SafeRelease(s_sharedDWriteFactory);
        SafeRelease(s_sharedD2DFactory);
    }
    m_registeredRenderer = false;

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
    m_pBits = nullptr;
}
