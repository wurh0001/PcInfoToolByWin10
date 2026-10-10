/*
 * 模块说明：Pro 图文与阴影资源封装
 * 缓存字体格式、图标位图和阴影位图，供概览、悬浮条和菜单使用。
 * ComPtr 负责接口引用释放；图标和阴影属于绘图目标，目标重建时需要清理。
 */
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
    enum Icon { Thermometer = 201, Expand, Pin, Close, Chevron, Clock, Settings, SettingsCpu, SettingsGpu, SettingsMemory, DiskDisclosure, DiskSelected, SettingsDisclosure, SettingsGrip, SettingsToggleOn, SettingsToggleOff, MenuOverview, MenuSettings, MenuExit };
    /**
     * 设置对齐并绘制文字；box 是布局区域，size/lineHeight 使用绘图逻辑单位。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param factory 借用的文字工厂，用于创建格式或布局。
     */
    void Text(ID2D1RenderTarget* rt, IDWriteFactory* factory, const wchar_t* value,
        D2D1_RECT_F box, float size, float lineHeight, DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING);
    /**
     * 通过 DirectWrite 布局测量文字宽度，包含末尾空白，失败返回 0。
     * @param factory 借用的文字工厂，用于创建格式或布局。
     */
    float Measure(IDWriteFactory* factory, const wchar_t* value, float size, DWRITE_FONT_WEIGHT weight);
    /**
     * 按资源 ID 解码 PNG 并缓存目标位图，再缩放绘制到 box。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param box 局部绘图矩形，与当前变换矩阵一起决定实际位置。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    bool DrawIcon(ID2D1RenderTarget* rt, Icon icon, D2D1_RECT_F box);
    /**
     * 生成并复用圆角高斯阴影，按偏移、透明度和目标尺寸绘制。
     * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
     * @param box 局部绘图矩形，与当前变换矩阵一起决定实际位置。
     */
    void Shadow(ID2D1RenderTarget* rt, D2D1_RECT_F box, float radius,
        float blur, float dx, float dy, float opacity, unsigned rgb = 0x1A263D, float stretchHeight = 0);
    /**
     * 清理与绘图目标绑定的图标和阴影，保留可复用的文字格式。
     */
    void ResetDeviceResources();
    /**
     * 除设备位图外也清理字体格式，供长期隐藏后的内存回收。
     */
    void ReleaseDormantResources();
    /**
     * 统计已缓存位图的像素数据估计量并写诊断日志，不等于图形库全部占用。
     */
    void ReportMemory() const;
private:
    friend struct ProOverviewRenderTest;
    /**
     * 按字号、行高和字重取得缓存文字格式，首次使用时选择字体并创建。
     * @param factory 借用的文字工厂，用于创建格式或布局。
     */
    IDWriteTextFormat* Format(IDWriteFactory* factory, float size, float lineHeight, DWRITE_FONT_WEIGHT weight);
    std::map<std::wstring, Microsoft::WRL::ComPtr<IDWriteTextFormat>> m_fonts;
    std::map<int, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_icons;
    std::map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_shadows;
    std::wstring m_family;
};
