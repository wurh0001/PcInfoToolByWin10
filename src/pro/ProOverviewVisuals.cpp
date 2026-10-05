#include "ProOverviewVisuals.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <vector>

using Microsoft::WRL::ComPtr;

IDWriteTextFormat* ProOverviewVisuals::Format(IDWriteFactory* factory, float size, float lineHeight, DWRITE_FONT_WEIGHT weight) {
    if (!factory) return nullptr;
    if (m_family.empty()) {
        ComPtr<IDWriteFontCollection> fonts;
        factory->GetSystemFontCollection(&fonts);
        for (auto family : {L"Noto Sans SC", L"MiSans", L"Microsoft YaHei UI", L"Segoe UI"}) {
            UINT32 index = 0; BOOL exists = FALSE;
            if (fonts) fonts->FindFamilyName(family, &index, &exists);
            if (exists) { m_family = family; break; }
        }
        if (m_family.empty()) m_family = L"Segoe UI";
    }
    const auto key = std::to_wstring(size) + L":" + std::to_wstring(lineHeight) + L":" + std::to_wstring(weight);
    auto& format = m_fonts[key];
    if (!format) {
        if (FAILED(factory->CreateTextFormat(m_family.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"zh-cn", &format))) return nullptr;
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        ComPtr<IDWriteFontCollection> collection;
        ComPtr<IDWriteFontFamily> family;
        ComPtr<IDWriteFont> font;
        UINT32 index = 0; BOOL exists = FALSE;
        factory->GetSystemFontCollection(&collection);
        if (collection) collection->FindFamilyName(m_family.c_str(), &index, &exists);
        if (exists && SUCCEEDED(collection->GetFontFamily(index, &family)) &&
            SUCCEEDED(family->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font))) {
            DWRITE_FONT_METRICS metrics{}; font->GetMetrics(&metrics);
            const float ascent = size * metrics.ascent / metrics.designUnitsPerEm;
            const float descent = size * metrics.descent / metrics.designUnitsPerEm;
            format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineHeight, (lineHeight - ascent - descent) * .5f + ascent);
        }
    }
    return format.Get();
}

void ProOverviewVisuals::Text(ID2D1RenderTarget* rt, IDWriteFactory* factory, const wchar_t* value,
    D2D1_RECT_F box, float size, float lineHeight, DWRITE_FONT_WEIGHT weight, D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align) {
    auto* format = Format(factory, size, lineHeight, weight);
    if (!format) return;
    format->SetTextAlignment(align);
    ComPtr<ID2D1SolidColorBrush> brush;
    if (SUCCEEDED(rt->CreateSolidColorBrush(color, &brush)))
        rt->DrawTextW(value, static_cast<UINT32>(wcslen(value)), format, box, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

float ProOverviewVisuals::Measure(IDWriteFactory* factory, const wchar_t* value, float size, DWRITE_FONT_WEIGHT weight) {
    auto* format = Format(factory, size, 20, weight);
    if (!format) return 0;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(value, static_cast<UINT32>(wcslen(value)), format, 1000, 40, &layout))) return 0;
    DWRITE_TEXT_METRICS metrics{}; layout->GetMetrics(&metrics);
    return metrics.widthIncludingTrailingWhitespace;
}

bool ProOverviewVisuals::DrawIcon(ID2D1RenderTarget* rt, Icon icon, D2D1_RECT_F box) {
    auto& bitmap = m_icons[icon];
    if (!bitmap) {
        auto module = GetModuleHandleW(nullptr);
        auto resource = FindResourceW(module, MAKEINTRESOURCEW(icon), RT_RCDATA);
        if (!resource) return false;
        auto loaded = LoadResource(module, resource);
        auto* bytes = static_cast<BYTE*>(LockResource(loaded));
        const DWORD length = SizeofResource(module, resource);
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(bytes, length)) ||
            FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
            FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                nullptr, 0, WICBitmapPaletteTypeCustom)) || FAILED(rt->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap))) return false;
    }
    rt->DrawBitmap(bitmap.Get(), box, 1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    return true;
}

void ProOverviewVisuals::Shadow(ID2D1RenderTarget* rt, D2D1_RECT_F box, float radius,
    float blur, float dx, float dy, float opacity, unsigned rgb) {
    // Cached separable Gaussian blur of a supersampled rounded mask, premultiplied BGRA.
    constexpr int scale = 2;
    const float sigma = blur * .5f;
    const int apron = static_cast<int>(std::ceil(3 * sigma + 1));
    const int width = static_cast<int>(std::ceil(box.right - box.left));
    const int height = static_cast<int>(std::ceil(box.bottom - box.top));
    const auto key = std::to_wstring(width) + L":" + std::to_wstring(height) + L":" + std::to_wstring(radius) +
        L":" + std::to_wstring(blur) + L":" + std::to_wstring(opacity) + L":" + std::to_wstring(rgb);
    auto& bitmap = m_shadows[key];
    if (!bitmap) {
        const int w = (width + 2 * apron) * scale, h = (height + 2 * apron) * scale;
        std::vector<float> mask(w * h), temp(w * h), blurred(w * h);
        const float halfW = width * .5f, halfH = height * .5f;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            const float qx = std::abs((x + .5f) / scale - apron - halfW) - (halfW - radius);
            const float qy = std::abs((y + .5f) / scale - apron - halfH) - (halfH - radius);
            const float distance = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - radius;
            mask[y * w + x] = std::clamp(.5f - distance * scale, 0.0f, 1.0f);
        }
        const int extent = static_cast<int>(std::ceil(sigma * scale * 3));
        std::vector<float> kernel(2 * extent + 1); float total = 0;
        for (int k = -extent; k <= extent; ++k) { kernel[k + extent] = std::exp(-.5f * k * k / (sigma * scale * sigma * scale)); total += kernel[k + extent]; }
        for (auto& k : kernel) k /= total;
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
            for (int k = -extent; k <= extent; ++k) if (x + k >= 0 && x + k < w) temp[y*w+x] += mask[y*w+x+k] * kernel[k+extent];
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
            for (int k = -extent; k <= extent; ++k) if (y + k >= 0 && y + k < h) blurred[y*w+x] += temp[(y+k)*w+x] * kernel[k+extent];
        std::vector<BYTE> pixels(w * h * 4);
        for (int i = 0; i < w * h; ++i) {
            const float alpha = blurred[i] * opacity;
            pixels[4*i] = static_cast<BYTE>((rgb & 255) * alpha + .5f);
            pixels[4*i+1] = static_cast<BYTE>(((rgb >> 8) & 255) * alpha + .5f);
            pixels[4*i+2] = static_cast<BYTE>(((rgb >> 16) & 255) * alpha + .5f);
            pixels[4*i+3] = static_cast<BYTE>(255 * alpha + .5f);
        }
        rt->CreateBitmap(D2D1::SizeU(w, h), pixels.data(), w * 4,
            D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
    }
    if (bitmap) rt->DrawBitmap(bitmap.Get(), D2D1::RectF(box.left + dx - apron, box.top + dy - apron,
        box.left + dx + width + apron, box.top + dy + height + apron));
}

void ProOverviewVisuals::ResetDeviceResources() { m_icons.clear(); m_shadows.clear(); }
