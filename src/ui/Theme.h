#pragma once
#include <d2d1.h>
#include <string>
#include <algorithm>
#include <cstdint>

namespace Theme {

    enum class ThemePreset {
        TranslucentGlass,// 雾透冰晶 🫧 (全透磨砂毛玻璃·水波荧光·极光冰雾)
        GlacierWhite,    // 冰川凝白 ❄️ (新亮色：超透白月光玻璃、柔光青蓝与霓紫)
        ObsidianDark,    // 深邃暗夜 🌙 (暗色毛玻璃、黑曜石质感)
        AuroraGlass,     // 极光晨曦 🔮 (幻彩极光流光渐变)
        CyberNeon        // 赛博赛道 ⚡ (硬核科技霓虹、纯黑暗夜高反差)
    };

    struct Palette {
        D2D1_COLOR_F WindowBg;
        D2D1_COLOR_F WindowBorder;
        D2D1_COLOR_F CardBg;
        D2D1_COLOR_F CardBorder;
        D2D1_COLOR_F PillBg;
        D2D1_COLOR_F PillBorder;
        D2D1_COLOR_F TextPrimary;
        D2D1_COLOR_F TextSecondary;
        D2D1_COLOR_F TextMuted;
        D2D1_COLOR_F CpuMint1;
        D2D1_COLOR_F CpuMint2;
        D2D1_COLOR_F CpuBlue;
        D2D1_COLOR_F CpuArea;
        D2D1_COLOR_F RamIndigo;
        D2D1_COLOR_F RamPurple;
        D2D1_COLOR_F GpuCyan;
        D2D1_COLOR_F GpuBlue;
        D2D1_COLOR_F NetDownGreen;
        D2D1_COLOR_F NetUpBlue;
        D2D1_COLOR_F DiskAmber;
        D2D1_COLOR_F TrackBg;
        D2D1_COLOR_F TrackBorder;
        D2D1_COLOR_F DividerLine;
        D2D1_COLOR_F ChevronBtnBg;
        D2D1_COLOR_F ChevronBtnBorder;
        D2D1_COLOR_F ChevronIcon;
        D2D1_COLOR_F GridLine;
        bool isLight = false;
        bool isTranslucent = false; // 是否启用超高透磨砂冰雾质感
    };

    inline Palette GetPresetPalette(ThemePreset preset) {
        switch (preset) {
        case ThemePreset::TranslucentGlass: {
            // 雾透冰晶 🫧 (极高透明度冷白磨砂毛玻璃，微发光白边，原版纯正质感)
            Palette p;
            p.WindowBg       = D2D1::ColorF(0.35f, 0.42f, 0.52f, 0.28f);   // 28% 超高透明度冷雾蓝黑底
            p.WindowBorder   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.75f);     // 75% 极光冰白明亮边缘光晕
            p.CardBg         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.05f);     // 5% 超透次级卡片底
            p.CardBorder     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.20f);     // 20% 细致半透明白色卡片轮廓
            p.PillBg         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);
            p.PillBorder     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.22f);
            p.TextPrimary    = D2D1::ColorF(0.96f, 0.98f, 1.0f, 0.98f);   // 纯洁晶莹白字
            p.TextSecondary  = D2D1::ColorF(0.75f, 0.82f, 0.92f, 0.90f);   // 冰霜冰蓝副文本
            p.TextMuted      = D2D1::ColorF(0.55f, 0.65f, 0.78f, 0.80f);
            p.CpuMint1       = D2D1::ColorF(0.20f, 0.92f, 0.80f, 1.0f);   // 亮眼荧光青绿 (#33ebd0)
            p.CpuMint2       = D2D1::ColorF(0.30f, 0.95f, 0.88f, 1.0f);
            p.CpuBlue        = D2D1::ColorF(0.30f, 0.95f, 0.88f, 1.0f);
            p.CpuArea        = D2D1::ColorF(0.20f, 0.92f, 0.80f, 0.15f);  // 极淡发光曲面
            p.RamIndigo      = D2D1::ColorF(0.70f, 0.45f, 0.98f, 1.0f);   // 荧光柔紫
            p.RamPurple      = D2D1::ColorF(0.85f, 0.52f, 0.98f, 1.0f);   // 电光兰花紫
            p.GpuCyan        = D2D1::ColorF(0.20f, 0.65f, 1.0f, 1.0f);    // 亮荧光天蓝
            p.GpuBlue        = D2D1::ColorF(0.35f, 0.78f, 1.0f, 1.0f);    // 极光晴空蓝
            p.NetDownGreen   = D2D1::ColorF(0.25f, 0.88f, 0.80f, 1.0f);   // 冰青色下载箭头
            p.NetUpBlue      = D2D1::ColorF(0.45f, 0.68f, 1.0f, 1.0f);    // 天蓝色上传箭头
            p.DiskAmber      = D2D1::ColorF(0.98f, 0.62f, 0.22f, 1.0f);   // 暖金琥珀
            p.TrackBg        = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.18f);     // 凹槽阴影感
            p.TrackBorder    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f);
            p.DividerLine    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f);     // 微光分割线
            p.ChevronBtnBg   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f);     // 晶透圆钮
            p.ChevronBtnBorder=D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.40f);
            p.ChevronIcon    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f);
            p.GridLine       = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);     // 纤细虚线网格
            p.isLight        = false;
            p.isTranslucent  = true;
            return p;
        }
        case ThemePreset::GlacierWhite: {
            // 冰川凝白 ❄️ (新高透纯净亮色，完美对齐用户最新 UI 视觉)
            Palette p;
            p.WindowBg       = D2D1::ColorF(0.95f, 0.97f, 1.0f, 0.94f);   // 纯净凝白毛玻璃
            p.WindowBorder   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f);    // 亮面微光外描边
            p.CardBg         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.90f);    // 纯白浮雕卡片
            p.CardBorder     = D2D1::ColorF(0.82f, 0.88f, 0.95f, 0.65f); // 极其柔和的浅蓝卡片描边
            p.PillBg         = D2D1::ColorF(0.90f, 0.94f, 0.98f, 0.90f); // 胶囊底色
            p.PillBorder     = D2D1::ColorF(0.80f, 0.86f, 0.94f, 0.50f);
            p.TextPrimary    = D2D1::ColorF(0.12f, 0.16f, 0.22f, 1.0f);  // 质感藏青深灰
            p.TextSecondary  = D2D1::ColorF(0.38f, 0.44f, 0.54f, 1.0f);  // 柔和副文本
            p.TextMuted      = D2D1::ColorF(0.55f, 0.62f, 0.72f, 1.0f);
            p.CpuMint1       = D2D1::ColorF(0.05f, 0.72f, 0.52f, 1.0f);  // 青翠绿
            p.CpuMint2       = D2D1::ColorF(0.12f, 0.82f, 0.68f, 1.0f);  // 薄荷青
            p.CpuBlue        = D2D1::ColorF(0.12f, 0.82f, 0.68f, 1.0f);
            p.CpuArea        = D2D1::ColorF(0.08f, 0.78f, 0.65f, 0.22f); // 折线渐变浅波澜
            p.RamIndigo      = D2D1::ColorF(0.52f, 0.38f, 0.98f, 1.0f);  // 柔美电光紫
            p.RamPurple      = D2D1::ColorF(0.68f, 0.42f, 0.98f, 1.0f);  // 梦幻紫
            p.GpuCyan        = D2D1::ColorF(0.15f, 0.58f, 0.98f, 1.0f);  // 蔚蓝
            p.GpuBlue        = D2D1::ColorF(0.25f, 0.70f, 1.0f, 1.0f);
            p.NetDownGreen   = D2D1::ColorF(0.05f, 0.70f, 0.48f, 1.0f);  // 翡翠绿
            p.NetUpBlue      = D2D1::ColorF(0.35f, 0.48f, 0.98f, 1.0f);  // 晴空蓝
            p.DiskAmber      = D2D1::ColorF(0.98f, 0.55f, 0.20f, 1.0f);  // 暖橙琥珀
            p.TrackBg        = D2D1::ColorF(0.90f, 0.93f, 0.97f, 0.95f); // 干净轨道底色
            p.TrackBorder    = D2D1::ColorF(0.82f, 0.87f, 0.94f, 0.60f);
            p.DividerLine    = D2D1::ColorF(0.80f, 0.85f, 0.92f, 0.65f); // 分割线
            p.ChevronBtnBg   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.90f);    // 展开圆钮白背景
            p.ChevronBtnBorder=D2D1::ColorF(0.80f, 0.86f, 0.94f, 0.80f);
            p.ChevronIcon    = D2D1::ColorF(0.25f, 0.32f, 0.42f, 1.0f);
            p.GridLine       = D2D1::ColorF(0.85f, 0.89f, 0.94f, 0.60f);
            p.isLight        = true;
            return p;
        }

        case ThemePreset::AuroraGlass: {
            // 极光晨曦 🔮 (带晨曦紫与冰蓝渐变微光，通透亮色)
            Palette p;
            p.WindowBg       = D2D1::ColorF(0.96f, 0.96f, 0.99f, 0.95f);
            p.WindowBorder   = D2D1::ColorF(0.85f, 0.82f, 0.98f, 0.80f);
            p.CardBg         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.92f);
            p.CardBorder     = D2D1::ColorF(0.88f, 0.85f, 0.96f, 0.70f);
            p.PillBg         = D2D1::ColorF(0.93f, 0.91f, 0.98f, 0.85f);
            p.PillBorder     = D2D1::ColorF(0.85f, 0.80f, 0.95f, 0.60f);
            p.TextPrimary    = D2D1::ColorF(0.14f, 0.12f, 0.24f, 1.0f);
            p.TextSecondary  = D2D1::ColorF(0.42f, 0.38f, 0.55f, 1.0f);
            p.TextMuted      = D2D1::ColorF(0.60f, 0.56f, 0.72f, 1.0f);
            p.CpuMint1       = D2D1::ColorF(0.08f, 0.75f, 0.65f, 1.0f);
            p.CpuMint2       = D2D1::ColorF(0.20f, 0.85f, 0.78f, 1.0f);
            p.CpuBlue        = D2D1::ColorF(0.20f, 0.85f, 0.78f, 1.0f);
            p.CpuArea        = D2D1::ColorF(0.18f, 0.82f, 0.75f, 0.25f);
            p.RamIndigo      = D2D1::ColorF(0.62f, 0.35f, 0.98f, 1.0f);
            p.RamPurple      = D2D1::ColorF(0.78f, 0.45f, 0.99f, 1.0f);
            p.GpuCyan        = D2D1::ColorF(0.20f, 0.60f, 1.0f, 1.0f);
            p.GpuBlue        = D2D1::ColorF(0.35f, 0.75f, 1.0f, 1.0f);
            p.NetDownGreen   = D2D1::ColorF(0.10f, 0.75f, 0.55f, 1.0f);
            p.NetUpBlue      = D2D1::ColorF(0.45f, 0.48f, 0.98f, 1.0f);
            p.DiskAmber      = D2D1::ColorF(0.98f, 0.58f, 0.28f, 1.0f);
            p.TrackBg        = D2D1::ColorF(0.91f, 0.89f, 0.96f, 0.95f);
            p.TrackBorder    = D2D1::ColorF(0.85f, 0.80f, 0.94f, 0.60f);
            p.DividerLine    = D2D1::ColorF(0.86f, 0.82f, 0.94f, 0.65f);
            p.ChevronBtnBg   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.92f);
            p.ChevronBtnBorder=D2D1::ColorF(0.85f, 0.80f, 0.95f, 0.80f);
            p.ChevronIcon    = D2D1::ColorF(0.35f, 0.28f, 0.52f, 1.0f);
            p.GridLine       = D2D1::ColorF(0.88f, 0.84f, 0.95f, 0.60f);
            p.isLight        = true;
            return p;
        }

        case ThemePreset::CyberNeon: {
            // 赛博赛道 ⚡ (深黑背景、高饱和度青绿粉紫霓虹)
            Palette p;
            p.WindowBg       = D2D1::ColorF(0.04f, 0.05f, 0.08f, 0.96f);
            p.WindowBorder   = D2D1::ColorF(0.0f, 0.95f, 0.85f, 0.45f); // 霓虹青边缘外发光
            p.CardBg         = D2D1::ColorF(0.08f, 0.10f, 0.15f, 0.85f);
            p.CardBorder     = D2D1::ColorF(0.0f, 0.80f, 0.85f, 0.25f);
            p.PillBg         = D2D1::ColorF(0.10f, 0.13f, 0.20f, 0.70f);
            p.PillBorder     = D2D1::ColorF(0.0f, 0.90f, 0.85f, 0.35f);
            p.TextPrimary    = D2D1::ColorF(0.98f, 1.0f, 1.0f, 1.0f);
            p.TextSecondary  = D2D1::ColorF(0.50f, 0.65f, 0.80f, 1.0f);
            p.TextMuted      = D2D1::ColorF(0.35f, 0.48f, 0.62f, 1.0f);
            p.CpuMint1       = D2D1::ColorF(0.0f, 1.0f, 0.65f, 1.0f);   // 极致荧光绿
            p.CpuMint2       = D2D1::ColorF(0.0f, 0.92f, 0.98f, 1.0f);  // 荧光青
            p.CpuBlue        = D2D1::ColorF(0.0f, 0.92f, 0.98f, 1.0f);
            p.CpuArea        = D2D1::ColorF(0.0f, 0.92f, 0.98f, 0.32f);
            p.RamIndigo      = D2D1::ColorF(0.70f, 0.20f, 1.0f, 1.0f);  // 赛博霓虹紫
            p.RamPurple      = D2D1::ColorF(0.95f, 0.25f, 0.85f, 1.0f); // 霓虹粉红
            p.GpuCyan        = D2D1::ColorF(0.0f, 0.75f, 1.0f, 1.0f);   // 赛博天蓝
            p.GpuBlue        = D2D1::ColorF(0.20f, 0.90f, 1.0f, 1.0f);
            p.NetDownGreen   = D2D1::ColorF(0.0f, 1.0f, 0.70f, 1.0f);
            p.NetUpBlue      = D2D1::ColorF(0.20f, 0.65f, 1.0f, 1.0f);
            p.DiskAmber      = D2D1::ColorF(1.0f, 0.60f, 0.0f, 1.0f);
            p.TrackBg        = D2D1::ColorF(0.09f, 0.12f, 0.18f, 0.95f);
            p.TrackBorder    = D2D1::ColorF(0.0f, 0.85f, 0.85f, 0.15f);
            p.DividerLine    = D2D1::ColorF(0.0f, 0.85f, 0.85f, 0.20f);
            p.ChevronBtnBg   = D2D1::ColorF(0.10f, 0.14f, 0.22f, 0.95f);
            p.ChevronBtnBorder=D2D1::ColorF(0.0f, 0.90f, 0.85f, 0.40f);
            p.ChevronIcon    = D2D1::ColorF(0.0f, 0.95f, 0.90f, 1.0f);
            p.GridLine       = D2D1::ColorF(0.0f, 0.85f, 0.85f, 0.12f);
            p.isLight        = false;
            return p;
        }

        case ThemePreset::ObsidianDark:
        default: {
            // 深邃暗夜 🌙 (黑曜石磨砂半透经典深色)
            Palette p;
            p.WindowBg       = D2D1::ColorF(0.09f, 0.11f, 0.15f, 0.94f);
            p.WindowBorder   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f);
            p.CardBg         = D2D1::ColorF(0.12f, 0.15f, 0.20f, 0.70f);
            p.CardBorder     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);
            p.PillBg         = D2D1::ColorF(0.14f, 0.17f, 0.22f, 0.60f);
            p.PillBorder     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f);
            p.TextPrimary    = D2D1::ColorF(0.96f, 0.97f, 0.99f, 1.0f);
            p.TextSecondary  = D2D1::ColorF(0.48f, 0.54f, 0.64f, 1.0f);
            p.TextMuted      = D2D1::ColorF(0.38f, 0.44f, 0.52f, 1.0f);
            p.CpuMint1       = D2D1::ColorF(0.06f, 0.72f, 0.51f, 1.0f);
            p.CpuMint2       = D2D1::ColorF(0.18f, 0.83f, 0.75f, 1.0f);
            p.CpuBlue        = D2D1::ColorF(0.18f, 0.83f, 0.75f, 1.0f);
            p.CpuArea        = D2D1::ColorF(0.18f, 0.83f, 0.75f, 0.28f);
            p.RamIndigo      = D2D1::ColorF(0.39f, 0.40f, 0.95f, 1.0f);
            p.RamPurple      = D2D1::ColorF(0.75f, 0.52f, 0.99f, 1.0f);
            p.GpuCyan        = D2D1::ColorF(0.20f, 0.60f, 0.98f, 1.0f);
            p.GpuBlue        = D2D1::ColorF(0.35f, 0.75f, 1.0f, 1.0f);
            p.NetDownGreen   = D2D1::ColorF(0.20f, 0.83f, 0.60f, 1.0f);
            p.NetUpBlue      = D2D1::ColorF(0.50f, 0.55f, 0.98f, 1.0f);
            p.DiskAmber      = D2D1::ColorF(0.98f, 0.57f, 0.24f, 1.0f);
            p.TrackBg        = D2D1::ColorF(0.13f, 0.16f, 0.21f, 0.95f);
            p.TrackBorder    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.05f);
            p.DividerLine    = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);
            p.ChevronBtnBg   = D2D1::ColorF(0.14f, 0.17f, 0.22f, 0.95f);
            p.ChevronBtnBorder=D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f);
            p.ChevronIcon    = D2D1::ColorF(0.85f, 0.88f, 0.94f, 1.0f);
            p.GridLine       = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f);
            p.isLight        = false;
            return p;
        }
        }
    }

    inline Palette GetPalette(bool lightMode) {
        return GetPresetPalette(lightMode ? ThemePreset::GlacierWhite : ThemePreset::ObsidianDark);
    }

    // Dynamic CPU status color
    inline D2D1_COLOR_F GetCpuStatusColor(float cpuPercent) {
        float t = std::clamp(cpuPercent / 100.0f, 0.0f, 1.0f);
        if (t < 0.45f) {
            // 0% ~ 45%: Cool Mint Green (#10b981) to Mint Cyan (#2dd4bf)
            float factor = t / 0.45f;
            float r = 0.06f + factor * (0.18f - 0.06f);
            float g = 0.72f + factor * (0.83f - 0.72f);
            float b = 0.51f + factor * (0.75f - 0.51f);
            return D2D1::ColorF(r, g, b, 1.0f);
        } else if (t < 0.75f) {
            // 45% ~ 75%: Cyan to Warm Golden Amber (#f59e0b)
            float factor = (t - 0.45f) / 0.30f;
            float r = 0.18f + factor * (0.96f - 0.18f);
            float g = 0.83f - factor * (0.83f - 0.62f);
            float b = 0.75f - factor * (0.75f - 0.04f);
            return D2D1::ColorF(r, g, b, 1.0f);
        } else {
            // 75% ~ 100%: Amber to Coral Crimson (#ef4444)
            float factor = (t - 0.75f) / 0.25f;
            float r = 0.96f - factor * (0.96f - 0.94f);
            float g = 0.62f - factor * (0.62f - 0.27f);
            float b = 0.04f + factor * (0.27f - 0.04f);
            return D2D1::ColorF(r, g, b, 1.0f);
        }
    }

    // Backward-compatible static palette access
    static const D2D1_COLOR_F WindowBg       = D2D1::ColorF(0.09f, 0.11f, 0.15f, 0.94f);
    static const D2D1_COLOR_F WindowBorder   = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f);
    static const D2D1_COLOR_F CardBg         = D2D1::ColorF(0.12f, 0.15f, 0.20f, 0.70f);
    static const D2D1_COLOR_F CardBorder     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f);
    static const D2D1_COLOR_F PillBg         = D2D1::ColorF(0.14f, 0.17f, 0.22f, 0.60f);
    static const D2D1_COLOR_F PillBorder     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f);
    static const D2D1_COLOR_F TextPrimary    = D2D1::ColorF(0.96f, 0.97f, 0.99f, 1.0f);
    static const D2D1_COLOR_F TextSecondary  = D2D1::ColorF(0.48f, 0.54f, 0.64f, 1.0f);
    static const D2D1_COLOR_F TextMuted      = D2D1::ColorF(0.38f, 0.44f, 0.52f, 1.0f);
    static const D2D1_COLOR_F CpuBlue        = D2D1::ColorF(0.18f, 0.83f, 0.75f, 1.0f);
    static const D2D1_COLOR_F CpuArea        = D2D1::ColorF(0.18f, 0.83f, 0.75f, 0.28f);
    static const D2D1_COLOR_F RamPurple      = D2D1::ColorF(0.75f, 0.52f, 0.99f, 1.0f);
    static const D2D1_COLOR_F NetDownGreen   = D2D1::ColorF(0.20f, 0.83f, 0.60f, 1.0f);
    static const D2D1_COLOR_F NetUpBlue      = D2D1::ColorF(0.50f, 0.55f, 0.98f, 1.0f);
    static const D2D1_COLOR_F DiskAmber      = D2D1::ColorF(0.98f, 0.57f, 0.24f, 1.0f);
    static const D2D1_COLOR_F TrackBg        = D2D1::ColorF(0.13f, 0.16f, 0.21f, 0.95f);
    static const D2D1_COLOR_F GridLine       = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f);

}

inline std::wstring FormatSpeed(uint64_t bytesPerSec) {
    wchar_t buf[64];
    double bps = static_cast<double>(bytesPerSec);
    if (bps < 1024.0) {
        swprintf_s(buf, L"%.0f B/s", bps);
    } else if (bps < 1024.0 * 1024.0) {
        swprintf_s(buf, L"%.1f K/s", bps / 1024.0);
    } else if (bps < 1024.0 * 1024.0 * 1024.0) {
        swprintf_s(buf, L"%.1f M/s", bps / (1024.0 * 1024.0));
    } else {
        swprintf_s(buf, L"%.2f G/s", bps / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

inline std::wstring FormatBytes(uint64_t bytes) {
    wchar_t buf[64];
    double b = static_cast<double>(bytes);
    double gb = b / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1000.0) {
        swprintf_s(buf, L"%.1f TB", gb / 1024.0);
    } else {
        swprintf_s(buf, L"%.0f GB", gb);
    }
    return buf;
}
