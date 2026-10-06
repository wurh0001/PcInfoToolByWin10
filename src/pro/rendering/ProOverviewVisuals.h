#pragma once
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <map>
#include <string>

// 138:905 only: DIP geometry is independent of the transparent shadow apron.
namespace ProOverviewLayout {
constexpr float Width = 360, Height = 284;
constexpr float ShadowApron = 40;
constexpr float ContentX = 11, ContentWidth = 338;
constexpr float RowY(int index) { return 42.0f + index * 34.0f; }
constexpr float LabelX = 23, SparkX = 79.6667f, ValueX = 188.3333f;
constexpr float ValueRight = 312.3333f, ArrowX = 321;
}

// Pro-local resources: no changes to Lite's existing fonts or renderer.
class ProOverviewVisuals {
public:
    enum Icon { Thermometer = 201, Expand, Pin, Close, Chevron, Clock, Settings, SettingsCpu, SettingsGpu, SettingsMemory, DiskDisclosure, DiskSelected, SettingsDisclosure, SettingsGrip, SettingsToggleOn, SettingsToggleOff };
    void Text(ID2D1RenderTarget* rt, IDWriteFactory* factory, const wchar_t* value,
        D2D1_RECT_F box, float size, float lineHeight, DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING);
    float Measure(IDWriteFactory* factory, const wchar_t* value, float size, DWRITE_FONT_WEIGHT weight);
    bool DrawIcon(ID2D1RenderTarget* rt, Icon icon, D2D1_RECT_F box);
    void Shadow(ID2D1RenderTarget* rt, D2D1_RECT_F box, float radius,
        float blur, float dx, float dy, float opacity, unsigned rgb = 0x1A263D, float stretchHeight = 0);
    void ResetDeviceResources();
private:
    IDWriteTextFormat* Format(IDWriteFactory* factory, float size, float lineHeight, DWRITE_FONT_WEIGHT weight);
    std::map<std::wstring, Microsoft::WRL::ComPtr<IDWriteTextFormat>> m_fonts;
    std::map<int, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_icons;
    std::map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_shadows;
    std::wstring m_family;
};
