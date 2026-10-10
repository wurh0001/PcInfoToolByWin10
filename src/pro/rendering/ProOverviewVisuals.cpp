/*
 * 模块说明：字体、PNG 图标和柔和阴影
 * 字体按字号/行高/字重缓存；WIC 解码嵌入 PNG；高斯模糊生成阴影再缓存。
 * 阴影采用对称计算和九宫格拉伸，避免每个动画尺寸都重新生成大位图。
 */
#include "pro/rendering/ProOverviewVisuals.h"
#include "core/diagnostics/PerformanceTrace.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <vector>

using Microsoft::WRL::ComPtr;

/**
 * 按字号、行高和字重取得缓存文字格式，首次使用时选择字体并创建。
 * @param factory 借用的文字工厂，用于创建格式或布局。
 */
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
    // 同一种格式只创建一次；map 中 ComPtr 持有引用，函数返回的是借用指针。
    // 字体格式描述字号/排版，并不意味着所有字形像素已一次性生成。
    const auto key = std::to_wstring(size) + L":" + std::to_wstring(lineHeight) + L":" + std::to_wstring(weight);
    auto& format = m_fonts[key];
    if (!format) {
        pcinfo::diagnostics::MemoryEvent("visuals.font.begin",0,m_fonts.size());
        pcinfo::diagnostics::MemoryScope memory("visuals.font.ready");
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
            // 字体度量以 designUnitsPerEm 为基准，换算到当前字号再计算文字基线。
            // 上升部和下降部居中到指定行高，避免字体切换后文字上下偏移。
            const float ascent = size * metrics.ascent / metrics.designUnitsPerEm;
            const float descent = size * metrics.descent / metrics.designUnitsPerEm;
            format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineHeight, (lineHeight - ascent - descent) * .5f + ascent);
        }
    }
    return format.Get();
}

/**
 * 设置对齐并绘制文字；box 是布局区域，size/lineHeight 使用绘图逻辑单位。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param factory 借用的文字工厂，用于创建格式或布局。
 */
void ProOverviewVisuals::Text(ID2D1RenderTarget* rt, IDWriteFactory* factory, const wchar_t* value,
    D2D1_RECT_F box, float size, float lineHeight, DWRITE_FONT_WEIGHT weight, D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align) {
    auto* format = Format(factory, size, lineHeight, weight);
    if (!format) return;
    format->SetTextAlignment(align);
    ComPtr<ID2D1SolidColorBrush> brush;
    if (SUCCEEDED(rt->CreateSolidColorBrush(color, &brush)))
        rt->DrawTextW(value, static_cast<UINT32>(wcslen(value)), format, box, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

/**
 * 通过 DirectWrite 布局测量文字宽度，包含末尾空白，失败返回 0。
 * @param factory 借用的文字工厂，用于创建格式或布局。
 */
float ProOverviewVisuals::Measure(IDWriteFactory* factory, const wchar_t* value, float size, DWRITE_FONT_WEIGHT weight) {
    auto* format = Format(factory, size, 20, weight);
    if (!format) return 0;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(value, static_cast<UINT32>(wcslen(value)), format, 1000, 40, &layout))) return 0;
    DWRITE_TEXT_METRICS metrics{}; layout->GetMetrics(&metrics);
    return metrics.widthIncludingTrailingWhitespace;
}

/**
 * 按资源 ID 解码 PNG 并缓存目标位图，再缩放绘制到 box。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param box 局部绘图矩形，与当前变换矩阵一起决定实际位置。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
bool ProOverviewVisuals::DrawIcon(ID2D1RenderTarget* rt, Icon icon, D2D1_RECT_F box) {
    auto& bitmap = m_icons[icon];
    if (!bitmap) {
        auto module = GetModuleHandleW(nullptr);
        // RCDATA 是 exe 中的原始资源字节；LockResource 返回资源地址，不需要 delete。
        // WIC 流和转换器临时存在，缓存中只保留解码后的目标位图。
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

/**
 * 生成并复用圆角高斯阴影，按偏移、透明度和目标尺寸绘制。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param box 局部绘图矩形，与当前变换矩阵一起决定实际位置。
 */
void ProOverviewVisuals::Shadow(ID2D1RenderTarget* rt, D2D1_RECT_F box, float radius,
    float blur, float dx, float dy, float opacity, unsigned rgb, float stretchHeight) {
    // 先生成高分辨率圆角遮罩，再横向/纵向分别高斯模糊，最后生成预乘 BGRA 位图。
    // 可分离卷积把二维邻域计算拆成两次一维计算，减少首次阴影生成的运算量。
    constexpr int scale = 2;
    const float sigma = blur * .5f;
    const int apron = static_cast<int>(std::ceil(3 * sigma + 1));
    const int drawWidth = static_cast<int>(std::ceil(box.right - box.left));
    int width=drawWidth;
    const int drawHeight = static_cast<int>(std::ceil(box.bottom - box.top));
    int height = static_cast<int>(std::ceil(box.bottom - box.top));
    // 高面板中间段无需重复计算圆角端部，可复用短阴影并拉伸中段。
    // Rounded shadow ends are independent of a tall panel's middle section.
    // Reuse one short mask across page heights, preserving both blurred ends.
    const float cornerBand=apron+radius+std::ceil(3*sigma)+1;
    const int reusableHeight=static_cast<int>(std::ceil(std::max(2*radius,cornerBand/.45f-2*apron)))+2;
    width=std::min(width,reusableHeight);
    if(height>reusableHeight){
        if(stretchHeight<=0)stretchHeight=box.bottom-box.top;
        height=reusableHeight;
    }
    const auto key = std::to_wstring(width) + L":" + std::to_wstring(height) + L":" + std::to_wstring(radius) +
        L":" + std::to_wstring(blur) + L":" + std::to_wstring(opacity) + L":" + std::to_wstring(rgb);
    auto& bitmap = m_shadows[key];
    if (!bitmap) {
        pcinfo::diagnostics::Scope perfScope("ProOverviewVisuals::GenerateShadow");
        const int w = (width + 2 * apron) * scale, h = (height + 2 * apron) * scale;
        std::vector<float> mask(w * h), temp(w * h), blurred(w * h);
        // 遮罩和核上下/左右对称，只计算四分之一，再镜像填入其余象限。
        // The centered mask and Gaussian kernel are symmetric on both axes.
        // Dimensions are even (2x sampling), so compute one quadrant and reflect.
        const auto reflect=[&](std::vector<float>& data,int x,int y,float value){
            data[y*w+x]=data[y*w+w-1-x]=data[(h-1-y)*w+x]=data[(h-1-y)*w+w-1-x]=value;
        };
        const float halfW = width * .5f, halfH = height * .5f;
        for (int y = 0; y < h/2; ++y) for (int x = 0; x < w/2; ++x) {
            const float qx = std::abs((x + .5f) / scale - apron - halfW) - (halfW - radius);
            const float qy = std::abs((y + .5f) / scale - apron - halfH) - (halfH - radius);
            const float distance = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - radius;
            reflect(mask,x,y,std::clamp(.5f - distance * scale, 0.0f, 1.0f));
        }
        const int extent = static_cast<int>(std::ceil(sigma * scale * 3));
        std::vector<float> kernel(2 * extent + 1); float total = 0;
        for (int k = -extent; k <= extent; ++k) { kernel[k + extent] = std::exp(-.5f * k * k / (sigma * scale * sigma * scale)); total += kernel[k + extent]; }
        // 核权重归一化为总和 1，避免模糊改变总体亮度。
        for (auto& k : kernel) k /= total;
        for (int y = 0; y < h/2; ++y) for (int x = 0; x < w/2; ++x) {
            float value=0;
            for(int k=std::max(-extent,-x);k<=std::min(extent,w-1-x);++k)value+=mask[y*w+x+k]*kernel[k+extent];
            reflect(temp,x,y,value);
        }
        for (int y = 0; y < h/2; ++y) for (int x = 0; x < w/2; ++x) {
            float value=0;
            for(int k=std::max(-extent,-y);k<=std::min(extent,h-1-y);++k)value+=temp[(y+k)*w+x]*kernel[k+extent];
            reflect(blurred,x,y,value);
        }
        std::vector<BYTE> pixels(w * h * 4);
        for (int i = 0; i < w * h; ++i) {
            // 预乘 Alpha：RGB 也乘透明度，才能与透明窗口合成约定一致。
            const float alpha = blurred[i] * opacity;
            pixels[4*i] = static_cast<BYTE>((rgb & 255) * alpha + .5f);
            pixels[4*i+1] = static_cast<BYTE>(((rgb >> 8) & 255) * alpha + .5f);
            pixels[4*i+2] = static_cast<BYTE>(((rgb >> 16) & 255) * alpha + .5f);
            pixels[4*i+3] = static_cast<BYTE>(255 * alpha + .5f);
        }
        rt->CreateBitmap(D2D1::SizeU(w, h), pixels.data(), w * 4,
            D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
    }
    if (bitmap) {
        const float left=box.left+dx-apron,top=box.top+dy-apron,right=box.left+dx+drawWidth+apron;
        if(stretchHeight>0||width!=drawWidth){
            // Preserve corner radius/blur; do not generate and cache a Gaussian
            // for every interpolated window height.
            const auto size=bitmap->GetSize();
            const float sourceHeight=height+2.f*apron,sourceWidth=width+2.f*apron;
            const float outputHeight=(stretchHeight>0?stretchHeight:drawHeight)+2*apron;
            const float bandY=std::min({sourceHeight*.45f,outputHeight*.45f,cornerBand});
            const float bandX=std::min({sourceWidth*.45f,(drawWidth+2.f*apron)*.45f,cornerBand});
            const float bottom=top+outputHeight,sx=size.width/sourceWidth,sy=size.height/sourceHeight;
            // 九宫格：四角保持原尺寸，边和中间区域拉伸；保留圆角与模糊宽度。
            const float tx[]={left,left+bandX,right-bandX,right},ty[]={top,top+bandY,bottom-bandY,bottom};
            const float ux[]={0,bandX*sx,size.width-bandX*sx,size.width},uy[]={0,bandY*sy,size.height-bandY*sy,size.height};
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)
                rt->DrawBitmap(bitmap.Get(),D2D1::RectF(tx[col],ty[row],tx[col+1],ty[row+1]),1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                    D2D1::RectF(ux[col],uy[row],ux[col+1],uy[row+1]));
        }else rt->DrawBitmap(bitmap.Get(),D2D1::RectF(left,top,right,top+height+2*apron));
    }
}

/**
 * 清理与绘图目标绑定的图标和阴影，保留可复用的文字格式。
 */
void ProOverviewVisuals::ResetDeviceResources() { m_icons.clear(); m_shadows.clear(); }

/**
 * 除设备位图外也清理字体格式，供长期隐藏后的内存回收。
 */
void ProOverviewVisuals::ReleaseDormantResources() {
    ResetDeviceResources();
    m_fonts.clear();
}

/**
 * 统计已缓存位图的像素数据估计量并写诊断日志，不等于图形库全部占用。
 */
void ProOverviewVisuals::ReportMemory() const {
    if(!pcinfo::diagnostics::GetTrace().file)return;
    size_t bytes=0,count=0;
    const auto add=[&](const auto& cache){for(const auto& item:cache)if(item.second){const auto size=item.second->GetPixelSize();bytes+=static_cast<size_t>(size.width)*size.height*4;++count;}};
    add(m_icons);add(m_shadows);
    pcinfo::diagnostics::MemoryEvent("visuals.cached-bitmaps",bytes,count);
    pcinfo::diagnostics::MemoryEvent("visuals.cached-font-formats",0,m_fonts.size());
}
