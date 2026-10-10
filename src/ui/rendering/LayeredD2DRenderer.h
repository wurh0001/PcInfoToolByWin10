/*
 * 模块说明：透明窗口渲染器接口
 * 封装 DIB、内存 DC、Direct2D 绘图目标和 DirectWrite 格式，Lite/Pro 共用。
 * BeginDraw 返回借用指针；调用方绘制完需 EndDraw，销毁前由 Cleanup 释放资源。
 */
#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

// 绘图流程：申请透明位图 → Direct2D 在位图上绘制 → Windows 把位图显示为窗口。
// 位图只是内存的一部分；字体和图形库还可能持有内部缓存，需用诊断工具测量。
class LayeredD2DRenderer {
public:
    LayeredD2DRenderer();
    ~LayeredD2DRenderer();

    /**
     * 建立本类所需的系统资源；参数指定初始化环境，失败路径由返回值通知调用方。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     */
    bool Initialize(HWND hwnd, int width, int height);
    /**
     * 成对释放本对象持有的系统与绘图资源；指针清空避免重复释放。
     */
    void Cleanup();
    /**
     * 同步绘图表面尺寸；是否复用容量由具体渲染器策略决定。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     */
    void Resize(int width, int height);
    // 在 Initialize 前启用：动画改变窗口高度时复用较大的画布，减少反复申请/释放。
    void SetSurfaceReuse(bool enabled) { m_reuseSurface = enabled; }

    /**
     * 开始画布的一帧绘制，返回借用的绘图目标；不可由调用方自行释放目标。
     */
    ID2D1DCRenderTarget* BeginDraw();
    /**
     * 结束当前绘制并把画布提交到透明窗口；与 BeginDraw 配对。
     */
    void EndDraw();

    IDWriteFactory* GetDWriteFactory() const { return m_pDWriteFactory; }
    ID2D1Factory* GetD2DFactory() const { return m_pD2DFactory; }
    ID2D1StrokeStyle* GetRoundStrokeStyle() const { return m_pRoundStrokeStyle; }

    IDWriteTextFormat* GetFormatTitle() const { return m_pFormatTitle; }
    IDWriteTextFormat* GetFormatValue() const { return m_pFormatValue; }
    IDWriteTextFormat* GetFormatLabel() const { return m_pFormatLabel; }
    IDWriteTextFormat* GetFormatSmall() const { return m_pFormatSmall; }
    IDWriteTextFormat* GetFormatTiny() const { return m_pFormatTiny; }
    IDWriteTextFormat* GetFormatBoldTiny() const { return m_pFormatBoldTiny; }

private:
    friend struct ProOverviewRenderTest;
    /**
     * 准备常用字号和字重的文字格式，后续绘制可复用而非每次重新创建。
     */
    void InitTypography();

private:
    HWND m_hwnd = nullptr;
    bool m_registeredRenderer = false;
    int m_width = 0;
    int m_height = 0;
    // m_height 是显示高度，m_surfaceHeight 是已申请画布的容量，二者可以不同。
    int m_surfaceHeight = 0;
    bool m_reuseSurface = false;

    HDC m_memDC = nullptr;
    HBITMAP m_hBitmap = nullptr;
    HBITMAP m_hOldBitmap = nullptr;
    // 指向 DIB 位图的像素；由 DeleteObject(m_hBitmap) 释放，不能直接 delete。
    void* m_pBits = nullptr;

    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    ID2D1DCRenderTarget* m_pDCRenderTarget = nullptr;
    ID2D1StrokeStyle* m_pRoundStrokeStyle = nullptr;

    IDWriteTextFormat* m_pFormatTitle = nullptr;
    IDWriteTextFormat* m_pFormatValue = nullptr;
    IDWriteTextFormat* m_pFormatLabel = nullptr;
    IDWriteTextFormat* m_pFormatSmall = nullptr;
    IDWriteTextFormat* m_pFormatTiny = nullptr;
    IDWriteTextFormat* m_pFormatBoldTiny = nullptr;
};
