/*
 * 模块说明：Pro 悬停提示卡绘制
 * Show 计算位置并复制快照，Render 绘制两组信息；Hide 只隐藏窗口。
 * 透明且不激活的扩展样式保证提示不抢焦点，鼠标可以继续操作悬浮条。
 */
#include "pro/windows/ProMiniBarTooltipWindow.h"
#include "ui/platform/ReadOnlyInput.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float widths[]{154,136,185,144}, offsets[]{0,44,90,216};
constexpr float primaryWidths[]{50,50,76,50}, secondaryWidths[]{76,58,76,64};
constexpr unsigned accents[]{0x266BCE,0xBC5648,0xA87517,0x138873};
const wchar_t* labelsLeft[]{L"CPU 占用",L"当前温度",L"正在使用",L"下载 MB/s"};
const wchar_t* labelsRight[]{L"有效频率",L"60 秒峰值",L"可用内存",L"上传 MB/s"};
/**
 * 将布局计算值四舍五入为整数像素，用于窗口尺寸和位置。
 */
int Pixel(float value) { return static_cast<int>(std::lround(value)); }
}
ProMiniBarTooltipWindow::ProMiniBarTooltipWindow() = default;
ProMiniBarTooltipWindow::~ProMiniBarTooltipWindow() {ReleaseBrushes();if(m_hwnd) DestroyWindow(m_hwnd);}
/**
 * 释放当前绘图目标关联的画笔和相应资源，清空指针以便下次重新创建。
 */
void ProMiniBarTooltipWindow::ReleaseBrushes() {
    auto release=[](auto*& p) {if(p) {p->Release();p=nullptr;}};
    release(m_pCardBg);release(m_pCardBorder);release(m_pSepBrush);
    m_visuals.ResetDeviceResources();
}
/**
 * 首次使用或重建目标后创建画笔，已有可用资源时复用。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 */
void ProMiniBarTooltipWindow::EnsureBrushes(ID2D1DCRenderTarget* rt) {
    if(m_pCardBg) return;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF,m_isPureWhite ? 1.f : std::clamp(m_opacity,.15f,1.f)),&m_pCardBg);
    // Page measurement: white outline; internal divider: pro-border.
    rt->CreateSolidColorBrush(D2D1::ColorF(m_isPureWhite ? 0xFFFFFF : 0xC2CBD9),&m_pCardBorder);
    rt->CreateSolidColorBrush(D2D1::ColorF(m_isPureWhite ? 0xDDE3EB : 0xC2CBD9,m_isPureWhite ? 1.f : .6f),&m_pSepBrush);
}
/**
 * 创建窗口及其必要资源；成功返回 true，失败由调用方处理，不等于开始消息循环。
 * @param owner 所属窗口，用于焦点、消息或窗口关系。
 */
bool ProMiniBarTooltipWindow::Create(HWND owner) {
    if(m_hwnd)return true;
    m_hOwner=owner;WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc=WndProc;wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"PcInfoToolProTooltipClass";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassExW(&wc);
    // Read-only peek must not activate or consume clicks.
    m_hwnd=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,
        wc.lpszClassName,L"PcInfoTool-Pro Tooltip",WS_POPUP,0,0,m_pixelWidth,m_pixelHeight,owner,nullptr,wc.hInstance,this);
    if(!m_hwnd) return false;
    DisableReadOnlyWindowIme(m_hwnd);
    return m_renderer.Initialize(m_hwnd,m_pixelWidth,m_pixelHeight);
}
// 根据主题和指标索引选择提示卡的 DIP 尺寸，再换算画布像素大小。
void ProMiniBarTooltipWindow::ConfigureSize(int segment) {
    const float legacy[]{200,184,216,224};
    m_dipWidth=m_isPureWhite ? widths[segment] : legacy[segment];m_dipHeight=m_isPureWhite ? 48.f : 56.f;
}
// bar/work 为屏幕像素矩形；提示布局尺寸按 scale 换算并限制到工作区。
POINT ProMiniBarTooltipWindow::Place(RECT bar,RECT work,float scale,float offset,float gap,float width,float height) {
    const int apron=Pixel(kShadowApron*scale),margin=Pixel(8*scale),w=Pixel(width*scale),h=Pixel(height*scale);
    int x=bar.left+Pixel(offset*scale)-apron,y=bar.bottom+Pixel(gap*scale)-apron;
    // Clamp visible geometry; transparent blur apron may extend beyond the work area.
    x=std::clamp<LONG>(x,work.left+margin-apron,std::max(work.left+margin-apron,work.right-margin-w-apron));
    if(y+apron+h>work.bottom-margin) y=bar.top-Pixel(gap*scale)-h-apron;
    y=std::clamp<LONG>(y,work.top+margin-apron,std::max(work.top+margin-apron,work.bottom-margin-h-apron));
    return {x,y};
}
// 复制当前快照，按指标选择卡片并定位；只读窗口用不激活方式显示。
void ProMiniBarTooltipWindow::Show(int segment,RECT bar,const MetricsSnapshot& snapshot,float scale) {
    if(segment<0 || segment>3) {Hide();return;}
    if(!m_hwnd&&!Create(m_hOwner))return;
    m_segmentIndex=segment;m_snapshot=snapshot;m_dpiScale=scale;ConfigureSize(segment);
    const int w=Pixel((m_dipWidth+2*kShadowApron)*scale),h=Pixel((m_dipHeight+2*kShadowApron)*scale);
    if(w!=m_pixelWidth || h!=m_pixelHeight) {m_pixelWidth=w;m_pixelHeight=h;m_renderer.Resize(w,h);ReleaseBrushes();}
    MONITORINFO info{sizeof(MONITORINFO)};
    if(!GetMonitorInfoW(MonitorFromRect(&bar,MONITOR_DEFAULTTONEAREST),&info)) return;
    const float legacyOffsets[]{0,22,70,156};
    const POINT pos=Place(bar,info.rcWork,scale,m_isPureWhite ? offsets[segment] : legacyOffsets[segment],
        m_isPureWhite ? (segment==1 ? 4.5f : 4.f) : 6.f,m_dipWidth,m_dipHeight);
    SetWindowPos(m_hwnd,m_alwaysOnTop?HWND_TOPMOST:HWND_NOTOPMOST,pos.x,pos.y,w,h,SWP_NOACTIVATE);
    m_visible=true;Render();ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);
}
/**
 * 隐藏当前窗口并更新交互状态；隐藏本身并不等于销毁所有 C++ 对象。
 */
void ProMiniBarTooltipWindow::Hide() {if(m_visible) {m_visible=false;ShowWindow(m_hwnd,SW_HIDE);}}
// 将左右两组指标写入固定 64 字符缓冲区，无数据先显示横线。
// 温度峰值来自当前历史有效点，单位换算只改变展示值；网络 MB/s 使用十进制。
void ProMiniBarTooltipWindow::FormatValues(wchar_t (&left)[64],wchar_t (&right)[64]) const {
    wcscpy_s(left,L"—");wcscpy_s(right,L"—");
    const int i=m_segmentIndex;const auto& s=m_snapshot;
    switch(i) {
    case 0:
        if(std::isfinite(s.cpuUsageTotal) && s.cpuUsageTotal>=0) swprintf_s(left,L"%.0f%%",std::clamp(s.cpuUsageTotal,0.f,100.f));
        if(std::isfinite(s.cpuFreqGHz) && s.cpuFreqGHz>0) swprintf_s(right,L"%.2f GHz",s.cpuFreqGHz);
        break;
    case 1: {
        auto temperature=[&](wchar_t (&value)[64],float c) {if(std::isfinite(c) && c>0) swprintf_s(value,m_fahrenheit ? L"%.0f°F" : L"%.0f°C",m_fahrenheit ? c*1.8f+32 : c);};
        temperature(left,s.cpuTempC);float peak=0;
        for(size_t n=0;n<s.tempHistory.Size();++n) if(std::isfinite(s.tempHistory[n]) && s.tempHistory[n]>0) peak=std::max(peak,s.tempHistory[n]);
        temperature(right,peak);break;
    }
    case 2:
        if(s.totalPhysMem>0) {const auto used=std::min(s.usedPhysMem,s.totalPhysMem);
            swprintf_s(left,L"%.1f GiB",used/1073741824.0);swprintf_s(right,L"%.1f GiB",(s.totalPhysMem-used)/1073741824.0);}
        break;
    case 3:
        swprintf_s(left,L"%.1f",s.downloadSpeedBps/1000000.0);swprintf_s(right,L"%.2f",s.uploadSpeedBps/1000000.0);break;
    }
}
// 组合卡片背景、分隔线和两组文字；文字超宽时缩小并裁剪，避免越界。
void ProMiniBarTooltipWindow::DrawWhitePeek(ID2D1DCRenderTarget* rt) {
    EnsureBrushes(rt);const int i=m_segmentIndex;if(i<0 || i>3) return;
    m_visuals.Shadow(rt,D2D1::RectF(0,0,m_dipWidth,m_dipHeight),10,12,0,4,m_isPureWhite ? .08f : .1f);
    const auto card=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,m_dipWidth-.5f,m_dipHeight-.5f),10,10);
    rt->FillRoundedRectangle(card,m_pCardBg);rt->DrawRoundedRectangle(card,m_pCardBorder,1);
    wchar_t left[64],right[64];FormatValues(left,right);
    const float x=m_isPureWhite ? 8.f : 12.f,first=m_isPureWhite ? primaryWidths[i] : (m_dipWidth-41)/2;
    const float second=m_isPureWhite ? secondaryWidths[i] : first,divider=x+first+8,next=divider+9,top=m_isPureWhite ? 4.f : 8.f;
    rt->FillRectangle(D2D1::RectF(divider,(m_dipHeight-28)/2,divider+1,(m_dipHeight+28)/2),m_pSepBrush);
    auto text=[&](const wchar_t* value,float tx,float width,float y,float size,float line,DWRITE_FONT_WEIGHT weight,unsigned color) {
        const float measured=m_visuals.Measure(m_renderer.GetDWriteFactory(),value,size,weight);
        const float fitted=measured>width ? std::max(10.f,size*width/measured) : size;
        rt->PushAxisAlignedClip(D2D1::RectF(tx,y,tx+width,y+line),D2D1_ANTIALIAS_MODE_ALIASED);
        m_visuals.Text(rt,m_renderer.GetDWriteFactory(),value,D2D1::RectF(tx,y,tx+width,y+line),fitted,line,weight,D2D1::ColorF(color));rt->PopAxisAlignedClip();
    };
    // 40-DIP content stack centered inside the 48-DIP Figma card.
    text(labelsLeft[i],x,first,top,10,14,DWRITE_FONT_WEIGHT_NORMAL,0x53657A);
    text(labelsRight[i],next,second,top,10,14,DWRITE_FONT_WEIGHT_NORMAL,0x53657A);
    text(left,x,first,top+18,16,22,DWRITE_FONT_WEIGHT_MEDIUM,accents[i]);
    text(right,next,second,top+18,16,22,DWRITE_FONT_WEIGHT_MEDIUM,i==3 ? 0x266BCE : 0x192739);
}
/**
 * 按当前状态绘制并提交一帧；数据来自已保存快照，绘图不等于重新采集硬件。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void ProMiniBarTooltipWindow::Render() {
    if(!m_hwnd || !m_visible) return;auto* rt=m_renderer.BeginDraw();if(!rt) return;
    rt->Clear(D2D1::ColorF(0,0,0,0));
    rt->SetTransform(D2D1::Matrix3x2F::Translation(kShadowApron,kShadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
    DrawWhitePeek(rt);m_renderer.EndDraw();
}
/**
 * Win32 窗口过程：按消息类型分发交互；用窗口用户数据关联 C++ 实例。
 * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
 * @param msg 窗口消息编号，决定后续分支。
 */
LRESULT CALLBACK ProMiniBarTooltipWindow::WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_NCCREATE) SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
    if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;if(msg==WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
