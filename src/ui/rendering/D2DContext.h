/*
 * 模块说明：保留的普通 HWND 绘图上下文
 * 使用 ID2D1HwndRenderTarget 直接画到窗口，与分层透明窗口的 DIB 路线不同。
 * 当前 SourceLists 未把此实现加入产品目标，阅读时不要把它当正在运行的渲染后端。
 */
#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

class D2DContext {
public:
    D2DContext();
    ~D2DContext();

    /**
     * 建立本类所需的系统资源；参数指定初始化环境，失败路径由返回值通知调用方。
     * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
     */
    bool Initialize(HWND hwnd);
    /**
     * 成对释放本对象持有的系统与绘图资源；指针清空避免重复释放。
     */
    void Cleanup();
    /**
     * 同步绘图表面尺寸；是否复用容量由具体渲染器策略决定。
     * @param width 目标宽度；布局绘制函数中通常是 DIP，窗口资源函数按实现使用像素。
     * @param height 目标高度，注意区分窗口像素尺寸与布局逻辑尺寸。
     */
    void Resize(UINT width, UINT height);

    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    ID2D1HwndRenderTarget* GetRenderTarget() const { return m_pRenderTarget; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    ID2D1Factory* GetD2DFactory() const { return m_pD2DFactory; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    IDWriteFactory* GetDWriteFactory() const { return m_pDWriteFactory; }

    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    IDWriteTextFormat* GetTextFormatTitle() const { return m_pTextFormatTitle; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    IDWriteTextFormat* GetTextFormatValue() const { return m_pTextFormatValue; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    IDWriteTextFormat* GetTextFormatLabel() const { return m_pTextFormatLabel; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    IDWriteTextFormat* GetTextFormatRegular() const { return m_pTextFormatRegular; }
    // 返回借用指针/句柄，不转移所有权，也不为调用方增加引用。
    IDWriteTextFormat* GetTextFormatSmall() const { return m_pTextFormatSmall; }

private:
    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

    IDWriteTextFormat* m_pTextFormatTitle = nullptr;
    IDWriteTextFormat* m_pTextFormatValue = nullptr;
    IDWriteTextFormat* m_pTextFormatLabel = nullptr;
    IDWriteTextFormat* m_pTextFormatRegular = nullptr;
    IDWriteTextFormat* m_pTextFormatSmall = nullptr;
};
