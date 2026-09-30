#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

class D2DContext {
public:
    D2DContext();
    ~D2DContext();

    bool Initialize(HWND hwnd);
    void Cleanup();
    void Resize(UINT width, UINT height);

    ID2D1HwndRenderTarget* GetRenderTarget() const { return m_pRenderTarget; }
    ID2D1Factory* GetD2DFactory() const { return m_pD2DFactory; }
    IDWriteFactory* GetDWriteFactory() const { return m_pDWriteFactory; }

    IDWriteTextFormat* GetTextFormatTitle() const { return m_pTextFormatTitle; }
    IDWriteTextFormat* GetTextFormatValue() const { return m_pTextFormatValue; }
    IDWriteTextFormat* GetTextFormatLabel() const { return m_pTextFormatLabel; }
    IDWriteTextFormat* GetTextFormatRegular() const { return m_pTextFormatRegular; }
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
