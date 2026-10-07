#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

class LayeredD2DRenderer {
public:
    LayeredD2DRenderer();
    ~LayeredD2DRenderer();

    bool Initialize(HWND hwnd, int width, int height);
    void Cleanup();
    void Resize(int width, int height);

    ID2D1DCRenderTarget* BeginDraw();
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
    void InitTypography();

private:
    HWND m_hwnd = nullptr;
    bool m_registeredRenderer = false;
    int m_width = 0;
    int m_height = 0;

    HDC m_memDC = nullptr;
    HBITMAP m_hBitmap = nullptr;
    HBITMAP m_hOldBitmap = nullptr;
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
