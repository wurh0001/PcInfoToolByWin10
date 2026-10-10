/*
 * 模块说明：Pro 右键菜单绘制和消息跟踪
 * 按三行菜单绘制图标、悬停和按下态；屏幕锚点转换成当前 DPI 的窗口位置。
 * Track 使用局部消息循环，仍分发系统消息；取消、失焦和退出必须停止循环并清理资源。
 */
#include "pro/windows/ProContextMenu.h"
#include "ui/platform/ReadOnlyInput.h"
#include "ui/platform/DpiHelper.h"
#include "pro/interaction/ProWindowBehavior.h"
#include <windowsx.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
// 行索引 0/1/2 是显示顺序，Commands 是交给主窗口的命令 ID，二者不要混用。
namespace { constexpr float RowTop[]={8,40,86}; constexpr int Commands[]={101,103,102}; }
/**
 * 将菜单局部坐标转换为行索引；边距和分隔区域不属于可点击行。
 */
int ProContextMenu::HitTest(float x,float y) {
    if(x<8||x>=212) return -1;
    for(int i=0;i<3;++i) if(y>=RowTop[i]&&y<RowTop[i]+32) return i;
    return -1;
}
/**
 * 按方向在有效菜单行间循环移动焦点，处理初始未选择状态。
 */
int ProContextMenu::Navigate(int current,int direction) {
    if(current<0) return direction>0?0:2;
    return (current+direction+3)%3;
}
/**
 * 绘制菜单当前状态，选择的执行动作由外层处理。
 * @param rt 借用的 Direct2D 绘图目标；调用方持有其生命周期。
 * @param factory 借用的文字工厂，用于创建格式或布局。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void ProContextMenu::Draw(ID2D1RenderTarget* rt,IDWriteFactory* factory,bool panelOpen,int highlight,int down,bool dark) {
    const auto color=[dark](unsigned light,unsigned night){return D2D1::ColorF(dark?night:light);};
    ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(color(0xffffff,0x1a1e26),&brush);
    if(!brush)return;
    const auto box=D2D1::RectF(.5f,.5f,Width-.5f,Height-.5f);
    visuals.Shadow(rt,box,10,12,0,4,.08f);
    rt->FillRoundedRectangle(D2D1::RoundedRect(box,10,10),brush.Get());
    brush->SetColor(color(0xdde3eb,0x353d4b));rt->DrawRoundedRectangle(D2D1::RoundedRect(box,10,10),brush.Get(),1);
    brush->SetColor(color(0xdde3eb,0x353d4b));rt->DrawLine({8,77.5f},{212,77.5f},brush.Get(),1);
    const wchar_t* labels[]={panelOpen?L"收起概览":L"展开概览",L"偏好设置",L"退出程序"};
    const ProOverviewVisuals::Icon icons[]={ProOverviewVisuals::MenuOverview,ProOverviewVisuals::MenuSettings,ProOverviewVisuals::MenuExit};
    for(int i=0;i<3;++i){
        if(i==highlight){brush->SetColor(i==down?color(0xdde3eb,0x35445a):color(0xedf3fb,0x29364a));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(8,RowTop[i],212,RowTop[i]+32),6,6),brush.Get());}
        visuals.DrawIcon(rt,icons[i],D2D1::RectF(16,RowTop[i]+8,32,RowTop[i]+24));
        visuals.Text(rt,factory,labels[i],D2D1::RectF(40,RowTop[i]+6,204,RowTop[i]+26),13,20,DWRITE_FONT_WEIGHT_NORMAL,
            i==highlight?(i==2?color(0xbc5648,0xe79487):color(0x266bce,0x78a9ff)):color(0x192739,0xedf2f8));
    }
}
/**
 * 按当前状态绘制并提交一帧；数据来自已保存快照，绘图不等于重新采集硬件。
 * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
 */
void ProContextMenu::Render(){
    if(auto* rt=renderer.BeginDraw()){
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        rt->SetTransform(D2D1::Matrix3x2F::Translation(Apron,Apron)*D2D1::Matrix3x2F::Scale(scale,scale));
        Draw(rt,renderer.GetDWriteFactory(),open,hovered,pressed,darkTheme);renderer.EndDraw();
    }
}
/**
 * Win32 窗口过程：按消息类型分发交互；用窗口用户数据关联 C++ 实例。
 * @param hwnd Win32 窗口句柄，不是 C++ 对象地址。
 * @param msg 窗口消息编号，决定后续分支。
 */
LRESULT CALLBACK ProContextMenu::WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto* self=reinterpret_cast<ProContextMenu*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<ProContextMenu*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(hwnd,msg,wp,lp);
    // 鼠标消息给出窗口客户区像素：除 DPI 再减阴影留白，得到菜单内容 DIP。
    const auto hit=[&]{return HitTest(GET_X_LPARAM(lp)/self->scale-Apron,GET_Y_LPARAM(lp)/self->scale-Apron);};
    switch(msg){
    // 鼠标移动消息，客户区坐标以像素给出；绘图命中前注意 DPI 和滚动偏移。
    case WM_MOUSEMOVE:{const int next=hit();if(next!=self->hovered){self->hovered=next;self->Render();}return 0;}
    // 左键按下：记录命中项或拖动起点；按下与松开是不同事件。
    case WM_LBUTTONDOWN:self->pressed=hit();if(self->pressed<0)self->tracking=false;self->Render();return 0;
    // 左键松开：完成拖动或确认点击，并结束鼠标捕获状态。
    case WM_LBUTTONUP:{const int target=hit();if(target>=0&&target==self->pressed){self->command=Commands[target];self->tracking=false;}self->pressed=-1;self->Render();return 0;}
    case WM_RBUTTONDOWN:self->tracking=false;return 0;
    // 键盘按下：wParam 为虚拟键码，处理导航/快捷键而非文字输入。
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE)self->tracking=false;
        else if(wp==VK_UP||wp==VK_DOWN){self->hovered=Navigate(self->hovered,wp==VK_DOWN?1:-1);self->Render();}
        else if(wp==VK_HOME||wp==VK_END){self->hovered=wp==VK_HOME?0:2;self->Render();}
        else if(wp==VK_RETURN&&self->hovered>=0){self->command=Commands[self->hovered];self->tracking=false;}
        return 0;
    case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)self->tracking=false;return 0;
    case WM_CAPTURECHANGED:case WM_CANCELMODE:self->tracking=false;return 0;
    case WM_NCHITTEST:return HTCLIENT;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
/**
 * 定位并显示菜单，运行局部消息循环直到选择或取消；返回命令 ID，0 表示取消。
 * @param owner 所属窗口，用于焦点、消息或窗口关系。
 */
int ProContextMenu::Track(HWND owner,POINT anchor,bool panelOpen,bool dark){
    if(tracking)return 0;
    const HWND previousFocus=GetFocus();
    const HMONITOR monitor=MonitorFromPoint(anchor,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};
    if(!GetMonitorInfoW(monitor,&info))return 0;
    scale=DpiHelper::GetScaleForWindow(owner);open=panelOpen;darkTheme=dark;hovered=pressed=-1;command=0;
    const auto px=[&](float dip){return DpiHelper::DipToPx(dip,scale);};
    const POINT origin=ProWindowBehavior::ClampVisiblePosition(anchor,{px(Width),px(Height)},info.rcWork,px(8));
    static const bool registered=[] {WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=WndProc;wc.lpszClassName=L"PcInfoProContextMenu";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);return RegisterClassExW(&wc)!=0||GetLastError()==ERROR_CLASS_ALREADY_EXISTS;}();
    if(!registered)return 0;
    window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"PcInfoProContextMenu",L"",WS_POPUP,
        origin.x-px(Apron),origin.y-px(Apron),px(Width+2*Apron),px(Height+2*Apron),owner,nullptr,GetModuleHandleW(nullptr),this);
    if(!window)return 0;
    DisableReadOnlyWindowIme(window);
    if(!renderer.Initialize(window,px(Width+2*Apron),px(Height+2*Apron))){DestroyWindow(window);window=nullptr;renderer.Cleanup();return 0;}
    tracking=true;Render();ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(window);SetCapture(window);
    // 局部消息循环让 Track 等待选择，同时仍派发其他窗口消息。
    // 如果读到 WM_QUIT，清理后重新投递，防止外层应用循环丢失退出信号。
    MSG message{};bool quit=false;int quitCode=0;
    while(tracking){
        const BOOL result=GetMessageW(&message,nullptr,0,0);
        if(result<=0){quit=result==0;quitCode=static_cast<int>(message.wParam);break;}
        TranslateMessage(&message);DispatchMessageW(&message);
        if(!IsWindow(owner))tracking=false;
    }
    tracking=false;if(GetCapture()==window)ReleaseCapture();
    DestroyWindow(window);window=nullptr;visuals.ResetDeviceResources();renderer.Cleanup();
    if(IsWindow(owner)){SetForegroundWindow(owner);if(IsWindow(previousFocus))SetFocus(previousFocus);}
    if(quit){PostQuitMessage(quitCode);return 0;}return command;
}
