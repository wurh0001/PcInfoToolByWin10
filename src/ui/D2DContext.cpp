// 项目 Direct2D/DirectWrite 绘图上下文的声明。
#include "D2DContext.h"
// 提供 std::max，用于确保渲染目标尺寸至少为 1。
#include <algorithm>

// Direct2D：二维图形绘制；DirectWrite：高质量文字排版和绘制。
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// 统一释放 COM 接口指针，避免重复编写空指针判断和 Release 调用。
template <typename T>
static void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

D2DContext::D2DContext() {}

// 对象销毁时释放 Direct2D、DirectWrite 和渲染目标资源。
D2DContext::~D2DContext() {
    Cleanup();
}

// 创建 Direct2D/DirectWrite 工厂、字体格式和普通窗口渲染目标。
bool D2DContext::Initialize(HWND hwnd) {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) return false;

    const wchar_t* fontFamily = L"Segoe UI";

    // Title: 14pt SemiBold
    m_pDWriteFactory->CreateTextFormat(
        fontFamily,
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"en-us",
        &m_pTextFormatTitle
    );

    // Value: 12.5pt SemiBold
    m_pDWriteFactory->CreateTextFormat(
        fontFamily,
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.5f,
        L"en-us",
        &m_pTextFormatValue
    );

    // Label: 10.5pt Medium
    m_pDWriteFactory->CreateTextFormat(
        fontFamily,
        NULL,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.5f,
        L"en-us",
        &m_pTextFormatLabel
    );

    // Regular: 11.5pt Regular
    m_pDWriteFactory->CreateTextFormat(
        fontFamily,
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.5f,
        L"en-us",
        &m_pTextFormatRegular
    );

    // Small: 9.5pt Regular
    m_pDWriteFactory->CreateTextFormat(
        fontFamily,
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        9.5f,
        L"en-us",
        &m_pTextFormatSmall
    );

    RECT rc;
    GetClientRect(hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        std::max(1L, rc.right - rc.left),
        std::max(1L, rc.bottom - rc.top)
    );

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();

    hr = m_pD2DFactory->CreateHwndRenderTarget(
        rtProps,
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &m_pRenderTarget
    );

    if (SUCCEEDED(hr) && m_pRenderTarget) {
        m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        m_pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        return true;
    }
    return false;
}

// 窗口大小改变时同步调整 Direct2D 渲染目标尺寸。
void D2DContext::Resize(UINT width, UINT height) {
    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }
}

// 按依赖关系释放所有图形资源。
void D2DContext::Cleanup() {
    SafeRelease(m_pTextFormatTitle);
    SafeRelease(m_pTextFormatValue);
    SafeRelease(m_pTextFormatLabel);
    SafeRelease(m_pTextFormatRegular);
    SafeRelease(m_pTextFormatSmall);
    SafeRelease(m_pRenderTarget);
    SafeRelease(m_pDWriteFactory);
    SafeRelease(m_pD2DFactory);
}
