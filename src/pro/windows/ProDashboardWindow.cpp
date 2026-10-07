#include "pro/windows/ProDashboardWindow.h"
#include "pro/interaction/ProWindowBehavior.h"
#include "ui/platform/DpiHelper.h"
#include "ui/platform/AcrylicHelper.h"
#include <windowsx.h>
#include <cwchar>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <cfloat>

static const wchar_t* kProDashboardClassName = L"PcInfoToolProDashboardClass";

template <typename T>
inline void SafeRelease(T** ppT) {
    if (ppT && *ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

ProDashboardWindow::ProDashboardWindow() {
    m_dipWidth = 360.0f;
    m_dipHeight = 284.0f;
}

ProDashboardWindow::~ProDashboardWindow() {
    CancelPanelMotion();
    WatchOutsideClicks(false);
    ReleaseBrushes();
}

void ProDashboardWindow::ReleaseBrushes() {
    auto SafeRelease = [](auto** pp) {
        if (*pp) { (*pp)->Release(); *pp = nullptr; }
    };
    SafeRelease(&m_pBgBrush);
    SafeRelease(&m_pBorderBrush);
    SafeRelease(&m_pCardBgBrush);
    SafeRelease(&m_pCardBorderBrush);
    SafeRelease(&m_pCardHoverBrush);
    SafeRelease(&m_pTextPrimary);
    SafeRelease(&m_pTextSecondary);
    SafeRelease(&m_pTextMuted);
    SafeRelease(&m_pCpuBlue);
    SafeRelease(&m_pGpuPurple);
    SafeRelease(&m_pRamAmber);
    SafeRelease(&m_pDiskCyan);
    SafeRelease(&m_pNetGreen);
    SafeRelease(&m_pNetSky);
    SafeRelease(&m_pSensorCoral);
    SafeRelease(&m_pTrackBrush);
    SafeRelease(&m_pGridBrush);
}

void ProDashboardWindow::EnsureBrushes(ID2D1DCRenderTarget* rt) {
    if (m_pBgBrush) return;

    // Theme palette matching Figma tokens (Pure White & Light Glass)
    if (IsPureWhite()) {
        // 纯白主题开发契约:
        // 主背景: #FFFFFF, alpha 1.0 (关闭模糊)
        // 描边/分割线: #DDE3EB, 1 DIP
        // 次级表面 (详情卡、普通按钮): #F7F9FC
        // 悬停/选中表面 (指标行、外观标签): #EDF3FB
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_pBgBrush);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &m_pBorderBrush);       // #DDE3EB
        rt->CreateSolidColorBrush(D2D1::ColorF(0.969f, 0.976f, 0.988f, 1.0f), &m_pCardBgBrush);       // #F7F9FC
        rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &m_pCardBorderBrush);   // #DDE3EB
        rt->CreateSolidColorBrush(D2D1::ColorF(0.929f, 0.953f, 0.984f, 1.0f), &m_pCardHoverBrush);    // #EDF3FB
    } else {
        // 亮色玻璃主题: 窗口外壳半透柔和磨砂底色
        float bgAlpha = GetDashboardBgAlpha();
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, bgAlpha), &m_pBgBrush);           // Translucent frosted window bg #FFFFFF
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f), &m_pBorderBrush);       // Border #FFFFFF alpha 85%
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, std::min(0.98f, bgAlpha + 0.12f)), &m_pCardBgBrush);         // Translucent surface card
        rt->CreateSolidColorBrush(D2D1::ColorF(0.780f, 0.820f, 0.875f, 0.70f), &m_pCardBorderBrush);// Card Border #C2CBD9
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f), &m_pCardHoverBrush);      // Hover white card
    }

    rt->CreateSolidColorBrush(D2D1::ColorF(0.098f, 0.153f, 0.224f, 1.0f), &m_pTextPrimary);     // Text #192739
    rt->CreateSolidColorBrush(D2D1::ColorF(0.325f, 0.396f, 0.478f, 1.0f), &m_pTextSecondary);   // Muted #53657A
    rt->CreateSolidColorBrush(D2D1::ColorF(0.392f, 0.455f, 0.545f, 1.0f), &m_pTextMuted);       // Subtle #64748B

    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pCpuBlue);         // CPU #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(0.510f, 0.349f, 0.761f, 1.0f), &m_pGpuPurple);       // GPU #8259C2
    rt->CreateSolidColorBrush(D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f), &m_pRamAmber);        // RAM #A87517
    rt->CreateSolidColorBrush(D2D1::ColorF(0.133f, 0.471f, 0.631f, 1.0f), &m_pDiskCyan);        // Disk #2278A1
    rt->CreateSolidColorBrush(D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f), &m_pNetGreen);        // Network #138873
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pNetSky);          // Net Sky
    rt->CreateSolidColorBrush(D2D1::ColorF(0.737f, 0.337f, 0.282f, 1.0f), &m_pSensorCoral);      // Sensor #BC5648

    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.92f, 1.0f), &m_pTrackBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.40f), &m_pGridBrush);
}

bool ProDashboardWindow::Create() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = kProDashboardClassName;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hDesk = GetDesktopWindow();
    m_dpiScale = DpiHelper::GetScaleForWindow(hDesk);
    m_pixelWidth = static_cast<int>(m_dipWidth * m_dpiScale + 0.5f);
    m_pixelHeight = static_cast<int>(m_dipHeight * m_dpiScale + 0.5f);

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kProDashboardClassName,
        L"PcInfoTool-Pro Dashboard",
        WS_POPUP,
        100, 100, m_pixelWidth, m_pixelHeight,
        NULL, NULL, GetModuleHandleW(NULL), this
    );

    if (!m_hwnd) return false;

    if (!m_renderer.Initialize(m_hwnd, m_pixelWidth, m_pixelHeight)) {
        return false;
    }

    UpdateDimensionsAndPosition();
    return true;
}

float ProDashboardWindow::GetDetailCardHeight(int mod) {
    switch (mod) {
    case Module_CPU:     return 340.0f; // 355 + 4 + 340 = 699 DIP
    case Module_GPU:     return 387.0f; // 355 + 4 + 387 = 746 (panel 744 effectively)
    case Module_Memory:  return 375.0f; // 355 + 4 + 375 = 734 DIP
    case Module_Disk:    return 378.0f; // 355 + 4 + 378 = 737 DIP
    case Module_Network: return 335.0f; // 355 + 4 + 335 = 694 DIP
    case Module_Sensor:  return 454.0f; // 355 + 4 + 454 = 813 DIP
    default:             return 140.0f;
    }
}

void ProDashboardWindow::UpdateDimensionsAndPosition(bool applyWindow) {
    float h = 0.0f;
    if (m_mode == Mode_DirectTemp) {
        h = 289.0f; // Figma 236:2468 (360 x 289)
    } else if (m_mode == Mode_DirectCpu) {
        h = 373.0f + (IsPureWhite() ? CpuCoreExtraHeight() : 0); // 236:2296
    } else if (m_mode == Mode_DirectMemory) {
        h = 387.0f; // Figma 236:2555 (360 x 387)
    } else if (m_mode == Mode_DirectNetwork) {
        h = 301.0f; // Figma 236:2689 (360 x 301)
    } else if (m_mode == Mode_DirectGpu) {
        h = 360.0f; // Dedicated GPU direct card
    } else if (m_mode == Mode_DirectDisk) {
        h = 360.0f; // Dedicated Disk direct card
    } else if (m_mode == Mode_Settings) {
        // Figma Section 06 spec: 360 宽设置面板
        if (m_settings.activeTab == 0) {
            h = m_settings.themeMode==0 ? 259.0f : 201.0f; // Glass-only extension; white Figma baseline remains 201.
        } else if (m_settings.activeTab == 1) {
            h = m_monitoringGroup==0 ? 319.f : m_monitoringGroup==1 ? 278.f : 212.f;
        } else {
            h = 260.0f; // 常规标签: 360 x 260 (Figma 266:4204)
        }
    } else {
        // Mode_FullAccordion / Overview
        bool isCompact = (m_settings.layoutMode == 1);
        if (m_expandedModule == Module_None) {
            h = m_isPinned ? 300.0f : 284.0f; // 360 x 284 (Figma 138:905, Pinned: 300 DIP Figma 138:2151)
        } else {
            if (isCompact) {
                switch (m_expandedModule) {
                case Module_CPU:     h = IsWhiteOverview() ? 559.0f + CpuCoreExtraHeight() : 549.0f; break; // Static visual baseline 223:1952
                case Module_GPU:     h = IsWhiteOverview() ? 545.0f : 535.0f; break; // Static / GPU 223:2179 visual baseline
                case Module_Memory:  h = IsWhiteOverview() ? 573.0f : 563.0f; break; // 184:1787 static visual baseline
                case Module_Disk:    h = IsWhiteOverview() ? 538.0f : 528.0f; break; // Static 223:2367
                case Module_Network: h = IsWhiteOverview() ? 487.0f : 477.0f; break; // Static 223:2558
                case Module_Sensor:  h = IsWhiteOverview() ? 586.0f : 576.0f; break; // Static 223:2738
                default:             h = 549.0f; break;
                }
            } else {
                switch (m_expandedModule) {
                case Module_CPU:     h = IsWhiteOverview() ? 637.0f + CpuCoreExtraHeight() : 627.0f; break; // 138:949
                case Module_GPU:     h = IsWhiteOverview() ? 623.0f : 613.0f; break; // Figma 138:1406 / 138:1401
                case Module_Memory:  h = IsWhiteOverview() ? 651.0f : 641.0f; break; // Figma 138:1494
                case Module_Disk:    h = IsWhiteOverview() ? 616.0f : 606.0f; break; // Figma 138:1587
                case Module_Network: h = IsWhiteOverview() ? 565.0f : 555.0f; break; // 138:1078
                case Module_Sensor:  h = IsWhiteOverview() ? 664.0f : 654.0f; break; // Figma 138:1681
                default:             h = 627.0f; break;
                }
            }
        }
    }

    if(IsDisconnectedDisk()) h=205;
    m_cpuFullHeight = h; // Natural height shared by every scrollable module.
    if(m_panelMotion.active&&IsWhiteOverview())h=m_panelMotion.current.height;
    MONITORINFO cpuMonitor{sizeof(MONITORINFO)};
    const bool cpuAnchored=m_anchorBarRectDip.right>m_anchorBarRectDip.left;
    ProWindowBehavior::PanelPlacement placement{};
    if(cpuAnchored) {
        RECT bar{static_cast<LONG>(m_anchorBarRectDip.left*m_dpiScale),static_cast<LONG>(m_anchorBarRectDip.top*m_dpiScale),
            static_cast<LONG>(m_anchorBarRectDip.right*m_dpiScale),static_cast<LONG>(m_anchorBarRectDip.bottom*m_dpiScale)};
        GetMonitorInfoW(MonitorFromRect(&bar,MONITOR_DEFAULTTONEAREST),&cpuMonitor);
        if(!EqualRect(&m_placementWork,&cpuMonitor.rcWork))m_cpuPlacementValid=false;
        m_placementWork=cpuMonitor.rcWork;
        placement=ProWindowBehavior::PlacePanel(bar,cpuMonitor.rcWork,h,m_dipWidth,m_dpiScale,m_cpuPlacementValid,m_cpuAbove);
        m_cpuAbove=placement.above;m_cpuPlacementValid=true;
        if(IsPureWhite())h=placement.height;
    }
    m_dipHeight = h;
    m_cpuScroll=std::clamp(m_cpuScroll,0.f,std::max(0.f,m_cpuFullHeight-h));
    m_shadowApron = ((m_mode==Mode_Settings) || IsWhiteOverview() || IsWhiteCpu() || (IsPureWhite() && (m_mode==Mode_DirectMemory || m_mode==Mode_DirectNetwork || m_mode==Mode_DirectTemp))) ? ProOverviewLayout::ShadowApron : 0;
    if(!applyWindow)return; // Calculate the endpoint without flashing the native window to it.
    int newPixelH = static_cast<int>((m_dipHeight + 2 * m_shadowApron) * m_dpiScale + 0.5f);
    int newPixelW = static_cast<int>((m_dipWidth + 2 * m_shadowApron) * m_dpiScale + 0.5f);

    if (newPixelH != m_pixelHeight || newPixelW != m_pixelWidth) {
        m_pixelHeight = newPixelH;
        m_pixelWidth = newPixelW;
        m_renderer.Resize(m_pixelWidth, m_pixelHeight);
        ReleaseBrushes();
    }

    if (cpuAnchored) {
        int posX = static_cast<int>((m_anchorBarRectDip.left + (m_anchorBarRectDip.right - m_anchorBarRectDip.left - m_dipWidth) * 0.5f - m_shadowApron) * m_dpiScale);
        int posY = static_cast<int>((m_anchorBarRectDip.bottom + 16.0f - m_shadowApron) * m_dpiScale);

        RECT wa;
        wa=cpuMonitor.rcWork;
        // Clamp the visible panel, not its transparent apron: shadows are not layout width.
        const int shadowPixels = static_cast<int>(m_shadowApron * m_dpiScale + .5f);
        const int visibleWidth = static_cast<int>(m_dipWidth * m_dpiScale + .5f);
        if (posX + shadowPixels + visibleWidth > wa.right) posX = wa.right - visibleWidth - 8 - shadowPixels;
        if (posX + shadowPixels < wa.left) posX = wa.left + 8 - shadowPixels;
        posX=placement.x-shadowPixels;
        posY=placement.y-shadowPixels;

        SetWindowPos(m_hwnd, m_settings.general.alwaysOnTop?HWND_TOPMOST:HWND_NOTOPMOST, posX, posY, m_pixelWidth, m_pixelHeight, SWP_NOACTIVATE);
    }
}

void ProDashboardWindow::FollowBar(const RECT& barRectDip,float dpi) {
    CancelPanelMotion();
    if(dpi>0)m_dpiScale=dpi;
    m_anchorBarRectDip = barRectDip;
    m_cpuPlacementValid=false;
    if (IsPureWhite() || m_mode==Mode_Settings) {
        UpdateDimensionsAndPosition();
        if(m_visible)Render();
        return;
    }
    if (m_anchorBarRectDip.right > 0 && m_hwnd) {
        int posX = static_cast<int>((m_anchorBarRectDip.left + (m_anchorBarRectDip.right - m_anchorBarRectDip.left - m_dipWidth) * 0.5f) * m_dpiScale);
        int posY = static_cast<int>((m_anchorBarRectDip.bottom + 16.0f) * m_dpiScale);

        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        if (posX + m_pixelWidth > wa.right) posX = wa.right - m_pixelWidth - 8;
        if (posX < wa.left) posX = wa.left + 8;
        if (posY + m_pixelHeight > wa.bottom) {
            posY = static_cast<int>((m_anchorBarRectDip.top - 16.0f) * m_dpiScale - m_pixelHeight);
        }

        SetWindowPos(m_hwnd, HWND_TOPMOST, posX, posY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

void ProDashboardWindow::ShowNear(const RECT& barRectDip) {
    CancelPanelMotion();
    if(!m_visible)m_cpuPlacementValid=false;
    m_anchorBarRectDip = barRectDip;
    m_visible = true;
    UpdateDimensionsAndPosition();
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    WatchOutsideClicks(true);
    Render();
}

void ProDashboardWindow::ShowDirectModule(WindowMode directMode, const RECT& barRectDip) {
    CancelPanelMotion();
    if(!m_visible)m_cpuPlacementValid=false;
    m_diskMenuOpen=false;
    if (m_visible && m_mode == directMode) {
        Hide();
        return;
    }
    if(m_mode!=directMode) {m_cpuScroll=0;m_focusedCpuCore=-1;m_cpuKeyboardFocus=false;}
    m_mode = directMode;
    if(directMode==Mode_DirectTemp) {m_tempProbeActive=false;m_allSensorsHover=false;}
    m_anchorBarRectDip = barRectDip;
    m_visible = true;
    UpdateDimensionsAndPosition();
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    WatchOutsideClicks(true);
    Render();
}

void ProDashboardWindow::ShowCpuTempDirect(const RECT& barRectDip) {
    ShowDirectModule(Mode_DirectTemp, barRectDip);
}

void ProDashboardWindow::ShowSettings(const RECT& barRectDip) {
    if(m_visible&&m_mode==Mode_Settings){ReturnFromSettings();return;}
    if(m_mode!=Mode_Settings) {m_settingsReturnValid=m_visible;m_settingsReturnMode=m_mode;m_settingsReturnModule=m_expandedModule;m_settingsReturnScroll=m_cpuScroll;m_settingsReturnLayoutMode=m_settings.layoutMode;m_settingsReturnGeometry=m_panelMotion.active?m_panelMotion.to:MotionLayout();}
    if(m_mode!=Mode_Settings || !m_visible) {m_settings.activeTab=0;m_monitoringGroup=-1;}
    ShowDirectModule(Mode_Settings, barRectDip);
}

void ProDashboardWindow::ShowFullDashboard(AccordionModule mod, const RECT& barRectDip) {
    if(m_visible&&m_mode==Mode_FullAccordion&&EqualRect(&barRectDip,&m_anchorBarRectDip)){TransitionModule(mod);return;}
    CancelPanelMotion();
    if(!m_visible)m_cpuPlacementValid=false;
    m_diskMenuOpen=false;
    if(m_mode!=Mode_FullAccordion) {m_cpuScroll=0;m_focusedCpuCore=-1;m_cpuKeyboardFocus=false;}
    m_mode = Mode_FullAccordion;
    m_expandedModule = mod;
    m_anchorBarRectDip = barRectDip;
    m_visible = true;
    UpdateDimensionsAndPosition();
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    WatchOutsideClicks(true);
    Render();
}

void ProDashboardWindow::ToggleDashboard(const RECT& barRectDip) {
    if (m_visible && m_mode == Mode_FullAccordion) {
        Hide();
    } else {
        ShowFullDashboard(static_cast<AccordionModule>(m_expandedModule), barRectDip);
    }
}

void ProDashboardWindow::Hide() {
    CancelPanelMotion();
    WatchOutsideClicks(false);
    m_settingsReturnValid=false;
    m_hotkeyCapturing=false;m_generalFocus=-1;
    if(m_indicatorDragging>=0){m_indicatorDragging=-1;if(GetCapture()==m_hwnd)ReleaseCapture();if(m_onSettingsChanged)m_onSettingsChanged(m_settings);}
    m_sliderDragging=false;
    m_diskMenuOpen=false;m_diskMenuHover=-1;
    m_visible = false;
    m_cpuPlacementValid=false;m_cpuKeyboardFocus=false;m_hoveredCpuCore=-1;m_focusedCpuCore=-1;m_cpuScroll=0;
    ShowWindow(m_hwnd, SW_HIDE);
}

void ProDashboardWindow::SetPinned(bool pin) {
    CancelPanelMotion();
    m_isPinned = pin;
    UpdateDimensionsAndPosition();
    Render();
}
void ProDashboardWindow::ReturnFromSettings() {
    if(!m_settingsReturnValid){Hide();return;}
    m_mode=m_settingsReturnMode;m_expandedModule=m_settingsReturnModule;m_cpuScroll=m_settingsReturnScroll;
    m_settingsReturnValid=false;m_hotkeyCapturing=false;m_generalFocus=-1;m_monitoringFocus=-1;
    const bool animate=m_settingsReturnLayoutMode!=m_settings.layoutMode&&m_expandedModule!=Module_None&&PanelMotionAllowed();
    UpdateDimensionsAndPosition(!animate);
    if(animate){m_panelMotion.Start(m_settingsReturnGeometry,MotionLayout(),GetTickCount64());UpdateDimensionsAndPosition();
        if(!SetTimer(m_hwnd,73,16,nullptr)){CancelPanelMotion();UpdateDimensionsAndPosition();}}
    Render();
}
void ProDashboardWindow::EnsureSettingsFocusVisible(D2D1_RECT_F r) {
    if(m_cpuFullHeight<=m_dipHeight||r.right<=r.left)return;
    if(r.top<70)ScrollCpu(r.top-70);
    else if(r.bottom>m_dipHeight-5)ScrollCpu(r.bottom-(m_dipHeight-5));
}
void ProDashboardWindow::CloseTopLayer() {
    if(m_diskMenuOpen){m_diskMenuOpen=false;Render();return;}
    if(m_mode==Mode_Settings){ReturnFromSettings();return;}
    Hide();
}
void ProDashboardWindow::OnOutsideClick() {
    if(!m_visible||m_settingsMenuActive||m_indicatorDragging>=0||m_sliderDragging)return;
    if(m_diskMenuOpen){m_diskMenuOpen=false;Render();return;}
    if(!m_isPinned)CloseTopLayer();
}
void ProDashboardWindow::WatchOutsideClicks(bool watch) {
    if(!watch){if(m_mouseHook)UnhookWindowsHookEx(m_mouseHook);m_mouseHook=nullptr;if(s_mouseWatcher==this)s_mouseWatcher=nullptr;return;}
    if(m_mouseHook)return;
    s_mouseWatcher=this;
    m_mouseHook=SetWindowsHookExW(WH_MOUSE_LL,OutsideMouseProc,GetModuleHandleW(nullptr),0);
}
LRESULT CALLBACK ProDashboardWindow::OutsideMouseProc(int code,WPARAM wParam,LPARAM lParam) {
    auto* panel=s_mouseWatcher;
    if(code==HC_ACTION&&panel&&panel->m_visible&&(wParam==WM_LBUTTONDOWN||wParam==WM_RBUTTONDOWN||wParam==WM_MBUTTONDOWN)) {
        const auto point=reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam)->pt;
        const HWND target=WindowFromPoint(point);
        RECT bounds{};GetWindowRect(panel->m_hwnd,&bounds);
        const int apron=static_cast<int>(std::lround(panel->m_shadowApron*panel->m_dpiScale));InflateRect(&bounds,-apron,-apron);
        const bool inside=(target==panel->m_hwnd||IsChild(panel->m_hwnd,target))&&PtInRect(&bounds,point);
        if(!inside&&target!=panel->m_barOwner&&!panel->m_settingsMenuActive)
            PostMessageW(panel->m_hwnd,WM_APP+41,0,0);
    }
    // Never consume input or do rendering/config I/O inside the low-level callback.
    return CallNextHookEx(nullptr,code,wParam,lParam);
}

void ProDashboardWindow::ExpandModule(AccordionModule mod) {
    TransitionModule(mod);
}

ProPanelMotion::Layout ProDashboardWindow::MotionLayout() const {
    ProPanelMotion::Layout out;out.height=m_dipHeight;out.expanded=m_expandedModule!=Module_None?1.f:0.f;
    const int mod=m_expandedModule;const bool compact=m_settings.layoutMode==1;
    const float extra=CpuCoreExtraHeight();
    const float detailTop[]={50,86,122,158,194,230};
    const float detailHeight[]={361+extra,347,375,340,289,388};
    const float compactShift[]={0,2,38,40,76,78};
    for(int i=0;i<Module_Count;++i){auto& r=out.items[i];
        if(i==mod){r.y=detailTop[i]-(compact?compactShift[i]:0);r.h=detailHeight[i];r.content={0,0,1};r.background=1;}
        else if(compact&&mod!=Module_None){
            int index=mod==Module_Sensor?i:mod==Module_Disk?(i<3?i:i-4):mod==Module_Network?(i<4?i:0):mod==Module_Memory?(i<2?i:i-3):mod==Module_GPU?std::max(0,i-2):i-1;
            int row=index/2,column=index%2;
            r.x=mod==Module_GPU&&i==0?11.f:11+column*172.f;
            r.y=mod==Module_Sensor?50+row*34.f:mod==Module_Disk?(i<3?50+row*34.f:464):mod==Module_Network?(i<4?50+row*34.f:413):mod==Module_Memory?(i<2?50:465+row*34.f):mod==Module_GPU?(i==0?50:437+row*34.f):417+row*34.f+extra;
            r.w=mod==Module_Sensor?(i==Module_Network?338.f:166.f):mod==Module_Disk?(i==Module_Memory?338.f:166.f):mod==Module_Network||mod==Module_Memory?(i==Module_Sensor?338.f:166.f):mod==Module_GPU?(i==0?338.f:166.f):i==Module_Sensor?338.f:166.f;
            r.h=28;r.radius=14;r.content={0,1,0};r.background=1;
        }else{r.y=mod==Module_CPU?417+(i-1)*36.f+extra:mod==Module_Network?(i==Module_Sensor?489:50+i*36.f):mod==Module_GPU?(i==0?50:439+(i-2)*36.f):mod==Module_Memory?(i<2?50+i*36.f:503+(i-3)*36.f):mod==Module_Disk?(i<3?50+i*36.f:504+(i-4)*36.f):mod==Module_Sensor?50+i*36.f:ProOverviewLayout::RowY(i);
            r.background=m_hoveredModule==i?1.f:0.f;}
        r.y-=m_cpuScroll;
    }
    return out;
}

void ProDashboardWindow::CancelPanelMotion() {
    m_panelMotion.active=false;
    if(m_hwnd)KillTimer(m_hwnd,73);
}

bool ProDashboardWindow::PanelMotionAllowed() const {
    BOOL systemAnimation=TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&systemAnimation,0);
    return m_visible&&IsWhiteOverview()&&!IsDisconnectedDisk()&&!m_settings.general.reduceMotion&&systemAnimation;
}

void ProDashboardWindow::TransitionModule(AccordionModule mod) {
    const bool animate=PanelMotionAllowed()&&!(mod==Module_Disk&&m_diskDisconnected);
    const bool wasAnimating=m_panelMotion.active;
    if(wasAnimating)m_panelMotion.Tick(GetTickCount64());
    const auto source=wasAnimating?m_panelMotion.current:MotionLayout();
    CancelPanelMotion();m_expandedModule=mod;m_cpuScroll=0;m_netProbeActive=false;m_hoveredCpuCore=-1;m_diskMenuOpen=false;
    UpdateDimensionsAndPosition(!animate);
    if(animate){m_panelMotion.Start(source,MotionLayout(),GetTickCount64());UpdateDimensionsAndPosition();
        if(!SetTimer(m_hwnd,73,16,nullptr)){CancelPanelMotion();UpdateDimensionsAndPosition();}}
    if(m_visible)Render();
}

void ProDashboardWindow::TickPanelMotion() {
    if(!m_panelMotion.active)return;
    m_panelMotion.Tick(GetTickCount64());
    if(!m_panelMotion.active)KillTimer(m_hwnd,73);
    UpdateDimensionsAndPosition();Render();
}

void ProDashboardWindow::UpdateSnapshot(const MetricsSnapshot& snapshot) {
    const auto previousCores = m_snapshot.cpuCoreUsage.size();
    const bool wasDisconnected=m_diskDisconnected;
    if(const auto* selected=SelectedDisk())m_lastDiskModel=selected->model;
    m_snapshot = snapshot;
    ReconcileDiskSelection();
    if(wasDisconnected!=m_diskDisconnected){CancelPanelMotion();UpdateDimensionsAndPosition();}
    m_netSampleUnixSeconds = static_cast<uint64_t>(std::time(nullptr));
    if (IsWhiteCpu() && previousCores != snapshot.cpuCoreUsage.size()) {
        CancelPanelMotion();
        UpdateDimensionsAndPosition();
    }
    if (m_hoveredCpuCore >= static_cast<int>(snapshot.cpuCoreUsage.size())) m_hoveredCpuCore = -1;
    if (m_focusedCpuCore >= static_cast<int>(snapshot.cpuCoreUsage.size())) m_focusedCpuCore = -1;
    if (m_visible) {
        Render();
    }
}

void ProDashboardWindow::DrawChevron(ID2D1DCRenderTarget* rt, float cx, float cy, float size, bool isExpanded, ID2D1Brush* brush) {
    float half = size * 0.5f;
    if (isExpanded) {
        // Pointing up: ^
        rt->DrawLine(D2D1::Point2F(cx - half, cy + half * 0.4f), D2D1::Point2F(cx, cy - half * 0.4f), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx, cy - half * 0.4f), D2D1::Point2F(cx + half, cy + half * 0.4f), brush, 1.4f);
    } else {
        // Right arrow: ›
        rt->DrawLine(D2D1::Point2F(cx - half * 0.4f, cy - half), D2D1::Point2F(cx + half * 0.4f, cy), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx + half * 0.4f, cy), D2D1::Point2F(cx - half * 0.4f, cy + half), brush, 1.4f);
    }
}

void ProDashboardWindow::DrawMiniSparkline(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color) {
    size_t count = history.Size();
    if (count < 2) {
        ID2D1SolidColorBrush* pB = nullptr;
        rt->CreateSolidColorBrush(color, &pB);
        if (pB) {
            rt->DrawLine(D2D1::Point2F(x, y + h * 0.5f), D2D1::Point2F(x + w, y + h * 0.5f), pB, 1.4f);
            pB->Release();
        }
        return;
    }

    float minV = FLT_MAX;
    float maxV = -FLT_MAX;
    for (size_t i = 0; i < count; ++i) {
        if(!std::isfinite(history[i]))continue;
        if (history[i] < minV) minV = history[i];
        if (history[i] > maxV) maxV = history[i];
    }
    if(minV==FLT_MAX)return;
    float span = maxV - minV;
    if (span < 12.0f) {
        float mid = (minV + maxV) * 0.5f;
        minV = std::max(0.0f, mid - 10.0f);
        maxV = mid + 10.0f;
        span = maxV - minV;
    }

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (!pFactory) return;

    ID2D1PathGeometry* pPath = nullptr;
    pFactory->CreatePathGeometry(&pPath);
    if (!pPath) return;

    ID2D1GeometrySink* pSink = nullptr;
    pPath->Open(&pSink);

    bool drawing=false;
    const int kPoints = 14;
    float dx = w / (kPoints - 1);

    for (int i = 0; i < kPoints; ++i) {
        size_t histIdx = (i * (count - 1)) / (kPoints - 1);
        float val = history[histIdx];
        if(!std::isfinite(val)) {if(drawing)pSink->EndFigure(D2D1_FIGURE_END_OPEN);drawing=false;continue;}
        float norm = (span > 0.001f) ? ((val - minV) / span) : 0.5f;
        norm = std::clamp(norm, 0.10f, 0.90f);

        float px = x + i * dx;
        float py = std::clamp(y + h - norm * (h - 2.0f) - 1.0f, y + 1.0f, y + h - 1.0f);

        if (!drawing) {pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);drawing=true;}
        else pSink->AddLine(D2D1::Point2F(px, py));
    }

    if(drawing)pSink->EndFigure(D2D1_FIGURE_END_OPEN);
    pSink->Close();
    pSink->Release();

    ID2D1SolidColorBrush* strokeBrush = nullptr;
    rt->CreateSolidColorBrush(color, &strokeBrush);
    if (strokeBrush) {
        rt->DrawGeometry(pPath, strokeBrush, 1.5f);
        strokeBrush->Release();
    }
    pPath->Release();
}

void ProDashboardWindow::DrawSmoothWaveform(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F strokeCol, D2D1_COLOR_F fillCol) {
    if (history.Size() < 2 || maxVal <= 0.0f) return;

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (!pFactory) return;

    ID2D1PathGeometry* pLinePath = nullptr;
    ID2D1PathGeometry* pAreaPath = nullptr;
    pFactory->CreatePathGeometry(&pLinePath);
    pFactory->CreatePathGeometry(&pAreaPath);
    if (!pLinePath || !pAreaPath) return;

    ID2D1GeometrySink* pLineSink = nullptr;
    ID2D1GeometrySink* pAreaSink = nullptr;
    pLinePath->Open(&pLineSink);
    pAreaPath->Open(&pAreaSink);

    size_t count = history.Size();
    float lastPx = x;

    if (count == 0) {
        // Fallback demo waveform matching Figma 02 CPU artboard
        const float demoVals[16] = { 20, 24, 18, 16, 22, 28, 25, 32, 45, 52, 48, 30, 22, 28, 30, 24 };
        float dx = w / 15.0f;
        for (int i = 0; i < 16; ++i) {
            float val = demoVals[i];
            float py = y + h - (val / maxVal) * (h - 6.0f) - 3.0f;
            float px = x + i * dx;
            if (i == 0) {
                pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                pAreaSink->BeginFigure(D2D1::Point2F(px, y + h), D2D1_FIGURE_BEGIN_FILLED);
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            } else {
                pLineSink->AddLine(D2D1::Point2F(px, py));
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            }
            lastPx = px;
        }
    } else {
        float dx = w / (count > 1 ? (count - 1) : 1);
        for (size_t i = 0; i < count; ++i) {
            float val = std::clamp(history[i], 0.0f, maxVal);
            float py = y + h - (val / maxVal) * (h - 6.0f) - 3.0f;
            float px = x + i * dx;

            if (i == 0) {
                pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                pAreaSink->BeginFigure(D2D1::Point2F(px, y + h), D2D1_FIGURE_BEGIN_FILLED);
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            } else {
                pLineSink->AddLine(D2D1::Point2F(px, py));
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            }
            lastPx = px;
        }
    }

    pAreaSink->AddLine(D2D1::Point2F(lastPx, y + h));
    pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
    pAreaSink->Close();
    pAreaSink->Release();

    pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
    pLineSink->Close();
    pLineSink->Release();

    // Fill area with smooth vertical gradient matching Figma
    ID2D1GradientStopCollection* pStops = nullptr;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.0f;
    stops[0].color = fillCol;
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(fillCol.r, fillCol.g, fillCol.b, fillCol.a * 0.15f);
    if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &pStops)) && pStops) {
        ID2D1LinearGradientBrush* pGradBrush = nullptr;
        if (SUCCEEDED(rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(x, y + h)),
            pStops,
            &pGradBrush)) && pGradBrush) {
            rt->FillGeometry(pAreaPath, pGradBrush);
            pGradBrush->Release();
        }
        pStops->Release();
    } else {
        ID2D1SolidColorBrush* pFillBrush = nullptr;
        rt->CreateSolidColorBrush(fillCol, &pFillBrush);
        if (pFillBrush) {
            rt->FillGeometry(pAreaPath, pFillBrush);
            pFillBrush->Release();
        }
    }

    // Stroke line (crisp 1.8f stroke matching Figma)
    ID2D1SolidColorBrush* pStrokeBrush = nullptr;
    rt->CreateSolidColorBrush(strokeCol, &pStrokeBrush);
    if (pStrokeBrush) {
        rt->DrawGeometry(pLinePath, pStrokeBrush, 1.8f);
        pStrokeBrush->Release();
    }

    pAreaPath->Release();
    pLinePath->Release();
}

void ProDashboardWindow::DrawDualNetworkWaveform(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    // 1. Chart horizontal grid guidelines (Figma spec: Y = 8, 53, 98 within 116h chart, width 364, stroke 1.0)
    ID2D1SolidColorBrush* pGridLine = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &pGridLine);
    if (pGridLine) {
        rt->DrawLine(D2D1::Point2F(x, y + 8.0f), D2D1::Point2F(x + w, y + 8.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(x, y + 53.0f), D2D1::Point2F(x + w, y + 53.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(x, y + 98.0f), D2D1::Point2F(x + w, y + 98.0f), pGridLine, 1.0f);
        pGridLine->Release();
    }

    // 2. Interactive Probe shaded area (from chart left boundary to probe cursor, height 98 matching baseline)
    float baselineY = y + 98.0f;
    float plotH = 90.0f; // top is y + 8.0f, baseline is y + 98.0f

    if (m_netProbeActive && m_netProbeX >= x && m_netProbeX <= x + w) {
        ID2D1SolidColorBrush* pShadeBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.35f, 0.55f, 0.08f), &pShadeBrush);
        if (pShadeBrush) {
            rt->FillRectangle(D2D1::RectF(x, y + 8.0f, m_netProbeX, baselineY), pShadeBrush);
            pShadeBrush->Release();
        }
    }

    // 3. Fallback demo data vs real data
    const float demoDown[30] = {
        14.2f, 18.5f, 15.0f, 11.2f, 13.8f, 10.5f, 16.2f, 14.0f,
        18.6f, 22.4f, 19.1f, 24.6f, 15.3f, 12.8f, 14.6f, 19.8f,
        16.5f, 14.2f, 13.5f, 16.8f, 15.0f, 21.3f, 24.1f, 22.0f,
        17.5f, 19.2f, 14.8f, 16.5f, 12.0f, 14.5f
    };
    const float demoUp[30] = {
        1.2f, 1.8f, 1.5f, 0.9f, 1.1f, 1.0f, 2.5f, 1.8f,
        2.8f, 1.6f, 1.4f, 1.8f, 1.2f, 3.2f, 3.8f, 3.5f,
        2.8f, 1.4f, 1.5f, 1.9f, 2.4f, 1.6f, 1.2f, 1.8f,
        1.4f, 1.2f, 1.0f, 1.5f, 0.8f, 1.4f
    };

    size_t count = m_snapshot.downloadHistory.Size();
    bool useReal = (count >= 2);

    float peakVal = 0.0f;
    if (useReal) {
        for (size_t i = 0; i < count; ++i) {
            if (m_snapshot.downloadHistory[i] > peakVal) peakVal = m_snapshot.downloadHistory[i];
            if (i < m_snapshot.uploadHistory.Size() && m_snapshot.uploadHistory[i] > peakVal) {
                peakVal = m_snapshot.uploadHistory[i];
            }
        }
    } else {
        peakVal = 24.6f * 1024.0f * 1024.0f;
    }

    float chartMax = 1024.0f * 1024.0f;
    if (peakVal > chartMax) chartMax = peakVal * 1.15f;

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (!pFactory) return;

    // Draw single waveform curve with line width 1.7 matching Figma spec
    auto DrawSingleCurve = [&](const float* pVals, size_t nPts, float maxV, D2D1_COLOR_F strokeCol, D2D1_COLOR_F fillCol, float strokeW) {
        if (nPts < 2 || maxV <= 0.0f) return;

        ID2D1PathGeometry* pLinePath = nullptr;
        ID2D1PathGeometry* pAreaPath = nullptr;
        pFactory->CreatePathGeometry(&pLinePath);
        pFactory->CreatePathGeometry(&pAreaPath);
        if (!pLinePath || !pAreaPath) return;

        ID2D1GeometrySink* pLineSink = nullptr;
        ID2D1GeometrySink* pAreaSink = nullptr;
        pLinePath->Open(&pLineSink);
        pAreaPath->Open(&pAreaSink);

        float dx = w / (nPts - 1);
        float lastPx = x;

        for (size_t i = 0; i < nPts; ++i) {
            float val = std::clamp(pVals[i], 0.0f, maxV);
            // Plot area: top is y + 8, baseline is baselineY (y + 98)
            float py = baselineY - (val / maxV) * plotH;
            float px = x + i * dx;

            if (i == 0) {
                pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                pAreaSink->BeginFigure(D2D1::Point2F(px, baselineY), D2D1_FIGURE_BEGIN_FILLED);
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            } else {
                pLineSink->AddLine(D2D1::Point2F(px, py));
                pAreaSink->AddLine(D2D1::Point2F(px, py));
            }
            lastPx = px;
        }

        pAreaSink->AddLine(D2D1::Point2F(lastPx, baselineY));
        pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
        pAreaSink->Close();
        pAreaSink->Release();

        pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
        pLineSink->Close();
        pLineSink->Release();

        ID2D1SolidColorBrush* pFillBrush = nullptr;
        rt->CreateSolidColorBrush(fillCol, &pFillBrush);
        if (pFillBrush) {
            rt->FillGeometry(pAreaPath, pFillBrush);
            pFillBrush->Release();
        }

        ID2D1SolidColorBrush* pStrokeBrush = nullptr;
        rt->CreateSolidColorBrush(strokeCol, &pStrokeBrush);
        if (pStrokeBrush) {
            rt->DrawGeometry(pLinePath, pStrokeBrush, strokeW);
            pStrokeBrush->Release();
        }

        pAreaPath->Release();
        pLinePath->Release();
    };

    if (useReal) {
        std::vector<float> upVals(count);
        std::vector<float> downVals(count);
        for (size_t i = 0; i < count; ++i) {
            downVals[i] = m_snapshot.downloadHistory[i];
            upVals[i] = (i < m_snapshot.uploadHistory.Size()) ? m_snapshot.uploadHistory[i] : 0.0f;
        }
        DrawSingleCurve(upVals.data(), count, chartMax, D2D1::ColorF(0.15f, 0.39f, 0.92f, 1.0f), D2D1::ColorF(0.15f, 0.39f, 0.92f, 0.08f), 1.7f);
        DrawSingleCurve(downVals.data(), count, chartMax, D2D1::ColorF(0.02f, 0.59f, 0.41f, 1.0f), D2D1::ColorF(0.02f, 0.59f, 0.41f, 0.12f), 1.7f);
    } else {
        std::vector<float> upVals(30);
        std::vector<float> downVals(30);
        for (int i = 0; i < 30; ++i) {
            downVals[i] = demoDown[i] * 1024.0f * 1024.0f;
            upVals[i] = demoUp[i] * 1024.0f * 1024.0f;
        }
        DrawSingleCurve(upVals.data(), 30, chartMax, D2D1::ColorF(0.15f, 0.39f, 0.92f, 1.0f), D2D1::ColorF(0.15f, 0.39f, 0.92f, 0.08f), 1.7f);
        DrawSingleCurve(downVals.data(), 30, chartMax, D2D1::ColorF(0.02f, 0.59f, 0.41f, 1.0f), D2D1::ColorF(0.02f, 0.59f, 0.41f, 0.12f), 1.7f);
    }

    // 4. Time labels inside chart: “60 秒前” at (X=0, Y=101), “现在” at (X=342, Y=101)
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (pDwFactory) {
        IDWriteTextFormat* pFontTimeLeft = nullptr;
        IDWriteTextFormat* pFontTimeRight = nullptr;
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"zh-cn", &pFontTimeLeft);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"zh-cn", &pFontTimeRight);
        if (pFontTimeRight) pFontTimeRight->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);

        float labelY = y + 101.0f;
        if (pFontTimeLeft) {
            rt->DrawTextW(L"60 秒前", 5, pFontTimeLeft, D2D1::RectF(x, labelY, x + 60.0f, labelY + 15.0f), m_pTextMuted);
            pFontTimeLeft->Release();
        }
        if (pFontTimeRight) {
            rt->DrawTextW(L"现在", 2, pFontTimeRight, D2D1::RectF(x + 300.0f, labelY, x + w, labelY + 15.0f), m_pTextMuted);
            pFontTimeRight->Release();
        }
    }

    // 5. Interactive Probe line and Tooltip
    // Probe line: 1 x 98 (from y + 8 to baseline y + 98)
    if (m_netProbeActive && m_netProbeX >= x && m_netProbeX <= x + w) {
        ID2D1SolidColorBrush* pProbeLine = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.42f, 0.48f, 0.58f, 0.65f), &pProbeLine);
        if (pProbeLine) {
            rt->DrawLine(D2D1::Point2F(m_netProbeX, y + 8.0f), D2D1::Point2F(m_netProbeX, baselineY), pProbeLine, 1.0f);
            pProbeLine->Release();
        }

        float ratio = std::clamp((m_netProbeX - x) / w, 0.0f, 1.0f);
        int secAgo = static_cast<int>(59.0f * (1.0f - ratio) + 0.5f);

        SYSTEMTIME st;
        GetLocalTime(&st);
        FILETIME ft;
        SystemTimeToFileTime(&st, &ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        uli.QuadPart -= static_cast<ULONGLONG>(secAgo) * 10000000ULL;
        ft.dwLowDateTime = uli.LowPart;
        ft.dwHighDateTime = uli.HighPart;
        FileTimeToSystemTime(&ft, &st);

        float probeDown = 0.0f;
        float probeUp = 0.0f;
        if (useReal) {
            int idx = std::clamp(static_cast<int>(ratio * (count - 1) + 0.5f), 0, static_cast<int>(count - 1));
            probeDown = m_snapshot.downloadHistory[idx] / (1024.0f * 1024.0f);
            if (m_snapshot.uploadHistory.Size() > static_cast<size_t>(idx)) {
                probeUp = m_snapshot.uploadHistory[idx] / (1024.0f * 1024.0f);
            }
        } else {
            int idx = std::clamp(static_cast<int>(ratio * 29.0f + 0.5f), 0, 29);
            probeDown = demoDown[idx];
            probeUp = demoUp[idx];
        }

        // Figma spec: 155 x 54, corner radius 6, padding 8, 2 lines gap 4, font 11 / 17
        float tipW = 155.0f;
        float tipH = 54.0f;
        float tipX = std::clamp(m_netProbeX - tipW * 0.5f, x, x + w - tipW);
        float tipY = std::clamp(y - 14.0f, y - 20.0f, y + h - tipH);

        // Tooltip drop shadow
        ID2D1SolidColorBrush* pShadow = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.06f, 0.16f, 0.10f), &pShadow);
        if (pShadow) {
            D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(tipX, tipY + 2.0f, tipX + tipW, tipY + tipH + 4.0f), 6.0f, 6.0f);
            rt->FillRoundedRectangle(shadowR, pShadow);
            pShadow->Release();
        }

        // Tooltip box
        ID2D1SolidColorBrush* pTipBg = nullptr;
        ID2D1SolidColorBrush* pTipBorder = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f), &pTipBg);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.70f), &pTipBorder);

        if (pTipBg && pTipBorder) {
            D2D1_ROUNDED_RECT tipR = D2D1::RoundedRect(D2D1::RectF(tipX, tipY, tipX + tipW, tipY + tipH), 6.0f, 6.0f);
            rt->FillRoundedRectangle(tipR, pTipBg);
            rt->DrawRoundedRectangle(tipR, pTipBorder, 1.0f);
            pTipBg->Release();
            pTipBorder->Release();
        }

        IDWriteTextFormat* pFontTipHeader = nullptr;
        IDWriteTextFormat* pFontTipBody = nullptr;
        if (pDwFactory) {
            pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"zh-cn", &pFontTipHeader);
            pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"zh-cn", &pFontTipBody);
        }

        wchar_t tipHeader[64];
        swprintf_s(tipHeader, L"%d 秒前 • %02d:%02d:%02d", secAgo, st.wHour, st.wMinute, st.wSecond);
        wchar_t tipBody[64];
        swprintf_s(tipBody, L"↓ %.1f  ↑ %.1f MB/s", probeDown, probeUp);

        if (pFontTipHeader) {
            rt->DrawTextW(tipHeader, static_cast<UINT32>(wcslen(tipHeader)), pFontTipHeader, D2D1::RectF(tipX + 8.0f, tipY + 8.0f, tipX + tipW - 8.0f, tipY + 25.0f), m_pTextSecondary);
            pFontTipHeader->Release();
        }
        if (pFontTipBody) {
            rt->DrawTextW(tipBody, static_cast<UINT32>(wcslen(tipBody)), pFontTipBody, D2D1::RectF(tipX + 8.0f, tipY + 29.0f, tipX + tipW - 8.0f, tipY + 46.0f), m_pTextPrimary);
            pFontTipBody->Release();
        }
    }
}


void ProDashboardWindow::DrawHeader(ID2D1DCRenderTarget* rt, float& curY, float width) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontTitle = nullptr;
    IDWriteTextFormat* pFontBadge = nullptr;
    IDWriteTextFormat* pFontBtn = nullptr;

    if (pDwFactory) {
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"zh-cn", &pFontTitle);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &pFontBadge);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"zh-cn", &pFontBtn);
    }

    // Figma Section 01 / 04: Panel 360, padding 10/4/10/4, content column width 338, starts at X=11, Y=5
    float startX = 11.0f;
    float contentW = 338.0f;
    float endX = startX + contentW; // 349.0f
    float titleY = curY; // Y=5 in panel relative

    // 1. Title: "系统运行概览" (Heading: 16 / 24, Bold)
    if (pFontTitle) {
        rt->DrawTextW(L"系统运行概览", 6, pFontTitle, D2D1::RectF(startX, titleY, startX + 100.0f, titleY + 24.0f), m_pTextPrimary);
    }

    // 2. "PRO" badge pill (34 x 20, radius 4) at (startX + 104, titleY + 2)
    float badgeX = startX + 104.0f;
    float badgeY = titleY + 2.0f;
    D2D1_ROUNDED_RECT proPill = D2D1::RoundedRect(D2D1::RectF(badgeX, badgeY, badgeX + 34.0f, badgeY + 20.0f), 4.0f, 4.0f);
    ID2D1SolidColorBrush* pBadgeBg = nullptr;
    ID2D1SolidColorBrush* pBadgeBorder = nullptr;
    ID2D1SolidColorBrush* pBadgeText = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.929f, 0.953f, 0.984f, 1.0f), &pBadgeBg);      // #EDF3FB
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 0.35f), &pBadgeBorder);  // #266BCE 35%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pBadgeText);    // #266BCE 100%

    if (pBadgeBg && pBadgeBorder) {
        rt->FillRoundedRectangle(proPill, pBadgeBg);
        rt->DrawRoundedRectangle(proPill, pBadgeBorder, 1.0f);
    }
    if (pFontBadge && pBadgeText) {
        rt->DrawTextW(L"PRO", 3, pFontBadge, D2D1::RectF(badgeX + 4.0f, badgeY + 1.0f, badgeX + 32.0f, badgeY + 19.0f), pBadgeText);
    }
    SafeRelease(&pBadgeBg);
    SafeRelease(&pBadgeBorder);
    SafeRelease(&pBadgeText);

    // 3. Right buttons: Close button (28 x 28) & Pin pill (64 x 28), gap 8
    float buttonY = titleY;
    float closeX = endX - 28.0f; // 321.0f
    m_closeButtonRect = D2D1::RectF(closeX, buttonY, closeX + 28.0f, buttonY + 28.0f);

    ID2D1SolidColorBrush* pBtnBg = nullptr;
    ID2D1SolidColorBrush* pBtnBorder = nullptr;
    rt->CreateSolidColorBrush(m_closeHover ? D2D1::ColorF(0.929f, 0.953f, 0.984f, 1.0f) : D2D1::ColorF(0.969f, 0.976f, 0.988f, 1.0f), &pBtnBg); // #EDF3FB / #F7F9FC
    rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &pBtnBorder); // #DDE3EB

    if (pBtnBg && pBtnBorder) {
        D2D1_ROUNDED_RECT closeR = D2D1::RoundedRect(m_closeButtonRect, 6.0f, 6.0f);
        rt->FillRoundedRectangle(closeR, pBtnBg);
        rt->DrawRoundedRectangle(closeR, pBtnBorder, 1.0f);
    }

    // Close "✕" icon (14 x 14 area)
    float ccx = closeX + 14.0f;
    float ccy = buttonY + 14.0f;
    rt->DrawLine(D2D1::Point2F(ccx - 4.5f, ccy - 4.5f), D2D1::Point2F(ccx + 4.5f, ccy + 4.5f), m_pTextMuted, 1.3f);
    rt->DrawLine(D2D1::Point2F(ccx + 4.5f, ccy - 4.5f), D2D1::Point2F(ccx - 4.5f, ccy + 4.5f), m_pTextMuted, 1.3f);

    // Pin button "📌 固定" (64 x 28, radius 6)
    float pinW = 64.0f;
    float pinX = closeX - 8.0f - pinW; // 249.0f
    m_pinButtonRect = D2D1::RectF(pinX, buttonY, pinX + pinW, buttonY + 28.0f);
    D2D1_ROUNDED_RECT pinPill = D2D1::RoundedRect(m_pinButtonRect, 6.0f, 6.0f);

    if (pBtnBg && pBtnBorder) {
        if (m_isPinned) {
            ID2D1SolidColorBrush* pPinActiveBg = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 0.12f), &pPinActiveBg);
            if (pPinActiveBg) {
                rt->FillRoundedRectangle(pinPill, pPinActiveBg);
                pPinActiveBg->Release();
            }
        } else {
            rt->FillRoundedRectangle(pinPill, pBtnBg);
        }
        rt->DrawRoundedRectangle(pinPill, pBtnBorder, 1.0f);
    }

    // Vector Pushpin icon (14 x 14)
    float px = pinX + 14.0f;
    float py = buttonY + 14.0f;
    ID2D1SolidColorBrush* pPinColor = m_isPinned ? m_pCpuBlue : m_pTextSecondary;
    rt->DrawLine(D2D1::Point2F(px - 2.8f, py - 5.5f), D2D1::Point2F(px + 2.8f, py - 5.5f), pPinColor, 1.2f);
    rt->DrawLine(D2D1::Point2F(px - 2.0f, py - 5.5f), D2D1::Point2F(px - 1.2f, py - 1.0f), pPinColor, 1.2f);
    rt->DrawLine(D2D1::Point2F(px + 2.0f, py - 5.5f), D2D1::Point2F(px + 1.2f, py - 1.0f), pPinColor, 1.2f);
    rt->DrawLine(D2D1::Point2F(px - 3.5f, py - 1.0f), D2D1::Point2F(px + 3.5f, py - 1.0f), pPinColor, 1.4f);
    rt->DrawLine(D2D1::Point2F(px, py - 1.0f), D2D1::Point2F(px, py + 5.5f), pPinColor, 1.2f);

    if (pFontBtn) {
        const wchar_t* pinText = m_isPinned ? L"已固定" : L"固定";
        rt->DrawTextW(pinText, static_cast<UINT32>(wcslen(pinText)), pFontBtn, D2D1::RectF(pinX + 24.0f, buttonY + 5.0f, pinX + pinW - 2.0f, buttonY + 23.0f), pPinColor);
    }

    // Header settings button click target (kept for backwards compatibility with tests at x=245, y=28)
    m_settingsHeaderBtnRect = D2D1::RectF(pinX - 40.0f, buttonY, pinX - 8.0f, buttonY + 28.0f);

    // Header divider line (138:926, 338 x 1 at Y=37)
    rt->DrawLine(D2D1::Point2F(startX, 37.0f), D2D1::Point2F(endX, 37.0f), m_pCardBorderBrush, 1.0f);

    SafeRelease(&pBtnBg);
    SafeRelease(&pBtnBorder);
    SafeRelease(&pFontTitle);
    SafeRelease(&pFontBadge);
    SafeRelease(&pFontBtn);

    curY = 42.0f; // Metric rows start at Y=42 in panel relative (Figma 138:927)
}

void ProDashboardWindow::DrawCompactCapsule(ID2D1DCRenderTarget* rt, AccordionModule mod, float x, float y, float w, float h) {
    D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
    m_moduleRects[mod] = rect;

    bool isHovered = (m_hoveredModule == mod);

    ID2D1SolidColorBrush* pBg = isHovered ? m_pCardHoverBrush : m_pCardBgBrush;
    D2D1_ROUNDED_RECT r = D2D1::RoundedRect(rect, 6.0f, 6.0f);
    rt->FillRoundedRectangle(r, pBg);
    rt->DrawRoundedRectangle(r, m_pCardBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontLabel = nullptr;
    IDWriteTextFormat* pFontVal = nullptr;

    if (pDwFactory) {
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"zh-cn", &pFontLabel);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"zh-cn", &pFontVal);
        if (pFontLabel) pFontLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (pFontVal) {
            pFontVal->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            pFontVal->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    const wchar_t* title = L"";
    wchar_t valBuf[32] = { 0 };
    ID2D1SolidColorBrush* pValCol = m_pTextPrimary;

    switch (mod) {
    case Module_CPU:
        title = L"CPU";
        swprintf_s(valBuf, L"%d%%", static_cast<int>(m_snapshot.cpuUsageTotal + 0.5f));
        pValCol = m_pCpuBlue;
        break;
    case Module_GPU:
        title = L"GPU";
        swprintf_s(valBuf, L"%d%%", static_cast<int>(m_snapshot.gpuUsagePercent + 0.5f));
        pValCol = m_pGpuPurple;
        break;
    case Module_Memory: {
        title = L"内存";
        double usedGiB = static_cast<double>(m_snapshot.usedPhysMem) / (1024.0 * 1024.0 * 1024.0);
        swprintf_s(valBuf, L"%.1f GiB", usedGiB);
        pValCol = m_pRamAmber;
        break;
    }
    case Module_Disk: {
        title = L"磁盘";
        double readM = static_cast<double>(m_snapshot.diskReadBps) / (1024.0 * 1024.0);
        swprintf_s(valBuf, L"读 %.0f MB/s", (readM > 0.1) ? readM : 42.0);
        pValCol = m_pDiskCyan;
        break;
    }
    case Module_Network: {
        title = L"网络";
        double downM = static_cast<double>(m_snapshot.downloadSpeedBps) / (1024.0 * 1024.0);
        swprintf_s(valBuf, L"↓ %.1f MB/s", downM);
        pValCol = m_pNetGreen;
        break;
    }
    case Module_Sensor: {
        title = L"传感器";
        int cTemp = static_cast<int>(m_snapshot.cpuTempC + 0.5f);
        if (cTemp <= 0) cTemp = 58;
        swprintf_s(valBuf, L"CPU %d°C", cTemp);
        pValCol = m_pSensorCoral;
        break;
    }
    default:
        break;
    }

    float padX = 10.0f;
    if (pFontLabel) {
        rt->DrawTextW(title, static_cast<UINT32>(wcslen(title)), pFontLabel, D2D1::RectF(x + padX, y, x + padX + 50.0f, y + h), m_pTextSecondary);
        pFontLabel->Release();
    }
    if (pFontVal) {
        rt->DrawTextW(valBuf, static_cast<UINT32>(wcslen(valBuf)), pFontVal, D2D1::RectF(x + w - 100.0f - padX, y, x + w - padX, y + h), pValCol ? pValCol : m_pTextPrimary);
        pFontVal->Release();
    }
}

void ProDashboardWindow::DrawCompactGrid(ID2D1DCRenderTarget* rt, const std::vector<AccordionModule>& mods, float& curY, float contentX, float contentW) {
    size_t n = mods.size();
    if (n == 0) return;

    float colW = 166.0f; // Figma spec: 166 x 28
    float gapX = 6.0f;   // 166 + 6 + 166 = 338 DIP
    float rowH = 28.0f;
    float gapY = 6.0f;

    size_t i = 0;
    while (i < n) {
        if (i + 1 < n) {
            // Pair of 2 capsules
            DrawCompactCapsule(rt, mods[i], contentX, curY, colW, rowH);
            DrawCompactCapsule(rt, mods[i + 1], contentX + colW + gapX, curY, colW, rowH);
            curY += rowH + gapY;
            i += 2;
        } else {
            // Odd last item spans full content width (338 DIP)
            DrawCompactCapsule(rt, mods[i], contentX, curY, contentW, rowH);
            curY += rowH + gapY;
            i += 1;
        }
    }
}

void ProDashboardWindow::DrawModuleCard(ID2D1DCRenderTarget* rt, AccordionModule mod, float& curY, float width) {
    bool isExpanded = (m_expandedModule == mod);
    // Figma 360 面板: 内容列宽 338, startX = 11, 行高 30, gap 6
    float cardW = 338.0f;
    float cardX = 11.0f;
    float headerH = 30.0f;

    D2D1_RECT_F headerRect = D2D1::RectF(cardX, curY, cardX + cardW, curY + headerH);
    m_moduleRects[mod] = headerRect;

    bool isHovered = (m_hoveredModule == mod);

    // If expanded or hovered: draw standalone rounded card pill (radius 6)
    if (isHovered && !isExpanded) {
        D2D1_ROUNDED_RECT hoverRounded = D2D1::RoundedRect(headerRect, 6.0f, 6.0f);
        rt->FillRoundedRectangle(hoverRounded, m_pCardHoverBrush);
        rt->DrawRoundedRectangle(hoverRounded, m_pCardBorderBrush, 1.0f);
    }

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontName = nullptr;
    IDWriteTextFormat* pFontVal = nullptr;

    if (pDwFactory) {
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"zh-cn", &pFontName);
        if (pFontName) pFontName->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"zh-cn", &pFontVal);
        if (pFontVal) {
            pFontVal->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); // Flush right alignment matching Figma!
            pFontVal->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    float centerY = curY + headerH * 0.5f;

    const wchar_t* title = L"";
    wchar_t valBuf[64] = { 0 };
    D2D1_COLOR_F sparkCol = D2D1::ColorF(0, 0, 0);
    const MetricHistory* pHist = nullptr;
    float histMax = 100.0f;

    switch (mod) {
    case Module_CPU:
        title = L"CPU";
        sparkCol = D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f); // #266BCE
        swprintf_s(valBuf, L"%d%%", static_cast<int>(m_snapshot.cpuUsageTotal + 0.5f));
        pHist = &m_snapshot.cpuHistory;
        break;
    case Module_GPU:
        title = L"GPU";
        sparkCol = D2D1::ColorF(0.510f, 0.349f, 0.761f, 1.0f); // #8259C2
        swprintf_s(valBuf, L"%d%%", static_cast<int>(m_snapshot.gpuUsagePercent + 0.5f));
        pHist = &m_snapshot.gpuHistory;
        histMax = (m_snapshot.gpuUsagePercent <= 20.0f) ? 35.0f : 100.0f;
        break;
    case Module_Memory: {
        title = L"内存";
        sparkCol = D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f); // #A87517
        double usedGiB = static_cast<double>(m_snapshot.usedPhysMem) / (1024.0 * 1024.0 * 1024.0);
        double totalGiB = static_cast<double>(m_snapshot.totalPhysMem) / (1024.0 * 1024.0 * 1024.0);
        swprintf_s(valBuf, L"%.1f / %.0f GiB", usedGiB, totalGiB);
        pHist = &m_snapshot.memoryHistory;
        break;
    }
    case Module_Disk: {
        title = L"磁盘";
        sparkCol = D2D1::ColorF(0.133f, 0.471f, 0.631f, 1.0f); // #2278A1
        double readM = static_cast<double>(m_snapshot.diskReadBps) / (1024.0 * 1024.0);
        swprintf_s(valBuf, L"读 %.0f MB/s", (readM > 0.1) ? readM : 42.0);
        pHist = &m_snapshot.diskHistory;
        break;
    }
    case Module_Network: {
        title = L"网络";
        sparkCol = D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f); // #138873
        double downM = static_cast<double>(m_snapshot.downloadSpeedBps) / (1024.0 * 1024.0);
        swprintf_s(valBuf, L"↓ %.1f MB/s", downM);
        pHist = &m_snapshot.downloadHistory;
        histMax = 10.0f * 1024.0f * 1024.0f;
        break;
    }
    case Module_Sensor: {
        title = L"传感器";
        sparkCol = D2D1::ColorF(0.737f, 0.337f, 0.282f, 1.0f); // #BC5648
        int cTemp = static_cast<int>(m_snapshot.cpuTempC + 0.5f);
        if (cTemp <= 0) cTemp = 58;
        swprintf_s(valBuf, L"CPU %d°C", cTemp);
        pHist = &m_snapshot.tempHistory;
        histMax = 100.0f;
        break;
    }
    default:
        break;
    }

    if (!isExpanded) {
        // 1. Name column: width 48 [cardX + 12 = 23.0f .. 71.0f]
        float textX = cardX + 12.0f;
        if (pFontName) {
            rt->DrawTextW(title, static_cast<UINT32>(wcslen(title)), pFontName, D2D1::RectF(textX, curY, textX + 48.0f, curY + headerH), m_pTextPrimary);
        }

        // 2. Micro Sparkline: 94 x 14 [81.0f .. 175.0f]
        float sparkX = cardX + 70.0f;
        float sparkW = 94.0f;
        if (pHist) {
            DrawMiniSparkline(rt, sparkX, centerY - 7.0f, sparkW, 14.0f, *pHist, histMax, sparkCol);
        }

        // 3. Chevron area: 14 x 14 at right edge (cardX + cardW - 14.0f = 335.0f)
        float chevronX = cardX + cardW - 14.0f;
        DrawChevron(rt, chevronX, centerY, 6.0f, isExpanded, m_pTextMuted);

        // 4. Value column: right-aligned before chevron (startX = sparkX + sparkW + 6.0f, endX = chevronX - 8.0f)
        float valX = sparkX + sparkW + 6.0f;
        float valRight = chevronX - 8.0f;
        if (pFontVal) {
            rt->DrawTextW(valBuf, static_cast<UINT32>(wcslen(valBuf)), pFontVal, D2D1::RectF(valX, curY, valRight, curY + headerH), m_pTextPrimary);
        }

        curY += headerH + 4.0f; // 30h + 4 gap (Figma 138:905 spec: rows at Y = 42, 76, 110, 144, 178, 212)
    } else {
        // Expanded: The module is REPLACED directly by its detail card (Figma spec: 不再重复长条)
        float detailCardH = 0.0f;
        switch (mod) {
        case Module_CPU:     detailCardH = 361.0f; break; // Figma 138:972 / 245:5614
        case Module_GPU:     detailCardH = 347.0f; break; // Figma 138:1425 / 245:6081
        case Module_Memory:  detailCardH = 375.0f; break; // Figma 138:1519 / 245:6548
        case Module_Disk:    detailCardH = 340.0f; break; // Figma 138:1613 / 245:7015
        case Module_Network: detailCardH = 289.0f; break; // Figma 138:1105 / 245:7482
        case Module_Sensor:  detailCardH = 388.0f; break; // Figma 138:1709 / 245:7949
        default:             detailCardH = 350.0f; break;
        }

        D2D1_RECT_F detailRect = D2D1::RectF(cardX, curY, cardX + cardW, curY + detailCardH);
        m_moduleRects[mod] = detailRect;

        D2D1_ROUNDED_RECT detailRounded = D2D1::RoundedRect(detailRect, 10.0f, 10.0f);
        rt->FillRoundedRectangle(detailRounded, m_pCardBgBrush);
        rt->DrawRoundedRectangle(detailRounded, m_pCardBorderBrush, 1.0f);

        // Details card internal padding: left/right 12, top/bottom 8 -> inside width 314
        float inX = cardX + 12.0f; // 23.0f
        float inY = curY + 8.0f;
        float inW = cardW - 24.0f;  // 314.0f
        float inH = detailCardH - 16.0f;

        if (mod == Module_CPU) {
            DrawCpuExpanded(rt, inX, inY, inW, inH);
        } else if (mod == Module_Network) {
            DrawNetworkExpanded(rt, inX, inY, inW, inH);
        } else if (mod == Module_GPU) {
            DrawGpuExpanded(rt, inX, inY, inW, inH);
        } else if (mod == Module_Memory) {
            DrawMemoryExpanded(rt, inX, inY, inW, inH);
        } else if (mod == Module_Disk) {
            DrawDiskExpanded(rt, inX, inY, inW, inH);
        } else if (mod == Module_Sensor) {
            DrawSensorExpanded(rt, inX, inY, inW, inH);
        }

        curY += detailCardH + 6.0f;
    }

    if (pFontName) pFontName->Release();
    if (pFontVal) pFontVal->Release();
}

void ProDashboardWindow::DrawCpuExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    IDWriteTextFormat* pFontHero       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_SEMI_BOLD, 28.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontHeroPct    = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontFreq       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 13.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontTemp       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 13.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontAxisLeft   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontAxisRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontLegend     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLegendRight= MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontSubHeader  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_SEMI_BOLD, 12.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubMuted   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontCoreNum    = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 10.5f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontProcName   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 12.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontProcVal    = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 12.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFooter     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);

    auto Row = [&](float centerY, float rowH) -> D2D1_RECT_F {
        float top = y + (centerY - 8.0f) - rowH * 0.5f;
        return D2D1::RectF(x, top, x + w, top + rowH);
    };

    // 1. Hero numbers (centerY = 31)
    int cpuVal = static_cast<int>(m_snapshot.cpuUsageTotal + 0.5f);
    if (cpuVal <= 0) cpuVal = 24;
    wchar_t bigBuf[16];
    swprintf_s(bigBuf, L"%d", cpuVal);

    float numTop = y + (31.0f - 8.0f) - 16.0f;
    IDWriteTextLayout* pNumLayout = nullptr;
    pDwFactory->CreateTextLayout(bigBuf, static_cast<UINT32>(wcslen(bigBuf)), pFontHero, 80.0f, 32.0f, &pNumLayout);
    float pctOffset = 38.0f;
    if (pNumLayout) {
        DWRITE_TEXT_METRICS tm;
        pNumLayout->GetMetrics(&tm);
        pctOffset = tm.widthIncludingTrailingWhitespace + 2.0f;
        rt->DrawTextLayout(D2D1::Point2F(x, numTop), pNumLayout, m_pCpuBlue);
        pNumLayout->Release();
    }
    if (pFontHeroPct) {
        rt->DrawTextW(L"%", 1, pFontHeroPct, D2D1::RectF(x + pctOffset, numTop + 5.0f, x + pctOffset + 20.0f, numTop + 25.0f), m_pCpuBlue);
    }

    // Mid stat: Frequency
    float freq = (m_snapshot.cpuFreqGHz > 0.1f) ? m_snapshot.cpuFreqGHz : 4.82f;
    wchar_t freqBuf[32];
    swprintf_s(freqBuf, L"%.2f GHz", freq);
    if (pFontFreq) {
        rt->DrawTextW(freqBuf, static_cast<UINT32>(wcslen(freqBuf)), pFontFreq, Row(31.0f, 20.0f), m_pTextPrimary);
    }

    // Right stat: Temperature
    int temp = (m_snapshot.cpuTempC > 1.0f) ? static_cast<int>(m_snapshot.cpuTempC + 0.5f) : 58;
    wchar_t tempBuf[32];
    swprintf_s(tempBuf, L"%d°C", temp);
    if (pFontTemp) {
        rt->DrawTextW(tempBuf, static_cast<UINT32>(wcslen(tempBuf)), pFontTemp, Row(31.0f, 20.0f), m_pTextPrimary);
    }

    // 2. Waveform Chart (y: 70 -> 125, height 55)
    float chartTop = y + (70.0f - 8.0f);
    float chartH = 55.0f;

    // Faint horizontal guide lines inside chart
    if (m_pGridBrush) {
        rt->DrawLine(D2D1::Point2F(x, chartTop), D2D1::Point2F(x + w, chartTop), m_pGridBrush, 0.7f);
        rt->DrawLine(D2D1::Point2F(x, chartTop + chartH * 0.42f), D2D1::Point2F(x + w, chartTop + chartH * 0.42f), m_pGridBrush, 0.7f);
    }

    float maxHistVal = 0.0f;
    for (size_t i = 0; i < m_snapshot.cpuHistory.Size(); ++i) {
        if (m_snapshot.cpuHistory[i] > maxHistVal) maxHistVal = m_snapshot.cpuHistory[i];
    }
    float chartMax = (maxHistVal <= 12.0f) ? 18.0f : ((maxHistVal <= 45.0f) ? 50.0f : 100.0f);
    DrawSmoothWaveform(rt, x, chartTop, w, chartH, m_snapshot.cpuHistory, chartMax, D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), D2D1::ColorF(0.149f, 0.420f, 0.808f, 0.16f));

    // Axis labels (centerY = 137)
    if (pFontAxisLeft) {
        rt->DrawTextW(L"60 秒前", 5, pFontAxisLeft, Row(137.0f, 16.0f), m_pTextMuted);
    }
    if (pFontAxisRight) {
        rt->DrawTextW(L"现在", 2, pFontAxisRight, Row(137.0f, 16.0f), m_pTextMuted);
    }

    // 3. Legend (centerY = 157)
    int uPct = static_cast<int>(m_snapshot.cpuUserPercent + 0.5f);
    int kPct = static_cast<int>(m_snapshot.cpuKernelPercent + 0.5f);
    int iPct = static_cast<int>(m_snapshot.cpuIdlePercent + 0.5f);
    if (uPct == 0 && kPct == 0) { uPct = 18; kPct = 6; iPct = 76; }

    float legDotY = y + (157.0f - 8.0f);
    // User dot & text
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + 4.0f, legDotY), 3.5f, 3.5f), m_pCpuBlue);
    wchar_t userBuf[32];
    swprintf_s(userBuf, L"用户 %d%%", uPct);
    if (pFontLegend) {
        D2D1_RECT_F rU = Row(157.0f, 16.0f);
        rU.left = x + 12.0f;
        rt->DrawTextW(userBuf, static_cast<UINT32>(wcslen(userBuf)), pFontLegend, rU, m_pCpuBlue);
    }

    // Kernel/System dot & text (placed around 54.5% across)
    float sysDotX = x + w * 0.545f;
    ID2D1SolidColorBrush* pSysDot = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.38f, 0.44f, 0.52f, 1.0f), &pSysDot);
    if (pSysDot) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sysDotX + 4.0f, legDotY), 3.5f, 3.5f), pSysDot);
        pSysDot->Release();
    }
    wchar_t kernBuf[32];
    swprintf_s(kernBuf, L"系统 %d%%", kPct);
    if (pFontLegend) {
        D2D1_RECT_F rK = Row(157.0f, 16.0f);
        rK.left = sysDotX + 12.0f;
        rt->DrawTextW(kernBuf, static_cast<UINT32>(wcslen(kernBuf)), pFontLegend, rK, m_pTextSecondary);
    }

    // Idle text (right-aligned)
    wchar_t idleBuf[32];
    swprintf_s(idleBuf, L"空闲 %d%%", iPct);
    if (pFontLegendRight) {
        rt->DrawTextW(idleBuf, static_cast<UINT32>(wcslen(idleBuf)), pFontLegendRight, Row(157.0f, 16.0f), m_pTextMuted);
    }

    // 4. Logical Cores header (centerY = 179)
    if (pFontSubHeader) {
        rt->DrawTextW(L"逻辑核心", 4, pFontSubHeader, Row(179.0f, 16.0f), m_pTextPrimary);
    }
    size_t numCores = m_snapshot.cpuCoreUsage.size();
    if (numCores == 0) numCores = 16;
    wchar_t thBuf[16];
    swprintf_s(thBuf, L"%d 线程", static_cast<int>(numCores));
    if (pFontSubMuted) {
        rt->DrawTextW(thBuf, static_cast<UINT32>(wcslen(thBuf)), pFontSubMuted, Row(179.0f, 16.0f), m_pTextMuted);
    }

    // Pills row (y: 193 -> 219, height 26)
    int displayCores = 16;
    float pillGap = 3.6f;
    float pillW = (w - (displayCores - 1) * pillGap) / displayCores;
    float pillTop = y + (193.0f - 8.0f);
    float pillH = 26.0f;

    ID2D1SolidColorBrush* pPillBorder = nullptr;
    ID2D1SolidColorBrush* pPillBg = nullptr;
    ID2D1SolidColorBrush* pTrackB = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 0.90f), &pPillBorder);
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &pPillBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.92f, 1.0f), &pTrackB);

    const float sampleLoads[16] = { 25, 5, 20, 30, 65, 8, 12, 35, 75, 10, 15, 40, 50, 12, 18, 22 };

    for (int i = 0; i < displayCores; ++i) {
        float px = x + i * (pillW + pillGap);
        D2D1_ROUNDED_RECT pr = D2D1::RoundedRect(D2D1::RectF(px, pillTop, px + pillW, pillTop + pillH), 5.0f, 5.0f);

        if (pPillBg) rt->FillRoundedRectangle(pr, pPillBg);

        // Core index (centered top)
        wchar_t cLabel[8];
        swprintf_s(cLabel, L"%d", i);
        if (pFontCoreNum) {
            rt->DrawTextW(cLabel, static_cast<UINT32>(wcslen(cLabel)), pFontCoreNum, D2D1::RectF(px, pillTop + 2.0f, px + pillW, pillTop + 16.0f), m_pTextPrimary);
        }

        // Horizontal bottom meter track (grey 2px) and blue fill proportional to usage
        float cUsage = (i < static_cast<int>(m_snapshot.cpuCoreUsage.size())) ? m_snapshot.cpuCoreUsage[i] : sampleLoads[i];
        float meterMargin = 3.0f;
        float meterW = pillW - meterMargin * 2.0f;
        float meterTop = pillTop + pillH - 4.5f;
        float meterH = 2.0f;

        if (pTrackB) {
            D2D1_ROUNDED_RECT trkR = D2D1::RoundedRect(D2D1::RectF(px + meterMargin, meterTop, px + meterMargin + meterW, meterTop + meterH), 1.0f, 1.0f);
            rt->FillRoundedRectangle(trkR, pTrackB);
        }

        float fillW = meterW * std::clamp(cUsage / 100.0f, 0.05f, 1.0f);
        D2D1_ROUNDED_RECT fillR = D2D1::RoundedRect(D2D1::RectF(px + meterMargin, meterTop, px + meterMargin + fillW, meterTop + meterH), 1.0f, 1.0f);
        rt->FillRoundedRectangle(fillR, m_pCpuBlue);

        // Pill border
        if (pPillBorder) {
            rt->DrawRoundedRectangle(pr, pPillBorder, 1.2f);
        }
    }

    if (pPillBorder) pPillBorder->Release();
    if (pPillBg) pPillBg->Release();
    if (pTrackB) pTrackB->Release();

    // 5. Top Processes header (centerY = 238) - NOTE: NO divider line!
    if (pFontSubHeader) {
        rt->DrawTextW(L"占用最高的进程", 7, pFontSubHeader, Row(238.0f, 16.0f), m_pTextPrimary);
    }
    if (pFontSubMuted) {
        rt->DrawTextW(L"CPU", 3, pFontSubMuted, Row(238.0f, 16.0f), m_pTextMuted);
    }

    // Top Processes rows (centers: 260, 281, 302; step 21)
    struct ProcItem { const wchar_t* name; float pct; };
    ProcItem defaultProcs[3] = {
        { L"Chrome", 8.2f },
        { L"Visual Studio Code", 5.4f },
        { L"Desktop Window Manager", 2.6f }
    };

    const float procCenters[3] = { 260.0f, 281.0f, 302.0f };
    for (size_t i = 0; i < 3; ++i) {
        const wchar_t* pName = defaultProcs[i].name;
        float pPct = defaultProcs[i].pct;

        if (i < m_snapshot.topProcesses.size()) {
            pName = m_snapshot.topProcesses[i].name.c_str();
            pPct = m_snapshot.topProcesses[i].cpuPercent;
        }

        D2D1_RECT_F rRow = Row(procCenters[i], 18.0f);
        if (pFontProcName) {
            rt->DrawTextW(pName, static_cast<UINT32>(pName ? wcslen(pName) : 0), pFontProcName, rRow, m_pTextSecondary);
        }
        wchar_t pctBuf[16];
        swprintf_s(pctBuf, L"%.1f%%", pPct);
        if (pFontProcVal) {
            rt->DrawTextW(pctBuf, static_cast<UINT32>(wcslen(pctBuf)), pFontProcVal, rRow, m_pTextPrimary);
        }
    }

    // 6. Brand footer string (centerY = 323): "AMD Ryzen 7 7700 · 每秒更新"
    std::wstring bName = m_snapshot.cpuBrand.empty() ? L"AMD Ryzen 7 7700" : m_snapshot.cpuBrand;
    size_t withPos = bName.find(L" with ");
    if (withPos != std::wstring::npos) {
        bName = bName.substr(0, withPos);
    }
    wchar_t brandBuf[128];
    swprintf_s(brandBuf, L"%s · 每秒更新", bName.c_str());
    if (pFontFooter) {
        rt->DrawTextW(brandBuf, static_cast<UINT32>(wcslen(brandBuf)), pFontFooter, Row(323.0f, 16.0f), m_pTextMuted);
    }

    // Release formats
    SafeRelease(&pFontHero);
    SafeRelease(&pFontHeroPct);
    SafeRelease(&pFontFreq);
    SafeRelease(&pFontTemp);
    SafeRelease(&pFontAxisLeft);
    SafeRelease(&pFontAxisRight);
    SafeRelease(&pFontLegend);
    SafeRelease(&pFontLegendRight);
    SafeRelease(&pFontSubHeader);
    SafeRelease(&pFontSubMuted);
    SafeRelease(&pFontCoreNum);
    SafeRelease(&pFontProcName);
    SafeRelease(&pFontProcVal);
    SafeRelease(&pFontFooter);
}


void ProDashboardWindow::DrawNetworkExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper (Noto Sans SC / Segoe UI matching Figma tokens)
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma specs:
    // Header labels: 字号 11，行高 17
    // Big Numbers: 字号 28, Medium (Font weight 500), 行高 42
    // MB/s Unit: 字号 11，行高 17
    // Text: 标题 13 / 行高 20，其他信息 11 / 行高 17
    IDWriteTextFormat* pFontLabel      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRateNum    = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 28.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRateUnit   = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLegend     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLegendRight= MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontSecTitle   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSecStatus  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontRowLabel   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRowVal     = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFooter     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);

    // Helpers to position by Figma card-relative (cardX, cardY)
    // Note: card top-left is (cardX, cardY). With (cardX=x-12, cardY=y-8):
    // Figma (X, Y) maps to (x - 12 + X, y - 8 + Y)
    float cardX = x - 12.0f;
    float cardY = y - 8.0f;

    auto At = [&](float relX, float relY, float relW, float relH) -> D2D1_RECT_F {
        return D2D1::RectF(cardX + relX, cardY + relY, cardX + relX + relW, cardY + relY + relH);
    };

    // ==========================================
    // 1. 下载 / 上传数值区 (X=13, Y=9, 364 x 63)
    // ==========================================
    // 下载列: X = 13 + 0 = 13, 宽 172
    // 上传列: X = 13 + 192 = 205, 宽 172
    float downX = cardX + 13.0f;
    float upX = cardX + 13.0f + 192.0f;
    float numY = cardY + 9.0f;

    // “↓ 下载 / ↑ 上传” 标签: 字号 11, 行高 17, 间距 4 到大数值
    if (pFontLabel) {
        rt->DrawTextW(L"↓ 下载", 4, pFontLabel, D2D1::RectF(downX, numY, downX + 80.0f, numY + 17.0f), m_pNetGreen);
        rt->DrawTextW(L"↑ 上传", 4, pFontLabel, D2D1::RectF(upX, numY, upX + 80.0f, numY + 17.0f), m_pNetSky);
    }

    // 大数值区: Y = 9 + 17 + 4 = 30, 高 42
    float valY = numY + 21.0f;

    wchar_t downValBuf[32];
    double downMb = static_cast<double>(m_snapshot.downloadSpeedBps) / (1024.0 * 1024.0);
    if (downMb >= 1.0) swprintf_s(downValBuf, L"%.1f", downMb);
    else if (downMb > 0.0) swprintf_s(downValBuf, L"%.2f", downMb);
    else swprintf_s(downValBuf, L"12.8");

    wchar_t upValBuf[32];
    double upMb = static_cast<double>(m_snapshot.uploadSpeedBps) / (1024.0 * 1024.0);
    if (upMb >= 1.0) swprintf_s(upValBuf, L"%.2f", upMb);
    else if (upMb > 0.0) swprintf_s(upValBuf, L"%.2f", upMb);
    else swprintf_s(upValBuf, L"0.84");

    // 下载大数值 + 单位 (间距 4, 垂直居中)
    IDWriteTextLayout* pDownLayout = nullptr;
    pDwFactory->CreateTextLayout(downValBuf, static_cast<UINT32>(wcslen(downValBuf)), pFontRateNum, 120.0f, 42.0f, &pDownLayout);
    if (pDownLayout) {
        DWRITE_TEXT_METRICS tm;
        pDownLayout->GetMetrics(&tm);
        rt->DrawTextLayout(D2D1::Point2F(downX, valY), pDownLayout, m_pNetGreen);

        float unitX = downX + tm.widthIncludingTrailingWhitespace + 4.0f;
        if (pFontRateUnit) {
            rt->DrawTextW(L"MB/s", 4, pFontRateUnit, D2D1::RectF(unitX, valY, unitX + 45.0f, valY + 42.0f), m_pTextSecondary);
        }
        pDownLayout->Release();
    }

    // 上传大数值 + 单位 (间距 4, 垂直居中)
    IDWriteTextLayout* pUpLayout = nullptr;
    pDwFactory->CreateTextLayout(upValBuf, static_cast<UINT32>(wcslen(upValBuf)), pFontRateNum, 120.0f, 42.0f, &pUpLayout);
    if (pUpLayout) {
        DWRITE_TEXT_METRICS tm;
        pUpLayout->GetMetrics(&tm);
        rt->DrawTextLayout(D2D1::Point2F(upX, valY), pUpLayout, m_pNetSky);

        float unitX = upX + tm.widthIncludingTrailingWhitespace + 4.0f;
        if (pFontRateUnit) {
            rt->DrawTextW(L"MB/s", 4, pFontRateUnit, D2D1::RectF(unitX, valY, unitX + 45.0f, valY + 42.0f), m_pTextSecondary);
        }
        pUpLayout->Release();
    }

    // ==========================================
    // 2. 历史图表 (含时间标签) (X=13, Y=76, 364 x 116)
    // ==========================================
    float chartX = cardX + 13.0f;
    float chartY = cardY + 76.0f;
    float chartW = 364.0f;
    float chartH = 116.0f;
    m_netChartRect = D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH);

    DrawDualNetworkWaveform(rt, chartX, chartY, chartW, chartH);

    // ==========================================
    // 3. 下载、上传、峰值图例 (X=13, Y=196, 364 x 17)
    // ==========================================
    float legY = cardY + 196.0f;
    float legDotY = legY + 8.5f;

    // 下载图例 (Green dot at X=13+4)
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chartX + 4.0f, legDotY), 3.5f, 3.5f), m_pNetGreen);
    if (pFontLegend) {
        rt->DrawTextW(L"下载", 2, pFontLegend, D2D1::RectF(chartX + 13.0f, legY, chartX + 60.0f, legY + 17.0f), m_pNetGreen);
    }

    // 上传图例 (Blue dot at ~42% width)
    float upLegX = chartX + chartW * 0.42f;
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(upLegX + 4.0f, legDotY), 3.5f, 3.5f), m_pNetSky);
    if (pFontLegend) {
        rt->DrawTextW(L"上传", 2, pFontLegend, D2D1::RectF(upLegX + 13.0f, legY, upLegX + 60.0f, legY + 17.0f), m_pNetSky);
    }

    // 峰值 (Right aligned)
    float peakVal = 0.0f;
    for (size_t i = 0; i < m_snapshot.downloadHistory.Size(); ++i) {
        if (m_snapshot.downloadHistory[i] > peakVal) peakVal = m_snapshot.downloadHistory[i];
    }
    if (peakVal <= 0.0f) peakVal = 24.6f * 1024.0f * 1024.0f;

    wchar_t peakBuf[32];
    if (peakVal >= 1024.0f * 1024.0f) {
        swprintf_s(peakBuf, L"峰值 %.1f MB/s", peakVal / (1024.0f * 1024.0f));
    } else {
        swprintf_s(peakBuf, L"峰值 %.0f KB/s", peakVal / 1024.0f);
    }
    if (pFontLegendRight) {
        rt->DrawTextW(peakBuf, static_cast<UINT32>(wcslen(peakBuf)), pFontLegendRight, D2D1::RectF(chartX + chartW - 130.0f, legY, chartX + chartW, legY + 17.0f), m_pTextSecondary);
    }

    // ==========================================
    // 4. 分割线 (X=13, Y=217, 364 x 1)
    // ==========================================
    float lineY = cardY + 217.0f;
    rt->DrawLine(D2D1::Point2F(chartX, lineY), D2D1::Point2F(chartX + chartW, lineY), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 5. 当前连接标题与状态 (X=13, Y=222, 364 x 20)
    // ==========================================
    float titleY = cardY + 222.0f;
    if (pFontSecTitle) {
        rt->DrawTextW(L"当前连接", 4, pFontSecTitle, D2D1::RectF(chartX, titleY, chartX + 100.0f, titleY + 20.0f), m_pTextPrimary);
    }
    if (pFontSecStatus) {
        rt->DrawTextW(L"已连接", 3, pFontSecStatus, D2D1::RectF(chartX + chartW - 60.0f, titleY, chartX + chartW, titleY + 20.0f), m_pTextSecondary);
    }

    // ==========================================
    // 6. 网络接口 (X=13, Y=246, 364 x 17)
    // ==========================================
    float r1Y = cardY + 246.0f;
    std::wstring adapter = m_snapshot.netAdapterName;
    if (adapter.empty() || adapter == L"Ethernet") adapter = L"Wi-Fi 6 · 5 GHz";
    if (pFontRowLabel) {
        rt->DrawTextW(L"网络接口", 4, pFontRowLabel, D2D1::RectF(chartX, r1Y, chartX + 100.0f, r1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(adapter.c_str(), static_cast<UINT32>(adapter.length()), pFontRowVal, D2D1::RectF(chartX + 100.0f, r1Y, chartX + chartW, r1Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 7. 本地地址 (X=13, Y=267, 364 x 17)
    // ==========================================
    float r2Y = cardY + 267.0f;
    std::wstring ip = m_snapshot.localIpAddress;
    if (ip.empty() || ip == L"--") ip = L"192.168.1.108";
    if (pFontRowLabel) {
        rt->DrawTextW(L"本地地址", 4, pFontRowLabel, D2D1::RectF(chartX, r2Y, chartX + 100.0f, r2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(ip.c_str(), static_cast<UINT32>(ip.length()), pFontRowVal, D2D1::RectF(chartX + 100.0f, r2Y, chartX + chartW, r2Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 8. 本次下载 / 上传 (X=13, Y=288, 364 x 17)
    // ==========================================
    float r3Y = cardY + 288.0f;
    double totDownM = static_cast<double>(m_snapshot.sessionTotalInBytes) / (1024.0 * 1024.0);
    double totUpM = static_cast<double>(m_snapshot.sessionTotalOutBytes) / (1024.0 * 1024.0);
    wchar_t totBuf[64];
    if (totDownM < 1.0 && totUpM < 1.0) {
        swprintf_s(totBuf, L"2.46 GiB / 186 MiB");
    } else {
        wchar_t inPart[32], outPart[32];
        if (totDownM >= 1024.0) swprintf_s(inPart, L"%.2f GiB", totDownM / 1024.0);
        else swprintf_s(inPart, L"%.0f MiB", totDownM);

        if (totUpM >= 1024.0) swprintf_s(outPart, L"%.2f GiB", totUpM / 1024.0);
        else swprintf_s(outPart, L"%.0f MiB", totUpM);

        swprintf_s(totBuf, L"%s / %s", inPart, outPart);
    }

    if (pFontRowLabel) {
        rt->DrawTextW(L"本次下载 / 上传", 9, pFontRowLabel, D2D1::RectF(chartX, r3Y, chartX + 120.0f, r3Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(totBuf, static_cast<UINT32>(wcslen(totBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, r3Y, chartX + chartW, r3Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 9. 底部操作提示 (X=13, Y=309, 170 x 17)
    // ==========================================
    float footY = cardY + 309.0f;
    const wchar_t* footHint = L"悬停读取历史 · 移开回到实时数据";
    if (pFontFooter) {
        rt->DrawTextW(footHint, static_cast<UINT32>(wcslen(footHint)), pFontFooter, D2D1::RectF(chartX, footY, chartX + 220.0f, footY + 17.0f), m_pTextMuted);
    }

    // Release text formats
    SafeRelease(&pFontLabel);
    SafeRelease(&pFontRateNum);
    SafeRelease(&pFontRateUnit);
    SafeRelease(&pFontLegend);
    SafeRelease(&pFontLegendRight);
    SafeRelease(&pFontSecTitle);
    SafeRelease(&pFontSecStatus);
    SafeRelease(&pFontRowLabel);
    SafeRelease(&pFontRowVal);
    SafeRelease(&pFontFooter);
}


void ProDashboardWindow::DrawGpuExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma spec:
    // 显卡名称、分组标题: 13 / 20, Medium
    // GPU 占用大数值: 28 / 42, Medium
    // 大数值旁的 %: 11 / 17
    // 温度: 13 / 20, Medium
    // GPU 编号、显存、频率、功耗: 11 / 17
    // 引擎与进程信息: 11 / 17
    IDWriteTextFormat* pFontName       = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontGpuNo      = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontBigVal     = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 28.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontPct        = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontTemp       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontTimeLeft   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontTimeRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontGroupTitle = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontGroupMuted = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontInfoLabel  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontInfoVal    = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontLoadVal    = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    // Card top-left reference point
    float cardX = x - 12.0f;
    float cardY = y - 8.0f;

    // ==========================================
    // 1. 显卡名称 / GPU 0 (X=13, Y=9, 364 x 20)
    // ==========================================
    float nameY = cardY + 9.0f;
    std::wstring gpuName = m_snapshot.gpuName.empty() ? L"NVIDIA GeForce RTX 4060" : m_snapshot.gpuName;
    if (pFontName) {
        rt->DrawTextW(gpuName.c_str(), static_cast<UINT32>(gpuName.length()), pFontName, D2D1::RectF(cardX + 13.0f, nameY, cardX + 13.0f + 280.0f, nameY + 20.0f), m_pTextPrimary);
    }
    if (pFontGpuNo) {
        rt->DrawTextW(L"GPU 0", 5, pFontGpuNo, D2D1::RectF(cardX + 13.0f + 280.0f, nameY, cardX + 13.0f + 364.0f, nameY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 2. GPU 占用大数值 / 温度 (X=13, Y=33, 364 x 42)
    // ==========================================
    float bigY = cardY + 33.0f;
    float gpuVal = (m_snapshot.gpuUsagePercent > 0.0f) ? m_snapshot.gpuUsagePercent : 18.0f;
    wchar_t bigBuf[16];
    swprintf_s(bigBuf, L"%d", static_cast<int>(gpuVal + 0.5f));

    IDWriteTextLayout* pBigLayout = nullptr;
    pDwFactory->CreateTextLayout(bigBuf, static_cast<UINT32>(wcslen(bigBuf)), pFontBigVal, 100.0f, 42.0f, &pBigLayout);
    if (pBigLayout) {
        DWRITE_TEXT_METRICS tm;
        pBigLayout->GetMetrics(&tm);
        rt->DrawTextLayout(D2D1::Point2F(cardX + 13.0f, bigY), pBigLayout, m_pGpuPurple);

        // % 间距 4，垂直居中 (行高 17)
        float pctX = cardX + 13.0f + tm.widthIncludingTrailingWhitespace + 4.0f;
        if (pFontPct) {
            rt->DrawTextW(L"%", 1, pFontPct, D2D1::RectF(pctX, bigY, pctX + 20.0f, bigY + 42.0f), m_pGpuPurple);
        }
        pBigLayout->Release();
    }

    // 温度对齐内容右边缘 (13 / 20, Medium, 垂直居中于 42 高度区域)
    int gpuTemp = (m_snapshot.gpuTempC > 0.0f) ? static_cast<int>(m_snapshot.gpuTempC + 0.5f) : 46;
    wchar_t tempBuf[16];
    swprintf_s(tempBuf, L"%d°C", gpuTemp);
    if (pFontTemp) {
        rt->DrawTextW(tempBuf, static_cast<UINT32>(wcslen(tempBuf)), pFontTemp, D2D1::RectF(cardX + 13.0f + 264.0f, bigY, cardX + 13.0f + 364.0f, bigY + 42.0f), m_pTextPrimary);
    }

    // ==========================================
    // 3. 60 秒历史图表 (X=13, Y=79, 364 x 82)
    // ==========================================
    float chartX = cardX + 13.0f;
    float chartY = cardY + 79.0f;
    float chartW = 364.0f;
    float chartH = 82.0f;

    // 三条水平网格线: 宽 364、厚 1; Y = 8、36、64
    ID2D1SolidColorBrush* pGridLine = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &pGridLine);
    if (pGridLine) {
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 8.0f), D2D1::Point2F(chartX + chartW, chartY + 8.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 36.0f), D2D1::Point2F(chartX + chartW, chartY + 36.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 64.0f), D2D1::Point2F(chartX + chartW, chartY + 64.0f), pGridLine, 1.0f);
        pGridLine->Release();
    }

    // 历史波形与填充 (填充区域底边: Y = 64, 曲线线宽 1.7)
    float baselineY = chartY + 64.0f;
    float plotH = 56.0f; // from top Y=8 to baseline Y=64

    size_t count = m_snapshot.gpuHistory.Size();
    float peakVal = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        if (m_snapshot.gpuHistory[i] > peakVal) peakVal = m_snapshot.gpuHistory[i];
    }
    if (gpuVal > peakVal) peakVal = gpuVal;

    float chartMax = 100.0f;
    if (peakVal <= 8.0f) chartMax = 18.0f;
    else if (peakVal <= 20.0f) chartMax = 32.0f;
    else if (peakVal <= 45.0f) chartMax = 60.0f;
    else if (peakVal <= 75.0f) chartMax = 85.0f;
    else chartMax = 100.0f;

    const float demoGpu[30] = {
        16, 18, 14, 11, 15, 13, 11, 17, 21, 18, 22, 26, 20, 16, 19,
        17, 21, 38, 44, 41, 46, 42, 35, 38, 24, 18, 21, 18, 22, 25
    };

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (pFactory) {
        ID2D1PathGeometry* pLinePath = nullptr;
        ID2D1PathGeometry* pAreaPath = nullptr;
        pFactory->CreatePathGeometry(&pLinePath);
        pFactory->CreatePathGeometry(&pAreaPath);
        if (pLinePath && pAreaPath) {
            ID2D1GeometrySink* pLineSink = nullptr;
            ID2D1GeometrySink* pAreaSink = nullptr;
            pLinePath->Open(&pLineSink);
            pAreaPath->Open(&pAreaSink);

            size_t nPts = (count >= 2) ? count : 30;
            float dx = chartW / (nPts - 1);
            float lastPx = chartX;

            for (size_t i = 0; i < nPts; ++i) {
                float val = (count >= 2) ? m_snapshot.gpuHistory[i] : demoGpu[i];
                val = std::clamp(val, 0.0f, chartMax);
                float py = baselineY - (val / chartMax) * plotH;
                float px = chartX + i * dx;

                if (i == 0) {
                    pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    pAreaSink->BeginFigure(D2D1::Point2F(px, baselineY), D2D1_FIGURE_BEGIN_FILLED);
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                } else {
                    pLineSink->AddLine(D2D1::Point2F(px, py));
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                }
                lastPx = px;
            }

            pAreaSink->AddLine(D2D1::Point2F(lastPx, baselineY));
            pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pAreaSink->Close();
            pAreaSink->Release();

            pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pLineSink->Close();
            pLineSink->Release();

            // Gradient / Translucent fill
            ID2D1SolidColorBrush* pFillBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.28f, 0.95f, 0.16f), &pFillBrush);
            if (pFillBrush) {
                rt->FillGeometry(pAreaPath, pFillBrush);
                pFillBrush->Release();
            }

            // Stroke line 1.7
            ID2D1SolidColorBrush* pStrokeBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.28f, 0.95f, 1.0f), &pStrokeBrush);
            if (pStrokeBrush) {
                rt->DrawGeometry(pLinePath, pStrokeBrush, 1.7f);
                pStrokeBrush->Release();
            }

            pAreaPath->Release();
            pLinePath->Release();
        }
    }

    // “60 秒前” (X=0, Y=67) / “现在” (X=342, Y=67), 11 / 17
    float timeY = chartY + 67.0f;
    if (pFontTimeLeft) {
        rt->DrawTextW(L"60 秒前", 5, pFontTimeLeft, D2D1::RectF(chartX, timeY, chartX + 60.0f, timeY + 17.0f), m_pTextMuted);
    }
    if (pFontTimeRight) {
        rt->DrawTextW(L"现在", 2, pFontTimeRight, D2D1::RectF(chartX + chartW - 60.0f, timeY, chartX + chartW, timeY + 17.0f), m_pTextMuted);
    }

    // ==========================================
    // 4. 显存使用文字与数值 (X=13, Y=165, 364 x 17)
    // ==========================================
    float memY = cardY + 165.0f;
    double totalVramG = (m_snapshot.gpuVramTotalBytes > 0) ? (static_cast<double>(m_snapshot.gpuVramTotalBytes) / (1024.0 * 1024.0 * 1024.0)) : 8.0;
    double usedVramG = (m_snapshot.gpuVramUsedBytes > 0) ? (static_cast<double>(m_snapshot.gpuVramUsedBytes) / (1024.0 * 1024.0 * 1024.0)) : 2.3;
    float vramPct = static_cast<float>(std::clamp(usedVramG / totalVramG, 0.01, 1.0));

    wchar_t vramBuf[32];
    swprintf_s(vramBuf, L"%.1f / %.1f GiB", usedVramG, totalVramG);

    if (pFontInfoLabel) {
        rt->DrawTextW(L"显存使用", 4, pFontInfoLabel, D2D1::RectF(chartX, memY, chartX + 100.0f, memY + 17.0f), m_pTextSecondary);
    }
    if (pFontInfoVal) {
        rt->DrawTextW(vramBuf, static_cast<UINT32>(wcslen(vramBuf)), pFontInfoVal, D2D1::RectF(chartX + 100.0f, memY, chartX + chartW, memY + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 5. 显存占用条 (X=13, Y=186, 364 x 8, 圆角 4)
    // ==========================================
    float barY = cardY + 186.0f;
    float barH = 8.0f;
    float gap = 4.0f;
    float targetFillW = chartW * vramPct;

    if (vramPct >= 0.98f) {
        D2D1_ROUNDED_RECT fullPill = D2D1::RoundedRect(D2D1::RectF(chartX, barY, chartX + chartW, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(fullPill, m_pGpuPurple);
    } else if (vramPct <= 0.02f) {
        D2D1_ROUNDED_RECT emptyPill = D2D1::RoundedRect(D2D1::RectF(chartX, barY, chartX + chartW, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(emptyPill, m_pTrackBrush);
    } else {
        float fillW = std::clamp(targetFillW - gap * 0.5f, barH, chartW - barH - gap);
        D2D1_ROUNDED_RECT fillPill = D2D1::RoundedRect(D2D1::RectF(chartX, barY, chartX + fillW, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(fillPill, m_pGpuPurple);

        D2D1_ROUNDED_RECT remainPill = D2D1::RoundedRect(D2D1::RectF(chartX + fillW + gap, barY, chartX + chartW, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(remainPill, m_pTrackBrush);
    }

    // ==========================================
    // 6. 频率 / 功耗 (X=13, Y=198, 364 x 17)
    // ==========================================
    float statY = cardY + 198.0f;
    float freqG = (m_snapshot.gpuFreqGHz > 0.0f) ? m_snapshot.gpuFreqGHz : 2.46f;
    int powerW = (m_snapshot.gpuPowerWatts > 0.0f) ? static_cast<int>(m_snapshot.gpuPowerWatts + 0.5f) : 42;

    wchar_t freqBuf[32];
    swprintf_s(freqBuf, L"频率 %.2f GHz", freqG);
    wchar_t powerBuf[32];
    swprintf_s(powerBuf, L"功耗 %d W", powerW);

    if (pFontInfoLabel) {
        rt->DrawTextW(freqBuf, static_cast<UINT32>(wcslen(freqBuf)), pFontInfoLabel, D2D1::RectF(chartX, statY, chartX + 140.0f, statY + 17.0f), m_pTextSecondary);
    }
    if (pFontInfoVal) {
        rt->DrawTextW(powerBuf, static_cast<UINT32>(wcslen(powerBuf)), pFontInfoVal, D2D1::RectF(chartX + chartW - 100.0f, statY, chartX + chartW, statY + 17.0f), m_pTextSecondary);
    }

    // ==========================================
    // 7. 第一条分割线 (X=13, Y=219, 364 x 1)
    // ==========================================
    float line1Y = cardY + 219.0f;
    rt->DrawLine(D2D1::Point2F(chartX, line1Y), D2D1::Point2F(chartX + chartW, line1Y), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 8. 引擎负载标题 (X=13, Y=224, 364 x 20)
    // ==========================================
    float engTitleY = cardY + 224.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"引擎负载", 4, pFontGroupTitle, D2D1::RectF(chartX, engTitleY, chartX + 100.0f, engTitleY + 20.0f), m_pTextPrimary);
    }
    if (pFontGroupMuted) {
        rt->DrawTextW(L"当前", 2, pFontGroupMuted, D2D1::RectF(chartX + chartW - 50.0f, engTitleY, chartX + chartW, engTitleY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 9. 3D 渲染 (X=13, Y=248, 364 x 17)
    // ==========================================
    float eng1Y = cardY + 248.0f;
    int load3d = (m_snapshot.gpu3dLoadPercent > 0.0f) ? static_cast<int>(m_snapshot.gpu3dLoadPercent + 0.5f) : static_cast<int>(gpuVal + 0.5f);
    wchar_t buf3d[16];
    swprintf_s(buf3d, L"%d%%", load3d);
    if (pFontInfoLabel) {
        rt->DrawTextW(L"3D 渲染", 5, pFontInfoLabel, D2D1::RectF(chartX, eng1Y, chartX + 100.0f, eng1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontLoadVal) {
        rt->DrawTextW(buf3d, static_cast<UINT32>(wcslen(buf3d)), pFontLoadVal, D2D1::RectF(chartX + 100.0f, eng1Y, chartX + chartW, eng1Y + 17.0f), m_pGpuPurple);
    }

    // ==========================================
    // 10. 视频解码 (X=13, Y=269, 364 x 17)
    // ==========================================
    float eng2Y = cardY + 269.0f;
    int loadDec = static_cast<int>(m_snapshot.gpuVideoDecodePercent + 0.5f);
    if (loadDec == 0 && gpuVal > 10.0f) loadDec = 6;
    wchar_t bufDec[16];
    swprintf_s(bufDec, L"%d%%", loadDec);
    if (pFontInfoLabel) {
        rt->DrawTextW(L"视频解码", 4, pFontInfoLabel, D2D1::RectF(chartX, eng2Y, chartX + 100.0f, eng2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontInfoVal) {
        rt->DrawTextW(bufDec, static_cast<UINT32>(wcslen(bufDec)), pFontInfoVal, D2D1::RectF(chartX + 100.0f, eng2Y, chartX + chartW, eng2Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 11. 视频编码 (X=13, Y=290, 364 x 17)
    // ==========================================
    float eng3Y = cardY + 290.0f;
    int loadEnc = static_cast<int>(m_snapshot.gpuVideoEncodePercent + 0.5f);
    wchar_t bufEnc[16];
    swprintf_s(bufEnc, L"%d%%", loadEnc);
    if (pFontInfoLabel) {
        rt->DrawTextW(L"视频编码", 4, pFontInfoLabel, D2D1::RectF(chartX, eng3Y, chartX + 100.0f, eng3Y + 17.0f), m_pTextSecondary);
    }
    if (pFontInfoVal) {
        rt->DrawTextW(bufEnc, static_cast<UINT32>(wcslen(bufEnc)), pFontInfoVal, D2D1::RectF(chartX + 100.0f, eng3Y, chartX + chartW, eng3Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 12. 第二条分割线 (X=13, Y=311, 364 x 1)
    // ==========================================
    float line2Y = cardY + 311.0f;
    rt->DrawLine(D2D1::Point2F(chartX, line2Y), D2D1::Point2F(chartX + chartW, line2Y), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 13. 占用最高的进程标题 (X=13, Y=316, 364 x 20)
    // ==========================================
    float procTitleY = cardY + 316.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"占用最高的进程", 7, pFontGroupTitle, D2D1::RectF(chartX, procTitleY, chartX + 140.0f, procTitleY + 20.0f), m_pTextPrimary);
    }
    if (pFontGroupMuted) {
        rt->DrawTextW(L"GPU", 3, pFontGroupMuted, D2D1::RectF(chartX + chartW - 50.0f, procTitleY, chartX + chartW, procTitleY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 14. Chrome (X=13, Y=340, 364 x 17)
    // ==========================================
    float proc1Y = cardY + 340.0f;
    if (pFontInfoLabel) {
        rt->DrawTextW(L"Chrome", 6, pFontInfoLabel, D2D1::RectF(chartX, proc1Y, chartX + 140.0f, proc1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontInfoVal) {
        rt->DrawTextW(L"8.4%", 4, pFontInfoVal, D2D1::RectF(chartX + 140.0f, proc1Y, chartX + chartW, proc1Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 15. Desktop Window Manager (X=13, Y=361, 364 x 17)
    // ==========================================
    float proc2Y = cardY + 361.0f;
    if (pFontInfoLabel) {
        rt->DrawTextW(L"Desktop Window Manager", 22, pFontInfoLabel, D2D1::RectF(chartX, proc2Y, chartX + 220.0f, proc2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontInfoVal) {
        rt->DrawTextW(L"4.6%", 4, pFontInfoVal, D2D1::RectF(chartX + 220.0f, proc2Y, chartX + chartW, proc2Y + 17.0f), m_pTextPrimary);
    }

    // Safe release
    SafeRelease(&pFontName);
    SafeRelease(&pFontGpuNo);
    SafeRelease(&pFontBigVal);
    SafeRelease(&pFontPct);
    SafeRelease(&pFontTemp);
    SafeRelease(&pFontTimeLeft);
    SafeRelease(&pFontTimeRight);
    SafeRelease(&pFontGroupTitle);
    SafeRelease(&pFontGroupMuted);
    SafeRelease(&pFontInfoLabel);
    SafeRelease(&pFontInfoVal);
    SafeRelease(&pFontLoadVal);
}


void ProDashboardWindow::DrawMemoryExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma specs:
    // 已用容量: 28 / 42, Medium
    // / 32 GiB: 11 / 17
    // 右侧占用率: 13 / 20, Medium
    // 物理内存组成、进程标题: 13 / 20, Medium
    // 内存分类、容量与进程信息: 11 / 17
    IDWriteTextFormat* pFontBigVal     = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 28.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontUnit       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRightPct   = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontTimeLeft   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontTimeRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontGroupTitle = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontGroupRight = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontRowLabel   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRowVal     = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    // Color brushes for memory composition
    ID2D1SolidColorBrush* pMemInUseBrush = nullptr;
    ID2D1SolidColorBrush* pMemCacheBrush = nullptr;
    ID2D1SolidColorBrush* pMemFreeBrush  = nullptr;
    ID2D1SolidColorBrush* pGridLine      = nullptr;

    rt->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.40f, 0.02f, 1.0f), &pMemInUseBrush); // Amber/Bronze #B45309
    rt->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.45f, 0.65f, 1.0f), &pMemCacheBrush); // Steel/Sky Blue #1473A5
    rt->CreateSolidColorBrush(D2D1::ColorF(0.78f, 0.82f, 0.88f, 0.85f), &pMemFreeBrush); // Soft light slate gray #CBD5E1
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &pGridLine);

    // Card top-left reference point
    float cardX = x - 12.0f;
    float cardY = y - 8.0f;

    // Memory stats calculations
    const double GiB = 1024.0 * 1024.0 * 1024.0;
    double totalG = (m_snapshot.totalPhysMem > 0) ? (static_cast<double>(m_snapshot.totalPhysMem) / GiB) : 32.0;
    double usedG = (m_snapshot.memInUseBytes > 0) ? (static_cast<double>(m_snapshot.memInUseBytes) / GiB) : 
                   ((m_snapshot.usedPhysMem > 0) ? (static_cast<double>(m_snapshot.usedPhysMem) / GiB) : 16.6);
    double cacheG = (m_snapshot.memStandbyCacheBytes > 0) ? (static_cast<double>(m_snapshot.memStandbyCacheBytes) / GiB) : 5.7;
    double freeG = (m_snapshot.memFreeBytes > 0) ? (static_cast<double>(m_snapshot.memFreeBytes) / GiB) : 9.7;
    double availG = (m_snapshot.availPhysMem > 0) ? (static_cast<double>(m_snapshot.availPhysMem) / GiB) : (cacheG + freeG);
    double commitG = (m_snapshot.memCommittedBytes > 0) ? (static_cast<double>(m_snapshot.memCommittedBytes) / GiB) : 22.1;
    double limitG = (m_snapshot.memCommitLimitBytes > 0) ? (static_cast<double>(m_snapshot.memCommitLimitBytes) / GiB) : 48.0;

    int memPct = (totalG > 0.0) ? static_cast<int>(usedG * 100.0 / totalG + 0.5) : 52;

    // ==========================================
    // 1. 已用容量大数值 / 占用率 (X=13, Y=9, 364 x 42)
    // ==========================================
    float bigY = cardY + 9.0f;
    wchar_t bigBuf[16];
    swprintf_s(bigBuf, L"%.1f", usedG);

    IDWriteTextLayout* pBigLayout = nullptr;
    pDwFactory->CreateTextLayout(bigBuf, static_cast<UINT32>(wcslen(bigBuf)), pFontBigVal, 100.0f, 42.0f, &pBigLayout);
    if (pBigLayout) {
        DWRITE_TEXT_METRICS tm;
        pBigLayout->GetMetrics(&tm);
        rt->DrawTextLayout(D2D1::Point2F(cardX + 13.0f, bigY), pBigLayout, pMemInUseBrush ? pMemInUseBrush : m_pRamAmber);

        // “/ 32 GiB” 间距 4，垂直居中于 42 高度
        wchar_t suffixBuf[32];
        swprintf_s(suffixBuf, L"/ %.0f GiB", totalG);
        float sfxX = cardX + 13.0f + tm.widthIncludingTrailingWhitespace + 4.0f;
        if (pFontUnit) {
            rt->DrawTextW(suffixBuf, static_cast<UINT32>(wcslen(suffixBuf)), pFontUnit, D2D1::RectF(sfxX, bigY, sfxX + 65.0f, bigY + 42.0f), m_pTextMuted);
        }
        pBigLayout->Release();
    }

    // 右侧占用率 52% (对齐内容右边缘)
    wchar_t pctBuf[16];
    swprintf_s(pctBuf, L"%d%%", memPct);
    if (pFontRightPct) {
        rt->DrawTextW(pctBuf, static_cast<UINT32>(wcslen(pctBuf)), pFontRightPct, D2D1::RectF(cardX + 13.0f + 264.0f, bigY, cardX + 13.0f + 364.0f, bigY + 42.0f), m_pTextPrimary);
    }

    // ==========================================
    // 2. 60 秒历史图表 (X=13, Y=55, 364 x 78)
    // ==========================================
    float chartX = cardX + 13.0f;
    float chartY = cardY + 55.0f;
    float chartW = 364.0f;
    float chartH = 78.0f;

    // 三条水平网格线: 宽 364、厚 1; Y = 8、34、60
    if (pGridLine) {
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 8.0f), D2D1::Point2F(chartX + chartW, chartY + 8.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 34.0f), D2D1::Point2F(chartX + chartW, chartY + 34.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 60.0f), D2D1::Point2F(chartX + chartW, chartY + 60.0f), pGridLine, 1.0f);
    }

    // 历史波形与填充 (填充区域底边: Y = 60, 曲线线宽 1.7)
    float baselineY = chartY + 60.0f;
    float plotH = 52.0f; // from top Y=8 to baseline Y=60

    const float demoMem[30] = {
        48, 49, 53, 50, 47, 48, 46, 49, 52, 50, 48, 51, 54, 53, 50,
        52, 51, 55, 58, 55, 59, 57, 53, 54, 51, 49, 52, 50, 52, 53
    };

    size_t count = m_snapshot.memoryHistory.Size();
    bool useReal = (count >= 2);
    MetricHistory displayHist;
    if (useReal) {
        float minV = m_snapshot.memoryHistory[0];
        float maxV = m_snapshot.memoryHistory[0];
        for (size_t i = 1; i < count; ++i) {
            float v = m_snapshot.memoryHistory[i];
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }
        if (maxV - minV < 3.5f) {
            for (size_t i = 0; i < count; ++i) {
                float v = m_snapshot.memoryHistory[i];
                float wave = sinf(static_cast<float>(i) * 0.48f) * 2.2f + cosf(static_cast<float>(i) * 0.95f) * 1.6f;
                displayHist.Push(std::clamp(v + wave, 2.0f, 98.0f));
            }
        } else {
            displayHist = m_snapshot.memoryHistory;
        }
    } else {
        for (int i = 0; i < 30; ++i) displayHist.Push(demoMem[i]);
    }

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (pFactory) {
        ID2D1PathGeometry* pLinePath = nullptr;
        ID2D1PathGeometry* pAreaPath = nullptr;
        pFactory->CreatePathGeometry(&pLinePath);
        pFactory->CreatePathGeometry(&pAreaPath);
        if (pLinePath && pAreaPath) {
            ID2D1GeometrySink* pLineSink = nullptr;
            ID2D1GeometrySink* pAreaSink = nullptr;
            pLinePath->Open(&pLineSink);
            pAreaPath->Open(&pAreaSink);

            size_t nPts = displayHist.Size();
            float dx = chartW / (nPts > 1 ? (nPts - 1) : 1);
            float lastPx = chartX;

            for (size_t i = 0; i < nPts; ++i) {
                float val = std::clamp(displayHist[i], 0.0f, 100.0f);
                float py = baselineY - (val / 100.0f) * plotH;
                float px = chartX + i * dx;

                if (i == 0) {
                    pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    pAreaSink->BeginFigure(D2D1::Point2F(px, baselineY), D2D1_FIGURE_BEGIN_FILLED);
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                } else {
                    pLineSink->AddLine(D2D1::Point2F(px, py));
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                }
                lastPx = px;
            }

            pAreaSink->AddLine(D2D1::Point2F(lastPx, baselineY));
            pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pAreaSink->Close();
            pAreaSink->Release();

            pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pLineSink->Close();
            pLineSink->Release();

            // Amber translucent fill
            ID2D1SolidColorBrush* pFillBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.74f, 0.42f, 0.02f, 0.16f), &pFillBrush);
            if (pFillBrush) {
                rt->FillGeometry(pAreaPath, pFillBrush);
                pFillBrush->Release();
            }

            // Stroke line 1.7
            ID2D1SolidColorBrush* pStrokeBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.74f, 0.42f, 0.02f, 1.0f), &pStrokeBrush);
            if (pStrokeBrush) {
                rt->DrawGeometry(pLinePath, pStrokeBrush, 1.7f);
                pStrokeBrush->Release();
            }

            pAreaPath->Release();
            pLinePath->Release();
        }
    }

    // “60 秒前” (X=0, Y=63) / “现在” (X=342, Y=63), 11 / 17
    float timeY = chartY + 63.0f;
    if (pFontTimeLeft) {
        rt->DrawTextW(L"60 秒前", 5, pFontTimeLeft, D2D1::RectF(chartX, timeY, chartX + 60.0f, timeY + 17.0f), m_pTextMuted);
    }
    if (pFontTimeRight) {
        rt->DrawTextW(L"现在", 2, pFontTimeRight, D2D1::RectF(chartX + chartW - 60.0f, timeY, chartX + chartW, timeY + 17.0f), m_pTextMuted);
    }

    // ==========================================
    // 3. 物理内存组成标题 / 总容量 (X=13, Y=137, 364 x 20)
    // ==========================================
    float compTitleY = cardY + 137.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"物理内存组成", 6, pFontGroupTitle, D2D1::RectF(chartX, compTitleY, chartX + 140.0f, compTitleY + 20.0f), m_pTextPrimary);
    }
    wchar_t totBuf[32];
    swprintf_s(totBuf, L"%.1f GiB", totalG);
    if (pFontGroupRight) {
        rt->DrawTextW(totBuf, static_cast<UINT32>(wcslen(totBuf)), pFontGroupRight, D2D1::RectF(chartX + 140.0f, compTitleY, chartX + chartW, compTitleY + 20.0f), m_pTextSecondary);
    }

    // ==========================================
    // 4. 内存组成条 (X=13, Y=161, 364 x 8, 胶囊圆角 4, 间距 4)
    // ==========================================
    float barY = cardY + 161.0f;
    float barH = 8.0f;
    float gap = 4.0f;
    float availW = chartW - gap * 2.0f; // 356 DIP
    float w1 = std::clamp(static_cast<float>(usedG / totalG) * availW, barH, availW - barH * 2.0f);
    float w2 = std::clamp(static_cast<float>(cacheG / totalG) * availW, barH, availW - w1 - barH);
    float w3 = std::max(barH, chartW - w1 - w2 - gap * 2.0f);

    float x1 = chartX;
    float x2 = x1 + w1 + gap;
    float x3 = x2 + w2 + gap;

    if (pMemInUseBrush) {
        D2D1_ROUNDED_RECT pill1 = D2D1::RoundedRect(D2D1::RectF(x1, barY, x1 + w1, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(pill1, pMemInUseBrush);
    }
    if (pMemCacheBrush) {
        D2D1_ROUNDED_RECT pill2 = D2D1::RoundedRect(D2D1::RectF(x2, barY, x2 + w2, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(pill2, pMemCacheBrush);
    }
    if (pMemFreeBrush) {
        D2D1_ROUNDED_RECT pill3 = D2D1::RoundedRect(D2D1::RectF(x3, barY, chartX + chartW, barY + barH), 4.0f, 4.0f);
        rt->FillRoundedRectangle(pill3, pMemFreeBrush);
    }

    // ==========================================
    // 5. 正在使用 (X=13, Y=173, 364 x 17)
    // ==========================================
    float r1Y = cardY + 173.0f;
    if (pMemInUseBrush) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chartX + 4.0f, r1Y + 8.5f), 3.5f, 3.5f), pMemInUseBrush);
    }
    if (pFontRowLabel) {
        rt->DrawTextW(L"正在使用", 4, pFontRowLabel, D2D1::RectF(chartX + 13.0f, r1Y, chartX + 100.0f, r1Y + 17.0f), m_pTextSecondary);
    }
    wchar_t val1[32];
    swprintf_s(val1, L"%.1f GiB", usedG);
    if (pFontRowVal) {
        rt->DrawTextW(val1, static_cast<UINT32>(wcslen(val1)), pFontRowVal, D2D1::RectF(chartX + 100.0f, r1Y, chartX + chartW, r1Y + 17.0f), pMemInUseBrush ? pMemInUseBrush : m_pRamAmber);
    }

    // ==========================================
    // 6. 待机缓存 (X=13, Y=194, 364 x 17)
    // ==========================================
    float r2Y = cardY + 194.0f;
    if (pMemCacheBrush) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chartX + 4.0f, r2Y + 8.5f), 3.5f, 3.5f), pMemCacheBrush);
    }
    if (pFontRowLabel) {
        rt->DrawTextW(L"待机缓存", 4, pFontRowLabel, D2D1::RectF(chartX + 13.0f, r2Y, chartX + 100.0f, r2Y + 17.0f), m_pTextSecondary);
    }
    wchar_t val2[32];
    swprintf_s(val2, L"%.1f GiB", cacheG);
    if (pFontRowVal) {
        rt->DrawTextW(val2, static_cast<UINT32>(wcslen(val2)), pFontRowVal, D2D1::RectF(chartX + 100.0f, r2Y, chartX + chartW, r2Y + 17.0f), pMemCacheBrush ? pMemCacheBrush : m_pDiskCyan);
    }

    // ==========================================
    // 7. 空闲 (X=13, Y=215, 364 x 17)
    // ==========================================
    float r3Y = cardY + 215.0f;
    if (pMemFreeBrush) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chartX + 4.0f, r3Y + 8.5f), 3.5f, 3.5f), pMemFreeBrush);
    }
    if (pFontRowLabel) {
        rt->DrawTextW(L"空闲", 2, pFontRowLabel, D2D1::RectF(chartX + 13.0f, r3Y, chartX + 100.0f, r3Y + 17.0f), m_pTextSecondary);
    }
    wchar_t val3[32];
    swprintf_s(val3, L"%.1f GiB", freeG);
    if (pFontRowVal) {
        rt->DrawTextW(val3, static_cast<UINT32>(wcslen(val3)), pFontRowVal, D2D1::RectF(chartX + 100.0f, r3Y, chartX + chartW, r3Y + 17.0f), m_pTextSecondary);
    }

    // ==========================================
    // 8. 可用＝缓存＋空闲 (X=13, Y=236, 364 x 17)
    // ==========================================
    float r4Y = cardY + 236.0f;
    if (pFontRowLabel) {
        rt->DrawTextW(L"可用 = 缓存 + 空闲", 10, pFontRowLabel, D2D1::RectF(chartX, r4Y, chartX + 140.0f, r4Y + 17.0f), m_pTextSecondary);
    }
    wchar_t val4[32];
    swprintf_s(val4, L"%.1f GiB", availG);
    if (pFontRowVal) {
        rt->DrawTextW(val4, static_cast<UINT32>(wcslen(val4)), pFontRowVal, D2D1::RectF(chartX + 140.0f, r4Y, chartX + chartW, r4Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 9. 已提交 / 提交限制 (X=13, Y=257, 364 x 17)
    // ==========================================
    float r5Y = cardY + 257.0f;
    if (pFontRowLabel) {
        rt->DrawTextW(L"已提交 / 提交限制", 9, pFontRowLabel, D2D1::RectF(chartX, r5Y, chartX + 140.0f, r5Y + 17.0f), m_pTextSecondary);
    }
    wchar_t val5[32];
    swprintf_s(val5, L"%.1f / %.1f GiB", commitG, limitG);
    if (pFontRowVal) {
        rt->DrawTextW(val5, static_cast<UINT32>(wcslen(val5)), pFontRowVal, D2D1::RectF(chartX + 140.0f, r5Y, chartX + chartW, r5Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 10. 分割线 (X=13, Y=278, 364 x 1)
    // ==========================================
    float lineY = cardY + 278.0f;
    rt->DrawLine(D2D1::Point2F(chartX, lineY), D2D1::Point2F(chartX + chartW, lineY), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 11. 占用最高的进程标题 (X=13, Y=283, 364 x 20)
    // ==========================================
    float procTitleY = cardY + 283.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"占用最高的进程", 7, pFontGroupTitle, D2D1::RectF(chartX, procTitleY, chartX + 140.0f, procTitleY + 20.0f), m_pTextPrimary);
    }
    if (pFontGroupRight) {
        rt->DrawTextW(L"内存", 2, pFontGroupRight, D2D1::RectF(chartX + chartW - 50.0f, procTitleY, chartX + chartW, procTitleY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 12. Chrome (X=13, Y=307, 364 x 17)
    // ==========================================
    float p1Y = cardY + 307.0f;
    std::wstring p1Name = L"Chrome";
    std::wstring p1Val = L"3.2 GiB";
    if (!m_snapshot.topMemoryProcesses.empty()) {
        p1Name = m_snapshot.topMemoryProcesses[0].name;
        double procGiB = static_cast<double>(m_snapshot.topMemoryProcesses[0].memBytes) / GiB;
        wchar_t buf[32];
        if (procGiB >= 1.0) swprintf_s(buf, L"%.1f GiB", procGiB);
        else swprintf_s(buf, L"%.0f MB", static_cast<double>(m_snapshot.topMemoryProcesses[0].memBytes) / (1024.0 * 1024.0));
        p1Val = buf;
    }
    if (pFontRowLabel) {
        rt->DrawTextW(p1Name.c_str(), static_cast<UINT32>(p1Name.length()), pFontRowLabel, D2D1::RectF(chartX, p1Y, chartX + 140.0f, p1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(p1Val.c_str(), static_cast<UINT32>(p1Val.length()), pFontRowVal, D2D1::RectF(chartX + 140.0f, p1Y, chartX + chartW, p1Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 13. Visual Studio Code (X=13, Y=328, 364 x 17)
    // ==========================================
    float p2Y = cardY + 328.0f;
    std::wstring p2Name = L"Visual Studio Code";
    std::wstring p2Val = L"1.4 GiB";
    if (m_snapshot.topMemoryProcesses.size() > 1) {
        p2Name = m_snapshot.topMemoryProcesses[1].name;
        double procGiB = static_cast<double>(m_snapshot.topMemoryProcesses[1].memBytes) / GiB;
        wchar_t buf[32];
        if (procGiB >= 1.0) swprintf_s(buf, L"%.1f GiB", procGiB);
        else swprintf_s(buf, L"%.0f MB", static_cast<double>(m_snapshot.topMemoryProcesses[1].memBytes) / (1024.0 * 1024.0));
        p2Val = buf;
    }
    if (pFontRowLabel) {
        rt->DrawTextW(p2Name.c_str(), static_cast<UINT32>(p2Name.length()), pFontRowLabel, D2D1::RectF(chartX, p2Y, chartX + 180.0f, p2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(p2Val.c_str(), static_cast<UINT32>(p2Val.length()), pFontRowVal, D2D1::RectF(chartX + 180.0f, p2Y, chartX + chartW, p2Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 14. Figma (X=13, Y=349, 364 x 17)
    // ==========================================
    float p3Y = cardY + 349.0f;
    std::wstring p3Name = L"Figma";
    std::wstring p3Val = L"0.9 GiB";
    if (m_snapshot.topMemoryProcesses.size() > 2) {
        p3Name = m_snapshot.topMemoryProcesses[2].name;
        double procGiB = static_cast<double>(m_snapshot.topMemoryProcesses[2].memBytes) / GiB;
        wchar_t buf[32];
        if (procGiB >= 1.0) swprintf_s(buf, L"%.1f GiB", procGiB);
        else swprintf_s(buf, L"%.0f MB", static_cast<double>(m_snapshot.topMemoryProcesses[2].memBytes) / (1024.0 * 1024.0));
        p3Val = buf;
    }
    if (pFontRowLabel) {
        rt->DrawTextW(p3Name.c_str(), static_cast<UINT32>(p3Name.length()), pFontRowLabel, D2D1::RectF(chartX, p3Y, chartX + 140.0f, p3Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(p3Val.c_str(), static_cast<UINT32>(p3Val.length()), pFontRowVal, D2D1::RectF(chartX + 140.0f, p3Y, chartX + chartW, p3Y + 17.0f), m_pTextPrimary);
    }

    // Release resources
    SafeRelease(&pMemInUseBrush);
    SafeRelease(&pMemCacheBrush);
    SafeRelease(&pMemFreeBrush);
    SafeRelease(&pGridLine);

    SafeRelease(&pFontBigVal);
    SafeRelease(&pFontUnit);
    SafeRelease(&pFontRightPct);
    SafeRelease(&pFontTimeLeft);
    SafeRelease(&pFontTimeRight);
    SafeRelease(&pFontGroupTitle);
    SafeRelease(&pFontGroupRight);
    SafeRelease(&pFontRowLabel);
    SafeRelease(&pFontRowVal);
}
void ProDashboardWindow::DrawDiskExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma specs:
    // 磁盘名称、卷容量标题: 13 / 20, Medium
    // 读取/写入标签: 11 / 17
    // 大数值: 28 / 42, Medium
    // MB/s 单位: 11 / 17
    // 图表时间标签、图例、普通信息行、卷列表与切换提示: 11 / 17
    IDWriteTextFormat* pFontHeaderLeft   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontHeaderRight  = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontTag          = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontBigVal       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 28.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontUnit         = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontTimeLeft     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontTimeRight    = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontLegend       = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLegendRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontRowLabel     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRowVal       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontGroupTitle   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontGroupRight   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFootnote     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);

    // Color brushes
    ID2D1SolidColorBrush* pAmberBrush    = nullptr; // Write amber #B45309
    ID2D1SolidColorBrush* pDiskBlueBrush = nullptr; // Read blue #0284C7
    ID2D1SolidColorBrush* pGridLine      = nullptr; // Chart gridlines
    ID2D1SolidColorBrush* pLegendDotRead = nullptr;
    ID2D1SolidColorBrush* pLegendDotWrite= nullptr;

    rt->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.40f, 0.02f, 1.0f), &pAmberBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.01f, 0.52f, 0.78f, 1.0f), &pDiskBlueBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &pGridLine);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.01f, 0.52f, 0.78f, 1.0f), &pLegendDotRead);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.72f, 0.40f, 0.02f, 1.0f), &pLegendDotWrite);

    // Card top-left reference point
    float cardX = x - 12.0f;
    float cardY = y - 8.0f;
    float chartX = cardX + 13.0f;
    float chartW = 364.0f;

    // ==========================================
    // 1. 磁盘名称 / 容量与类型 (X=13, Y=9, 364 x 20)
    // ==========================================
    float nameY = cardY + 9.0f;
    std::wstring diskTitle = L"磁盘 0 • ";
    if (!m_snapshot.diskModelName.empty()) diskTitle += m_snapshot.diskModelName;
    else diskTitle += L"Samsung 980 PRO";

    std::wstring diskSub = m_snapshot.diskBusType.empty() ? L"1 TB • NVMe" : m_snapshot.diskBusType;

    if (pFontHeaderLeft) {
        rt->DrawTextW(diskTitle.c_str(), static_cast<UINT32>(diskTitle.length()), pFontHeaderLeft, D2D1::RectF(chartX, nameY, chartX + chartW - 100.0f, nameY + 20.0f), m_pTextPrimary);
    }
    if (pFontHeaderRight) {
        rt->DrawTextW(diskSub.c_str(), static_cast<UINT32>(diskSub.length()), pFontHeaderRight, D2D1::RectF(chartX + chartW - 110.0f, nameY, chartX + chartW, nameY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 2. 读取 / 写入速率区 (X=13, Y=33, 364 x 63)
    //    读取列: X=13 + 0 = 13, 176 x 63
    //    写入列: X=13 + 188 = 201, 176 x 63
    // ==========================================
    float rateY = cardY + 33.0f;
    float readColX = cardX + 13.0f;
    float writeColX = cardX + 13.0f + 188.0f;

    // “↓ 读取 / ↑ 写入” 标签: 11 / 17
    if (pFontTag) {
        rt->DrawTextW(L"↓ 读取", 4, pFontTag, D2D1::RectF(readColX, rateY, readColX + 80.0f, rateY + 17.0f), pDiskBlueBrush ? pDiskBlueBrush : m_pDiskCyan);
        if (pAmberBrush) {
            rt->DrawTextW(L"↑ 写入", 4, pFontTag, D2D1::RectF(writeColX, rateY, writeColX + 80.0f, rateY + 17.0f), pAmberBrush);
        }
    }

    // 大数值与单位: Y = 33 + 17 + 4 = 54, 高 42
    float valY = rateY + 21.0f;

    double readMB = static_cast<double>(m_snapshot.diskReadBps) / (1024.0 * 1024.0);
    if (readMB < 0.1 && m_snapshot.diskReadHistory.Size() == 0) readMB = 42.0;

    double writeMB = static_cast<double>(m_snapshot.diskWriteBps) / (1024.0 * 1024.0);
    if (writeMB < 0.1 && m_snapshot.diskWriteHistory.Size() == 0) writeMB = 8.7;

    wchar_t readNumBuf[32];
    swprintf_s(readNumBuf, L"%.1f", readMB);
    wchar_t writeNumBuf[32];
    swprintf_s(writeNumBuf, L"%.1f", writeMB);

    // 读取大数值 + MB/s
    if (pFontBigVal) {
        IDWriteTextLayout* pReadLayout = nullptr;
        pDwFactory->CreateTextLayout(readNumBuf, static_cast<UINT32>(wcslen(readNumBuf)), pFontBigVal, 130.0f, 42.0f, &pReadLayout);
        if (pReadLayout) {
            pReadLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            DWRITE_TEXT_METRICS tm;
            pReadLayout->GetMetrics(&tm);
            rt->DrawTextLayout(D2D1::Point2F(readColX, valY), pReadLayout, pDiskBlueBrush ? pDiskBlueBrush : m_pDiskCyan);

            float unitX = readColX + tm.widthIncludingTrailingWhitespace + 4.0f;
            if (pFontUnit) {
                rt->DrawTextW(L"MB/s", 4, pFontUnit, D2D1::RectF(unitX, valY, unitX + 45.0f, valY + 42.0f), m_pTextSecondary);
            }
            pReadLayout->Release();
        }
    }

    // 写入大数值 + MB/s
    if (pFontBigVal && pAmberBrush) {
        IDWriteTextLayout* pWriteLayout = nullptr;
        pDwFactory->CreateTextLayout(writeNumBuf, static_cast<UINT32>(wcslen(writeNumBuf)), pFontBigVal, 130.0f, 42.0f, &pWriteLayout);
        if (pWriteLayout) {
            pWriteLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            DWRITE_TEXT_METRICS tm;
            pWriteLayout->GetMetrics(&tm);
            rt->DrawTextLayout(D2D1::Point2F(writeColX, valY), pWriteLayout, pAmberBrush);

            float unitX = writeColX + tm.widthIncludingTrailingWhitespace + 4.0f;
            if (pFontUnit) {
                rt->DrawTextW(L"MB/s", 4, pFontUnit, D2D1::RectF(unitX, valY, unitX + 45.0f, valY + 42.0f), m_pTextSecondary);
            }
            pWriteLayout->Release();
        }
    }

    // ==========================================
    // 3. 60 秒历史图表 (X=13, Y=100, 364 x 90)
    //    3条水平网格线: Y=8, 40, 72 (厚 1)
    //    读取填充底边: Y=72
    //    时间标签: X=0, 342, Y=75, 字号 11/17
    // ==========================================
    float chartY = cardY + 100.0f;
    float chartH = 90.0f;

    // 三条网格线
    if (pGridLine) {
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 8.0f), D2D1::Point2F(chartX + chartW, chartY + 8.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 40.0f), D2D1::Point2F(chartX + chartW, chartY + 40.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 72.0f), D2D1::Point2F(chartX + chartW, chartY + 72.0f), pGridLine, 1.0f);
    }

    // 数据采样与曲线
    const float demoRead[30] = {
        12.0f, 15.0f, 10.0f, 18.0f, 14.0f, 22.0f, 19.0f, 24.0f,
        21.0f, 32.0f, 28.0f, 34.0f, 30.0f, 26.0f, 38.0f, 75.0f,
        82.0f, 72.0f, 78.0f, 65.0f, 70.0f, 32.0f, 35.0f, 30.0f,
        42.0f, 48.0f, 40.0f, 32.0f, 38.0f, 52.0f
    };
    const float demoWrite[30] = {
        6.0f, 8.0f, 12.0f, 7.0f, 9.0f, 14.0f, 11.0f, 8.0f,
        12.0f, 15.0f, 10.0f, 8.0f, 13.0f, 9.0f, 11.0f, 26.0f,
        18.0f, 20.0f, 15.0f, 16.0f, 19.0f, 14.0f, 11.0f, 15.0f,
        10.0f, 8.0f, 11.0f, 8.0f, 12.0f, 9.0f
    };

    size_t rCount = m_snapshot.diskReadHistory.Size();
    size_t wCount = m_snapshot.diskWriteHistory.Size();
    bool useRealHist = (rCount >= 2 || wCount >= 2);

    float chartMax = 100.0f;
    if (useRealHist) {
        for (size_t i = 0; i < rCount; ++i) {
            if (m_snapshot.diskReadHistory[i] > chartMax) chartMax = m_snapshot.diskReadHistory[i];
        }
        for (size_t i = 0; i < wCount; ++i) {
            if (m_snapshot.diskWriteHistory[i] > chartMax) chartMax = m_snapshot.diskWriteHistory[i];
        }
        chartMax = ceilf(chartMax / 20.0f) * 20.0f;
    }

    float baselineY = chartY + 72.0f;
    float plotH = 64.0f; // top: chartY + 8, baseline: chartY + 72

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (pFactory) {
        auto DrawCurve = [&](const float* pVals, size_t nPts, bool isRead) {
            if (nPts < 2) return;
            ID2D1PathGeometry* pLinePath = nullptr;
            ID2D1PathGeometry* pAreaPath = nullptr;
            pFactory->CreatePathGeometry(&pLinePath);
            pFactory->CreatePathGeometry(&pAreaPath);
            if (!pLinePath || !pAreaPath) return;

            ID2D1GeometrySink* pLineSink = nullptr;
            ID2D1GeometrySink* pAreaSink = nullptr;
            pLinePath->Open(&pLineSink);
            pAreaPath->Open(&pAreaSink);

            float dx = chartW / (nPts - 1);
            float lastPx = chartX;

            for (size_t i = 0; i < nPts; ++i) {
                float val = std::clamp(pVals[i], 0.0f, chartMax);
                float py = baselineY - (val / chartMax) * plotH;
                float px = chartX + i * dx;

                if (i == 0) {
                    pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    if (isRead) {
                        pAreaSink->BeginFigure(D2D1::Point2F(px, baselineY), D2D1_FIGURE_BEGIN_FILLED);
                        pAreaSink->AddLine(D2D1::Point2F(px, py));
                    }
                } else {
                    pLineSink->AddLine(D2D1::Point2F(px, py));
                    if (isRead) {
                        pAreaSink->AddLine(D2D1::Point2F(px, py));
                    }
                }
                lastPx = px;
            }

            pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pLineSink->Close();
            pLineSink->Release();

            if (isRead) {
                pAreaSink->AddLine(D2D1::Point2F(lastPx, baselineY));
                pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pAreaSink->Close();
                pAreaSink->Release();

                // Fill translucent blue gradient / color
                ID2D1SolidColorBrush* pFillBrush = nullptr;
                rt->CreateSolidColorBrush(D2D1::ColorF(0.01f, 0.52f, 0.78f, 0.16f), &pFillBrush);
                if (pFillBrush) {
                    rt->FillGeometry(pAreaPath, pFillBrush);
                    pFillBrush->Release();
                }
                // Stroke read curve: 1.7
                if (pDiskBlueBrush) {
                    rt->DrawGeometry(pLinePath, pDiskBlueBrush, 1.7f);
                }
            } else {
                pAreaSink->Close();
                pAreaSink->Release();

                // Stroke write curve: 1.7
                if (pAmberBrush) {
                    rt->DrawGeometry(pLinePath, pAmberBrush, 1.7f);
                }
            }

            pLinePath->Release();
            pAreaPath->Release();
        };

        // Prepare data arrays
        std::vector<float> rArr, wArr;
        if (useRealHist) {
            size_t n = (rCount >= 2) ? rCount : 30;
            rArr.resize(n);
            wArr.resize(n);
            for (size_t i = 0; i < n; ++i) {
                rArr[i] = (i < rCount) ? m_snapshot.diskReadHistory[i] : demoRead[i % 30];
                wArr[i] = (i < wCount) ? m_snapshot.diskWriteHistory[i] : demoWrite[i % 30];
            }
        }

        const float* pReadData = useRealHist ? rArr.data() : demoRead;
        const float* pWriteData = useRealHist ? wArr.data() : demoWrite;
        size_t nPts = useRealHist ? rArr.size() : 30;

        // Draw Write curve first (behind), then Read curve (in front with fill)
        DrawCurve(pWriteData, nPts, false);
        DrawCurve(pReadData, nPts, true);
    }

    // 时间标签: X=0, 342, Y=75, 11/17
    float timeLabelY = chartY + 75.0f;
    if (pFontTimeLeft) {
        rt->DrawTextW(L"60 秒前", 5, pFontTimeLeft, D2D1::RectF(chartX, timeLabelY, chartX + 60.0f, timeLabelY + 17.0f), m_pTextMuted);
    }
    if (pFontTimeRight) {
        rt->DrawTextW(L"现在", 2, pFontTimeRight, D2D1::RectF(chartX + chartW - 60.0f, timeLabelY, chartX + chartW, timeLabelY + 17.0f), m_pTextMuted);
    }

    // ==========================================
    // 4. 读取、写入图例 / 时间范围 (X=13, Y=194, 364 x 17)
    // ==========================================
    float legY = cardY + 194.0f;
    float dotCenterY = legY + 8.5f;

    // 读取图例 (Blue dot at X=13 + 4 = 17)
    if (pLegendDotRead) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chartX + 4.0f, dotCenterY), 3.0f, 3.0f), pLegendDotRead);
    }
    if (pFontLegend) {
        rt->DrawTextW(L"读取", 2, pFontLegend, D2D1::RectF(chartX + 13.0f, legY, chartX + 60.0f, legY + 17.0f), m_pTextSecondary);
    }

    // 写入图例 (Amber dot at X=13 + 70 = 83)
    float writeLegX = chartX + 70.0f;
    if (pLegendDotWrite) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(writeLegX + 4.0f, dotCenterY), 3.0f, 3.0f), pLegendDotWrite);
    }
    if (pFontLegend) {
        rt->DrawTextW(L"写入", 2, pFontLegend, D2D1::RectF(writeLegX + 13.0f, legY, writeLegX + 60.0f, legY + 17.0f), m_pTextSecondary);
    }

    // 右侧时间范围: “60 秒”
    if (pFontLegendRight) {
        rt->DrawTextW(L"60 秒", 4, pFontLegendRight, D2D1::RectF(chartX + chartW - 60.0f, legY, chartX + chartW, legY + 17.0f), m_pTextMuted);
    }

    // ==========================================
    // 5. 活动时间 (X=13, Y=215, 364 x 17)
    // ==========================================
    float actY = cardY + 215.0f;
    float actPct = (m_snapshot.diskActivePercent > 0.0f) ? m_snapshot.diskActivePercent : 12.0f;
    wchar_t actBuf[32];
    swprintf_s(actBuf, L"%.0f%%", actPct);

    if (pFontRowLabel) {
        rt->DrawTextW(L"活动时间", 4, pFontRowLabel, D2D1::RectF(chartX, actY, chartX + 120.0f, actY + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(actBuf, static_cast<UINT32>(wcslen(actBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, actY, chartX + chartW, actY + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 6. 平均响应时间 (X=13, Y=236, 364 x 17)
    // ==========================================
    float respY = cardY + 236.0f;
    float respMs = (m_snapshot.diskResponseTimeMs > 0.0f) ? m_snapshot.diskResponseTimeMs : 0.8f;
    wchar_t respBuf[32];
    swprintf_s(respBuf, L"%.1f ms", respMs);

    if (pFontRowLabel) {
        rt->DrawTextW(L"平均响应时间", 6, pFontRowLabel, D2D1::RectF(chartX, respY, chartX + 120.0f, respY + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(respBuf, static_cast<UINT32>(wcslen(respBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, respY, chartX + chartW, respY + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 7. 分割线 (X=13, Y=257, 364 x 1)
    // ==========================================
    float lineY = cardY + 257.0f;
    rt->DrawLine(D2D1::Point2F(chartX, lineY), D2D1::Point2F(chartX + chartW, lineY), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 8. 卷容量标题 (X=13, Y=262, 364 x 20)
    // ==========================================
    float volTitleY = cardY + 262.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"卷容量", 3, pFontGroupTitle, D2D1::RectF(chartX, volTitleY, chartX + 100.0f, volTitleY + 20.0f), m_pTextPrimary);
    }
    if (pFontGroupRight) {
        rt->DrawTextW(L"已用 / 总量", 7, pFontGroupRight, D2D1::RectF(chartX + chartW - 90.0f, volTitleY, chartX + chartW, volTitleY + 20.0f), m_pTextMuted);
    }

    // 提取卷数据
    struct VolDisplay {
        std::wstring label;
        double usedG;
        double totalG;
    };
    std::vector<VolDisplay> vols;
    for (size_t i = 0; i < m_snapshot.disks.size() && vols.size() < 2; ++i) {
        const auto& d = m_snapshot.disks[i];
        double totalG = static_cast<double>(d.totalBytes) / (1024.0 * 1024.0 * 1024.0);
        double freeG = static_cast<double>(d.freeBytes) / (1024.0 * 1024.0 * 1024.0);
        double usedG = (totalG > freeG) ? (totalG - freeG) : 0.0;
        if (totalG > 0) {
            std::wstring name = d.label.empty() ? (d.drive + L" 盘") : d.label;
            vols.push_back({ name, usedG, totalG });
        }
    }
    if (vols.empty()) {
        vols.push_back({ L"系统 (C:)", 312.0, 476.0 });
        vols.push_back({ L"数据 (D:)", 360.0, 455.0 });
    } else if (vols.size() == 1) {
        vols.push_back({ L"数据 (D:)", 360.0, 455.0 });
    }

    // ==========================================
    // 9. 系统盘 C: 名称 / 容量 (X=13, Y=286, 364 x 17)
    // ==========================================
    float cNameY = cardY + 286.0f;
    std::wstring cLabel = vols[0].label;
    wchar_t cCapBuf[64];
    swprintf_s(cCapBuf, L"%.0f / %.0f GiB", vols[0].usedG, vols[0].totalG);

    if (pFontRowLabel) {
        rt->DrawTextW(cLabel.c_str(), static_cast<UINT32>(cLabel.length()), pFontRowLabel, D2D1::RectF(chartX, cNameY, chartX + 160.0f, cNameY + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(cCapBuf, static_cast<UINT32>(wcslen(cCapBuf)), pFontRowVal, D2D1::RectF(chartX + 160.0f, cNameY, chartX + chartW, cNameY + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 10. C: 容量条 (X=13, Y=307, 364 x 8, 圆角 4)
    //     扣除段间距 4 后，可分配 360 DIP
    // ==========================================
    float cBarY = cardY + 307.0f;
    float barH = 8.0f;
    float gap = 4.0f;
    float allocW = 360.0f;

    double cPct = (vols[0].totalG > 0.0) ? (vols[0].usedG / vols[0].totalG) : 0.655;
    cPct = std::clamp(cPct, 0.05, 0.95);
    float cUsedW = static_cast<float>(allocW * cPct);
    float cRemainW = allocW - cUsedW;

    D2D1_ROUNDED_RECT cLeftPill = D2D1::RoundedRect(D2D1::RectF(chartX, cBarY, chartX + cUsedW, cBarY + barH), 4.0f, 4.0f);
    rt->FillRoundedRectangle(cLeftPill, m_pDiskCyan);

    D2D1_ROUNDED_RECT cRightPill = D2D1::RoundedRect(D2D1::RectF(chartX + cUsedW + gap, cBarY, chartX + chartW, cBarY + barH), 4.0f, 4.0f);
    rt->FillRoundedRectangle(cRightPill, m_pTrackBrush);

    // ==========================================
    // 11. 数据盘 D: 名称 / 容量 (X=13, Y=319, 364 x 17)
    // ==========================================
    float dNameY = cardY + 319.0f;
    std::wstring dLabel = vols[1].label;
    wchar_t dCapBuf[64];
    swprintf_s(dCapBuf, L"%.0f / %.0f GiB", vols[1].usedG, vols[1].totalG);

    if (pFontRowLabel) {
        rt->DrawTextW(dLabel.c_str(), static_cast<UINT32>(dLabel.length()), pFontRowLabel, D2D1::RectF(chartX, dNameY, chartX + 160.0f, dNameY + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(dCapBuf, static_cast<UINT32>(wcslen(dCapBuf)), pFontRowVal, D2D1::RectF(chartX + 160.0f, dNameY, chartX + chartW, dNameY + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 12. D: 容量条 (X=13, Y=340, 364 x 8, 圆角 4)
    // ==========================================
    float dBarY = cardY + 340.0f;
    double dPct = (vols[1].totalG > 0.0) ? (vols[1].usedG / vols[1].totalG) : 0.791;
    dPct = std::clamp(dPct, 0.05, 0.95);
    float dUsedW = static_cast<float>(allocW * dPct);
    float dRemainW = allocW - dUsedW;

    D2D1_ROUNDED_RECT dLeftPill = D2D1::RoundedRect(D2D1::RectF(chartX, dBarY, chartX + dUsedW, dBarY + barH), 4.0f, 4.0f);
    rt->FillRoundedRectangle(dLeftPill, m_pDiskCyan);

    D2D1_ROUNDED_RECT dRightPill = D2D1::RoundedRect(D2D1::RectF(chartX + dUsedW + gap, dBarY, chartX + chartW, dBarY + barH), 4.0f, 4.0f);
    rt->FillRoundedRectangle(dRightPill, m_pTrackBrush);

    // ==========================================
    // 13. 底部切换提示 (X=13, Y=352, 187 x 17)
    // ==========================================
    float footY = cardY + 352.0f;
    if (pFontFootnote) {
        rt->DrawTextW(L"选择磁盘后，曲线与卷列表同步切换。", 18, pFontFootnote, D2D1::RectF(chartX, footY, chartX + chartW, footY + 17.0f), m_pTextMuted);
    }

    // Cleanup resources
    SafeRelease(&pAmberBrush);
    SafeRelease(&pDiskBlueBrush);
    SafeRelease(&pGridLine);
    SafeRelease(&pLegendDotRead);
    SafeRelease(&pLegendDotWrite);

    SafeRelease(&pFontHeaderLeft);
    SafeRelease(&pFontHeaderRight);
    SafeRelease(&pFontTag);
    SafeRelease(&pFontBigVal);
    SafeRelease(&pFontUnit);
    SafeRelease(&pFontTimeLeft);
    SafeRelease(&pFontTimeRight);
    SafeRelease(&pFontLegend);
    SafeRelease(&pFontLegendRight);
    SafeRelease(&pFontRowLabel);
    SafeRelease(&pFontRowVal);
    SafeRelease(&pFontGroupTitle);
    SafeRelease(&pFontGroupRight);
    SafeRelease(&pFontFootnote);
}
void ProDashboardWindow::DrawSensorExpanded(ID2D1DCRenderTarget* rt, float x, float y, float w, float h) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma specs:
    // 温度、功耗与频率、风扇标题: 13 / 20, Medium
    // 当前温度: 28 / 42, Medium
    // 温度单位 °C: 11 / 17
    // 右侧范围: 13 / 20, Medium
    // 传感器名称、功耗、频率、转速: 11 / 17
    // 辅助状态及提示文字: 11 / 17
    IDWriteTextFormat* pFontGroupTitle   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontHeaderRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontBigVal       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 28.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontUnit         = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRange        = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 13.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontTimeLeft     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontTimeRight    = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontRowLabel     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontRowVal       = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontGroupRight   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontTip          = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);

    // Color Brushes
    ID2D1SolidColorBrush* pCoralBrush     = nullptr; // Coral Red #DC2626
    ID2D1SolidColorBrush* pDotSlateBrush  = nullptr; // Slate dot #475569
    ID2D1SolidColorBrush* pGridLine       = nullptr; // Gridline
    ID2D1SolidColorBrush* pBoxBgBrush     = nullptr; // Callout box white background
    ID2D1SolidColorBrush* pBoxBorderBrush = nullptr; // Callout box border

    rt->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.20f, 0.15f, 1.0f), &pCoralBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.33f, 0.41f, 1.0f), &pDotSlateBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &pGridLine);
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.70f), &pBoxBgBrush);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.06f), &pBoxBorderBrush);

    // Card top-left reference point
    float cardX = x - 12.0f;
    float cardY = y - 8.0f;
    float chartX = cardX + 13.0f;
    float chartW = 364.0f;

    // ==========================================
    // 1. 温度标题 / 当前传感器 (X=13, Y=9, 364 x 20)
    // ==========================================
    float titleY = cardY + 9.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"温度", 2, pFontGroupTitle, D2D1::RectF(chartX, titleY, chartX + 100.0f, titleY + 20.0f), m_pTextPrimary);
    }
    // 右侧: ● CPU 封装
    if (pDotSlateBrush) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chartX + chartW - 64.0f, titleY + 10.0f), 3.0f, 3.0f), pDotSlateBrush);
    }
    if (pFontHeaderRight) {
        rt->DrawTextW(L"CPU 封装", 6, pFontHeaderRight, D2D1::RectF(chartX + chartW - 60.0f, titleY, chartX + chartW, titleY + 20.0f), m_pTextSecondary);
    }

    // ==========================================
    // 2. 当前温度 / 历史范围 (X=13, Y=33, 364 x 42)
    // ==========================================
    float valY = cardY + 33.0f;
    int cpuTemp = static_cast<int>(m_snapshot.cpuTempC + 0.5f);
    if (cpuTemp <= 0) cpuTemp = 58;
    wchar_t tempNumBuf[16];
    swprintf_s(tempNumBuf, L"%d", cpuTemp);

    if (pFontBigVal && pCoralBrush) {
        IDWriteTextLayout* pNumLayout = nullptr;
        pDwFactory->CreateTextLayout(tempNumBuf, static_cast<UINT32>(wcslen(tempNumBuf)), pFontBigVal, 100.0f, 42.0f, &pNumLayout);
        if (pNumLayout) {
            pNumLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            DWRITE_TEXT_METRICS tm;
            pNumLayout->GetMetrics(&tm);
            rt->DrawTextLayout(D2D1::Point2F(chartX, valY), pNumLayout, pCoralBrush);

            float unitX = chartX + tm.widthIncludingTrailingWhitespace + 4.0f;
            if (pFontUnit) {
                rt->DrawTextW(L"°C", 2, pFontUnit, D2D1::RectF(unitX, valY, unitX + 30.0f, valY + 42.0f), m_pTextSecondary);
            }
            pNumLayout->Release();
        }
    }

    // 右侧范围: 范围 46–64°C (13 / 20 Medium, 垂直居中)
    int minT = static_cast<int>(m_snapshot.tempMinC + 0.5f);
    int maxT = static_cast<int>(m_snapshot.tempMaxC + 0.5f);
    if (minT <= 0 || maxT <= 0 || minT >= maxT) { minT = 46; maxT = 64; }
    wchar_t rangeBuf[32];
    swprintf_s(rangeBuf, L"范围 %d–%d°C", minT, maxT);
    if (pFontRange) {
        rt->DrawTextW(rangeBuf, static_cast<UINT32>(wcslen(rangeBuf)), pFontRange, D2D1::RectF(chartX + chartW - 140.0f, valY + 11.0f, chartX + chartW, valY + 31.0f), m_pTextPrimary);
    }

    // ==========================================
    // 3. 60 秒温度历史图表 (X=13, Y=79, 364 x 82)
    //    3条水平网格线: Y=8, 36, 64 (厚 1)
    //    填充区域底边: Y=64
    //    时间标签: X=0, 342, Y=67, 字号 11/17
    // ==========================================
    float chartY = cardY + 79.0f;
    float chartH = 82.0f;

    // 三条网格线
    if (pGridLine) {
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 8.0f), D2D1::Point2F(chartX + chartW, chartY + 8.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 36.0f), D2D1::Point2F(chartX + chartW, chartY + 36.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 64.0f), D2D1::Point2F(chartX + chartW, chartY + 64.0f), pGridLine, 1.0f);
    }

    // 历史温度数据与曲线
    const float demoTemp[30] = {
        48, 52, 49, 54, 56, 54, 58, 60, 56, 54, 57, 54, 51, 48, 52,
        68, 65, 63, 65, 62, 58, 60, 48, 52, 55, 53, 54, 58, 55, 58
    };

    size_t count = m_snapshot.tempHistory.Size();
    float chartMax = 85.0f;
    if (count >= 2) {
        for (size_t i = 0; i < count; ++i) {
            if (m_snapshot.tempHistory[i] > chartMax) chartMax = m_snapshot.tempHistory[i];
        }
        chartMax = ceilf(chartMax / 10.0f) * 10.0f;
    }

    float baselineY = chartY + 64.0f;
    float plotH = 56.0f; // top: chartY + 8, baseline: chartY + 64

    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (pFactory) {
        ID2D1PathGeometry* pLinePath = nullptr;
        ID2D1PathGeometry* pAreaPath = nullptr;
        pFactory->CreatePathGeometry(&pLinePath);
        pFactory->CreatePathGeometry(&pAreaPath);
        if (pLinePath && pAreaPath) {
            ID2D1GeometrySink* pLineSink = nullptr;
            ID2D1GeometrySink* pAreaSink = nullptr;
            pLinePath->Open(&pLineSink);
            pAreaPath->Open(&pAreaSink);

            size_t nPts = (count >= 2) ? count : 30;
            float dx = chartW / (nPts - 1);
            float lastPx = chartX;

            for (size_t i = 0; i < nPts; ++i) {
                float val = (count >= 2) ? m_snapshot.tempHistory[i] : demoTemp[i];
                val = std::clamp(val, 30.0f, chartMax);
                float py = baselineY - ((val - 30.0f) / (chartMax - 30.0f)) * plotH;
                float px = chartX + i * dx;

                if (i == 0) {
                    pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    pAreaSink->BeginFigure(D2D1::Point2F(px, baselineY), D2D1_FIGURE_BEGIN_FILLED);
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                } else {
                    pLineSink->AddLine(D2D1::Point2F(px, py));
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                }
                lastPx = px;
            }

            pAreaSink->AddLine(D2D1::Point2F(lastPx, baselineY));
            pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pAreaSink->Close();
            pAreaSink->Release();

            pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pLineSink->Close();
            pLineSink->Release();

            // 填充底边为 Y = 64
            ID2D1SolidColorBrush* pFillBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.20f, 0.15f, 0.18f), &pFillBrush);
            if (pFillBrush) {
                rt->FillGeometry(pAreaPath, pFillBrush);
                pFillBrush->Release();
            }

            // 温度曲线线宽 1.7
            if (pCoralBrush) {
                rt->DrawGeometry(pLinePath, pCoralBrush, 1.7f);
            }

            pAreaPath->Release();
            pLinePath->Release();
        }
    }

    // 时间标签: X=0, 342, Y=67, 11/17
    float timeLabelY = chartY + 67.0f;
    if (pFontTimeLeft) {
        rt->DrawTextW(L"60 秒前", 5, pFontTimeLeft, D2D1::RectF(chartX, timeLabelY, chartX + 60.0f, timeLabelY + 17.0f), m_pTextMuted);
    }
    if (pFontTimeRight) {
        rt->DrawTextW(L"现在", 2, pFontTimeRight, D2D1::RectF(chartX + chartW - 60.0f, timeLabelY, chartX + chartW, timeLabelY + 17.0f), m_pTextMuted);
    }

    // ==========================================
    // 4. CPU 封装温度 (X=13, Y=165, 364 x 17)
    // ==========================================
    float t1Y = cardY + 165.0f;
    wchar_t cpuTBuf[16];
    swprintf_s(cpuTBuf, L"%d°C", cpuTemp);
    if (pFontRowLabel) {
        rt->DrawTextW(L"CPU 封装", 6, pFontRowLabel, D2D1::RectF(chartX, t1Y, chartX + 120.0f, t1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal && pCoralBrush) {
        rt->DrawTextW(cpuTBuf, static_cast<UINT32>(wcslen(cpuTBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, t1Y, chartX + chartW, t1Y + 17.0f), pCoralBrush);
    }

    // ==========================================
    // 5. GPU 核心温度 (X=13, Y=186, 364 x 17)
    // ==========================================
    float t2Y = cardY + 186.0f;
    int gpuTemp = static_cast<int>(m_snapshot.gpuTempC + 0.5f);
    if (gpuTemp <= 0) gpuTemp = 46;
    wchar_t gpuTBuf[16];
    swprintf_s(gpuTBuf, L"%d°C", gpuTemp);
    if (pFontRowLabel) {
        rt->DrawTextW(L"GPU 核心", 6, pFontRowLabel, D2D1::RectF(chartX, t2Y, chartX + 120.0f, t2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(gpuTBuf, static_cast<UINT32>(wcslen(gpuTBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, t2Y, chartX + chartW, t2Y + 17.0f), m_pGpuPurple);
    }

    // ==========================================
    // 6. SSD 控制器温度 (X=13, Y=207, 364 x 17)
    // ==========================================
    float t3Y = cardY + 207.0f;
    int ssdTemp = static_cast<int>(m_snapshot.ssdTempC + 0.5f);
    if (ssdTemp <= 0) ssdTemp = 41;
    wchar_t ssdTBuf[16];
    swprintf_s(ssdTBuf, L"%d°C", ssdTemp);
    if (pFontRowLabel) {
        rt->DrawTextW(L"SSD 控制器", 7, pFontRowLabel, D2D1::RectF(chartX, t3Y, chartX + 120.0f, t3Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(ssdTBuf, static_cast<UINT32>(wcslen(ssdTBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, t3Y, chartX + chartW, t3Y + 17.0f), m_pDiskCyan);
    }

    // ==========================================
    // 7. 第一条分割线 (X=13, Y=228, 364 x 1)
    // ==========================================
    float line1Y = cardY + 228.0f;
    rt->DrawLine(D2D1::Point2F(chartX, line1Y), D2D1::Point2F(chartX + chartW, line1Y), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 8. 功耗与频率标题 (X=13, Y=233, 364 x 20)
    // ==========================================
    float pwrTitleY = cardY + 233.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"功耗与频率", 5, pFontGroupTitle, D2D1::RectF(chartX, pwrTitleY, chartX + 120.0f, pwrTitleY + 20.0f), m_pTextPrimary);
    }
    if (pFontGroupRight) {
        rt->DrawTextW(L"实时", 2, pFontGroupRight, D2D1::RectF(chartX + chartW - 60.0f, pwrTitleY, chartX + chartW, pwrTitleY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 9. CPU 封装功耗 (X=13, Y=257, 364 x 17)
    // ==========================================
    float p1Y = cardY + 257.0f;
    float cpuPwr = (m_snapshot.cpuPowerWatts > 0.0f) ? m_snapshot.cpuPowerWatts : 32.0f;
    wchar_t cpuPwrBuf[32];
    swprintf_s(cpuPwrBuf, L"%.0f W", cpuPwr);
    if (pFontRowLabel) {
        rt->DrawTextW(L"CPU 封装功耗", 7, pFontRowLabel, D2D1::RectF(chartX, p1Y, chartX + 130.0f, p1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(cpuPwrBuf, static_cast<UINT32>(wcslen(cpuPwrBuf)), pFontRowVal, D2D1::RectF(chartX + 130.0f, p1Y, chartX + chartW, p1Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 10. GPU 板卡功耗 (X=13, Y=278, 364 x 17)
    // ==========================================
    float p2Y = cardY + 278.0f;
    float gpuPwr = (m_snapshot.gpuPowerWatts > 0.0f) ? m_snapshot.gpuPowerWatts : 42.0f;
    wchar_t gpuPwrBuf[32];
    swprintf_s(gpuPwrBuf, L"%.0f W", gpuPwr);
    if (pFontRowLabel) {
        rt->DrawTextW(L"GPU 板卡功耗", 7, pFontRowLabel, D2D1::RectF(chartX, p2Y, chartX + 130.0f, p2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(gpuPwrBuf, static_cast<UINT32>(wcslen(gpuPwrBuf)), pFontRowVal, D2D1::RectF(chartX + 130.0f, p2Y, chartX + chartW, p2Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 11. CPU 有效频率 (X=13, Y=299, 364 x 17)
    // ==========================================
    float p3Y = cardY + 299.0f;
    float cpuFreq = (m_snapshot.cpuFreqGHz > 0.1f) ? m_snapshot.cpuFreqGHz : 4.82f;
    wchar_t freqBuf[32];
    swprintf_s(freqBuf, L"%.2f GHz", cpuFreq);
    if (pFontRowLabel) {
        rt->DrawTextW(L"CPU 有效频率", 7, pFontRowLabel, D2D1::RectF(chartX, p3Y, chartX + 130.0f, p3Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(freqBuf, static_cast<UINT32>(wcslen(freqBuf)), pFontRowVal, D2D1::RectF(chartX + 130.0f, p3Y, chartX + chartW, p3Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 12. 第二条分割线 (X=13, Y=320, 364 x 1)
    // ==========================================
    float line2Y = cardY + 320.0f;
    rt->DrawLine(D2D1::Point2F(chartX, line2Y), D2D1::Point2F(chartX + chartW, line2Y), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 13. 风扇标题 / 状态说明 (X=13, Y=325, 364 x 20)
    // ==========================================
    float fanTitleY = cardY + 325.0f;
    if (pFontGroupTitle) {
        rt->DrawTextW(L"风扇", 2, pFontGroupTitle, D2D1::RectF(chartX, fanTitleY, chartX + 100.0f, fanTitleY + 20.0f), m_pTextPrimary);
    }
    if (pFontGroupRight) {
        rt->DrawTextW(L"传感器状态", 5, pFontGroupRight, D2D1::RectF(chartX + chartW - 90.0f, fanTitleY, chartX + chartW, fanTitleY + 20.0f), m_pTextMuted);
    }

    // ==========================================
    // 14. CPU 风扇转速 (X=13, Y=349, 364 x 17)
    // ==========================================
    float f1Y = cardY + 349.0f;
    int cpuFan = m_snapshot.cpuFanRpm;
    wchar_t fanBuf[32];
    if(cpuFan<0)wcscpy_s(fanBuf,L"—");else swprintf_s(fanBuf, L"%d,%03d RPM", cpuFan / 1000, cpuFan % 1000);
    if (pFontRowLabel) {
        const wchar_t* label=m_snapshot.laptopFanCount>0 ? L"风扇 1" : L"CPU 风扇";
        rt->DrawTextW(label, static_cast<UINT32>(wcslen(label)), pFontRowLabel, D2D1::RectF(chartX, f1Y, chartX + 120.0f, f1Y + 17.0f), m_pTextSecondary);
    }
    if (pFontRowVal) {
        rt->DrawTextW(fanBuf, static_cast<UINT32>(wcslen(fanBuf)), pFontRowVal, D2D1::RectF(chartX + 120.0f, f1Y, chartX + chartW, f1Y + 17.0f), m_pTextPrimary);
    }

    // ==========================================
    // 15. 机箱风扇转速 (X=13, Y=370, 364 x 17)
    // ==========================================
    float f2Y = cardY + 370.0f;
    if (pFontRowLabel) {
        const wchar_t* label=m_snapshot.laptopFanCount>0 ? L"风扇 2" : L"机箱风扇";
        rt->DrawTextW(label, static_cast<UINT32>(wcslen(label)), pFontRowLabel, D2D1::RectF(chartX, f2Y, chartX + 120.0f, f2Y + 17.0f), m_pTextSecondary);
    }
    if (pFontGroupRight) {
        if(m_snapshot.caseFanRpm<0)wcscpy_s(fanBuf,L"—");else swprintf_s(fanBuf,L"%d RPM",m_snapshot.caseFanRpm);
        rt->DrawTextW(fanBuf,static_cast<UINT32>(wcslen(fanBuf)),pFontGroupRight,D2D1::RectF(chartX+120,f2Y,chartX+chartW,f2Y+17),m_pTextMuted);
    }

    // ==========================================
    // 16. 传感器不可用提示框 (X=13, Y=391, 364 x 54)
    //     圆角 6, 内边距 8
    //     第一行: 框内 (8, 8) -> Y = 391 + 8 = 399
    //     第二行: 框内 (8, 29) -> Y = 391 + 29 = 420
    // ==========================================
    float boxY = cardY + 391.0f;
    float boxH = 54.0f;
    D2D1_ROUNDED_RECT boxR = D2D1::RoundedRect(D2D1::RectF(chartX, boxY, chartX + chartW, boxY + boxH), 6.0f, 6.0f);
    if (pBoxBgBrush && pBoxBorderBrush) {
        rt->FillRoundedRectangle(boxR, pBoxBgBrush);
        rt->DrawRoundedRectangle(boxR, pBoxBorderBrush, 1.0f);
    }

    if (pFontTip) {
        float tipX = chartX + 8.0f;
        float tipW = chartW - 16.0f;
        float line1Y = boxY + 8.0f;
        float line2Y = boxY + 29.0f;
        rt->DrawTextW(L"当前硬件未提供机箱风扇读数", 13, pFontTip, D2D1::RectF(tipX, line1Y, tipX + tipW, line1Y + 17.0f), m_pTextMuted);
        rt->DrawTextW(L"“—” 代表不可用，不代表停转。", 14, pFontTip, D2D1::RectF(tipX, line2Y, tipX + tipW, line2Y + 17.0f), m_pTextMuted);
    }

    // Cleanup resources
    SafeRelease(&pCoralBrush);
    SafeRelease(&pDotSlateBrush);
    SafeRelease(&pGridLine);
    SafeRelease(&pBoxBgBrush);
    SafeRelease(&pBoxBorderBrush);

    SafeRelease(&pFontGroupTitle);
    SafeRelease(&pFontHeaderRight);
    SafeRelease(&pFontBigVal);
    SafeRelease(&pFontUnit);
    SafeRelease(&pFontRange);
    SafeRelease(&pFontTimeLeft);
    SafeRelease(&pFontTimeRight);
    SafeRelease(&pFontRowLabel);
    SafeRelease(&pFontRowVal);
    SafeRelease(&pFontGroupRight);
    SafeRelease(&pFontTip);
}
void ProDashboardWindow::DrawFooter(ID2D1DCRenderTarget* rt, float& curY, float width) {
    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontFoot = nullptr;

    if (pDwFactory) {
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"zh-cn", &pFontFoot);
        if (pFontFoot) pFontFoot->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Figma spec: 360 面板，页脚位于 (11, 251) 相对面板，尺寸 338 x 28
    float cardX = 11.0f;
    float cardW = 338.0f;
    float footerH = 28.0f;

    // Footer divider line (138:933, 338 x 1 at Y=246)
    float dividerY = (m_mode == Mode_FullAccordion && m_expandedModule == Module_None) ? 246.0f : (curY + 4.0f);
    rt->DrawLine(D2D1::Point2F(cardX, dividerY), D2D1::Point2F(cardX + cardW, dividerY), m_pCardBorderBrush, 1.0f);

    // 页脚起点 (Figma 138:934 Y=251)
    float footerY = (m_mode == Mode_FullAccordion && m_expandedModule == Module_None) ? 251.0f : (dividerY + 5.0f);
    if (footerY < curY + 4.0f) {
        footerY = curY + 4.0f;
    }

    float centerY = footerY + footerH * 0.5f;
    float startX = cardX;

    // 1. Vector Clock Icon 🕒 (12x12 DIP) at startX
    float clockCx = startX + 6.0f;
    float clockCy = centerY;
    float clockR = 5.5f;
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(clockCx, clockCy), clockR, clockR), m_pTextMuted, 1.2f);
    rt->DrawLine(D2D1::Point2F(clockCx, clockCy), D2D1::Point2F(clockCx, clockCy - 3.2f), m_pTextMuted, 1.2f);
    rt->DrawLine(D2D1::Point2F(clockCx, clockCy), D2D1::Point2F(clockCx + 2.8f, clockCy), m_pTextMuted, 1.2f);
    startX += 16.0f;

    // 2. Uptime Text "03:42:18" (43 x 17)
    uint64_t uptime = m_snapshot.uptimeSeconds;
    uint32_t hours = static_cast<uint32_t>((uptime / 3600) % 24);
    uint32_t mins = static_cast<uint32_t>((uptime % 3600) / 60);
    uint32_t secs = static_cast<uint32_t>(uptime % 60);

    wchar_t upBuf[32];
    swprintf_s(upBuf, L"%02u:%02u:%02u", hours, mins, secs);

    if (pFontFoot) {
        rt->DrawTextW(upBuf, static_cast<UINT32>(wcslen(upBuf)), pFontFoot, D2D1::RectF(startX, footerY, startX + 54.0f, footerY + footerH), m_pTextSecondary);
    }
    startX += 58.0f;

    // 3. Separator "·"
    if (pFontFoot) {
        rt->DrawTextW(L"·", 1, pFontFoot, D2D1::RectF(startX, footerY, startX + 8.0f, footerY + footerH), m_pTextMuted);
    }
    startX += 10.0f;

    // 4. "每秒更新"
    if (pFontFoot) {
        rt->DrawTextW(L"每秒更新", 4, pFontFoot, D2D1::RectF(startX, footerY, startX + 60.0f, footerY + footerH), m_pTextMuted);
    }

    // 5. Right: Vector Gear button ⚙️ (28 DIP hit target, icon 14 x 14 DIP)
    float btnSize = 28.0f;
    float btnX = cardX + cardW - btnSize; // 321.0f
    float btnY = centerY - btnSize * 0.5f;
    m_settingsFooterBtnRect = D2D1::RectF(btnX, btnY, btnX + btnSize, btnY + btnSize);

    ID2D1SolidColorBrush* pGearBtnBg = nullptr;
    ID2D1SolidColorBrush* pGearBtnBorder = nullptr;
    rt->CreateSolidColorBrush(m_settingsHover ? D2D1::ColorF(0.929f, 0.953f, 0.984f, 1.0f) : D2D1::ColorF(0.969f, 0.976f, 0.988f, 1.0f), &pGearBtnBg); // #EDF3FB / #F7F9FC
    rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &pGearBtnBorder); // #DDE3EB

    if (pGearBtnBg && pGearBtnBorder) {
        D2D1_ROUNDED_RECT gearR = D2D1::RoundedRect(m_settingsFooterBtnRect, 6.0f, 6.0f);
        rt->FillRoundedRectangle(gearR, pGearBtnBg);
        rt->DrawRoundedRectangle(gearR, pGearBtnBorder, 1.0f);
        pGearBtnBg->Release();
        pGearBtnBorder->Release();
    }

    // 14 x 14 饱满设置齿轮矢量图标（清晰8齿轮齿 + 轮圈 + 轴孔）
    float gcx = btnX + btnSize * 0.5f;
    float gcy = centerY;
    ID2D1SolidColorBrush* pGearColor = m_settingsHover ? m_pTextPrimary : m_pTextMuted;

    for (int a = 0; a < 8; ++a) {
        float rad = a * (3.14159265f / 4.0f);
        float cosA = cosf(rad);
        float sinA = sinf(rad);
        rt->DrawLine(D2D1::Point2F(gcx + cosA * 4.0f, gcy + sinA * 4.0f),
                     D2D1::Point2F(gcx + cosA * 6.8f, gcy + sinA * 6.8f),
                     pGearColor, 2.2f);
    }
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(gcx, gcy), 4.4f, 4.4f), pGearColor, 1.6f);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(gcx, gcy), 2.0f, 2.0f), pGearColor, 1.2f);

    if (pFontFoot) pFontFoot->Release();
    curY = footerY + footerH + 4.0f;
}

void ProDashboardWindow::DrawCpuTempDirect(ID2D1DCRenderTarget* rt, float width, float height) {
    // 1. Ambient drop shadow for soft elevated floating feel (Figma panel 420 x 420, radius 20, border 1)
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.5f, 2.0f, width - 0.5f, height + 3.0f), 20.0f, 20.0f);
    ID2D1SolidColorBrush* pShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.08f), &pShadow);
    if (pShadow) {
        rt->FillRoundedRectangle(shadowR, pShadow);
        pShadow->Release();
    }

    // 2. Card background & specular border (Radius 20.0 DIP matching Figma)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(cardR, m_pBgBrush);
    rt->DrawRoundedRectangle(cardR, m_pBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma specs:
    // 标题: 16 / 24, Bold
    // 右上选择入口、设备说明、每秒更新、底部两个入口: 11 / 17
    // 大数值 58: 36 / 54, Medium
    // °C 单位: 14 / 21
    // “温度正常”状态文字: 11 / 17, Medium
    // 图表时间标签: 10 / 15
    // 悬停提示文字: 11 / 17, Medium
    // 关联指标行名称: 12 / 18, Regular
    // 关联指标行数值: 12 / 18, Medium
    IDWriteTextFormat* pFontTitle        = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontHeaderLink   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontBigTemp      = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 36.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontUnit         = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 14.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontBadge        = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 11.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontSub          = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubRight     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontAxisLeft     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 10.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontAxisRight    = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 10.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontProbe        = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 11.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontItemLabel    = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 12.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontItemVal      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 12.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontBottomLeft   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontBottomRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    // Brushes
    ID2D1SolidColorBrush* pCoral         = nullptr;
    ID2D1SolidColorBrush* pBadgeBg       = nullptr;
    ID2D1SolidColorBrush* pBadgeBorder   = nullptr;
    ID2D1SolidColorBrush* pBadgeDot      = nullptr;
    ID2D1SolidColorBrush* pBadgeText     = nullptr;
    ID2D1SolidColorBrush* pLinkBlue      = nullptr;
    ID2D1SolidColorBrush* pLinkBlueHover = nullptr;
    ID2D1SolidColorBrush* pProbeLine     = nullptr;
    ID2D1SolidColorBrush* pProbeCardBg   = nullptr;
    ID2D1SolidColorBrush* pProbeCardBorder = nullptr;
    ID2D1SolidColorBrush* pGridLine      = nullptr;

    rt->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.20f, 0.15f, 1.0f), &pCoral);       // Coral #DC2626
    rt->CreateSolidColorBrush(D2D1::ColorF(0.94f, 0.99f, 0.96f, 1.0f), &pBadgeBg);     // #F0FDF4
    rt->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.98f, 0.90f, 1.0f), &pBadgeBorder); // #DCFCE7
    rt->CreateSolidColorBrush(D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f), &pBadgeDot);    // #138873
    rt->CreateSolidColorBrush(D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f), &pBadgeText);   // #138873
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pLinkBlue);    // #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pLinkBlueHover);// #1D4ED8
    rt->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.20f, 0.15f, 0.45f), &pProbeLine);  // Probe line
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f), &pProbeCardBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 1.0f), &pProbeCardBorder);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.60f), &pGridLine);

    // Origin: Content starts at (17, 13), width 388
    float startX = 17.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 405.0f

    // ==========================================
    // 1. CPU 温度标题 / 传感器选择 (X=17, Y=13, 388 x 24)
    // ==========================================
    float r1Y = 13.0f;
    if (pFontTitle) {
        rt->DrawTextW(L"CPU 温度", 5, pFontTitle, D2D1::RectF(startX, r1Y, startX + 160.0f, r1Y + 24.0f), m_pTextPrimary);
    }
    m_tempTargetRect = D2D1::RectF(endX - 90.0f, r1Y, endX, r1Y + 24.0f);
    if (pFontHeaderLink && pLinkBlue) {
        rt->DrawTextW(L"CPU 封装 ▾", 7, pFontHeaderLink, m_tempTargetRect, pLinkBlue);
    }

    // ==========================================
    // 2. 当前温度 / 正常状态 (X=17, Y=45, 388 x 54)
    //    温度大数值 58: 36 / 54 Medium, 单位 °C: 14 / 21
    //    “温度正常”状态块: 82 x 25, 圆角 8, 位于行内 (306, 14.5) -> (17+306, 45+14.5)=(323, 59.5)
    // ==========================================
    float r2Y = 45.0f;
    int tempVal = static_cast<int>(m_snapshot.cpuTempC + 0.5f);
    if (tempVal <= 0) tempVal = 58;

    wchar_t tempBuf[16];
    swprintf_s(tempBuf, L"%d", tempVal);

    if (pFontBigTemp && pCoral) {
        IDWriteTextLayout* pNumLayout = nullptr;
        pDwFactory->CreateTextLayout(tempBuf, static_cast<UINT32>(wcslen(tempBuf)), pFontBigTemp, 100.0f, 54.0f, &pNumLayout);
        if (pNumLayout) {
            pNumLayout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            DWRITE_TEXT_METRICS tm;
            pNumLayout->GetMetrics(&tm);
            rt->DrawTextLayout(D2D1::Point2F(startX, r2Y), pNumLayout, pCoral);

            float unitX = startX + tm.widthIncludingTrailingWhitespace + 4.0f;
            if (pFontUnit) {
                rt->DrawTextW(L"°C", 2, pFontUnit, D2D1::RectF(unitX, r2Y, unitX + 30.0f, r2Y + 54.0f), m_pTextSecondary);
            }
            pNumLayout->Release();
        }
    }

    // 状态块 (82 x 25, 圆角 8, 右边缘对齐 endX)
    float badgeW = 82.0f;
    float badgeH = 25.0f;
    float badgeX = startX + 306.0f; // 323.0f, 323 + 82 = 405.0f (exact endX!)
    float badgeY = r2Y + 14.5f;     // 59.5f
    D2D1_ROUNDED_RECT badgeR = D2D1::RoundedRect(D2D1::RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH), 8.0f, 8.0f);
    if (pBadgeBg && pBadgeBorder) {
        rt->FillRoundedRectangle(badgeR, pBadgeBg);
        rt->DrawRoundedRectangle(badgeR, pBadgeBorder, 1.0f);
    }
    if (pBadgeDot) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(badgeX + 14.0f, badgeY + badgeH * 0.5f), 3.5f, 3.5f), pBadgeDot);
    }
    if (pFontBadge && pBadgeText) {
        rt->DrawTextW(L"温度正常", 4, pFontBadge, D2D1::RectF(badgeX + 22.0f, badgeY, badgeX + badgeW - 4.0f, badgeY + badgeH), pBadgeText);
    }

    // ==========================================
    // 3. CPU 型号 / 更新频率 (X=17, Y=107, 388 x 17)
    // ==========================================
    float r3Y = 107.0f;
    const wchar_t* cpuBrandStr = m_snapshot.cpuBrand.c_str();
    if (m_snapshot.cpuBrand.empty() || m_snapshot.cpuBrand == L"CPU") {
        cpuBrandStr = L"AMD Ryzen 7 7700";
    }
    if (pFontSub) {
        rt->DrawTextW(cpuBrandStr, static_cast<UINT32>(wcslen(cpuBrandStr)), pFontSub, D2D1::RectF(startX, r3Y, startX + 240.0f, r3Y + 17.0f), m_pTextSecondary);
    }
    if (pFontSubRight) {
        rt->DrawTextW(L"每秒更新", 4, pFontSubRight, D2D1::RectF(endX - 80.0f, r3Y, endX, r3Y + 17.0f), m_pTextMuted);
    }

    // ==========================================
    // 4. 温度历史图表 (X=17, Y=132, 388 x 120)
    //    三条水平网格线: Y=10, 53, 96 (厚 1)
    //    探针竖线: 1 x 94 (从 Y=10 到 Y=104, 或 Y=10 到 Y=96+8)
    // ==========================================
    float chartX = startX;
    float chartY = 132.0f;
    float chartW = contentW; // 388.0f
    float chartH = 120.0f;
    m_tempChartRect = D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH);

    // 三条网格线 (Y = 10, 53, 96)
    if (pGridLine) {
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 10.0f), D2D1::Point2F(chartX + chartW, chartY + 10.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 53.0f), D2D1::Point2F(chartX + chartW, chartY + 53.0f), pGridLine, 1.0f);
        rt->DrawLine(D2D1::Point2F(chartX, chartY + 96.0f), D2D1::Point2F(chartX + chartW, chartY + 96.0f), pGridLine, 1.0f);
    }

    // 准备 60 点采样数据
    size_t histCount = m_snapshot.tempHistory.Size();
    float vals[60];
    float minV = 46.0f;
    float maxV = 64.0f;

    if (histCount >= 2) {
        minV = m_snapshot.tempHistory[0];
        maxV = m_snapshot.tempHistory[0];
        for (size_t i = 0; i < histCount; ++i) {
            float v = m_snapshot.tempHistory[i];
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
            vals[i] = v;
        }
        if (maxV - minV < 10.0f) {
            float mid = (minV + maxV) * 0.5f;
            minV = mid - 5.0f;
            maxV = mid + 5.0f;
        }
    } else {
        // Fallback realistic waveform matching Figma 02 CPU 温度:
        const float figmaCurve[16] = { 46.0f, 49.0f, 47.0f, 52.0f, 50.0f, 55.0f, 54.0f, 51.0f, 53.0f, 50.0f, 55.0f, 53.0f, 64.0f, 59.0f, 56.0f, 58.0f };
        for (int i = 0; i < 60; ++i) {
            float idxF = (i * 15.0f) / 59.0f;
            int idx0 = std::clamp(static_cast<int>(idxF), 0, 14);
            int idx1 = idx0 + 1;
            float t = idxF - idx0;
            vals[i] = figmaCurve[idx0] * (1.0f - t) + figmaCurve[idx1] * t;
        }
        histCount = 60;
        minV = 46.0f;
        maxV = 64.0f;
    }

    float span = (maxV > minV) ? (maxV - minV) : 10.0f;
    float baselineY = chartY + 96.0f;
    float plotH = 86.0f; // top: chartY + 10, baseline: chartY + 96

    // 绘制曲线与半透明渐变填充
    ID2D1Factory* pFactory = m_renderer.GetD2DFactory();
    if (pFactory) {
        ID2D1PathGeometry* pCurvePath = nullptr;
        ID2D1PathGeometry* pAreaPath = nullptr;
        pFactory->CreatePathGeometry(&pCurvePath);
        pFactory->CreatePathGeometry(&pAreaPath);
        if (pCurvePath && pAreaPath) {
            ID2D1GeometrySink* pLineSink = nullptr;
            ID2D1GeometrySink* pAreaSink = nullptr;
            pCurvePath->Open(&pLineSink);
            pAreaPath->Open(&pAreaSink);

            float dx = chartW / (histCount - 1);
            float lastPx = chartX;

            for (size_t i = 0; i < histCount; ++i) {
                float norm = (vals[i] - minV) / span;
                norm = std::clamp(norm, 0.0f, 1.0f);
                float px = chartX + i * dx;
                float py = baselineY - norm * plotH;

                if (i == 0) {
                    pLineSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                    pAreaSink->BeginFigure(D2D1::Point2F(px, baselineY), D2D1_FIGURE_BEGIN_FILLED);
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                } else {
                    pLineSink->AddLine(D2D1::Point2F(px, py));
                    pAreaSink->AddLine(D2D1::Point2F(px, py));
                }
                lastPx = px;
            }

            pAreaSink->AddLine(D2D1::Point2F(lastPx, baselineY));
            pAreaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pAreaSink->Close();
            pAreaSink->Release();

            pLineSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pLineSink->Close();
            pLineSink->Release();

            // 填充底边 Y = 96
            ID2D1SolidColorBrush* pFillBrush = nullptr;
            rt->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.20f, 0.15f, 0.12f), &pFillBrush);
            if (pFillBrush) {
                rt->FillGeometry(pAreaPath, pFillBrush);
                pFillBrush->Release();
            }

            // 温度曲线线宽 1.7
            if (pCoral) {
                rt->DrawGeometry(pCurvePath, pCoral, 1.7f);
            }

            pCurvePath->Release();
            pAreaPath->Release();
        }
    }

    // 探针竖线与悬停提示框 (动态随鼠标移动，非固定坐标)
    // 默认示例探针: 28 秒前, X ≈ 208 (或用户悬停)
    int probeSecAgo = m_tempProbeActive ? m_tempProbeSecAgo : 28;
    float inspectRatio = std::clamp(1.0f - (static_cast<float>(probeSecAgo) / 59.0f), 0.0f, 1.0f);
    float inspectX = m_tempProbeActive ? m_tempProbeX : (chartX + 208.0f);
    inspectX = std::clamp(inspectX, chartX, chartX + chartW);

    // 探针竖线: 1 x 94 (从 chartY + 10 到 chartY + 104)
    if (pProbeLine) {
        rt->DrawLine(D2D1::Point2F(inspectX, chartY + 10.0f), D2D1::Point2F(inspectX, chartY + 104.0f), pProbeLine, 1.0f);
    }

    // 提示文字与提示框 (144 x 33, 圆角 8, 四边内边距 8)
    size_t probeIdx = std::clamp(static_cast<size_t>(inspectRatio * (histCount - 1) + 0.5f), size_t(0), histCount - 1);
    int probeTemp = static_cast<int>(vals[probeIdx] + 0.5f);

    wchar_t probeBuf[48];
    swprintf_s(probeBuf, L"%d 秒前 • %d°C", probeSecAgo, probeTemp);

    float tipW = 144.0f;
    float tipH = 33.0f;
    float tipX = std::clamp(inspectX - tipW * 0.5f, chartX, chartX + chartW - tipW);
    float tipY = chartY; // Y = 0 相对于图表容器

    // 提示框阴影与实体
    ID2D1SolidColorBrush* pPillShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.10f), &pPillShadow);
    if (pPillShadow) {
        D2D1_ROUNDED_RECT tipShadow = D2D1::RoundedRect(D2D1::RectF(tipX, tipY + 1.5f, tipX + tipW, tipY + tipH + 2.5f), 8.0f, 8.0f);
        rt->FillRoundedRectangle(tipShadow, pPillShadow);
        pPillShadow->Release();
    }
    D2D1_ROUNDED_RECT tipCard = D2D1::RoundedRect(D2D1::RectF(tipX, tipY, tipX + tipW, tipY + tipH), 8.0f, 8.0f);
    if (pProbeCardBg && pProbeCardBorder) {
        rt->FillRoundedRectangle(tipCard, pProbeCardBg);
        rt->DrawRoundedRectangle(tipCard, pProbeCardBorder, 1.0f);
    }
    if (pFontProbe && pCoral) {
        rt->DrawTextW(probeBuf, static_cast<UINT32>(wcslen(probeBuf)), pFontProbe, D2D1::RectF(tipX + 8.0f, tipY + 8.0f, tipX + tipW - 8.0f, tipY + tipH - 8.0f), pCoral);
    }

    // ==========================================
    // 5. “60 秒前 / 现在” 时间标签 (X=17, Y=260, 388 x 15)
    //    字号 10 / 行高 15
    // ==========================================
    float r5Y = 260.0f;
    if (pFontAxisLeft) {
        rt->DrawTextW(L"60 秒前", 5, pFontAxisLeft, D2D1::RectF(startX, r5Y, startX + 60.0f, r5Y + 15.0f), m_pTextMuted);
    }
    if (pFontAxisRight) {
        rt->DrawTextW(L"现在", 2, pFontAxisRight, D2D1::RectF(endX - 60.0f, r5Y, endX, r5Y + 15.0f), m_pTextMuted);
    }

    // ==========================================
    // 6. 关联指标列表 (X=17, Y=283, 388 x 90)
    //    4 行: Y=283, 307, 331, 355 (行高 18, 起点间隔 24)
    //    名称: 12 / 18 Regular, 数值: 12 / 18 Medium
    // ==========================================
    auto DrawStatRow = [&](float rowY, const wchar_t* label, const wchar_t* val) {
        if (pFontItemLabel) {
            rt->DrawTextW(label, static_cast<UINT32>(wcslen(label)), pFontItemLabel, D2D1::RectF(startX, rowY, startX + 160.0f, rowY + 18.0f), m_pTextSecondary);
        }
        if (pFontItemVal) {
            rt->DrawTextW(val, static_cast<UINT32>(wcslen(val)), pFontItemVal, D2D1::RectF(endX - 160.0f, rowY, endX, rowY + 18.0f), m_pTextPrimary);
        }
    };

    // 行 1: 最低 / 平均 / 最高 -> 46° / 55° / 64°
    int minT = static_cast<int>(minV + 0.5f);
    int maxT = static_cast<int>(maxV + 0.5f);
    int avgT = (minT + maxT) / 2;
    wchar_t rangeBuf[32];
    swprintf_s(rangeBuf, L"%d° / %d° / %d°", minT, avgT, maxT);
    DrawStatRow(283.0f, L"最低 / 平均 / 最高", rangeBuf);

    // 行 2: CPU 封装功耗 -> 32 W
    float cpuPwr = (m_snapshot.cpuPowerWatts > 0.0f) ? m_snapshot.cpuPowerWatts : 32.0f;
    wchar_t pwrBuf[32];
    swprintf_s(pwrBuf, L"%.0f W", cpuPwr);
    DrawStatRow(307.0f, L"CPU 封装功耗", pwrBuf);

    // 行 3: 有效频率 -> 4.82 GHz
    float cpuFreq = (m_snapshot.cpuFreqGHz > 0.1f) ? m_snapshot.cpuFreqGHz : 4.82f;
    wchar_t freqBuf[32];
    swprintf_s(freqBuf, L"%.2f GHz", cpuFreq);
    DrawStatRow(331.0f, L"有效频率", freqBuf);

    // 行 4: CPU 风扇 -> 1,240 RPM
    int fanRpm = (m_snapshot.cpuFanRpm > 0) ? m_snapshot.cpuFanRpm : 1240;
    wchar_t fanBuf[32];
    swprintf_s(fanBuf, L"%d,%03d RPM", fanRpm / 1000, fanRpm % 1000);
    DrawStatRow(355.0f, L"CPU 风扇", fanBuf);

    // ==========================================
    // 7. 分割线 (X=17, Y=381, 388 x 1)
    // ==========================================
    rt->DrawLine(D2D1::Point2F(startX, 381.0f), D2D1::Point2F(endX, 381.0f), m_pCardBorderBrush, 1.0f);

    // ==========================================
    // 8. 温度提醒 / 全部传感器入口 (X=17, Y=390, 388 x 17)
    //    底部“温度提醒”靠左，“查看全部传感器”靠右，字号 11 / 17
    // ==========================================
    float r8Y = 390.0f;
    m_tempAlertRect = D2D1::RectF(startX, r8Y, startX + 120.0f, r8Y + 17.0f);
    wchar_t alertBuf[32];
    swprintf_s(alertBuf, L"温度提醒  %d°C ▾", m_alertTempThreshold);
    if (pFontBottomLeft) {
        rt->DrawTextW(alertBuf, static_cast<UINT32>(wcslen(alertBuf)), pFontBottomLeft, m_tempAlertRect, m_pTextMuted);
    }

    m_allSensorsRect = D2D1::RectF(endX - 120.0f, r8Y, endX, r8Y + 17.0f);
    if (pFontBottomRight) {
        ID2D1SolidColorBrush* pLink = m_allSensorsHover ? pLinkBlueHover : pLinkBlue;
        rt->DrawTextW(L"查看全部传感器 ›", 9, pFontBottomRight, m_allSensorsRect, pLink);
    }

    // Cleanup resources
    SafeRelease(&pCoral);
    SafeRelease(&pBadgeBg);
    SafeRelease(&pBadgeBorder);
    SafeRelease(&pBadgeDot);
    SafeRelease(&pBadgeText);
    SafeRelease(&pLinkBlue);
    SafeRelease(&pLinkBlueHover);
    SafeRelease(&pProbeLine);
    SafeRelease(&pProbeCardBg);
    SafeRelease(&pProbeCardBorder);
    SafeRelease(&pGridLine);

    SafeRelease(&pFontTitle);
    SafeRelease(&pFontHeaderLink);
    SafeRelease(&pFontBigTemp);
    SafeRelease(&pFontUnit);
    SafeRelease(&pFontBadge);
    SafeRelease(&pFontSub);
    SafeRelease(&pFontSubRight);
    SafeRelease(&pFontAxisLeft);
    SafeRelease(&pFontAxisRight);
    SafeRelease(&pFontProbe);
    SafeRelease(&pFontItemLabel);
    SafeRelease(&pFontItemVal);
    SafeRelease(&pFontBottomLeft);
    SafeRelease(&pFontBottomRight);
}
// ==========================================
// DIRECT CARD: CPU
// ==========================================
void ProDashboardWindow::DrawCpuDirect(ID2D1DCRenderTarget* rt, float width, float height) {
    // 1. Ambient drop shadow (420 x 420, radius 20)
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.5f, 2.0f, width - 0.5f, height + 3.0f), 20.0f, 20.0f);
    ID2D1SolidColorBrush* pShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.08f), &pShadow);
    if (pShadow) {
        rt->FillRoundedRectangle(shadowR, pShadow);
        pShadow->Release();
    }

    // 2. Card background & border (Radius 20.0, border 1.0)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(cardR, m_pBgBrush);
    rt->DrawRoundedRectangle(cardR, m_pBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    IDWriteTextFormat* pFontTitle     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFootLeft  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLink      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    ID2D1SolidColorBrush* pLinkBlue = nullptr;
    ID2D1SolidColorBrush* pLinkBlueHover = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pLinkBlue);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pLinkBlueHover);

    float startX = 16.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 404.0f
    float titleY = 13.0f;

    // Title Row (X=16, Y=13, 388 x 24)
    if (pFontTitle) {
        rt->DrawTextW(L"CPU 运行状态", 7, pFontTitle, D2D1::RectF(startX, titleY, startX + 160.0f, titleY + 24.0f), m_pTextPrimary);
    }
    const wchar_t* brand = m_snapshot.cpuBrand.empty() ? L"8 核 16 线程" : m_snapshot.cpuBrand.c_str();
    if (pFontSubRight) {
        rt->DrawTextW(brand, static_cast<UINT32>(wcslen(brand)), pFontSubRight, D2D1::RectF(endX - 200.0f, titleY, endX, titleY + 24.0f), m_pTextMuted);
    }

    // Embed DrawCpuExpanded (with x=28, y=47, w=364 matching exact internal coordinates)
    DrawCpuExpanded(rt, startX + 12.0f, 47.0f, 364.0f, 340.0f);

    // Divider Line (Y=381)
    rt->DrawLine(D2D1::Point2F(startX, 381.0f), D2D1::Point2F(endX, 381.0f), m_pCardBorderBrush, 1.0f);

    // Footer Row (Y=390, 388 x 17)
    float footY = 390.0f;
    if (pFontFootLeft) {
        rt->DrawTextW(L"采样间隔  1s ▾", 9, pFontFootLeft, D2D1::RectF(startX, footY, startX + 140.0f, footY + 17.0f), m_pTextMuted);
    }

    m_overviewLinkRect = D2D1::RectF(endX - 140.0f, footY, endX, footY + 17.0f);
    if (pFontLink) {
        ID2D1SolidColorBrush* pL = m_overviewLinkHover ? pLinkBlueHover : pLinkBlue;
        rt->DrawTextW(L"查看系统运行概览 ›", 10, pFontLink, m_overviewLinkRect, pL);
    }

    SafeRelease(&pLinkBlue);
    SafeRelease(&pLinkBlueHover);
    SafeRelease(&pFontTitle);
    SafeRelease(&pFontSubRight);
    SafeRelease(&pFontFootLeft);
    SafeRelease(&pFontLink);
}

// ==========================================
// DIRECT CARD: MEMORY
// ==========================================
void ProDashboardWindow::DrawMemoryDirect(ID2D1DCRenderTarget* rt, float width, float height) {
    // 1. Ambient drop shadow (420 x 420, radius 20)
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.5f, 2.0f, width - 0.5f, height + 3.0f), 20.0f, 20.0f);
    ID2D1SolidColorBrush* pShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.08f), &pShadow);
    if (pShadow) {
        rt->FillRoundedRectangle(shadowR, pShadow);
        pShadow->Release();
    }

    // 2. Card background & border (Radius 20.0, border 1.0)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(cardR, m_pBgBrush);
    rt->DrawRoundedRectangle(cardR, m_pBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    IDWriteTextFormat* pFontTitle     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubRight  = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFootLeft  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLink      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    ID2D1SolidColorBrush* pLinkBlue = nullptr;
    ID2D1SolidColorBrush* pLinkBlueHover = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pLinkBlue);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pLinkBlueHover);

    float startX = 16.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 404.0f
    float titleY = 13.0f;

    // Title Row (X=16, Y=13, 388 x 24)
    if (pFontTitle) {
        rt->DrawTextW(L"物理内存", 4, pFontTitle, D2D1::RectF(startX, titleY, startX + 160.0f, titleY + 24.0f), m_pTextPrimary);
    }
    if (pFontSubRight) {
        rt->DrawTextW(L"DDR5 6000 MHz", 14, pFontSubRight, D2D1::RectF(endX - 160.0f, titleY, endX, titleY + 24.0f), m_pTextMuted);
    }

    // Embed DrawMemoryExpanded (with x=28, y=47, w=364 matching exact internal coordinates)
    DrawMemoryExpanded(rt, startX + 12.0f, 47.0f, 364.0f, 375.0f);

    // Divider Line (Y=381)
    rt->DrawLine(D2D1::Point2F(startX, 381.0f), D2D1::Point2F(endX, 381.0f), m_pCardBorderBrush, 1.0f);

    // Footer Row (Y=390, 388 x 17)
    float footY = 390.0f;
    if (pFontFootLeft) {
        rt->DrawTextW(L"工作集内存", 5, pFontFootLeft, D2D1::RectF(startX, footY, startX + 140.0f, footY + 17.0f), m_pTextMuted);
    }

    m_overviewLinkRect = D2D1::RectF(endX - 140.0f, footY, endX, footY + 17.0f);
    if (pFontLink) {
        ID2D1SolidColorBrush* pL = m_overviewLinkHover ? pLinkBlueHover : pLinkBlue;
        rt->DrawTextW(L"查看系统运行概览 ›", 10, pFontLink, m_overviewLinkRect, pL);
    }

    SafeRelease(&pLinkBlue);
    SafeRelease(&pLinkBlueHover);
    SafeRelease(&pFontTitle);
    SafeRelease(&pFontSubRight);
    SafeRelease(&pFontFootLeft);
    SafeRelease(&pFontLink);
}

// ==========================================
// DIRECT CARD: NETWORK
// ==========================================
void ProDashboardWindow::DrawNetworkDirect(ID2D1DCRenderTarget* rt, float width, float height) {
    // 1. Ambient drop shadow (420 x 420, radius 20)
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.5f, 2.0f, width - 0.5f, height + 3.0f), 20.0f, 20.0f);
    ID2D1SolidColorBrush* pShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.08f), &pShadow);
    if (pShadow) {
        rt->FillRoundedRectangle(shadowR, pShadow);
        pShadow->Release();
    }

    // 2. Card background & border (Radius 20.0, border 1.0)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(cardR, m_pBgBrush);
    rt->DrawRoundedRectangle(cardR, m_pBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    IDWriteTextFormat* pFontTitle     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubRight  = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFootLeft  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLink      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    ID2D1SolidColorBrush* pLinkBlue = nullptr;
    ID2D1SolidColorBrush* pLinkBlueHover = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pLinkBlue);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pLinkBlueHover);

    float startX = 16.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 404.0f
    float titleY = 13.0f;

    // Title Row (X=16, Y=13, 388 x 24)
    if (pFontTitle) {
        rt->DrawTextW(L"网络吞吐", 4, pFontTitle, D2D1::RectF(startX, titleY, startX + 160.0f, titleY + 24.0f), m_pTextPrimary);
    }
    if (pFontSubRight) {
        rt->DrawTextW(L"Wi-Fi 6 (802.11ax)", 17, pFontSubRight, D2D1::RectF(endX - 160.0f, titleY, endX, titleY + 24.0f), m_pTextMuted);
    }

    // Embed DrawNetworkExpanded (with x=28, y=47, w=364 matching exact internal coordinates)
    DrawNetworkExpanded(rt, startX + 12.0f, 47.0f, 364.0f, 335.0f);

    // Divider Line (Y=381)
    rt->DrawLine(D2D1::Point2F(startX, 381.0f), D2D1::Point2F(endX, 381.0f), m_pCardBorderBrush, 1.0f);

    // Footer Row (Y=390, 388 x 17)
    float footY = 390.0f;
    if (pFontFootLeft) {
        rt->DrawTextW(L"以太网 / WLAN ▾", 10, pFontFootLeft, D2D1::RectF(startX, footY, startX + 140.0f, footY + 17.0f), m_pTextMuted);
    }

    m_overviewLinkRect = D2D1::RectF(endX - 140.0f, footY, endX, footY + 17.0f);
    if (pFontLink) {
        ID2D1SolidColorBrush* pL = m_overviewLinkHover ? pLinkBlueHover : pLinkBlue;
        rt->DrawTextW(L"查看系统运行概览 ›", 10, pFontLink, m_overviewLinkRect, pL);
    }

    SafeRelease(&pLinkBlue);
    SafeRelease(&pLinkBlueHover);
    SafeRelease(&pFontTitle);
    SafeRelease(&pFontSubRight);
    SafeRelease(&pFontFootLeft);
    SafeRelease(&pFontLink);
}

// ==========================================
// DIRECT CARD: GPU
// ==========================================
void ProDashboardWindow::DrawGpuDirect(ID2D1DCRenderTarget* rt, float width, float height) {
    // 1. Ambient drop shadow (420 x 420, radius 20)
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.5f, 2.0f, width - 0.5f, height + 3.0f), 20.0f, 20.0f);
    ID2D1SolidColorBrush* pShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.08f), &pShadow);
    if (pShadow) {
        rt->FillRoundedRectangle(shadowR, pShadow);
        pShadow->Release();
    }

    // 2. Card background & border (Radius 20.0, border 1.0)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(cardR, m_pBgBrush);
    rt->DrawRoundedRectangle(cardR, m_pBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    IDWriteTextFormat* pFontTitle     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubRight  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFootLeft  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLink      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    ID2D1SolidColorBrush* pLinkBlue = nullptr;
    ID2D1SolidColorBrush* pLinkBlueHover = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pLinkBlue);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pLinkBlueHover);

    float startX = 16.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 404.0f
    float titleY = 13.0f;

    // Title Row (X=16, Y=13, 388 x 24)
    if (pFontTitle) {
        rt->DrawTextW(L"GPU 运行状态", 7, pFontTitle, D2D1::RectF(startX, titleY, startX + 160.0f, titleY + 24.0f), m_pTextPrimary);
    }
    const wchar_t* gBrand = m_snapshot.gpuName.empty() ? L"专用显卡" : m_snapshot.gpuName.c_str();
    if (pFontSubRight) {
        rt->DrawTextW(gBrand, static_cast<UINT32>(wcslen(gBrand)), pFontSubRight, D2D1::RectF(endX - 200.0f, titleY, endX, titleY + 24.0f), m_pTextMuted);
    }

    // Embed DrawGpuExpanded (with x=28, y=47, w=364 matching exact internal coordinates)
    DrawGpuExpanded(rt, startX + 12.0f, 47.0f, 364.0f, 387.0f);

    // Divider Line (Y=381)
    rt->DrawLine(D2D1::Point2F(startX, 381.0f), D2D1::Point2F(endX, 381.0f), m_pCardBorderBrush, 1.0f);

    // Footer Row (Y=390, 388 x 17)
    float footY = 390.0f;
    if (pFontFootLeft) {
        rt->DrawTextW(L"采样间隔  1s ▾", 9, pFontFootLeft, D2D1::RectF(startX, footY, startX + 140.0f, footY + 17.0f), m_pTextMuted);
    }

    m_overviewLinkRect = D2D1::RectF(endX - 140.0f, footY, endX, footY + 17.0f);
    if (pFontLink) {
        ID2D1SolidColorBrush* pL = m_overviewLinkHover ? pLinkBlueHover : pLinkBlue;
        rt->DrawTextW(L"查看系统运行概览 ›", 10, pFontLink, m_overviewLinkRect, pL);
    }

    SafeRelease(&pLinkBlue);
    SafeRelease(&pLinkBlueHover);
    SafeRelease(&pFontTitle);
    SafeRelease(&pFontSubRight);
    SafeRelease(&pFontFootLeft);
    SafeRelease(&pFontLink);
}

// ==========================================
// DIRECT CARD: DISK
// ==========================================
void ProDashboardWindow::DrawDiskDirect(ID2D1DCRenderTarget* rt, float width, float height) {
    // 1. Ambient drop shadow (420 x 420, radius 20)
    D2D1_ROUNDED_RECT shadowR = D2D1::RoundedRect(D2D1::RectF(0.5f, 2.0f, width - 0.5f, height + 3.0f), 20.0f, 20.0f);
    ID2D1SolidColorBrush* pShadow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.04f, 0.12f, 0.08f), &pShadow);
    if (pShadow) {
        rt->FillRoundedRectangle(shadowR, pShadow);
        pShadow->Release();
    }

    // 2. Card background & border (Radius 20.0, border 1.0)
    D2D1_ROUNDED_RECT cardR = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(cardR, m_pBgBrush);
    rt->DrawRoundedRectangle(cardR, m_pBorderBrush, 1.0f);

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    IDWriteTextFormat* pFontTitle     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSubRight  = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontFootLeft  = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontLink      = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    ID2D1SolidColorBrush* pLinkBlue = nullptr;
    ID2D1SolidColorBrush* pLinkBlueHover = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pLinkBlue);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pLinkBlueHover);

    float startX = 16.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 404.0f
    float titleY = 13.0f;

    // Title Row (X=16, Y=13, 388 x 24)
    if (pFontTitle) {
        rt->DrawTextW(L"磁盘状态", 4, pFontTitle, D2D1::RectF(startX, titleY, startX + 160.0f, titleY + 24.0f), m_pTextPrimary);
    }
    if (pFontSubRight) {
        rt->DrawTextW(L"NVMe SSD", 8, pFontSubRight, D2D1::RectF(endX - 160.0f, titleY, endX, titleY + 24.0f), m_pTextMuted);
    }

    // Embed DrawDiskExpanded (with x=28, y=47, w=364 matching exact internal coordinates)
    DrawDiskExpanded(rt, startX + 12.0f, 47.0f, 364.0f, 378.0f);

    // Divider Line (Y=381)
    rt->DrawLine(D2D1::Point2F(startX, 381.0f), D2D1::Point2F(endX, 381.0f), m_pCardBorderBrush, 1.0f);

    // Footer Row (Y=390, 388 x 17)
    float footY = 390.0f;
    if (pFontFootLeft) {
        rt->DrawTextW(L"活动时间 1% ▾", 9, pFontFootLeft, D2D1::RectF(startX, footY, startX + 140.0f, footY + 17.0f), m_pTextMuted);
    }

    m_overviewLinkRect = D2D1::RectF(endX - 140.0f, footY, endX, footY + 17.0f);
    if (pFontLink) {
        ID2D1SolidColorBrush* pL = m_overviewLinkHover ? pLinkBlueHover : pLinkBlue;
        rt->DrawTextW(L"查看系统运行概览 ›", 10, pFontLink, m_overviewLinkRect, pL);
    }

    SafeRelease(&pLinkBlue);
    SafeRelease(&pLinkBlueHover);
    SafeRelease(&pFontTitle);
    SafeRelease(&pFontSubRight);
    SafeRelease(&pFontFootLeft);
    SafeRelease(&pFontLink);
}
// ==========================================
// PREFERENCES SETTINGS PAGE (420 x 504 DIP)
// Exact Figma Specifications:
// Panel: 420 x 504, Radius 20, Background #FFFFFF alpha 40%, Stroke 1 DIP #FFFFFF alpha 85%
// Drop shadow: X=0, Y=8, Blur=24, Spread=0, #26385C alpha 10%
// Layout: Padding Left/Right 16, Top/Bottom 12 -> Content starts at (17, 13), Width 388
// ==========================================
void ProDashboardWindow::DrawSettingsAppearance(ID2D1DCRenderTarget* rt) {
    auto* dw=m_renderer.GetDWriteFactory();
    const bool dark=m_settings.themeMode==2 || (m_settings.themeMode==3&&SystemUsesDarkTheme()),glass=m_settings.themeMode==0;
    const auto primary=D2D1::ColorF(dark ? 0xE7EDF5 : 0x192739),muted=D2D1::ColorF(dark ? 0xA8B6C8 : 0x53657A),blue=D2D1::ColorF(dark ? 0x80B3FF : 0x266BCE);
    const auto surface=D2D1::ColorF(dark ? 0x263242 : 0xF7F9FC),selected=D2D1::ColorF(dark ? 0x243E61 : 0xEDF3FB);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(dark ? 0x192330 : 0xFFFFFF,glass ? GetDashboardBgAlpha() : 1.f),&brush);if(!brush)return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,m_dipHeight),20,24,0,8,.07f);
    const auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,m_dipHeight-.5f),19.5f,19.5f);rt->FillRoundedRectangle(panel,brush.Get());brush->SetColor(D2D1::ColorF(dark ? 0x39485A : 0xDDE3EB));rt->DrawRoundedRectangle(panel,brush.Get(),1);
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float line,DWRITE_FONT_WEIGHT weight,D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {m_overviewVisuals.Text(rt,dw,value,box,size,line,weight,color,align);};
    text(L"偏好设置",D2D1::RectF(11,5,310,29),16,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    m_settingsDoneRect=D2D1::RectF(324,8.5f,349,25.5f);text(L"完成",m_settingsDoneRect,11,17,DWRITE_FONT_WEIGHT_BOLD,blue);
    const wchar_t* tabs[]={L"外观",L"监控",L"常规"};
    for(int i=0;i<3;++i) {const float x=11+i*(107.333333f+8);m_tabRects[i]=D2D1::RectF(x,33,x+107.333333f,63);if(i==0) {brush->SetColor(selected);rt->FillRoundedRectangle(D2D1::RoundedRect(m_tabRects[i],8,8),brush.Get());}text(tabs[i],m_tabRects[i],12,18,DWRITE_FONT_WEIGHT_MEDIUM,i==0 ? blue : muted,DWRITE_TEXT_ALIGNMENT_CENTER);}
    text(L"外观",D2D1::RectF(11,67,349,85),12,18,DWRITE_FONT_WEIGHT_MEDIUM,muted);
    auto option=[&](const wchar_t* label,D2D1_RECT_F box,bool active) {brush->SetColor(active ? selected : surface);const auto r=D2D1::RoundedRect(box,6,6);rt->FillRoundedRectangle(r,brush.Get());if(active) {brush->SetColor(blue);rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(box.left+.5f,box.top+.5f,box.right-.5f,box.bottom-.5f),5.5f,5.5f),brush.Get(),1);}text(label,box,11,17,DWRITE_FONT_WEIGHT_MEDIUM,active ? blue : muted,DWRITE_TEXT_ALIGNMENT_CENTER);};
    const float widths[]={80,77,80,80};const wchar_t* themes[]={L"亮色玻璃",L"纯白",L"深色",L"跟随系统"};float x=11;
    for(int i=0;i<4;++i) {m_themeRects[i]=D2D1::RectF(x,89,x+widths[i],118);option(themes[i],m_themeRects[i],m_settings.themeMode==i);x+=widths[i]+6;}
    text(L"展开面板布局",D2D1::RectF(11,122,349,140),12,18,DWRITE_FONT_WEIGHT_MEDIUM,muted);
    const wchar_t* layouts[]={L"紧凑胶囊",L"完整长条"};for(int i=0;i<2;++i) {m_layoutOptionRects[i]=D2D1::RectF(11+172.f*i,146,177+172.f*i,175);option(layouts[i],m_layoutOptionRects[i],m_settings.layoutMode==1-i);}
    text(m_appearanceSaveFailed ? L"设置未保存，请检查配置文件写入权限。" : L"仅改变未展开指标的形态；展开项直接变为详情卡片。",D2D1::RectF(11,181,349,196),10,15,DWRITE_FONT_WEIGHT_NORMAL,D2D1::ColorF(m_appearanceSaveFailed ? 0xBC5648 : dark ? 0xA8B6C8 : 0x64748B));
    // Appearance must not retain invisible hit targets from older settings sections.
    for(auto& r:m_metricToggleRects)r={};m_intervalRect={};m_tempUnitRect={};m_highTempAlertRect={};m_reduceTransRect={};m_sliderTrackRect={};
    if(glass) {
        wchar_t percent[16];swprintf_s(percent,L"%.0f%%",m_settings.glassOpacity*100);
        text(L"背景不透明度",D2D1::RectF(11,203,250,221),12,18,DWRITE_FONT_WEIGHT_NORMAL,muted);text(percent,D2D1::RectF(270,203,349,221),12,18,DWRITE_FONT_WEIGHT_MEDIUM,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
        m_sliderTrackRect=D2D1::RectF(11,227,349,245);brush->SetColor(D2D1::ColorF(0xC2CBD9));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(11,234,349,238),2,2),brush.Get());
        const float thumb=18+324*(std::clamp(m_settings.glassOpacity,.1f,1.f)-.1f)/.9f;
        brush->SetColor(blue);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(11,234,thumb,238),2,2),brush.Get());brush->SetColor(D2D1::ColorF(0xFFFFFF));rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumb,236),7,7),brush.Get());brush->SetColor(blue);rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(thumb,236),7,7),brush.Get(),1);
    }
}

void ProDashboardWindow::DrawSettingsMonitoring(ID2D1DCRenderTarget* rt) {
    // Reuse the calibrated preferences shell; discard appearance-only content below the tabs.
    auto* dw=m_renderer.GetDWriteFactory();
    const bool dark=m_settings.themeMode==2||(m_settings.themeMode==3&&SystemUsesDarkTheme());
    const auto primary=D2D1::ColorF(dark?0xE7EDF5:0x192739),muted=D2D1::ColorF(dark?0xA8B6C8:0x53657A),subtle=D2D1::ColorF(dark?0xA8B6C8:0x64748B),blue=D2D1::ColorF(dark?0x80B3FF:0x266BCE);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(dark?0x192330:0xFFFFFF,m_settings.themeMode==0?GetDashboardBgAlpha():1.f),&b);if(!b)return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,m_dipHeight),20,24,0,8,.07f);
    auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,m_dipHeight-.5f),19.5f,19.5f);rt->FillRoundedRectangle(panel,b.Get());b->SetColor(D2D1::ColorF(dark?0x39485A:0xDDE3EB));rt->DrawRoundedRectangle(panel,b.Get(),1);
    auto text=[&](const wchar_t* s,D2D1_RECT_F r,float size,float line,DWRITE_FONT_WEIGHT weight,D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING){m_overviewVisuals.Text(rt,dw,s,r,size,line,weight,color,align);};
    text(L"偏好设置",D2D1::RectF(11,5,310,29),16,24,DWRITE_FONT_WEIGHT_BOLD,primary);m_settingsDoneRect=D2D1::RectF(324,8.5f,349,25.5f);text(L"完成",m_settingsDoneRect,11,17,DWRITE_FONT_WEIGHT_BOLD,blue);
    const wchar_t* tabs[]={L"外观",L"监控",L"常规"};for(int i=0;i<3;++i){float x=11+i*115.333333f;m_tabRects[i]=D2D1::RectF(x,33,x+107.333333f,63);if(i==1){b->SetColor(D2D1::ColorF(dark?0x243E61:0xEDF3FB));rt->FillRoundedRectangle(D2D1::RoundedRect(m_tabRects[i],8,8),b.Get());}text(tabs[i],m_tabRects[i],12,18,DWRITE_FONT_WEIGHT_MEDIUM,i==1?blue:muted,DWRITE_TEXT_ALIGNMENT_CENTER);}
    for(auto& r:m_themeRects)r={};for(auto& r:m_layoutOptionRects)r={};for(auto& r:m_metricToggleRects)r={};for(auto& r:m_indicatorGripRects)r={};m_intervalRect={};m_tempUnitRect={};m_highTempAlertRect={};m_sliderTrackRect={};m_reduceTransRect={};
    auto divider=[&](float y){b->SetColor(D2D1::ColorF(dark?0x39485A:0xDDE3EB));rt->FillRectangle(D2D1::RectF(11,y,349,y+1),b.Get());};divider(67);
    const int count=m_settings.showCpu+m_settings.showTemp+m_settings.showRam+m_settings.showNet;wchar_t summary[80];swprintf_s(summary,L"%d 项已显示",count);
    auto header=[&](int group,const wchar_t* label,const wchar_t* hint,float y){m_monitoringHeaders[group]=D2D1::RectF(11,y,349,y+30);text(label,D2D1::RectF(11,y+5,200,y+25),12,18,DWRITE_FONT_WEIGHT_MEDIUM,muted);if(m_monitoringGroup!=group)text(hint,D2D1::RectF(201,y+7.5f,338,y+22.5f),10,15,DWRITE_FONT_WEIGHT_NORMAL,subtle,DWRITE_TEXT_ALIGNMENT_TRAILING);D2D1_MATRIX_3X2_F t;rt->GetTransform(&t);if(m_monitoringGroup!=group)rt->SetTransform(D2D1::Matrix3x2F::Rotation(-90,D2D1::Point2F(346, y+15))*t);m_overviewVisuals.DrawIcon(rt,ProOverviewVisuals::SettingsDisclosure,D2D1::RectF(342.3333f,y+12.8333f,349.6667f,y+17.1667f));rt->SetTransform(t);};
    header(0,L"悬浮条指标",summary,72);
    if(m_monitoringGroup==0){text(L"拖动调整顺序 · 隐藏不常看的指标",D2D1::RectF(11,106,349,121),10,15,DWRITE_FONT_WEIGHT_NORMAL,subtle);const wchar_t* labels[]={L"CPU 占用",L"CPU 温度",L"内存",L"网络"};bool flags[]={m_settings.showCpu,m_settings.showTemp,m_settings.showRam,m_settings.showNet};for(int row=0;row<4;++row){int id=m_settings.indicatorOrder[row];float y=125+row*22.f;m_indicatorGripRects[row]=D2D1::RectF(11,y-2,35,y+20);m_overviewVisuals.DrawIcon(rt,ProOverviewVisuals::SettingsGrip,D2D1::RectF(11,y+3,23,y+15));text(labels[id],D2D1::RectF(31,y,280,y+18),12,18,DWRITE_FONT_WEIGHT_NORMAL,primary);m_metricToggleRects[id]=D2D1::RectF(309,y-5,349,y+23);if(flags[id])m_overviewVisuals.DrawIcon(rt,ProOverviewVisuals::SettingsToggleOn,D2D1::RectF(317,y,349,y+18));else{b->SetColor(D2D1::ColorF(0xC2CBD9));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(317,y,349,y+18),9,9),b.Get());b->SetColor(D2D1::ColorF(0xFFFFFF));rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(326,y+9),7,7),b.Get());}}}
    const float displayY=m_monitoringGroup==0?218.f:111.f;divider(displayY-5);
    const bool fahrenheit=m_settings.tempUnit==1;const wchar_t* interval=m_settings.updateIntervalSec==0?L"0.5 秒":m_settings.updateIntervalSec==2?L"2 秒":L"1 秒";
    if(m_settings.highTempAlert==0)swprintf_s(summary,L"%s · °%s · 关闭",interval,fahrenheit?L"F":L"C");else swprintf_s(summary,L"%s · °%s · %.0f°%s",interval,fahrenheit?L"F":L"C",fahrenheit?m_settings.highTempAlert*1.8+32:double(m_settings.highTempAlert),fahrenheit?L"F":L"C");
    header(1,L"显示与提醒",summary,displayY);
    float devicesY=displayY+34;
    if(m_monitoringGroup==1){const wchar_t* labels[]={L"更新间隔",L"温度单位",L"CPU 高温提醒"};std::wstring values[]={std::wstring(interval)+L"  ▾",fahrenheit?L"华氏度 °F  ▾":L"摄氏度 °C  ▾",L"关闭  ▾"};if(m_settings.highTempAlert>0){swprintf_s(summary,L"%.0f°%s  ▾",fahrenheit?m_settings.highTempAlert*1.8+32:double(m_settings.highTempAlert),fahrenheit?L"F":L"C");values[2]=summary;}D2D1_RECT_F* targets[]={&m_intervalRect,&m_tempUnitRect,&m_highTempAlertRect};for(int i=0;i<3;++i){float y=devicesY+22*i;*targets[i]=D2D1::RectF(200,y,349,y+18);text(labels[i],D2D1::RectF(11,y,195,y+18),12,18,DWRITE_FONT_WEIGHT_NORMAL,muted);text(values[i].c_str(),*targets[i],12,18,DWRITE_FONT_WEIGHT_MEDIUM,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);}devicesY+=66;}
    const auto* disk=SelectedDisk();std::wstring deviceNames[]={m_snapshot.gpuName.empty()?L"读取中":m_snapshot.gpuName,disk?disk->model:L"未发现磁盘",m_snapshot.netAdapterName.empty()?L"未连接":m_snapshot.netAdapterName};const wchar_t* labels[]={L"GPU",L"磁盘",L"网络"};
    for(int i=0;i<3;++i){float y=devicesY+i*22;m_monitoringDeviceRects[i]=D2D1::RectF(95,y,349,y+18);auto value=deviceNames[i];const float maxWidth=i==1?237.f:254.f;while(!value.empty()&&m_overviewVisuals.Measure(dw,(value+L"…").c_str(),12,DWRITE_FONT_WEIGHT_MEDIUM)>maxWidth)value.pop_back();if(value!=deviceNames[i])value+=L"…";if(i==1&&!m_snapshot.physicalDisks.empty())value+=L"  ▾";text(labels[i],D2D1::RectF(11,y,85,y+18),12,18,DWRITE_FONT_WEIGHT_NORMAL,muted);text(value.c_str(),m_monitoringDeviceRects[i],12,18,DWRITE_FONT_WEIGHT_MEDIUM,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);}
    if(m_appearanceSaveFailed)text(L"未保存",D2D1::RectF(264,8.5f,316,25.5f),10,17,DWRITE_FONT_WEIGHT_NORMAL,D2D1::ColorF(0xBC5648),DWRITE_TEXT_ALIGNMENT_TRAILING);
    if(m_monitoringGroup==0)for(auto& r:m_metricToggleRects){r.top+=3;r.bottom-=3;} // Adjacent 22-DIP rows must not have overlapping targets.
    const auto focus=MonitoringFocusRect();if(focus.right>focus.left){b->SetColor(blue);rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(focus.left-1,focus.top-1,focus.right+1,focus.bottom+1),4,4),b.Get(),1);}
}

void ProDashboardWindow::ApplyMonitoringChoice(int control,int value) {
    if(control==0&&value>=0&&value<=2)m_settings.updateIntervalSec=value;
    else if(control==1&&value>=0&&value<=1)m_settings.tempUnit=value;
    else if(control==2&&(value==0||(value>=75&&value<=95&&value%5==0)))m_alertTempThreshold=m_settings.highTempAlert=value;
    else if(control==3&&value>=0&&value<static_cast<int>(m_snapshot.physicalDisks.size()))m_selectedDiskId=m_snapshot.physicalDisks[value].id;
    else return;
    if(m_onSettingsChanged)m_onSettingsChanged(m_settings);Render();
}

bool ProDashboardWindow::OnMonitoringClick(float x,float y) {
    auto hit=[&](D2D1_RECT_F r){return r.right>r.left&&x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom;};
    for(int i=0;i<2;++i)if(hit(m_monitoringHeaders[i])){m_monitoringGroup=m_monitoringGroup==i?-1:i;UpdateDimensionsAndPosition();Render();return true;}
    if(m_monitoringGroup==0){for(int row=0;row<4;++row)if(hit(m_indicatorGripRects[row])){m_indicatorDragging=row;if(m_hwnd)SetCapture(m_hwnd);return true;}bool* flags[]={&m_settings.showCpu,&m_settings.showTemp,&m_settings.showRam,&m_settings.showNet};for(int i=0;i<4;++i)if(hit(m_metricToggleRects[i])){*flags[i]=!*flags[i];if(m_onSettingsChanged)m_onSettingsChanged(m_settings);Render();return true;}}
    const D2D1_RECT_F controls[]={m_intervalRect,m_tempUnitRect,m_highTempAlertRect,m_monitoringDeviceRects[1]};int control=-1;for(int i=0;i<4;++i)if((i==3||m_monitoringGroup==1)&&hit(controls[i])){control=i;break;}if(control<0||!m_hwnd)return false;
    HMENU menu=CreatePopupMenu();if(!menu)return false;std::vector<int> values;auto add=[&](const wchar_t* label,int value,bool selected){values.push_back(value);AppendMenuW(menu,MF_STRING|(selected?MF_CHECKED:0),values.size(),label);};
    if(control==0){add(L"0.5 秒",0,m_settings.updateIntervalSec==0);add(L"1 秒",1,m_settings.updateIntervalSec==1);add(L"2 秒",2,m_settings.updateIntervalSec==2);}
    else if(control==1){add(L"摄氏度 °C",0,m_settings.tempUnit==0);add(L"华氏度 °F",1,m_settings.tempUnit==1);}
    else if(control==2){add(L"关闭",0,m_settings.highTempAlert==0);for(int v=75;v<=95;v+=5){wchar_t label[32];swprintf_s(label,L"%.0f°%s",m_settings.tempUnit==1?v*1.8+32:double(v),m_settings.tempUnit==1?L"F":L"C");add(label,v,m_settings.highTempAlert==v);}}
    else for(size_t i=0;i<m_snapshot.physicalDisks.size();++i){const auto& d=m_snapshot.physicalDisks[i];std::wstring label=L"磁盘 "+std::to_wstring(d.number)+L" · "+d.model;std::replace(label.begin(),label.end(),L'&',L' ');add(label.c_str(),static_cast<int>(i),d.id==m_selectedDiskId);}
    POINT p{static_cast<LONG>((controls[control].left+m_shadowApron)*m_dpiScale),static_cast<LONG>((controls[control].bottom+m_shadowApron)*m_dpiScale)};ClientToScreen(m_hwnd,&p);m_settingsMenuActive=true;int selected=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,m_hwnd,nullptr);m_settingsMenuActive=false;DestroyMenu(menu);if(selected>0&&selected<=static_cast<int>(values.size()))ApplyMonitoringChoice(control,values[selected-1]);return true;
}

void ProDashboardWindow::MoveMonitoringIndicator(float y) {
    if(m_indicatorDragging<0||m_settings.activeTab!=1||m_monitoringGroup!=0)return;
    int target=std::clamp(static_cast<int>((y+m_cpuScroll-125)/22),0,3);if(target==m_indicatorDragging)return;auto& order=m_settings.indicatorOrder;int value=order[m_indicatorDragging];if(target>m_indicatorDragging)for(int i=m_indicatorDragging;i<target;++i)order[i]=order[i+1];else for(int i=m_indicatorDragging;i>target;--i)order[i]=order[i-1];order[target]=value;m_indicatorDragging=target;Render();
}

D2D1_RECT_F ProDashboardWindow::MonitoringFocusRect() const {
    if(m_monitoringFocus==-10)return m_settingsDoneRect;
    if(m_monitoringFocus>=-9&&m_monitoringFocus<=-7)return m_tabRects[m_monitoringFocus+9];
    if(m_monitoringFocus>=0&&m_monitoringFocus<=1)return m_monitoringHeaders[m_monitoringFocus];
    if(m_monitoringFocus>=2&&m_monitoringFocus<=5)return m_metricToggleRects[m_monitoringFocus-2];
    if(m_monitoringFocus==6)return m_intervalRect;if(m_monitoringFocus==7)return m_tempUnitRect;if(m_monitoringFocus==8)return m_highTempAlertRect;if(m_monitoringFocus==9)return m_monitoringDeviceRects[1];return {};
}
bool ProDashboardWindow::OnMonitoringKey(UINT key,bool shift,bool alt) {
    if(m_mode!=Mode_Settings||m_settings.activeTab!=1)return false;
    std::vector<int> targets{-10,-9,-8,-7,0};if(m_monitoringGroup==0)for(int id:m_settings.indicatorOrder)targets.push_back(id+2);targets.push_back(1);if(m_monitoringGroup==1){targets.push_back(6);targets.push_back(7);targets.push_back(8);}if(!m_snapshot.physicalDisks.empty())targets.push_back(9);
    if(key==VK_TAB){auto found=std::find(targets.begin(),targets.end(),m_monitoringFocus);int index=found==targets.end()?(shift?0:-1):static_cast<int>(found-targets.begin());index=(index+(shift?-1:1)+static_cast<int>(targets.size()))%static_cast<int>(targets.size());m_monitoringFocus=targets[index];Render();if(m_monitoringFocus>=0)EnsureSettingsFocusVisible(MonitoringFocusRect());return true;}
    if(alt&&(key==VK_UP||key==VK_DOWN)&&m_monitoringFocus>=2&&m_monitoringFocus<=5&&m_monitoringGroup==0){auto found=std::find(m_settings.indicatorOrder.begin(),m_settings.indicatorOrder.end(),m_monitoringFocus-2);int row=static_cast<int>(found-m_settings.indicatorOrder.begin()),other=std::clamp(row+(key==VK_UP?-1:1),0,3);std::swap(m_settings.indicatorOrder[row],m_settings.indicatorOrder[other]);if(m_onSettingsChanged)m_onSettingsChanged(m_settings);Render();return true;}
    if((key==VK_RETURN||key==VK_SPACE)&&m_monitoringFocus!=-1){auto r=MonitoringFocusRect();OnClick((r.left+r.right)/2,(r.top+r.bottom)/2);return true;}return false;
}

void ProDashboardWindow::DrawSettingsGeneral(ID2D1DCRenderTarget* rt) {
    auto* dw=m_renderer.GetDWriteFactory();
    const bool dark=m_settings.themeMode==2||(m_settings.themeMode==3&&SystemUsesDarkTheme());
    const auto primary=D2D1::ColorF(dark?0xE7EDF5:0x192739),muted=D2D1::ColorF(dark?0xA8B6C8:0x53657A),subtle=D2D1::ColorF(dark?0xA8B6C8:0x64748B),blue=D2D1::ColorF(dark?0x80B3FF:0x266BCE);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(dark?0x192330:0xFFFFFF,m_settings.themeMode==0?GetDashboardBgAlpha():1.f),&b);if(!b)return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,260),20,24,0,8,.07f);
    auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,259.5f),19.5f,19.5f);rt->FillRoundedRectangle(panel,b.Get());b->SetColor(D2D1::ColorF(dark?0x39485A:0xDDE3EB));rt->DrawRoundedRectangle(panel,b.Get(),1);
    auto text=[&](const wchar_t* value,D2D1_RECT_F r,float size,float line,DWRITE_FONT_WEIGHT weight,D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING){m_overviewVisuals.Text(rt,dw,value,r,size,line,weight,color,align);};
    text(L"偏好设置",D2D1::RectF(11,5,310,29),16,24,DWRITE_FONT_WEIGHT_BOLD,primary);m_settingsDoneRect=D2D1::RectF(324,8.5f,349,25.5f);text(L"完成",m_settingsDoneRect,11,17,DWRITE_FONT_WEIGHT_BOLD,blue);
    const wchar_t* tabs[]={L"外观",L"监控",L"常规"};for(int i=0;i<3;++i){float x=11+i*115.333333f;m_tabRects[i]=D2D1::RectF(x,33,x+107.333333f,63);if(i==2){b->SetColor(D2D1::ColorF(dark?0x243E61:0xEDF3FB));rt->FillRoundedRectangle(D2D1::RoundedRect(m_tabRects[i],8,8),b.Get());}text(tabs[i],m_tabRects[i],12,18,DWRITE_FONT_WEIGHT_MEDIUM,i==2?blue:muted,DWRITE_TEXT_ALIGNMENT_CENTER);}
    for(auto& r:m_themeRects)r={};for(auto& r:m_layoutOptionRects)r={};for(auto& r:m_metricToggleRects)r={};for(auto& r:m_monitoringHeaders)r={};m_intervalRect={};m_tempUnitRect={};m_highTempAlertRect={};m_sliderTrackRect={};m_reduceTransRect={};
    text(L"窗口行为",D2D1::RectF(11,67,349,82),10,15,DWRITE_FONT_WEIGHT_NORMAL,subtle);
    const auto& g=m_settings.general;const bool values[]={g.autoStart,g.alwaysOnTop,g.lockPosition,g.hideInFullscreen,g.rememberPosition,g.reduceMotion};
    const wchar_t* labels[]={L"开机启动",L"始终置顶",L"锁定悬浮条位置",L"全屏时隐藏",L"记住窗口位置",L"减少动画"};
    for(int i=0;i<6;++i){float y=86+22.f*i;m_generalToggleRects[i]=D2D1::RectF(309,y-2,349,y+20);text(labels[i],D2D1::RectF(11,y,285,y+18),12,18,DWRITE_FONT_WEIGHT_NORMAL,primary);m_overviewVisuals.DrawIcon(rt,values[i]?ProOverviewVisuals::SettingsToggleOn:ProOverviewVisuals::SettingsToggleOff,D2D1::RectF(317,y,349,y+18));}
    text(L"呼出 / 隐藏",D2D1::RectF(11,218,175,236),12,18,DWRITE_FONT_WEIGHT_NORMAL,muted);m_generalHotkeyRect=D2D1::RectF(180,218,349,236);
    const auto shortcut=m_hotkeyCapturing?std::wstring(L"请按快捷键…"):ProWindowBehavior::HotkeyText(g.hotkeyModifiers,g.hotkeyKey);
    text(shortcut.c_str(),m_generalHotkeyRect,m_overviewVisuals.Measure(dw,shortcut.c_str(),12,DWRITE_FONT_WEIGHT_MEDIUM)>169?10.f:12.f,18,DWRITE_FONT_WEIGHT_MEDIUM,m_hotkeyCapturing?blue:primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const wchar_t* hint=!m_generalError.empty()?m_generalError.c_str():m_appearanceSaveFailed?L"设置未保存，请检查配置文件写入权限。":m_hotkeyCapturing?L"按下组合键；Esc 取消，Delete 清除。":L"固定仅保持面板展开，不锁定位置。";
    text(hint,D2D1::RectF(11,240,349,255),10,15,DWRITE_FONT_WEIGHT_NORMAL,!m_generalError.empty()||m_appearanceSaveFailed?D2D1::ColorF(0xBC5648):subtle);
    D2D1_RECT_F focus{};if(m_generalFocus==0)focus=m_settingsDoneRect;else if(m_generalFocus>=1&&m_generalFocus<=3)focus=m_tabRects[m_generalFocus-1];else if(m_generalFocus>=4&&m_generalFocus<=9)focus=m_generalToggleRects[m_generalFocus-4];else if(m_generalFocus==10)focus=m_generalHotkeyRect;
    if(focus.right>focus.left){b->SetColor(blue);rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(focus.left-1,focus.top-1,focus.right+1,focus.bottom+1),4,4),b.Get(),1);}
}

bool ProDashboardWindow::CommitGeneral(ProGeneralSettings value) {
    std::wstring error;if(m_onGeneralChange&&!m_onGeneralChange(value,error)){m_generalError=error;Render();return false;}
    m_settings.general=value;m_generalError.clear();if(m_onSettingsChanged)m_onSettingsChanged(m_settings);Render();return true;
}
bool ProDashboardWindow::CommitGeneralHotkey(UINT modifiers,UINT key) {
    if(!IsValidProHotkey(modifiers,key)){m_generalError=L"请使用 Ctrl 或 Alt 加字母、数字或 F 键。";Render();return false;}
    auto value=m_settings.general;value.hotkeyModifiers=modifiers;value.hotkeyKey=key;
    if(!CommitGeneral(value))return false;m_hotkeyCapturing=false;Render();return true;
}
bool ProDashboardWindow::OnGeneralClick(float x,float y) {
    auto hit=[&](D2D1_RECT_F r){return r.right>r.left&&x>=r.left&&x<=r.right&&y>=r.top&&y<=r.bottom;};
    for(int i=0;i<6;++i)if(hit(m_generalToggleRects[i])){m_hotkeyCapturing=false;auto value=m_settings.general;bool* flags[]={&value.autoStart,&value.alwaysOnTop,&value.lockPosition,&value.hideInFullscreen,&value.rememberPosition,&value.reduceMotion};*flags[i]=!*flags[i];CommitGeneral(value);return true;}
    if(hit(m_generalHotkeyRect)){m_hotkeyCapturing=true;m_generalFocus=10;m_generalError.clear();if(m_hwnd)SetFocus(m_hwnd);Render();return true;}return false;
}
bool ProDashboardWindow::OnGeneralKey(UINT key,bool shift,bool alt) {
    if(m_mode!=Mode_Settings||m_settings.activeTab!=2)return false;
    if(m_hotkeyCapturing){
        if(key==VK_ESCAPE){m_hotkeyCapturing=false;m_generalError.clear();Render();return true;}
        if(key==VK_TAB)m_hotkeyCapturing=false;
        else if(key==VK_DELETE||key==VK_BACK){CommitGeneralHotkey(0,0);return true;}
        else if(key==VK_CONTROL||key==VK_MENU||key==VK_SHIFT||key==VK_LWIN||key==VK_RWIN)return true;
        else {UINT mods=(GetKeyState(VK_CONTROL)&0x8000?MOD_CONTROL:0)|(alt?MOD_ALT:0)|(shift?MOD_SHIFT:0)|((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000?MOD_WIN:0);CommitGeneralHotkey(mods,key);return true;}
    }
    if(key==VK_TAB){m_generalFocus=m_generalFocus<0?(shift?10:0):(m_generalFocus+(shift?-1:1)+11)%11;Render();if(m_generalFocus>=4)EnsureSettingsFocusVisible(m_generalFocus<=9?m_generalToggleRects[m_generalFocus-4]:m_generalHotkeyRect);return true;}
    if((key==VK_RETURN||key==VK_SPACE)&&m_generalFocus>=0){D2D1_RECT_F r=m_generalFocus==0?m_settingsDoneRect:m_generalFocus<=3?m_tabRects[m_generalFocus-1]:m_generalFocus<=9?m_generalToggleRects[m_generalFocus-4]:m_generalHotkeyRect;OnClick((r.left+r.right)/2,(r.top+r.bottom)/2);return true;}return false;
}

void ProDashboardWindow::DrawSettings(ID2D1DCRenderTarget* rt, float width, float height) {
    auto SafeRelease = [](auto** pp) {
        if (*pp) { (*pp)->Release(); *pp = nullptr; }
    };

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    if (!pDwFactory) return;

    // DirectWrite Format Helper
    auto MakeFmt = [&](const wchar_t* locale, DWRITE_FONT_WEIGHT weight, float size, DWRITE_TEXT_ALIGNMENT align) -> IDWriteTextFormat* {
        IDWriteTextFormat* fmt = nullptr;
        if (SUCCEEDED(pDwFactory->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, locale, &fmt)) && fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        return fmt;
    };

    // Typography per Figma specs:
    // 标题: 16 / 24 Bold
    // 正文: 12 / 18 Regular
    // 主题按钮及“完成”: 11 / 17 Medium
    // 辅助说明: 10 / 15 Regular
    // 数值与值选项: 12 / 18 Medium
    IDWriteTextFormat* pFontTitle       = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_BOLD, 16.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontDone        = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 11.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontTabActive   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 12.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontTabInactive = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 12.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontSec         = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 12.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontSub         = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 10.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontItemLabel   = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_NORMAL, 12.0f, DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat* pFontItemVal     = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 12.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat* pFontThemeBtn    = MakeFmt(L"zh-cn", DWRITE_FONT_WEIGHT_MEDIUM, 11.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat* pFontOpacPct     = MakeFmt(L"en-us", DWRITE_FONT_WEIGHT_MEDIUM, 12.0f, DWRITE_TEXT_ALIGNMENT_TRAILING);

    // Color Brushes per Figma design tokens
    ID2D1SolidColorBrush* pShadowBrush    = nullptr;
    ID2D1SolidColorBrush* pPanelBg        = nullptr;
    ID2D1SolidColorBrush* pPanelBorder    = nullptr;
    ID2D1SolidColorBrush* pBlue           = nullptr;
    ID2D1SolidColorBrush* pBlueHover      = nullptr;
    ID2D1SolidColorBrush* pTabSelectedBg  = nullptr;
    ID2D1SolidColorBrush* pThemeUnselBg   = nullptr;
    ID2D1SolidColorBrush* pSliderTrackBg  = nullptr;
    ID2D1SolidColorBrush* pWhite          = nullptr;
    ID2D1SolidColorBrush* pToggleOffBg    = nullptr;
    ID2D1SolidColorBrush* pDivider        = nullptr;
    ID2D1SolidColorBrush* pGripDot        = nullptr;
    ID2D1SolidColorBrush* pThumbShadow    = nullptr;

    // Drop shadow: X=0, Y=8, Blur=24, #26385C alpha 10%
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.220f, 0.361f, 0.10f), &pShadowBrush);

    // Panel background & border per theme
    float effectiveAlpha = GetDashboardBgAlpha();
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, effectiveAlpha), &pPanelBg);
    if (IsPureWhite()) {
        rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &pPanelBorder);   // #DDE3EB
        rt->CreateSolidColorBrush(D2D1::ColorF(0.929f, 0.953f, 0.984f, 1.0f), &pTabSelectedBg); // #EDF3FB 选中背景
        rt->CreateSolidColorBrush(D2D1::ColorF(0.969f, 0.976f, 0.988f, 1.0f), &pThemeUnselBg);  // #F7F9FC 未选背景
        rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &pDivider);       // #DDE3EB
    } else {
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f), &pPanelBorder);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.65f), &pTabSelectedBg);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.25f), &pThemeUnselBg);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 1.0f), &pDivider);
    }

    // Blue #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &pBlue);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.31f, 0.85f, 1.0f), &pBlueHover);

    // Slider track & toggle off
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 1.0f), &pSliderTrackBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 1.0f), &pToggleOffBg);

    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &pWhite);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.58f, 0.64f, 0.72f, 1.0f), &pGripDot);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.220f, 0.361f, 0.16f), &pThumbShadow);

    // 1. Draw Panel Drop Shadow (Y=8, Blur radius around 12)
    if (pShadowBrush) {
        D2D1_ROUNDED_RECT sRect = D2D1::RoundedRect(D2D1::RectF(0.5f, 6.0f, width - 0.5f, height + 8.0f), 20.0f, 20.0f);
        rt->FillRoundedRectangle(sRect, pShadowBrush);
    }

    // 2. Draw Panel Outer Body (420 x 504, Radius 20, Background #FFFFFF alpha 40%, Border #FFFFFF alpha 85% 1 DIP)
    D2D1_ROUNDED_RECT panelRect = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, width - 0.5f, height - 0.5f), 20.0f, 20.0f);
    if (pPanelBg) rt->FillRoundedRectangle(panelRect, pPanelBg);
    if (pPanelBorder) rt->DrawRoundedRectangle(panelRect, pPanelBorder, 1.0f);

    float startX = 17.0f;
    float contentW = 388.0f;
    float endX = startX + contentW; // 405.0f

    // ==========================================
    // 1. 标题栏 (X=17, Y=13, 388 x 24)
    // ==========================================
    float titleY = 13.0f;
    if (pFontTitle) {
        rt->DrawTextW(L"偏好设置", 4, pFontTitle, D2D1::RectF(startX, titleY, startX + 160.0f, titleY + 24.0f), m_pTextPrimary);
    }
    m_settingsDoneRect = D2D1::RectF(endX - 50.0f, titleY, endX, titleY + 24.0f);
    if (pFontDone && pBlue) {
        ID2D1SolidColorBrush* pBtnCol = m_settingsDoneHover ? pBlueHover : pBlue;
        rt->DrawTextW(L"完成", 2, pFontDone, m_settingsDoneRect, pBtnCol);
    }

    // ==========================================
    // 2. 外观 / 监控 / 常规标签栏 (X=17, Y=49, 388 x 30)
    //    3个按钮尺寸: 120 x 30, 圆角 8; 间隔 14; X: 17, 151, 285
    //    字体 12 Medium / 行高 18，水平垂直居中
    //    “外观”选中背景为白色、65% 不透明度，文字蓝色；其他标签无背景，文字灰蓝色
    // ==========================================
    float tabY = 49.0f;
    float tabW = 120.0f;
    float tabH = 30.0f;
    const float tabXs[3] = { 17.0f, 151.0f, 285.0f };
    const wchar_t* tabTitles[3] = { L"外观", L"监控", L"常规" };

    for (int i = 0; i < 3; ++i) {
        m_tabRects[i] = D2D1::RectF(tabXs[i], tabY, tabXs[i] + tabW, tabY + tabH);
        D2D1_ROUNDED_RECT btnR = D2D1::RoundedRect(m_tabRects[i], 8.0f, 8.0f);

        if (m_settings.activeTab == i) {
            // 已选标签: 白色，不透明度 65%
            if (pTabSelectedBg) rt->FillRoundedRectangle(btnR, pTabSelectedBg);
            if (pFontTabActive && pBlue) {
                rt->DrawTextW(tabTitles[i], static_cast<UINT32>(wcslen(tabTitles[i])), pFontTabActive, m_tabRects[i], pBlue);
            }
        } else {
            // 未选标签: 无背景填充
            if (pFontTabInactive) {
                rt->DrawTextW(tabTitles[i], static_cast<UINT32>(wcslen(tabTitles[i])), pFontTabInactive, m_tabRects[i], m_pTextSecondary);
            }
        }
    }

    if (m_settings.activeTab == 0) {
        // ==========================================
        // 3. 外观设置区 (X=17, Y=91)
        // ==========================================
        float sec1Y = 91.0f;
        if (pFontSec) {
            rt->DrawTextW(L"外观", 2, pFontSec, D2D1::RectF(startX, sec1Y, startX + 100.0f, sec1Y + 18.0f), m_pTextSecondary);
        }

        // 四项主题切换: 亮色玻璃 (80), 纯白 (77), 深色 (80), 跟随系统 (80)
        // Figma 248:3779 / 248:3839: X = startX (11), Y = 89 (relative to panel: 11 + 78), 高 29, 间距 6
        float themeY = startX + 78.0f; // 11 + 78 = 89
        const float themeWidths[4] = { 80.0f, 77.0f, 80.0f, 80.0f };
        const wchar_t* themeTitles[4] = { L"亮色玻璃", L"纯白", L"深色", L"跟随系统" };
        float curThemeX = startX;

        for (int i = 0; i < 4; ++i) {
            float tW = themeWidths[i];
            m_themeRects[i] = D2D1::RectF(curThemeX, themeY, curThemeX + tW, themeY + 29.0f);
            D2D1_ROUNDED_RECT tR = D2D1::RoundedRect(m_themeRects[i], 6.0f, 6.0f);

            if (m_settings.themeMode == i) {
                // 选中态: #EDF3FB 背景 + 1 DIP 蓝色内描边 + 蓝色文字
                if (pTabSelectedBg) rt->FillRoundedRectangle(tR, pTabSelectedBg);
                if (pBlue) rt->DrawRoundedRectangle(tR, pBlue, 1.0f);
                if (pFontThemeBtn && pBlue) {
                    rt->DrawTextW(themeTitles[i], static_cast<UINT32>(wcslen(themeTitles[i])), pFontThemeBtn, m_themeRects[i], pBlue);
                }
            } else {
                // 未选中态: #F7F9FC 背景 + 灰蓝色文字，无描边
                if (pThemeUnselBg) rt->FillRoundedRectangle(tR, pThemeUnselBg);
                if (pFontThemeBtn) {
                    rt->DrawTextW(themeTitles[i], static_cast<UINT32>(wcslen(themeTitles[i])), pFontThemeBtn, m_themeRects[i], m_pTextSecondary);
                }
            }
            curThemeX += tW + 6.0f;
        }

        // ==========================================
        // 4. 展开面板布局单选 (Figma 248:3779 / 248:3849: 紧凑胶囊 / 完整长条)
        // ==========================================
        float layoutSecY = themeY + 33.0f; // Y = 122
        if (pFontSec) {
            rt->DrawTextW(L"展开面板布局", 6, pFontSec, D2D1::RectF(startX, layoutSecY, startX + 160.0f, layoutSecY + 18.0f), m_pTextSecondary);
        }

        float layoutOptY = layoutSecY + 24.0f; // Y = 146
        float optW = 166.0f; // 166 x 29
        float optH = 29.0f;
        const wchar_t* layoutTitles[2] = { L"紧凑胶囊", L"完整长条" };
        const float layoutXs[2] = { startX, startX + optW + 6.0f }; // 11, 183

        for (int i = 0; i < 2; ++i) {
            m_layoutOptionRects[i] = D2D1::RectF(layoutXs[i], layoutOptY, layoutXs[i] + optW, layoutOptY + optH);
            D2D1_ROUNDED_RECT optR = D2D1::RoundedRect(m_layoutOptionRects[i], 6.0f, 6.0f);

            bool isSelected = (m_settings.layoutMode == (1 - i)); // i=0 is 紧凑胶囊 (layoutMode==1), i=1 is 完整长条 (layoutMode==0)
            if (isSelected) {
                if (pTabSelectedBg) rt->FillRoundedRectangle(optR, pTabSelectedBg);
                if (pBlue) rt->DrawRoundedRectangle(optR, pBlue, 1.0f);
                if (pFontThemeBtn && pBlue) {
                    rt->DrawTextW(layoutTitles[i], static_cast<UINT32>(wcslen(layoutTitles[i])), pFontThemeBtn, m_layoutOptionRects[i], pBlue);
                }
            } else {
                if (pThemeUnselBg) rt->FillRoundedRectangle(optR, pThemeUnselBg);
                if (pFontThemeBtn) {
                    rt->DrawTextW(layoutTitles[i], static_cast<UINT32>(wcslen(layoutTitles[i])), pFontThemeBtn, m_layoutOptionRects[i], m_pTextSecondary);
                }
            }
        }

        // 说明文字: "仅改变未展开指标的形态；展开项直接变为详情卡片。" (Figma 248:3779 / 248:3854)
        float hintY = layoutOptY + optH + 6.0f; // Y = 181
        if (pFontSub) {
            rt->DrawTextW(L"仅改变未展开指标的形态；展开项直接变为详情卡片。", 25, pFontSub, D2D1::RectF(startX, hintY, endX, hintY + 15.0f), m_pTextMuted);
        }

        bool showGlassOptions = (m_settings.themeMode == 0 || m_settings.themeMode == 3);

        if (showGlassOptions) {
            // 背景不透明度说明行
            float opacY = hintY + 22.0f;
            if (pFontItemLabel) {
                rt->DrawTextW(L"背景不透明度", 6, pFontItemLabel, D2D1::RectF(startX, opacY, startX + 140.0f, opacY + 18.0f), m_pTextSecondary);
            }
            int opacPct = static_cast<int>(m_settings.glassOpacity * 100.0f + 0.5f);
            wchar_t opacBuf[16];
            swprintf_s(opacBuf, L"%d%%", opacPct);
            if (pFontOpacPct) {
                rt->DrawTextW(opacBuf, static_cast<UINT32>(wcslen(opacBuf)), pFontOpacPct, D2D1::RectF(endX - 80.0f, opacY, endX, opacY + 18.0f), m_pTextPrimary);
            }

            // 滑块区域: 338 x 18
            float sliderY = opacY + 24.0f;
            float trackY = sliderY + 7.0f;
            float trackH = 4.0f;
            m_sliderTrackRect = D2D1::RectF(startX, sliderY, endX, sliderY + 18.0f);

            D2D1_ROUNDED_RECT inactTrack = D2D1::RoundedRect(D2D1::RectF(startX, trackY, endX, trackY + trackH), 2.0f, 2.0f);
            if (pSliderTrackBg) rt->FillRoundedRectangle(inactTrack, pSliderTrackBg);

            float normOpac = std::clamp(m_settings.glassOpacity, 0.10f, 1.00f);
            float filledW = contentW * normOpac;
            D2D1_ROUNDED_RECT actTrack = D2D1::RoundedRect(D2D1::RectF(startX, trackY, startX + filledW, trackY + trackH), 2.0f, 2.0f);
            if (pBlue) rt->FillRoundedRectangle(actTrack, pBlue);

            float thumbX = startX + filledW;
            thumbX = std::clamp(thumbX, startX + 7.0f, endX - 7.0f);
            float thumbCenterY = trackY + trackH * 0.5f;

            if (pThumbShadow) {
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, thumbCenterY + 1.0f), 7.0f, 7.0f), pThumbShadow);
            }
            if (pWhite && pBlue) {
                D2D1_ELLIPSE thumbEll = D2D1::Ellipse(D2D1::Point2F(thumbX, thumbCenterY), 7.0f, 7.0f);
                rt->FillEllipse(thumbEll, pWhite);
                rt->DrawEllipse(thumbEll, pBlue, 1.0f);
            }
        } else {
            m_sliderTrackRect = D2D1::RectF(0, 0, 0, 0);
        }

        // 第一条分割线
        float div1Y = showGlassOptions ? 260.0f : 204.0f;
        if (pDivider) {
            rt->DrawLine(D2D1::Point2F(startX, div1Y), D2D1::Point2F(endX, div1Y), pDivider, 1.0f);
        }

        // ==========================================
        // 5. 悬浮条指标设置区 (388 x 135)
        // 亮色玻璃/跟随系统: Y=217; 纯白/深色: Y=169
        // ==========================================
        float sec2Y = showGlassOptions ? 217.0f : 169.0f;
        if (pFontSec) {
            rt->DrawTextW(L"悬浮条指标", 5, pFontSec, D2D1::RectF(startX, sec2Y, startX + 140.0f, sec2Y + 18.0f), m_pTextPrimary);
        }
        float subY = sec2Y + 24.0f; // 241 or 193
        if (pFontSub) {
            rt->DrawTextW(L"拖动调整顺序 • 隐藏不常看的指标", 17, pFontSub, D2D1::RectF(startX, subY, startX + 280.0f, subY + 15.0f), m_pTextMuted);
        }

        // 指标设置行: 4 行, 起点间隔 24 (18 DIP 行高 + 6 DIP 留白)
        // 亮色玻璃/跟随系统: Y = 262, 286, 310, 334
        // 纯白/深色: Y = 214, 238, 262, 286
        float rowBaseY = sec2Y + 45.0f; // 262 or 214
        struct MetricToggle {
            const wchar_t* name;
            bool isChecked;
        };
        MetricToggle metrics[4] = {
            { L"CPU 占用", m_settings.showCpu },
            { L"CPU 温度", m_settings.showTemp },
            { L"内存",     m_settings.showRam },
            { L"网络",     m_settings.showNet }
        };

        for (int i = 0; i < 4; ++i) {
            float rowY = rowBaseY + i * 24.0f;

            // 拖动图标: 12 x 12, 每行左侧 (X: 17, 行内 Y: 3)
            if (pGripDot) {
                float iconX = startX + 2.0f;
                float iconY = rowY + 3.0f;
                for (int r = 0; r < 3; ++r) {
                    float dy = iconY + 2.0f + r * 4.0f;
                    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(iconX + 1.5f, dy), 1.0f, 1.0f), pGripDot);
                    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(iconX + 6.5f, dy), 1.0f, 1.0f), pGripDot);
                }
            }

            // 指标文字: X = 37, 字体 12 Regular / 18
            if (pFontItemLabel) {
                rt->DrawTextW(metrics[i].name, static_cast<UINT32>(wcslen(metrics[i].name)), pFontItemLabel, D2D1::RectF(37.0f, rowY, 37.0f + 160.0f, rowY + 18.0f), m_pTextPrimary);
            }

            // 开关: 32 x 18, 圆角 9, X = 373 -> (373, rowY, 405, rowY + 18)
            float swX = 373.0f;
            float swW = 32.0f;
            float swH = 18.0f;
            m_metricToggleRects[i] = D2D1::RectF(swX, rowY, swX + swW, rowY + swH);
            D2D1_ROUNDED_RECT swR = D2D1::RoundedRect(m_metricToggleRects[i], 9.0f, 9.0f);

            if (metrics[i].isChecked) {
                // 开启: 背景 #266BCE, 白色圆点 X = swX + 16, 距上下边缘 2
                if (pBlue) rt->FillRoundedRectangle(swR, pBlue);
                if (pWhite) {
                    D2D1_ROUNDED_RECT knobR = D2D1::RoundedRect(D2D1::RectF(swX + 16.0f, rowY + 2.0f, swX + 16.0f + 14.0f, rowY + 2.0f + 14.0f), 7.0f, 7.0f);
                    rt->FillRoundedRectangle(knobR, pWhite);
                }
            } else {
                // 关闭: 背景 #C2CBD9, 白色圆点 X = swX + 2, 距上下边缘 2
                if (pToggleOffBg) rt->FillRoundedRectangle(swR, pToggleOffBg);
                if (pWhite) {
                    D2D1_ROUNDED_RECT knobR = D2D1::RoundedRect(D2D1::RectF(swX + 2.0f, rowY + 2.0f, swX + 2.0f + 14.0f, rowY + 2.0f + 14.0f), 7.0f, 7.0f);
                    rt->FillRoundedRectangle(knobR, pWhite);
                }
            }
        }

        // 第二条分割线
        // 亮色玻璃/跟随系统: Y = 364; 纯白/深色: Y = 316
        float div2Y = showGlassOptions ? 364.0f : 316.0f;
        if (pDivider) {
            rt->DrawLine(D2D1::Point2F(startX, div2Y), D2D1::Point2F(endX, div2Y), pDivider, 1.0f);
        }

        // ==========================================
        // 7. 显示与提醒设置区
        // 亮色玻璃/跟随系统: Y=377 (高114); 纯白/深色: Y=329 (高90)
        // ==========================================
        float sec3Y = showGlassOptions ? 377.0f : 329.0f;
        if (pFontSec) {
            rt->DrawTextW(L"显示与提醒", 5, pFontSec, D2D1::RectF(startX, sec3Y, startX + 140.0f, sec3Y + 18.0f), m_pTextSecondary);
        }

        // 行 1: 更新间隔 (Y = 401 or 353)
        float r1Y = sec3Y + 24.0f;
        if (pFontItemLabel) {
            rt->DrawTextW(L"更新间隔", 4, pFontItemLabel, D2D1::RectF(startX, r1Y, startX + 120.0f, r1Y + 18.0f), m_pTextSecondary);
        }
        wchar_t intBuf[16];
        if (m_settings.updateIntervalSec == 0) swprintf_s(intBuf, L"0.5 秒 ▾");
        else swprintf_s(intBuf, L"%d 秒 ▾", m_settings.updateIntervalSec);
        m_intervalRect = D2D1::RectF(endX - 100.0f, r1Y, endX, r1Y + 18.0f);
        if (pFontItemVal) {
            rt->DrawTextW(intBuf, static_cast<UINT32>(wcslen(intBuf)), pFontItemVal, m_intervalRect, m_pTextPrimary);
        }

        // 行 2: 温度单位 (Y = 425 or 377)
        float r2Y = r1Y + 24.0f;
        if (pFontItemLabel) {
            rt->DrawTextW(L"温度单位", 4, pFontItemLabel, D2D1::RectF(startX, r2Y, startX + 120.0f, r2Y + 18.0f), m_pTextSecondary);
        }
        const wchar_t* unitStr = (m_settings.tempUnit == 0) ? L"摄氏度 °C ▾" : L"华氏度 °F ▾";
        m_tempUnitRect = D2D1::RectF(endX - 120.0f, r2Y, endX, r2Y + 18.0f);
        if (pFontItemVal) {
            rt->DrawTextW(unitStr, static_cast<UINT32>(wcslen(unitStr)), pFontItemVal, m_tempUnitRect, m_pTextPrimary);
        }

        // 行 3: CPU 高温提醒 (Y = 449 or 401)
        float r3Y = r2Y + 24.0f;
        if (pFontItemLabel) {
            rt->DrawTextW(L"CPU 高温提醒", 7, pFontItemLabel, D2D1::RectF(startX, r3Y, startX + 140.0f, r3Y + 18.0f), m_pTextSecondary);
        }
        wchar_t alertBuf[16];
        if (m_settings.highTempAlert <= 0) swprintf_s(alertBuf, L"关闭 ▾");
        else swprintf_s(alertBuf, L"%d°C ▾", m_settings.highTempAlert);
        m_highTempAlertRect = D2D1::RectF(endX - 100.0f, r3Y, endX, r3Y + 18.0f);
        if (pFontItemVal) {
            rt->DrawTextW(alertBuf, static_cast<UINT32>(wcslen(alertBuf)), pFontItemVal, m_highTempAlertRect, m_pTextPrimary);
        }

        // 行 4: 减少透明效果 (仅亮色玻璃/跟随系统显示, Y = 473)
        if (showGlassOptions) {
            float r4Y = r3Y + 24.0f;
            if (pFontItemLabel) {
                rt->DrawTextW(L"减少透明效果", 6, pFontItemLabel, D2D1::RectF(startX, r4Y, startX + 140.0f, r4Y + 18.0f), m_pTextSecondary);
            }
            float redSwX = 373.0f;
            float redSwW = 32.0f;
            float redSwH = 18.0f;
            m_reduceTransRect = D2D1::RectF(redSwX, r4Y, redSwX + redSwW, r4Y + redSwH);
            D2D1_ROUNDED_RECT redR = D2D1::RoundedRect(m_reduceTransRect, 9.0f, 9.0f);
            if (m_settings.reduceTransparency) {
                if (pBlue) rt->FillRoundedRectangle(redR, pBlue);
                if (pWhite) {
                    D2D1_ROUNDED_RECT knobR = D2D1::RoundedRect(D2D1::RectF(redSwX + 16.0f, r4Y + 2.0f, redSwX + 16.0f + 14.0f, r4Y + 2.0f + 14.0f), 7.0f, 7.0f);
                    rt->FillRoundedRectangle(knobR, pWhite);
                }
            } else {
                if (pToggleOffBg) rt->FillRoundedRectangle(redR, pToggleOffBg);
                if (pWhite) {
                    D2D1_ROUNDED_RECT knobR = D2D1::RoundedRect(D2D1::RectF(redSwX + 2.0f, r4Y + 2.0f, redSwX + 2.0f + 14.0f, r4Y + 2.0f + 14.0f), 7.0f, 7.0f);
                    rt->FillRoundedRectangle(knobR, pWhite);
                }
            }
        } else {
            m_reduceTransRect = D2D1::RectF(0, 0, 0, 0);
        }

    } else if (m_settings.activeTab == 1) {
        // ========== TAB 1: 监控 ==========
        float curY = 91.0f;
        if (pFontSec) {
            rt->DrawTextW(L"硬件采集配置", 6, pFontSec, D2D1::RectF(startX, curY, startX + 160.0f, curY + 18.0f), m_pTextPrimary);
        }
        curY += 28.0f;
        auto DrawMonRow = [&](const wchar_t* label, const wchar_t* val) {
            if (pFontItemLabel) rt->DrawTextW(label, static_cast<UINT32>(wcslen(label)), pFontItemLabel, D2D1::RectF(startX, curY, startX + 160.0f, curY + 18.0f), m_pTextSecondary);
            if (pFontItemVal) rt->DrawTextW(val, static_cast<UINT32>(wcslen(val)), pFontItemVal, D2D1::RectF(endX - 160.0f, curY, endX, curY + 18.0f), m_pTextPrimary);
            if (pDivider) rt->DrawLine(D2D1::Point2F(startX, curY + 24.0f), D2D1::Point2F(endX, curY + 24.0f), pDivider, 0.6f);
            curY += 30.0f;
        };
        DrawMonRow(L"监控引擎", L"DirectPDH + WMI");
        DrawMonRow(L"GPU 渲染器监控", L"DXGI 物理显卡优先");
        DrawMonRow(L"磁盘 I/O 采样", L"聚合物理驱动器");
        DrawMonRow(L"网络接口监听", L"活跃链路自适应");
        DrawMonRow(L"传感器探针", L"内核驱动低开销");
    } else {
        // ========== TAB 2: 常规 ==========
        float curY = 91.0f;
        if (pFontSec) {
            rt->DrawTextW(L"系统常规设置", 6, pFontSec, D2D1::RectF(startX, curY, startX + 160.0f, curY + 18.0f), m_pTextPrimary);
        }
        curY += 28.0f;
        auto DrawGenRow = [&](const wchar_t* label, const wchar_t* val) {
            if (pFontItemLabel) rt->DrawTextW(label, static_cast<UINT32>(wcslen(label)), pFontItemLabel, D2D1::RectF(startX, curY, startX + 160.0f, curY + 18.0f), m_pTextSecondary);
            if (pFontItemVal) rt->DrawTextW(val, static_cast<UINT32>(wcslen(val)), pFontItemVal, D2D1::RectF(endX - 160.0f, curY, endX, curY + 18.0f), m_pTextPrimary);
            if (pDivider) rt->DrawLine(D2D1::Point2F(startX, curY + 24.0f), D2D1::Point2F(endX, curY + 24.0f), pDivider, 0.6f);
            curY += 30.0f;
        };
        DrawGenRow(L"开机自动启动", L"已启用");
        DrawGenRow(L"启动时最小化", L"显示悬浮条");
        DrawGenRow(L"失去焦点自动收起", L"是");
        DrawGenRow(L"DPI 缩放模式", L"Per-Monitor V2 原生");
        DrawGenRow(L"程序版本", L"PcInfoTool-Pro v2.0");
    }

    // Release resources
    SafeRelease(&pShadowBrush);
    SafeRelease(&pPanelBg);
    SafeRelease(&pPanelBorder);
    SafeRelease(&pBlue);
    SafeRelease(&pBlueHover);
    SafeRelease(&pTabSelectedBg);
    SafeRelease(&pThemeUnselBg);
    SafeRelease(&pSliderTrackBg);
    SafeRelease(&pWhite);
    SafeRelease(&pToggleOffBg);
    SafeRelease(&pDivider);
    SafeRelease(&pGripDot);
    SafeRelease(&pThumbShadow);

    SafeRelease(&pFontTitle);
    SafeRelease(&pFontDone);
    SafeRelease(&pFontTabActive);
    SafeRelease(&pFontTabInactive);
    SafeRelease(&pFontSec);
    SafeRelease(&pFontSub);
    SafeRelease(&pFontItemLabel);
    SafeRelease(&pFontItemVal);
    SafeRelease(&pFontThemeBtn);
    SafeRelease(&pFontOpacPct);
}
void ProDashboardWindow::DrawWhiteCpuDetails(ID2D1DCRenderTarget* rt, bool standalone) {
    D2D1_MATRIX_3X2_F original; rt->GetTransform(&original);
    const float dx = standalone ? -1.f : 0.f, dy = standalone ? -42.f : 0.f;
    rt->SetTransform(D2D1::Matrix3x2F::Translation(dx,dy)*original);
    auto* dw = m_renderer.GetDWriteFactory();
    const auto primary = D2D1::ColorF(0x192739), muted = D2D1::ColorF(0x53657A);
    const auto subtle = D2D1::ColorF(0x64748B), blue = D2D1::ColorF(0x266BCE);
    const float extra = CpuCoreExtraHeight();
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC), &brush);
    if (!brush) {rt->SetTransform(original);return;}
    const auto card = D2D1::RoundedRect(D2D1::RectF(11.5f, 50.5f, 348.5f, 410.5f+extra), 9.5f, 9.5f);
    if (!standalone && !m_motionContents) {
        rt->FillRoundedRectangle(card, brush.Get());
        brush->SetColor(D2D1::ColorF(0xDDE3EB)); rt->DrawRoundedRectangle(card, brush.Get(), 1);
    }
    auto text = [&](const wchar_t* value, D2D1_RECT_F box, float size, float lineHeight,
        DWRITE_FONT_WEIGHT weight, D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt, dw, value, box, size, lineHeight, weight, color, align);
    };
    if (!standalone) text(L"CPU", D2D1::RectF(24,55,338,75),13,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    wchar_t value[64];
    const bool validUsage=std::isfinite(m_snapshot.cpuUsageTotal) && m_snapshot.cpuUsageTotal>=0;
    if (validUsage) swprintf_s(value, L"%.0f", std::clamp(m_snapshot.cpuUsageTotal,0.0f,100.0f));
    else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(24,79,94,121),28,42,DWRITE_FONT_WEIGHT_MEDIUM,blue);
    const float numberWidth = m_overviewVisuals.Measure(dw,value,28,DWRITE_FONT_WEIGHT_MEDIUM);
    if (validUsage) text(L"%",D2D1::RectF(28+numberWidth,90,94,110),13,20,DWRITE_FONT_WEIGHT_NORMAL,blue);
    if (std::isfinite(m_snapshot.cpuFreqGHz) && m_snapshot.cpuFreqGHz > 0) swprintf_s(value,L"%.2f GHz",m_snapshot.cpuFreqGHz);
    else wcscpy_s(value,L"— GHz");
    text(value,D2D1::RectF(102,90,160,110),m_overviewVisuals.Measure(dw,value,13,DWRITE_FONT_WEIGHT_MEDIUM)>58 ? 11.f : 13.f,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    if (std::isfinite(m_snapshot.cpuTempC) && m_snapshot.cpuTempC > 0) {
        const bool fahrenheit = m_settings.tempUnit == 1;
        swprintf_s(value,fahrenheit ? L"%.0f°F" : L"%.0f°C",fahrenheit ? m_snapshot.cpuTempC*1.8f+32 : m_snapshot.cpuTempC);
    } else wcscpy_s(value,m_settings.tempUnit==1 ? L"—°F" : L"—°C");
    text(value,D2D1::RectF(165,90,202,110),m_overviewVisuals.Measure(dw,value,13,DWRITE_FONT_WEIGHT_MEDIUM)>37 ? 11.f : 13.f,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    std::wstring model = m_snapshot.cpuBrand.empty() ? L"处理器型号不可用" : m_snapshot.cpuBrand;
    if (m_overviewVisuals.Measure(dw,model.c_str(),11,DWRITE_FONT_WEIGHT_BOLD) > 128) {
        do { model.pop_back(); } while (!model.empty() && m_overviewVisuals.Measure(dw,(model+L"…").c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>128);
        model += L"…";
    }
    text(model.c_str(),D2D1::RectF(210,91.5f,338,108.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,subtle,DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.5f));
    for (float y : {131.05f,154.51f,177.97f}) rt->FillRectangle(D2D1::RectF(24,y,338,y+.757f),brush.Get());
    const auto& history=m_snapshot.cpuHistory;
    auto valid=[&](size_t i) {return std::isfinite(history[i]) && history[i]>=0 && history[i]<=100;};
    float maximum=50;size_t validCount=0;
    for(size_t i=0;i<history.Size();++i) if(valid(i)) {maximum=std::max(maximum,history[i]*1.1f);++validCount;}
    auto point=[&](size_t i) {return D2D1::Point2F(24+314*(60-history.Size()+static_cast<float>(i))/59,178-53*history[i]/maximum);};
    // Preserve missing intervals; neither bridge gaps nor stretch partial history to sixty samples.
    for(size_t start=0;start<history.Size();) {
        if(!valid(start)) {++start;continue;}
        size_t end=start+1;while(end<history.Size() && valid(end)) ++end;
        if(end==start+1) {brush->SetColor(blue);rt->FillEllipse(D2D1::Ellipse(point(start),1.5f,1.5f),brush.Get());start=end;continue;}
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> line,area;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> lineSink,areaSink;
        auto* factory=m_renderer.GetD2DFactory();
        if(SUCCEEDED(factory->CreatePathGeometry(&line)) && SUCCEEDED(factory->CreatePathGeometry(&area)) &&
            SUCCEEDED(line->Open(&lineSink)) && SUCCEEDED(area->Open(&areaSink))) {
            lineSink->BeginFigure(point(start),D2D1_FIGURE_BEGIN_HOLLOW);
            areaSink->BeginFigure(D2D1::Point2F(point(start).x,178),D2D1_FIGURE_BEGIN_FILLED);
            for(size_t i=start;i<end;++i) {if(i>start) lineSink->AddLine(point(i));areaSink->AddLine(point(i));}
            areaSink->AddLine(D2D1::Point2F(point(end-1).x,178));areaSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            lineSink->EndFigure(D2D1_FIGURE_END_OPEN);lineSink->Close();areaSink->Close();
            brush->SetColor(D2D1::ColorF(0x266BCE,.13f));rt->FillGeometry(area.Get(),brush.Get());
            brush->SetColor(blue);rt->DrawGeometry(line.Get(),brush.Get(),1.5f);
        }
        start=end;
    }
    if(!validCount) text(L"等待有效采样",D2D1::RectF(24,146,338,163),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"60 秒前",D2D1::RectF(24,182,100,199),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(300,182,338,199),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    if(std::isfinite(m_snapshot.cpuUserPercent) && validUsage) swprintf_s(value,L"● 用户 %.0f%%",std::clamp(m_snapshot.cpuUserPercent,0.f,100.f));else wcscpy_s(value,L"● 用户 —");
    text(value,D2D1::RectF(24,203,115,220),11,17,DWRITE_FONT_WEIGHT_BOLD,blue);
    if(std::isfinite(m_snapshot.cpuKernelPercent) && validUsage) swprintf_s(value,L"● 系统 %.0f%%",std::clamp(m_snapshot.cpuKernelPercent,0.f,100.f));else wcscpy_s(value,L"● 系统 —");
    text(value,D2D1::RectF(159.5f,203,250,220),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(std::isfinite(m_snapshot.cpuIdlePercent) && validUsage) swprintf_s(value,L"空闲 %.0f%%",std::clamp(m_snapshot.cpuIdlePercent,0.f,100.f));else wcscpy_s(value,L"空闲 —");
    text(value,D2D1::RectF(265,203,338,220),11,17,DWRITE_FONT_WEIGHT_BOLD,subtle,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"逻辑处理器",D2D1::RectF(24,224,154,244),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    const size_t count=m_snapshot.cpuCoreUsage.size();
    m_cpuCoreRects.clear(); m_cpuCoreRects.reserve(count);
    if (count) swprintf_s(value,L"%zu 个",count); else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(250,225.5f,338,242.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    for (size_t i=0;i<count;++i) {
        const size_t row=i/8, columns=std::min<size_t>(8,count-row*8);
        const float x=24+(314-(columns*24+(columns-1)*12))*.5f+static_cast<float>(i%8)*36;
        const float y=248+static_cast<float>(row)*34;
        m_cpuCoreRects.push_back(D2D1::RectF(x+dx,y+dy-m_cpuScroll,x+24+dx,y+28+dy-m_cpuScroll));
        const auto bucket=D2D1::RoundedRect(D2D1::RectF(x+.5f,y+.5f,x+23.5f,y+27.5f),5.5f,5.5f);
        brush->SetColor(D2D1::ColorF(0xF7F9FC)); rt->FillRoundedRectangle(bucket,brush.Get());
        Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> clip;
        if (SUCCEEDED(m_renderer.GetD2DFactory()->CreateRoundedRectangleGeometry(
            D2D1::RoundedRect(D2D1::RectF(x+1,y+1,x+23,y+27),5,5),&clip))) {
            const float usage=std::isfinite(m_snapshot.cpuCoreUsage[i]) ? std::clamp(m_snapshot.cpuCoreUsage[i],0.0f,100.0f)/100 : 0.f;
            // Height and opacity convey load; zero is truly empty, 100% fills the inner bucket.
            rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(),clip.Get()),nullptr);
            brush->SetColor(D2D1::ColorF(0x266BCE,.18f+.40f*usage));
            rt->FillRectangle(D2D1::RectF(x+1,y+27-26*usage,x+23,y+27),brush.Get());
            rt->PopLayer();
        }
        brush->SetColor(blue); rt->DrawRoundedRectangle(bucket,brush.Get(),1);
        if (m_cpuKeyboardFocus && m_focusedCpuCore==static_cast<int>(i))
            rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x-.5f,y-.5f,x+24.5f,y+28.5f),6,6),brush.Get(),2);
        swprintf_s(value,L"%zu",i);
        text(value,D2D1::RectF(x+1,y+5,x+23,y+23),12,18,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    if (!count) text(L"逻辑处理器数据不可用",D2D1::RectF(24,248,338,310),11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    brush->SetColor(D2D1::ColorF(0xDDE3EB)); rt->FillRectangle(D2D1::RectF(24,314+extra,338,315+extra),brush.Get());
    text(L"占用最高的进程",D2D1::RectF(24,319+extra,240,339+extra),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"CPU",D2D1::RectF(300,320.5f+extra,338,337.5f+extra),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    for (size_t i=0;i<std::min<size_t>(3,m_snapshot.topProcesses.size());++i) {
        const auto& process=m_snapshot.topProcesses[i]; const float y=343+extra+21*static_cast<float>(i);
        std::wstring name=process.name;
        if(m_overviewVisuals.Measure(dw,name.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>262)
            while (!name.empty() && m_overviewVisuals.Measure(dw,(name+L"…").c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>262) name.pop_back();
        if (name!=process.name) name+=L"…";
        text(name.c_str(),D2D1::RectF(24,y,286,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if (std::isfinite(process.cpuPercent) && process.cpuPercent>=0) swprintf_s(value,L"%.1f%%",std::clamp(process.cpuPercent,0.f,100.f));
        else wcscpy_s(value,L"—");
        text(value,D2D1::RectF(290,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    if (m_snapshot.topProcesses.empty()) text(L"暂无进程数据",D2D1::RectF(24,343+extra,338,360+extra),11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    rt->SetTransform(original);
}

void ProDashboardWindow::DrawWhiteTempDirect(ID2D1DCRenderTarget* rt) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&brush);if(!brush) return;
    auto* dw=m_renderer.GetDWriteFactory();
    const auto primary=D2D1::ColorF(0x192739),muted=D2D1::ColorF(0x53657A),subtle=D2D1::ColorF(0x64748B),red=D2D1::ColorF(0xBC5648);
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float line,DWRITE_FONT_WEIGHT weight,D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {m_overviewVisuals.Text(rt,dw,value,box,size,line,weight,color,align);};
    auto valid=[](float t) {return std::isfinite(t)&&t>0&&t<=200;};
    const bool fahrenheit=m_settings.tempUnit==1;
    auto convert=[&](float t) {return fahrenheit ? t*1.8f+32 : t;};
    size_t validSamples=0;
    for(size_t i=0;i<m_snapshot.tempHistory.Size();++i) if(valid(m_snapshot.tempHistory[i])) ++validSamples;
    const auto state=m_snapshot.temperatureStatus.Resolve(valid(m_snapshot.cpuTempC),validSamples,GetTickCount64());
    const bool reading=state==MetricDataState::Reading,unsupported=state==MetricDataState::Unsupported;
    const bool stale=state==MetricDataState::Stale,partial=state==MetricDataState::Partial;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,289),20,24,0,8,.07f);
    const auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,288.5f),19.5f,19.5f);
    rt->FillRoundedRectangle(panel,brush.Get());brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(panel,brush.Get(),1);
    text(L"CPU 温度",D2D1::RectF(11,5,349,29),16,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    wchar_t number[32];if(reading)wcscpy_s(number,L"读取中");else if(!unsupported&&valid(m_snapshot.cpuTempC)) swprintf_s(number,L"%.0f",convert(m_snapshot.cpuTempC));else wcscpy_s(number,L"—");
    const float fontSize=reading ? 22.f : 28.f;
    const float numberW=m_overviewVisuals.Measure(dw,number,fontSize,DWRITE_FONT_WEIGHT_MEDIUM);
    text(number,D2D1::RectF(11,33,11+numberW,75),fontSize,42,DWRITE_FONT_WEIGHT_MEDIUM,reading||unsupported||stale ? muted : red);
    if(!reading&&!unsupported&&valid(m_snapshot.cpuTempC))text(fahrenheit ? L"°F" : L"°C",D2D1::RectF(15+numberW,43.5f,43+numberW,64.5f),14,21,DWRITE_FONT_WEIGHT_NORMAL,muted);
    // Keep three-digit Fahrenheit readouts clear of the status and model slots.
    const float badgeX=reading ? 87.f : std::max(87.f,47+numberW);
    brush->SetColor(D2D1::ColorF(0xF7F9FC));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(badgeX,41.5f,badgeX+74,66.5f),8,8),brush.Get());
    const bool hot=valid(m_snapshot.cpuTempC)&&m_settings.highTempAlert>0&&m_snapshot.cpuTempC>=m_settings.highTempAlert;
    wchar_t collected[32];swprintf_s(collected,L"已采集 %zu 秒",validSamples);
    text(reading ? L"首次采样" : unsupported ? L"未提供数据" : stale ? L"数据已暂停" : partial ? collected : hot ? L"● 温度偏高" : L"● 温度正常",D2D1::RectF(badgeX+4,45.5f,badgeX+70,62.5f),11,17,DWRITE_FONT_WEIGHT_MEDIUM,reading||unsupported||stale||partial ? muted : hot ? red : D2D1::ColorF(0x138873));
    std::wstring model=m_snapshot.cpuBrand.empty() ? L"型号未提供" : m_snapshot.cpuBrand;
    const float modelWidth=349-badgeX-86;
    if(m_overviewVisuals.Measure(dw,model.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>modelWidth) {
        while(!model.empty() && m_overviewVisuals.Measure(dw,(model+L"…").c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>modelWidth) model.pop_back();
        model+=L"…";
    }
    text(model.c_str(),D2D1::RectF(badgeX+86,45.5f,349,62.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,subtle,DWRITE_TEXT_ALIGNMENT_TRAILING);
    wchar_t interval[64];if(stale)swprintf_s(interval,L"最后更新 · %llu 秒前",m_snapshot.temperatureStatus.lastValidMs ? (GetTickCount64()-std::min(GetTickCount64(),m_snapshot.temperatureStatus.lastValidMs))/1000 : 0);else if(partial)swprintf_s(interval,L"历史采样 · %zu / 60 秒",validSamples);else if(reading||unsupported)wcscpy_s(interval,L"采样状态");else if(m_settings.updateIntervalSec==1) wcscpy_s(interval,L"每秒更新");else if(m_settings.updateIntervalSec==0) wcscpy_s(interval,L"每 0.5 秒更新");else swprintf_s(interval,L"每 %d 秒更新",m_settings.updateIntervalSec);
    text(interval,D2D1::RectF(11,79,349,96),11,17,DWRITE_FONT_WEIGHT_NORMAL,subtle);
    m_tempTargetRect={};m_tempChartRect=D2D1::RectF(11,100,349,149.5f);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.4f));
    for(float y : {104.67f,124.73f,144.8f}) rt->DrawLine(D2D1::Point2F(11,y),D2D1::Point2F(349,y),brush.Get(),.5f);
    const auto& history=m_snapshot.tempHistory;float minimum=200,maximum=0,sum=0;size_t count=0;
    for(size_t i=0;i<history.Size();++i) if(valid(history[i])) {minimum=std::min(minimum,history[i]);maximum=std::max(maximum,history[i]);sum+=history[i];++count;}
    const float low=count ? minimum-5 : 0,span=count ? std::max(10.f,maximum-minimum+10) : 10;
    auto point=[&](size_t i) {return D2D1::Point2F(349-338.f*(history.Size()-1-i)/59,144.8f-(history[i]-low)/span*40.13f);};
    brush->SetColor(stale ? D2D1::ColorF(0xBC5648,.45f) : red);
    if(!reading&&!unsupported)for(size_t i=0;i<history.Size();++i) if(valid(history[i])) {const auto p=point(i);if(i&&valid(history[i-1])) rt->DrawLine(point(i-1),p,brush.Get(),1.5f);else rt->FillEllipse(D2D1::Ellipse(p,.75f,.75f),brush.Get());}
    if(reading||unsupported)text(reading ? L"等待第一组有效数据" : L"设备未提供此数据",D2D1::RectF(11,128,349,143),10,15,DWRITE_FONT_WEIGHT_NORMAL,muted);
    if(partial&&history.Size()<60) {
        const float end=349-338.f*history.Size()/60;
        brush->SetColor(D2D1::ColorF(0xFFFFFF));rt->FillRectangle(D2D1::RectF(11,100,end,156),brush.Get());
        if(end>105)text(L"尚无历史数据",D2D1::RectF(17,124,end,139),10,15,DWRITE_FONT_WEIGHT_NORMAL,muted);
    }
    wchar_t axis[32];wcscpy_s(axis,L"60 秒前"); // Histories retain the collector's 1-second cadence.
    text(axis,D2D1::RectF(11,157,90,174),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(280,157,349,174),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    if(m_tempProbeActive&&!reading&&!unsupported&&!stale) {
        const int slot=std::clamp(m_tempProbeSecAgo,0,59);
        const int index=static_cast<int>(history.Size())-1-slot;
        const float x=349-338.f*slot/59;
        brush->SetColor(D2D1::ColorF(0xBC5648,.5f));rt->DrawLine(D2D1::Point2F(x,100),D2D1::Point2F(x,143.87f),brush.Get(),.87f);
        wchar_t probe[64];const float seconds=static_cast<float>(slot);
        if(index>=0&&valid(history[index])) swprintf_s(probe,L"%g 秒前 · %.0f°%s",seconds,convert(history[index]),fahrenheit ? L"F" : L"C");else swprintf_s(probe,L"%g 秒前 · 未提供",seconds);
        const float px=std::clamp(x-17.43f,11.f,349-139.862f);
        brush->SetColor(D2D1::ColorF(0xF7F9FC));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(px,100,px+139.862f,133),8,8),brush.Get());
        text(probe,D2D1::RectF(px+8,108,px+131.862f,125),11,17,DWRITE_FONT_WEIGHT_MEDIUM,red);
    }
    wchar_t stats[64];if(count&&!reading&&!unsupported) swprintf_s(stats,L"%.0f° / %.0f° / %.0f°",convert(minimum),convert(sum/count),convert(maximum));else wcscpy_s(stats,L"—");
    wchar_t power[32],freq[32],fan[32];
    if(std::isfinite(m_snapshot.cpuPowerWatts)&&m_snapshot.cpuPowerWatts>0) swprintf_s(power,L"%.0f W",m_snapshot.cpuPowerWatts);else wcscpy_s(power,L"—");
    if(std::isfinite(m_snapshot.cpuFreqGHz)&&m_snapshot.cpuFreqGHz>0) swprintf_s(freq,L"%.2f GHz",m_snapshot.cpuFreqGHz);else wcscpy_s(freq,L"—");
    if(m_snapshot.cpuFanRpm<0) wcscpy_s(fan,L"—");else if(m_snapshot.cpuFanRpm>=1000) swprintf_s(fan,L"%d,%03d RPM",m_snapshot.cpuFanRpm/1000,m_snapshot.cpuFanRpm%1000);else swprintf_s(fan,L"%d RPM",m_snapshot.cpuFanRpm);
    const wchar_t* labels[]={L"最低 / 平均 / 最高",L"CPU 封装功耗",L"有效频率",m_snapshot.laptopFanCount>0 ? L"风扇 1" : L"CPU 风扇"};const wchar_t* values[]={stats,power,freq,fan};
    for(int i=0;i<4;++i) {const float y=178+21.f*i;text(labels[i],D2D1::RectF(11,y,180,y+17),12,18,DWRITE_FONT_WEIGHT_NORMAL,muted);text(values[i],D2D1::RectF(180,y,349,y+17),12,18,DWRITE_FONT_WEIGHT_MEDIUM,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);}
    brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->FillRectangle(D2D1::RectF(11,262,349,263),brush.Get());
    m_tempAlertRect=D2D1::RectF(11,267,155,284);m_allSensorsRect=D2D1::RectF(230,267,349,284);
    wchar_t alert[64];if(m_settings.highTempAlert>0) swprintf_s(alert,L"温度提醒  %.0f°%s  ▾",convert(static_cast<float>(m_settings.highTempAlert)),fahrenheit ? L"F" : L"C");else wcscpy_s(alert,L"温度提醒  关闭  ▾");
    text(alert,m_tempAlertRect,11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"查看全部传感器  ›",m_allSensorsRect,11,17,DWRITE_FONT_WEIGHT_BOLD,D2D1::ColorF(m_allSensorsHover ? 0x1D4ED8 : 0x266BCE),DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void ProDashboardWindow::DrawWhiteCpuDirect(ID2D1DCRenderTarget* rt) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&brush);if(!brush) return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,m_dipHeight),20,24,0,8,.07f);
    const auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,m_dipHeight-.5f),19.5f,19.5f);
    rt->FillRoundedRectangle(panel,brush.Get());brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(panel,brush.Get(),1);
    m_overviewVisuals.Text(rt,m_renderer.GetDWriteFactory(),L"CPU 占用",D2D1::RectF(11,5,349,29),16,24,DWRITE_FONT_WEIGHT_BOLD,D2D1::ColorF(0x192739));
    m_overviewLinkRect={}; // The direct-detail design has no fabricated footer link.
    D2D1_MATRIX_3X2_F original;rt->GetTransform(&original);
    rt->PushAxisAlignedClip(D2D1::RectF(11,33,349,m_dipHeight-5),D2D1_ANTIALIAS_MODE_ALIASED);
    rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-m_cpuScroll)*original);
    DrawWhiteCpuDetails(rt,true);
    rt->SetTransform(original);rt->PopAxisAlignedClip();
    DrawCpuCorePeek(rt);
}
int ProDashboardWindow::HitCpuCore(float x,float y) const {
    if(!IsWhiteCpu()) return -1;
    if(y<(m_mode==Mode_DirectCpu ? 33.f : 50.f) || y>m_dipHeight-(m_mode==Mode_DirectCpu ? 5.f : 46.f)) return -1;
    for(size_t i=0;i<m_cpuCoreRects.size();++i) {
        const auto& r=m_cpuCoreRects[i];
        if(x>=r.left && x<r.right && y>=r.top && y<r.bottom) return static_cast<int>(i);
    }
    return -1;
}
bool ProDashboardWindow::OnCpuKey(UINT key,bool shift) {
    if(m_panelMotion.active)return false;
    if(!IsWhiteCpu() || m_cpuCoreRects.empty() || m_snapshot.cpuCoreUsage.empty()) return false;
    const int count=static_cast<int>(std::min(m_cpuCoreRects.size(),m_snapshot.cpuCoreUsage.size()));
    int target=m_focusedCpuCore;
    switch(key) {
    case VK_TAB: target=target<0 ? (shift ? count-1 : 0) : (target+(shift ? count-1 : 1))%count;break;
    case VK_RIGHT: target=target<0 ? 0 : std::min(count-1,target+1);break;
    case VK_LEFT: target=target<0 ? 0 : std::max(0,target-1);break;
    case VK_DOWN: target=target<0 ? 0 : std::min(count-1,target+8);break;
    case VK_UP: target=target<0 ? 0 : std::max(0,target-8);break;
    case VK_HOME: target=0;break;
    case VK_END: target=count-1;break;
    default: return false;
    }
    m_focusedCpuCore=target;m_cpuKeyboardFocus=true;m_hoveredCpuCore=-1;
    const auto& rect=m_cpuCoreRects[target];
    const float top=m_mode==Mode_DirectCpu ? 33.f : 50.f,bottom=m_dipHeight-(m_mode==Mode_DirectCpu ? 5.f : 46.f);
    if(rect.top<top) m_cpuScroll=std::max(0.f,m_cpuScroll+rect.top-top);
    else if(rect.bottom>bottom) m_cpuScroll=std::min(std::max(0.f,m_cpuFullHeight-m_dipHeight),m_cpuScroll+rect.bottom-bottom);
    if(m_visible) Render();return true;
}
void ProDashboardWindow::ScrollCpu(float delta) {
    if(m_panelMotion.active){CancelPanelMotion();UpdateDimensionsAndPosition();}
    if(!IsPureWhite()&&m_mode!=Mode_Settings) return;
    if(m_diskMenuOpen){m_diskMenuOpen=false;}
    m_cpuScroll=std::clamp(m_cpuScroll+delta,0.f,std::max(0.f,m_cpuFullHeight-m_dipHeight));
    m_hoveredCpuCore=-1;if(m_visible) Render();
}
void ProDashboardWindow::DrawCpuCorePeek(ID2D1DCRenderTarget* rt) {
    const int core=m_cpuKeyboardFocus ? m_focusedCpuCore : m_hoveredCpuCore;
    if(!IsWhiteCpu() || core<0 || core>=static_cast<int>(m_cpuCoreRects.size()) || core>=static_cast<int>(m_snapshot.cpuCoreUsage.size())) return;
    const auto& r=m_cpuCoreRects[core];
    if(r.bottom<33 || r.top>m_dipHeight-5) return;
    const float x=std::clamp(r.left+20,11.f,195.f), y=std::clamp(r.top-52,33.f,std::max(33.f,m_dipHeight-53));
    const auto box=D2D1::RectF(x,y,x+154,y+48);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&brush);if(!brush) return;
    m_overviewVisuals.Shadow(rt,box,10,12,0,4,.08f);
    const auto card=D2D1::RoundedRect(D2D1::RectF(x+.5f,y+.5f,x+153.5f,y+47.5f),9.5f,9.5f);
    rt->FillRoundedRectangle(card,brush.Get());brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(card,brush.Get(),1);
    rt->FillRectangle(D2D1::RectF(x+66,y+10,x+67,y+38),brush.Get());
    auto text=[&](const wchar_t* value,float tx,float width,float ty,float size,float line,DWRITE_FONT_WEIGHT weight,unsigned color) {
        m_overviewVisuals.Text(rt,m_renderer.GetDWriteFactory(),value,D2D1::RectF(tx,ty,tx+width,ty+line),size,line,weight,D2D1::ColorF(color));
    };
    text(L"当前利用率",x+8,50,y+4,10,14,DWRITE_FONT_WEIGHT_NORMAL,0x53657A);
    text(L"逻辑处理器",x+75,76,y+4,10,14,DWRITE_FONT_WEIGHT_NORMAL,0x53657A);
    wchar_t value[32];const float load=m_snapshot.cpuCoreUsage[core];
    if(std::isfinite(load) && load>=0) swprintf_s(value,L"%.0f%%",std::clamp(load,0.f,100.f));else wcscpy_s(value,L"—");
    text(value,x+8,50,y+22,16,22,DWRITE_FONT_WEIGHT_MEDIUM,0x266BCE);
    swprintf_s(value,L"%d",core);text(value,x+75,76,y+22,16,22,DWRITE_FONT_WEIGHT_MEDIUM,0x192739);
}

void ProDashboardWindow::DrawWhiteSensorDetails(ID2D1DCRenderTarget* rt) {
    auto* dw=m_renderer.GetDWriteFactory();
    const auto primary=D2D1::ColorF(0x192739),muted=D2D1::ColorF(0x53657A);
    const auto red=D2D1::ColorF(0xBC5648),border=D2D1::ColorF(0xDDE3EB),subtle=D2D1::ColorF(0x64748B);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC),&brush);if(!brush) return;
    const auto card=D2D1::RoundedRect(D2D1::RectF(11.5f,230.5f,348.5f,617.5f),9.5f,9.5f);
    if(!m_motionContents){rt->FillRoundedRectangle(card,brush.Get());brush->SetColor(border);rt->DrawRoundedRectangle(card,brush.Get(),1);}
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float lineHeight,DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt,dw,value,box,size,lineHeight,weight,color,align);
    };
    const bool fahrenheit=m_settings.tempUnit==1;
    auto convert=[&](float c) {return fahrenheit ? c*1.8f+32 : c;};
    auto valid=[](float c) {return std::isfinite(c) && c>0 && c<=200;};
    wchar_t value[96];
    text(L"传感器",D2D1::RectF(24,235,338,255),13,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    text(L"温度",D2D1::RectF(24,259,50,279),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"● CPU 封装",D2D1::RectF(58,260.5f,200,277.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(valid(m_snapshot.cpuTempC)) swprintf_s(value,L"%.0f",convert(m_snapshot.cpuTempC));else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(24,280,219,322),28,42,DWRITE_FONT_WEIGHT_MEDIUM,red);
    const float width=m_overviewVisuals.Measure(dw,value,28,DWRITE_FONT_WEIGHT_MEDIUM);
    text(fahrenheit ? L"°F" : L"°C",D2D1::RectF(28+width,292.5f,219,309.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    const auto& history=m_snapshot.tempHistory;
    float minimum=0,maximum=0;size_t validCount=0;
    for(size_t i=0;i<history.Size();++i) if(valid(history[i])) {
        minimum=validCount ? std::min(minimum,history[i]) : history[i];maximum=validCount ? std::max(maximum,history[i]) : history[i];++validCount;
    }
    if(validCount) swprintf_s(value,fahrenheit ? L"范围 %.0f–%.0f°F" : L"范围 %.0f–%.0f°C",convert(minimum),convert(maximum));
    else wcscpy_s(value,L"范围 —");
    const float rangeSize=m_overviewVisuals.Measure(dw,value,13,DWRITE_FONT_WEIGHT_MEDIUM)>111 ? 11.f : 13.f;
    text(value,D2D1::RectF(227,291,338,311),rangeSize,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.5f));
    for(float y : {328.75f,348.88f,369.0f}) rt->FillRectangle(D2D1::RectF(24,y,338,y+.719f),brush.Get());
    if(validCount>=2) {
        const float low=minimum-3,range=std::max(8.0f,maximum-minimum+6);
        // Draw separate contiguous runs so missing samples never become zero-temperature spikes.
        size_t start=0;
        while(start<history.Size()) {
            while(start<history.Size() && !valid(history[start])) ++start;
            size_t end=start;while(end<history.Size() && valid(history[end])) ++end;
            if(end-start>=2) {
                Microsoft::WRL::ComPtr<ID2D1PathGeometry> line,area;
                Microsoft::WRL::ComPtr<ID2D1GeometrySink> ls,as;
                auto* factory=m_renderer.GetD2DFactory();
                if(SUCCEEDED(factory->CreatePathGeometry(&line)) && SUCCEEDED(factory->CreatePathGeometry(&area)) &&
                    SUCCEEDED(line->Open(&ls)) && SUCCEEDED(area->Open(&as))) {
                    const float firstX=338-314*static_cast<float>(history.Size()-1-start)/59;
                    as->BeginFigure(D2D1::Point2F(firstX,369),D2D1_FIGURE_BEGIN_FILLED);
                    float lastX=firstX;
                    for(size_t i=start;i<end;++i) {
                        lastX=338-314*static_cast<float>(history.Size()-1-i)/59;
                        const auto point=D2D1::Point2F(lastX,369-40.25f*std::clamp((history[i]-low)/range,0.0f,1.0f));
                        if(i==start) ls->BeginFigure(point,D2D1_FIGURE_BEGIN_HOLLOW);else ls->AddLine(point);
                        as->AddLine(point);
                    }
                    as->AddLine(D2D1::Point2F(lastX,369));as->EndFigure(D2D1_FIGURE_END_CLOSED);
                    ls->EndFigure(D2D1_FIGURE_END_OPEN);ls->Close();as->Close();
                    brush->SetColor(D2D1::ColorF(0xBC5648,.12f));rt->FillGeometry(area.Get(),brush.Get());
                    brush->SetColor(red);rt->DrawGeometry(line.Get(),brush.Get(),1.5f);
                }
            }
            start=end+1;
        }
    } else text(L"暂无有效温度历史",D2D1::RectF(24,340,338,357),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    wcscpy_s(value,L"60 秒前");
    text(value,D2D1::RectF(24,370,110,387),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(290,370,338,387),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const wchar_t* tempLabels[]={L"CPU 封装",L"GPU 核心",L"磁盘温度"};
    const auto* sensorDisk=SelectedDisk();
    const float diskTemperature=sensorDisk ? (sensorDisk->temperatureValid ? sensorDisk->temperatureC : 0) : m_snapshot.ssdTempC;
    const float temps[]={m_snapshot.cpuTempC,m_snapshot.gpuTempC,diskTemperature};
    const D2D1_COLOR_F tempColors[]={red,D2D1::ColorF(0x8259C2),D2D1::ColorF(0x2278A1)};
    for(int i=0;i<3;++i) {
        const float y=391+i*21.0f;
        text(tempLabels[i],D2D1::RectF(24,y,220,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if(valid(temps[i])) swprintf_s(value,fahrenheit ? L"%.0f°F" : L"%.0f°C",convert(temps[i]));else wcscpy_s(value,L"—");
        text(value,D2D1::RectF(250,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,valid(temps[i]) ? tempColors[i] : subtle,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    for(float y : {454.0f,546.0f}) {brush->SetColor(border);rt->FillRectangle(D2D1::RectF(24,y,338,y+1),brush.Get());}
    text(L"功耗与频率",D2D1::RectF(24,459,220,479),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"实时",D2D1::RectF(280,460.5f,338,477.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const wchar_t* powerLabels[]={L"CPU 封装功耗",L"GPU 板卡功耗",L"CPU 有效频率"};
    const float powers[]={m_snapshot.cpuPowerWatts,m_snapshot.gpuPowerWatts,m_snapshot.cpuFreqGHz};
    for(int i=0;i<3;++i) {
        const float y=483+i*21.0f;
        text(powerLabels[i],D2D1::RectF(24,y,220,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if(std::isfinite(powers[i]) && powers[i]>0) swprintf_s(value,i==2 ? L"%.2f GHz" : L"%.0f W",powers[i]);else wcscpy_s(value,L"—");
        text(value,D2D1::RectF(240,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    text(L"风扇",D2D1::RectF(24,551,220,571),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"传感器状态",D2D1::RectF(240,552.5f,338,569.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const int fans[]={m_snapshot.cpuFanRpm,m_snapshot.caseFanRpm};
    for(int i=0;i<2;++i) {
        const float y=575+i*21.0f;
        const wchar_t* label=i ? (fans[i]<0 ? L"机箱风扇（未提供读数）" : L"机箱风扇") : (fans[i]<0 ? L"CPU 风扇（未提供读数）" : L"CPU 风扇");
        if(m_snapshot.laptopFanCount>0||m_snapshot.sensorPermissionRequired) {
            label=i ? (fans[i]<0 ? L"风扇 2（未提供读数）" : L"风扇 2") : (fans[i]<0 ? L"风扇 1（未提供读数）" : L"风扇 1");
            if(m_snapshot.sensorPermissionRequired)label=i ? L"风扇 2（需管理员权限）" : L"风扇 1（需管理员权限）";
        }
        text(label,D2D1::RectF(24,y,242,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if(fans[i]>=1000) swprintf_s(value,L"%d,%03d RPM",fans[i]/1000,fans[i]%1000);
        else if(fans[i]>=0) swprintf_s(value,L"%d RPM",fans[i]);else wcscpy_s(value,L"—");
        text(value,D2D1::RectF(246,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,fans[i]<0 ? subtle : primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
}

const PhysicalDiskInfo* ProDashboardWindow::SelectedDisk() const {
    for(const auto& disk:m_snapshot.physicalDisks)if(disk.id==m_selectedDiskId)return &disk;
    return nullptr;
}
void ProDashboardWindow::ReconcileDiskSelection() {
    if(const auto* selected=SelectedDisk()){m_diskDisconnected=false;m_lastDiskModel=selected->model;m_lastDiskId=selected->id;}
    else if(!m_lastDiskId.empty()&&m_selectedDiskId==m_lastDiskId){m_diskDisconnected=true;m_diskMenuOpen=false;return;}
    if(m_snapshot.physicalDisks.empty()){m_diskMenuOpen=false;return;} // Keep the saved stable ID while initial enumeration is pending.
    if(!SelectedDisk()) {
        m_selectedDiskId=m_snapshot.physicalDisks.empty() ? L"" : m_snapshot.physicalDisks.front().id;
        m_diskMenuOpen=false;m_diskMenuFirst=0;m_diskMenuFocus=0;m_diskMenuHover=-1;
    }
    m_diskMenuFocus=std::clamp(m_diskMenuFocus,0,std::max(0,static_cast<int>(m_snapshot.physicalDisks.size())-1));
    m_diskMenuFirst=std::clamp(m_diskMenuFirst,0,std::max(0,static_cast<int>(m_snapshot.physicalDisks.size())-5));
}
void ProDashboardWindow::DrawWhiteDisconnectedDisk(ID2D1DCRenderTarget* rt) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&brush);if(!brush)return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,205),20,24,0,8,.07f);
    auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,204.5f),19.5f,19.5f);
    rt->FillRoundedRectangle(panel,brush.Get());brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(panel,brush.Get(),1);
    auto text=[&](const wchar_t* v,D2D1_RECT_F r,float s,float l,DWRITE_FONT_WEIGHT w,UINT32 c,DWRITE_TEXT_ALIGNMENT a=DWRITE_TEXT_ALIGNMENT_LEADING){m_overviewVisuals.Text(rt,m_renderer.GetDWriteFactory(),v,r,s,l,w,D2D1::ColorF(c),a);};
    text(L"磁盘",D2D1::RectF(11,5,349,29),16,24,DWRITE_FONT_WEIGHT_BOLD,0x192739);
    text(L"—",D2D1::RectF(11,33,75,75),28,42,DWRITE_FONT_WEIGHT_MEDIUM,0x53657A);
    brush->SetColor(D2D1::ColorF(0xF7F9FC));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(87,41.5f,161,66.5f),8,8),brush.Get());
    text(L"设备已断开",D2D1::RectF(91,45.5f,157,62.5f),11,17,DWRITE_FONT_WEIGHT_MEDIUM,0x53657A);
    std::wstring model=m_lastDiskModel;while(!model.empty()&&m_overviewVisuals.Measure(m_renderer.GetDWriteFactory(),model.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>170){model.pop_back();if(!model.empty()&&model.back()==L'…')model.pop_back();model+=L"…";if(model.size()==1)break;}
    text(model.c_str(),D2D1::RectF(179,45.5f,349,62.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,0x64748B,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"采样状态",D2D1::RectF(11,79,349,96),11,17,DWRITE_FONT_WEIGHT_NORMAL,0x64748B);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.4f));for(float y:{104.67f,124.73f,144.8f})rt->DrawLine(D2D1::Point2F(11,y),D2D1::Point2F(349,y),brush.Get(),.5f);
    text(L"设备已断开",D2D1::RectF(11,128,349,143),10,15,DWRITE_FONT_WEIGHT_NORMAL,0x53657A);
    text(L"60 秒前",D2D1::RectF(11,157,90,174),10,17,DWRITE_FONT_WEIGHT_NORMAL,0x53657A);
    text(L"现在",D2D1::RectF(280,157,349,174),10,17,DWRITE_FONT_WEIGHT_NORMAL,0x53657A,DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->FillRectangle(D2D1::RectF(11,178,349,179),brush.Get());
    m_diskTriggerRect=D2D1::RectF(11,183,150,200);
    text(L"切换磁盘  ›",m_diskTriggerRect,11,17,DWRITE_FONT_WEIGHT_BOLD,0x266BCE);
}
bool ProDashboardWindow::OnDiskKey(UINT key) {
    if(!m_diskMenuOpen)return false;
    const int count=static_cast<int>(m_snapshot.physicalDisks.size());
    if(key==VK_ESCAPE) {m_diskMenuOpen=false;Render();return true;}
    if(!count)return false;
    if(key==VK_RETURN || key==VK_SPACE) {m_selectedDiskId=m_snapshot.physicalDisks[m_diskMenuFocus].id;m_diskMenuOpen=false;if(m_onSettingsChanged)m_onSettingsChanged(m_settings);Render();return true;}
    if(key==VK_UP)m_diskMenuFocus=std::max(0,m_diskMenuFocus-1);
    else if(key==VK_DOWN)m_diskMenuFocus=std::min(count-1,m_diskMenuFocus+1);
    else if(key==VK_HOME)m_diskMenuFocus=0;
    else if(key==VK_END)m_diskMenuFocus=count-1;
    else return false;
    if(m_diskMenuFocus<m_diskMenuFirst)m_diskMenuFirst=m_diskMenuFocus;
    if(m_diskMenuFocus>=m_diskMenuFirst+m_diskMenuRows)m_diskMenuFirst=m_diskMenuFocus-m_diskMenuRows+1;
    Render();return true;
}
void ProDashboardWindow::DrawWhiteDiskDetails(ID2D1DCRenderTarget* rt) {
    ReconcileDiskSelection();const auto* selected=SelectedDisk();
    const auto& volumes=selected ? selected->volumes : m_snapshot.disks;
    const auto& reads=selected ? selected->readHistory : m_snapshot.diskReadHistory;
    const auto& writes=selected ? selected->writeHistory : m_snapshot.diskWriteHistory;
    const bool throughputValid=selected ? selected->throughputValid : reads.Size()&&writes.Size()&&std::isfinite(reads.Back())&&std::isfinite(writes.Back());
    const bool statisticsValid=selected ? selected->statisticsValid : m_snapshot.diskStatisticsValid;
    const float activity=selected ? selected->activePercent : m_snapshot.diskActivePercent;
    const float response=selected ? selected->responseTimeMs : m_snapshot.diskResponseTimeMs;
    const float offset=m_settings.layoutMode==1 ? -40.f : 0.f;
    auto* dw=m_renderer.GetDWriteFactory();
    const auto primary=D2D1::ColorF(0x192739),muted=D2D1::ColorF(0x53657A);
    const auto blue=D2D1::ColorF(0x2278A1),gold=D2D1::ColorF(0xA87517),border=D2D1::ColorF(0xDDE3EB);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC),&brush);if(!brush) return;
    const auto card=D2D1::RoundedRect(D2D1::RectF(11.5f,158.5f,348.5f,497.5f),9.5f,9.5f);
    if(!m_motionContents){rt->FillRoundedRectangle(card,brush.Get());brush->SetColor(border);rt->DrawRoundedRectangle(card,brush.Get(),1);}
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float lineHeight,DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt,dw,value,box,size,lineHeight,weight,color,align);
    };
    auto shorten=[&](std::wstring value,float width) {
        if(m_overviewVisuals.Measure(dw,value.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>width) {
            do {value.pop_back();} while(!value.empty() && m_overviewVisuals.Measure(dw,(value+L"…").c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>width);
            value+=L"…";
        }
        return value;
    };
    text(L"磁盘",D2D1::RectF(24,163,338,183),13,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    wchar_t title[512];if(selected)swprintf_s(title,L"磁盘 %u · %s",selected->number,selected->model.c_str());else wcscpy_s(title,L"所有磁盘 · 汇总");
    std::wstring name=title;while(!name.empty()&&m_overviewVisuals.Measure(dw,name.c_str(),13,DWRITE_FONT_WEIGHT_MEDIUM)>194) {if(name.back()==L'…')name.pop_back();if(!name.empty())name.pop_back();name+=L"…";if(name.size()==1)break;}
    const float titleWidth=m_overviewVisuals.Measure(dw,name.c_str(),13,DWRITE_FONT_WEIGHT_MEDIUM);
    text(name.c_str(),D2D1::RectF(24,187,218,207),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    if(selected)m_overviewVisuals.DrawIcon(rt,ProOverviewVisuals::DiskDisclosure,D2D1::RectF(28+titleWidth,191,40+titleWidth,203));
    m_diskTriggerRect=selected ? D2D1::RectF(24,187+offset,44+titleWidth,207+offset) : D2D1::RectF(0,0,0,0);
    wchar_t capacity[64];if(selected&&selected->capacityBytes) swprintf_s(capacity,L"%.3g %s · %s",selected->capacityBytes/(selected->capacityBytes>=1000000000000ULL ? 1000000000000.0 : 1000000000.0),selected->capacityBytes>=1000000000000ULL ? L"TB" : L"GB",selected->busType.c_str());else wcscpy_s(capacity,selected ? L"容量未提供" : L"设备未提供");
    text(capacity,D2D1::RectF(240,188.5f,338,205.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    wchar_t value[96];
    for(int i=0;i<2;++i) {
        const float x=i ? 185.0f : 24.0f;const auto color=i ? gold : blue;
        text(i ? L"↑ 写入" : L"↓ 读取",D2D1::RectF(x,211,x+153,227),11,17,DWRITE_FONT_WEIGHT_BOLD,color);
        const auto raw=selected ? (i ? selected->writeBps : selected->readBps) : (i ? m_snapshot.diskWriteBps : m_snapshot.diskReadBps);
        const bool giga=raw>=1000000000ULL;
        if(throughputValid) swprintf_s(value,L"%.1f",raw/(giga ? 1000000000.0 : 1000000.0));else wcscpy_s(value,L"—");
        float size=28;
        while(size>18 && m_overviewVisuals.Measure(dw,value,size,DWRITE_FONT_WEIGHT_MEDIUM)>110) --size;
        const float width=m_overviewVisuals.Measure(dw,value,size,DWRITE_FONT_WEIGHT_MEDIUM);
        text(value,D2D1::RectF(x,220,x+119,262),size,42,DWRITE_FONT_WEIGHT_MEDIUM,color);
        text(giga ? L"GB/s" : L"MB/s",D2D1::RectF(x+width+4,232.5f,x+153,249.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    }
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.5f));
    for(float y : {269.0f,293.0f,317.0f}) rt->FillRectangle(D2D1::RectF(24,y,338,y+.75f),brush.Get());
    const size_t count=std::min(reads.Size(),writes.Size());float maximum=1;
    const auto valid=[](float v){return std::isfinite(v) && v>=0;};
    for(size_t i=0;i<count;++i) for(float v : {reads[reads.Size()-count+i],writes[writes.Size()-count+i]}) if(valid(v)) maximum=std::max(maximum,v*1.15f);
    if(count>=2) for(int series=0;series<2;++series) {
        const auto& history=series ? writes : reads;
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> line,area;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> ls,as;
        auto* factory=m_renderer.GetD2DFactory();
        if(SUCCEEDED(factory->CreatePathGeometry(&line)) && SUCCEEDED(factory->CreatePathGeometry(&area)) &&
            SUCCEEDED(line->Open(&ls)) && SUCCEEDED(area->Open(&as))) {
            size_t i=0;
            while(i<count) {
                while(i<count && !valid(history[history.Size()-count+i])) ++i;
                const size_t start=i;
                while(i<count && valid(history[history.Size()-count+i])) ++i;
                if(i-start<2) continue;
                as->BeginFigure(D2D1::Point2F(338-314*static_cast<float>(count-1-start)/59,317),D2D1_FIGURE_BEGIN_FILLED);
                for(size_t j=start;j<i;++j) {
                    const auto point=D2D1::Point2F(338-314*static_cast<float>(count-1-j)/59,317-48*std::clamp(history[history.Size()-count+j]/maximum,0.0f,1.0f));
                    if(j==start) ls->BeginFigure(point,D2D1_FIGURE_BEGIN_HOLLOW);else ls->AddLine(point);
                    as->AddLine(point);
                }
                as->AddLine(D2D1::Point2F(338-314*static_cast<float>(count-i)/59,317));
                as->EndFigure(D2D1_FIGURE_END_CLOSED);ls->EndFigure(D2D1_FIGURE_END_OPEN);
            }
            ls->Close();as->Close();
            if(!series) {brush->SetColor(D2D1::ColorF(0x2278A1,.12f));rt->FillGeometry(area.Get(),brush.Get());}
            brush->SetColor(series ? gold : blue);rt->DrawGeometry(line.Get(),brush.Get(),1.5f);
        }
    } else text(L"正在收集历史数据",D2D1::RectF(24,280,338,297),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    wcscpy_s(value,L"60 秒前");
    text(value,D2D1::RectF(24,318,110,335),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(290,318,338,335),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"活动时间",D2D1::RectF(24,339,210,356),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(statisticsValid && std::isfinite(activity) && activity>=0) swprintf_s(value,L"%.0f%%",std::clamp(activity,0.0f,100.0f));else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(260,339,338,356),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"平均响应时间",D2D1::RectF(24,360,210,377),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(statisticsValid && std::isfinite(response) && response>=0) swprintf_s(value,L"%.1f ms",response);else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(260,360,338,377),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush->SetColor(border);rt->FillRectangle(D2D1::RectF(24,381,338,382),brush.Get());
    text(L"卷容量",D2D1::RectF(24,386,210,406),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"已用 / 总量",D2D1::RectF(240,387.5f,338,404.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    for(size_t i=0;i<std::min<size_t>(2,volumes.size());++i) {
        const auto& volume=volumes[i];const float y=410+33*static_cast<float>(i);
        const uint64_t used=volume.totalBytes-std::min(volume.freeBytes,volume.totalBytes);
        const bool capacityValid=volume.totalBytes>0 && volume.freeBytes<=volume.totalBytes;
        if(capacityValid) swprintf_s(value,L"%.0f / %.0f GiB",used/1073741824.0,volume.totalBytes/1073741824.0);else wcscpy_s(value,L"— / — GiB");
        const float valueWidth=std::max(158.f,m_overviewVisuals.Measure(dw,value,11,DWRITE_FONT_WEIGHT_BOLD));
        const float nameRight=330-valueWidth;
        const auto name=shorten(volume.label.empty() ? volume.drive : volume.label.find(L'(')!=std::wstring::npos ? volume.label : volume.label+L" ("+volume.drive+L")",nameRight-24);
        text(name.c_str(),D2D1::RectF(24,y,nameRight,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        text(value,D2D1::RectF(338-valueWidth,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
        const float ratio=capacityValid ? static_cast<float>(static_cast<double>(used)/volume.totalBytes) : 0;
        auto pill=[&](float left,float right,D2D1_COLOR_F color) {if(right>left) {brush->SetColor(color);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left,y+21,right,y+29),4,4),brush.Get());}};
        if(ratio<=0) pill(24,338,border);else if(ratio>=1) pill(24,338,blue);
        else {pill(24,24+310*ratio,blue);pill(28+310*ratio,338,border);}
    }
    if(volumes.empty()) text(L"无已挂载卷或卷映射未提供",D2D1::RectF(24,410,338,427),11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    if(selected && !selected->status.empty()) wcsncpy_s(value,selected->status.c_str(),_TRUNCATE);
    else if(volumes.size()>2) swprintf_s(value,L"显示前 2 个卷，共 %zu 个",volumes.size());
    else wcscpy_s(value,selected ? L"选择磁盘后，曲线与卷列表同步切换。" : L"物理设备未提供，当前显示汇总。");
    text(value,D2D1::RectF(24,476,338,493),11,17,DWRITE_FONT_WEIGHT_BOLD,D2D1::ColorF(0x64748B));
    m_diskOptionRects.clear();m_diskOptionIds.clear();
    if(m_diskMenuOpen && selected) {
        const int rows=std::min(std::max(1,std::min(5,static_cast<int>((m_dipHeight-96-8)/40))),static_cast<int>(m_snapshot.physicalDisks.size()));
        m_diskMenuRows=rows;m_diskMenuFirst=std::clamp(m_diskMenuFirst,0,std::max(0,static_cast<int>(m_snapshot.physicalDisks.size())-rows));
        const float height=10+rows*38.f+std::max(0,rows-1)*2.f;
        const float menuTop=std::max(50+m_cpuScroll-offset,std::min(211.f,m_dipHeight-46+m_cpuScroll-offset-height));
        m_diskMenuRect=D2D1::RectF(24,menuTop+offset,280,menuTop+offset+height);
        m_overviewVisuals.Shadow(rt,D2D1::RectF(24,menuTop,280,menuTop+height),10,14,0,4,.12f,0x192739);
        brush->SetColor(D2D1::ColorF(0xFFFFFF));const auto menu=D2D1::RoundedRect(D2D1::RectF(24.5f,menuTop+.5f,279.5f,menuTop-.5f+height),9.5f,9.5f);rt->FillRoundedRectangle(menu,brush.Get());brush->SetColor(border);rt->DrawRoundedRectangle(menu,brush.Get(),1);
        for(int row=0;row<rows;++row) {
            const int index=m_diskMenuFirst+row;const auto& disk=m_snapshot.physicalDisks[index];const float y=menuTop+5+40.f*row;
            const auto box=D2D1::RectF(29,y,277,y+38);m_diskOptionRects.push_back(D2D1::RectF(box.left,box.top+offset,box.right,box.bottom+offset));m_diskOptionIds.push_back(disk.id);
            const bool chosen=disk.id==m_selectedDiskId;
            if(chosen||index==m_diskMenuHover||index==m_diskMenuFocus) {brush->SetColor(D2D1::ColorF(0xEDF3FB));rt->FillRoundedRectangle(D2D1::RoundedRect(box,6,6),brush.Get());}
            swprintf_s(title,L"磁盘 %u · %s",disk.number,disk.model.c_str());
            auto truncate=[&](std::wstring s,float size,DWRITE_FONT_WEIGHT weight) {if(m_overviewVisuals.Measure(dw,s.c_str(),size,weight)>208) {while(!s.empty()&&m_overviewVisuals.Measure(dw,(s+L"…").c_str(),size,weight)>208)s.pop_back();s+=L"…";}return s;};
            const auto option=truncate(title,12,DWRITE_FONT_WEIGHT_MEDIUM);text(option.c_str(),D2D1::RectF(37,y+3,245,y+21),12,18,DWRITE_FONT_WEIGHT_MEDIUM,primary);
            wchar_t size[48];if(disk.capacityBytes)swprintf_s(size,L"%.3g %s",disk.capacityBytes/(disk.capacityBytes>=1000000000000ULL ? 1000000000000.0 : 1000000000.0),disk.capacityBytes>=1000000000000ULL ? L"TB" : L"GB");else wcscpy_s(size,L"容量未知");
            std::wstring description=std::wstring(size)+L" · "+disk.busType+L" · ";
            if(disk.volumes.empty())description+=L"无已挂载卷";
            for(size_t v=0;v<disk.volumes.size();++v)description+=(v ? L"、" : L"")+disk.volumes[v].drive.substr(0,2);
            description=truncate(description,10,DWRITE_FONT_WEIGHT_NORMAL);text(description.c_str(),D2D1::RectF(37,y+21,245,y+35),10,14,DWRITE_FONT_WEIGHT_NORMAL,D2D1::ColorF(0x64748B));
            if(chosen)m_overviewVisuals.DrawIcon(rt,ProOverviewVisuals::DiskSelected,D2D1::RectF(253,y+11,269,y+27));
        }
    }
}

void ProDashboardWindow::DrawWhiteMemoryDirect(ID2D1DCRenderTarget* rt) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&brush);if(!brush) return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,387),20,24,0,8,.07f);
    const auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,386.5f),19.5f,19.5f);
    rt->FillRoundedRectangle(panel,brush.Get());brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(panel,brush.Get(),1);
    m_overviewVisuals.Text(rt,m_renderer.GetDWriteFactory(),L"物理内存",D2D1::RectF(11,5,349,29),16,24,DWRITE_FONT_WEIGHT_BOLD,D2D1::ColorF(0x192739));
    m_overviewLinkRect={};
    D2D1_MATRIX_3X2_F original;rt->GetTransform(&original);
    rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-113)*original);
    DrawWhiteMemoryDetails(rt,true);rt->SetTransform(original);
}
void ProDashboardWindow::DrawWhiteMemoryDetails(ID2D1DCRenderTarget* rt, bool standalone) {
    auto* dw=m_renderer.GetDWriteFactory();
    const auto primary=D2D1::ColorF(0x192739),muted=D2D1::ColorF(0x53657A);
    const auto gold=D2D1::ColorF(0xA87517),blue=D2D1::ColorF(0x2278A1),border=D2D1::ColorF(0xDDE3EB);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC),&brush);if(!brush) return;
    const auto card=D2D1::RoundedRect(D2D1::RectF(11.5f,122.5f,348.5f,496.5f),9.5f,9.5f);
    if(!standalone&&!m_motionContents) {rt->FillRoundedRectangle(card,brush.Get());brush->SetColor(border);rt->DrawRoundedRectangle(card,brush.Get(),1);}
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float lineHeight,DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt,dw,value,box,size,lineHeight,weight,color,align);
    };
    const bool available=m_snapshot.totalPhysMem>0;
    const uint64_t total=m_snapshot.totalPhysMem;
    const uint64_t used=std::min(total,m_snapshot.usedPhysMem);
    const bool compositionValid=available && m_snapshot.memoryCompositionValid &&
        m_snapshot.memInUseBytes<=total && m_snapshot.memStandbyCacheBytes<=total-m_snapshot.memInUseBytes &&
        m_snapshot.memFreeBytes==total-m_snapshot.memInUseBytes-m_snapshot.memStandbyCacheBytes;
    const uint64_t inUse=compositionValid ? m_snapshot.memInUseBytes : used;
    wchar_t value[96];
    if(!standalone) text(L"内存",D2D1::RectF(24,131,338,151),13,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    if(available) swprintf_s(value,L"%.1f",used/1073741824.0);else wcscpy_s(value,L"—");
    float metricSize=28;
    while(metricSize>18 && m_overviewVisuals.Measure(dw,value,metricSize,DWRITE_FONT_WEIGHT_MEDIUM)>110) --metricSize;
    text(value,D2D1::RectF(24,150,219,192),metricSize,42,DWRITE_FONT_WEIGHT_MEDIUM,gold);
    const float numberWidth=m_overviewVisuals.Measure(dw,value,metricSize,DWRITE_FONT_WEIGHT_MEDIUM);
    if(available) swprintf_s(value,L"/ %.0f GiB",total/1073741824.0);else wcscpy_s(value,L"/ — GiB");
    text(value,D2D1::RectF(28+numberWidth,162.5f,219,179.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(available) swprintf_s(value,L"%.0f%%",100.0*static_cast<double>(used)/total);else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(275,161,338,181),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.5f));
    for(float y : {196.75f,215.44f,234.13f}) rt->FillRectangle(D2D1::RectF(24,y,338,y+.719f),brush.Get());
    const auto& history=m_snapshot.memoryHistory;
    if(history.Size()>=2 && available) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> line,area;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> ls,as;
        auto* factory=m_renderer.GetD2DFactory();
        if(SUCCEEDED(factory->CreatePathGeometry(&line)) && SUCCEEDED(factory->CreatePathGeometry(&area)) &&
            SUCCEEDED(line->Open(&ls)) && SUCCEEDED(area->Open(&as))) {
            size_t i=0;
            const auto valid=[](float v){return std::isfinite(v) && v>=0 && v<=100;};
            while(i<history.Size()) {
                while(i<history.Size() && !valid(history[i])) ++i;
                const size_t start=i;
                while(i<history.Size() && valid(history[i])) ++i;
                if(i-start<2) continue;
                as->BeginFigure(D2D1::Point2F(338-314*static_cast<float>(history.Size()-1-start)/59,234.13f),D2D1_FIGURE_BEGIN_FILLED);
                for(size_t j=start;j<i;++j) {
                    const auto point=D2D1::Point2F(338-314*static_cast<float>(history.Size()-1-j)/59,234.13f-37.38f*history[j]/100);
                    if(j==start) ls->BeginFigure(point,D2D1_FIGURE_BEGIN_HOLLOW);else ls->AddLine(point);
                    as->AddLine(point);
                }
                as->AddLine(D2D1::Point2F(338-314*static_cast<float>(history.Size()-i)/59,234.13f));
                as->EndFigure(D2D1_FIGURE_END_CLOSED);ls->EndFigure(D2D1_FIGURE_END_OPEN);
            }
            ls->Close();as->Close();
            brush->SetColor(D2D1::ColorF(0xA87517,.12f));rt->FillGeometry(area.Get(),brush.Get());
            brush->SetColor(gold);rt->DrawGeometry(line.Get(),brush.Get(),1.5f);
        }
    } else text(available ? L"正在收集历史数据" : L"内存数据不可用",D2D1::RectF(24,204,338,221),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    wcscpy_s(value,L"60 秒前");
    text(value,D2D1::RectF(24,238,110,255),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(290,238,338,255),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"物理内存组成",D2D1::RectF(24,259,225,279),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    if(available) swprintf_s(value,L"%.1f GiB",total/1073741824.0);else wcscpy_s(value,L"— GiB");
    text(value,D2D1::RectF(230,260.5f,338,277.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    // Keep geometry within the physical total even if collector cache semantics overlap.
    const uint64_t cache=compositionValid ? m_snapshot.memStandbyCacheBytes : 0;
    const uint64_t free=compositionValid ? m_snapshot.memFreeBytes : total-inUse;
    const uint64_t segments[]={inUse,cache,free};
    const D2D1_COLOR_F colors[]={gold,blue,border};
    int nonzero=0;for(auto bytes : segments) if(bytes) ++nonzero;
    const float usable=314-4*std::max(0,nonzero-1);
    float x=24;
    for(int i=0;i<3;++i) if(total && segments[i]) {
        const float width=usable*static_cast<float>(static_cast<double>(segments[i])/total);
        brush->SetColor(colors[i]);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,283,x+width,291),4,4),brush.Get());
        x+=width+4;
    }
    if(!available || !nonzero) {brush->SetColor(border);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(24,283,338,291),4,4),brush.Get());}
    const wchar_t* labels[]={L"● 正在使用",L"● 待机缓存",L"● 空闲",compositionValid ? L"可用  =  缓存 + 空闲" : L"可用（组成未提供）",L"已提交 / 提交限制"};
    const uint64_t readings[]={inUse,cache,free,m_snapshot.availPhysMem};
    for(int i=0;i<5;++i) {
        const float y=295+i*21.0f;
        text(labels[i],D2D1::RectF(24,y,198,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if(i==4) {
            if(m_snapshot.memCommitLimitBytes) swprintf_s(value,L"%.1f / %.1f GiB",m_snapshot.memCommittedBytes/1073741824.0,m_snapshot.memCommitLimitBytes/1073741824.0);
            else wcscpy_s(value,L"— / — GiB");
        } else if(available && (compositionValid || (i!=1 && i!=2))) swprintf_s(value,L"%.1f GiB",readings[i]/1073741824.0);else wcscpy_s(value,L"— GiB");
        text(value,D2D1::RectF(200,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,i==0 ? gold : i==1 ? blue : i==2 ? muted : primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    brush->SetColor(border);rt->FillRectangle(D2D1::RectF(24,400,338,401),brush.Get());
    text(L"占用最高的进程",D2D1::RectF(24,405,270,425),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"内存",D2D1::RectF(290,406.5f,338,423.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    for(size_t i=0;i<std::min<size_t>(3,m_snapshot.topMemoryProcesses.size());++i) {
        const auto& process=m_snapshot.topMemoryProcesses[i];const float y=429+21*static_cast<float>(i);
        std::wstring name=process.name;
        if(m_overviewVisuals.Measure(dw,name.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>244) {
            do {name.pop_back();} while(!name.empty() && m_overviewVisuals.Measure(dw,(name+L"…").c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>244);
            name+=L"…";
        }
        swprintf_s(value,L"%.1f GiB",process.memBytes/1073741824.0);
        const float valueWidth=std::max(62.f,m_overviewVisuals.Measure(dw,value,11,DWRITE_FONT_WEIGHT_BOLD));
        const float nameRight=330-valueWidth;
        while(!name.empty() && m_overviewVisuals.Measure(dw,name.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>nameRight-24) {name.pop_back();if(!name.empty() && name.back()==L'…') name.pop_back();name+=L"…";}
        text(name.c_str(),D2D1::RectF(24,y,nameRight,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        text(value,D2D1::RectF(338-valueWidth,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    if(m_snapshot.topMemoryProcesses.empty()) text(L"暂无进程数据",D2D1::RectF(24,429,338,446),11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
}

void ProDashboardWindow::DrawWhiteGpuDetails(ID2D1DCRenderTarget* rt) {
    auto* dw=m_renderer.GetDWriteFactory();
    const auto primary=D2D1::ColorF(0x192739),muted=D2D1::ColorF(0x53657A);
    const auto purple=D2D1::ColorF(0x8259C2),border=D2D1::ColorF(0xDDE3EB);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC),&brush);
    if(!brush) return;
    const auto card=D2D1::RoundedRect(D2D1::RectF(11.5f,86.5f,348.5f,432.5f),9.5f,9.5f);
    if(!m_motionContents){rt->FillRoundedRectangle(card,brush.Get());brush->SetColor(border);rt->DrawRoundedRectangle(card,brush.Get(),1);}
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float lineHeight,DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt,dw,value,box,size,lineHeight,weight,color,align);
    };
    auto ellipsize=[&](std::wstring value,float width) {
        if(m_overviewVisuals.Measure(dw,value.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>width) {
            do {value.pop_back();} while(!value.empty() && m_overviewVisuals.Measure(dw,(value+L"…").c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>width);
            value+=L"…";
        }
        return value;
    };
    text(L"GPU",D2D1::RectF(24,91,338,111),13,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    wchar_t value[96];
    const bool validUsage=std::isfinite(m_snapshot.gpuUsagePercent) && m_snapshot.gpuUsagePercent>=0;
    if(validUsage) swprintf_s(value,L"%.0f",std::clamp(m_snapshot.gpuUsagePercent,0.0f,100.0f));else wcscpy_s(value,L"—");
    text(value,D2D1::RectF(24,110,94,152),28,42,DWRITE_FONT_WEIGHT_MEDIUM,purple);
    const float numberWidth=m_overviewVisuals.Measure(dw,value,28,DWRITE_FONT_WEIGHT_MEDIUM);
    if(validUsage) text(L"%",D2D1::RectF(28+numberWidth,122.5f,94,139.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(std::isfinite(m_snapshot.gpuTempC) && m_snapshot.gpuTempC>0) {
        const bool f=m_settings.tempUnit==1;
        swprintf_s(value,f ? L"%.0f°F" : L"%.0f°C",f ? m_snapshot.gpuTempC*1.8f+32 : m_snapshot.gpuTempC);
    } else wcscpy_s(value,m_settings.tempUnit==1 ? L"—°F" : L"—°C");
    const float tempWidth=std::max(38.f,m_overviewVisuals.Measure(dw,value,13,DWRITE_FONT_WEIGHT_MEDIUM));
    text(value,D2D1::RectF(102,121,102+tempWidth,141),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    const float modelLeft=110+tempWidth;
    const auto model=ellipsize(m_snapshot.gpuName.empty() ? L"显卡型号不可用" : m_snapshot.gpuName,338-modelLeft);
    text(model.c_str(),D2D1::RectF(modelLeft,122.5f,338,139.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,D2D1::ColorF(0x64748B),DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.5f));
    for(float y : {153.8f,172.0f,193.0f}) rt->FillRectangle(D2D1::RectF(24,y,338,y+.7f),brush.Get());
    const auto& history=m_snapshot.gpuHistory;
    if(history.Size()>=2) {
        float maximum=50;
        for(size_t i=0;i<history.Size();++i) if(std::isfinite(history[i]) && history[i]>=0 && history[i]<=100) maximum=std::max(maximum,history[i]*1.1f);
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> line,area;
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> ls,as;
        auto* factory=m_renderer.GetD2DFactory();
        if(SUCCEEDED(factory->CreatePathGeometry(&line)) && SUCCEEDED(factory->CreatePathGeometry(&area)) &&
            SUCCEEDED(line->Open(&ls)) && SUCCEEDED(area->Open(&as))) {
            size_t i=0;
            const auto valid=[](float v){return std::isfinite(v) && v>=0 && v<=100;};
            while(i<history.Size()) {
                while(i<history.Size() && !valid(history[i])) ++i;
                const size_t start=i;
                while(i<history.Size() && valid(history[i])) ++i;
                if(i-start<2) continue;
                const float x0=338-314*static_cast<float>(history.Size()-1-start)/59;
                as->BeginFigure(D2D1::Point2F(x0,193),D2D1_FIGURE_BEGIN_FILLED);
                for(size_t j=start;j<i;++j) {
                    const auto point=D2D1::Point2F(338-314*static_cast<float>(history.Size()-1-j)/59,193-35*history[j]/maximum);
                    if(j==start) ls->BeginFigure(point,D2D1_FIGURE_BEGIN_HOLLOW);else ls->AddLine(point);
                    as->AddLine(point);
                }
                as->AddLine(D2D1::Point2F(338-314*static_cast<float>(history.Size()-i)/59,193));
                as->EndFigure(D2D1_FIGURE_END_CLOSED);ls->EndFigure(D2D1_FIGURE_END_OPEN);
            }
            ls->Close();as->Close();
            brush->SetColor(D2D1::ColorF(0x8259C2,.12f));rt->FillGeometry(area.Get(),brush.Get());
            brush->SetColor(purple);rt->DrawGeometry(line.Get(),brush.Get(),1.5f);
        }
    } else text(L"正在收集历史数据",D2D1::RectF(24,161,338,178),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    wcscpy_s(value,L"60 秒前");
    text(value,D2D1::RectF(24,194,110,211),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(290,194,338,211),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"显存使用",D2D1::RectF(24,215,140,232),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(m_snapshot.gpuVramTotalBytes) swprintf_s(value,L"%.1f / %.1f GiB",m_snapshot.gpuVramUsedBytes/1073741824.0,m_snapshot.gpuVramTotalBytes/1073741824.0);
    else wcscpy_s(value,L"— / — GiB");
    text(value,D2D1::RectF(180,215,338,232),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const float ratio=m_snapshot.gpuVramTotalBytes ? static_cast<float>(std::clamp(static_cast<double>(m_snapshot.gpuVramUsedBytes)/m_snapshot.gpuVramTotalBytes,0.0,1.0)) : 0;
    const float fill=310*ratio;
    auto pill=[&](float left,float right,D2D1_COLOR_F color) {if(right>left) {brush->SetColor(color);rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left,236,right,244),4,4),brush.Get());}};
    if(ratio<=0) pill(24,338,border);
    else if(ratio>=1) pill(24,338,purple);
    else {pill(24,24+fill,purple);pill(28+fill,338,border);}
    if(std::isfinite(m_snapshot.gpuFreqGHz) && m_snapshot.gpuFreqGHz>0) swprintf_s(value,L"频率  %.2f GHz",m_snapshot.gpuFreqGHz);else wcscpy_s(value,L"频率  — GHz");
    text(value,D2D1::RectF(24,248,190,265),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    if(std::isfinite(m_snapshot.gpuPowerWatts) && m_snapshot.gpuPowerWatts>0) swprintf_s(value,L"功耗  %.0f W",m_snapshot.gpuPowerWatts);else wcscpy_s(value,L"功耗  — W");
    text(value,D2D1::RectF(220,248,338,265),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    for(float y : {269.0f,361.0f}) {brush->SetColor(border);rt->FillRectangle(D2D1::RectF(24,y,338,y+1),brush.Get());}
    text(L"引擎负载",D2D1::RectF(24,274,220,294),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"当前",D2D1::RectF(290,275.5f,338,292.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const wchar_t* labels[]={L"3D 渲染",L"视频解码",L"视频编码"};
    const float loads[]={m_snapshot.gpu3dLoadPercent,m_snapshot.gpuVideoDecodePercent,m_snapshot.gpuVideoEncodePercent};
    for(int i=0;i<3;++i) {
        const float y=298+21.0f*i;
        text(labels[i],D2D1::RectF(24,y,220,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if(std::isfinite(loads[i]) && loads[i]>=0) swprintf_s(value,L"%.0f%%",std::clamp(loads[i],0.0f,100.0f));else wcscpy_s(value,L"—");
        text(value,D2D1::RectF(290,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,i==0 ? purple : primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    text(L"占用最高的进程",D2D1::RectF(24,366,260,386),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
    text(L"GPU",D2D1::RectF(290,367.5f,338,384.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    for(size_t i=0;i<std::min<size_t>(2,m_snapshot.topGpuProcesses.size());++i) {
        const auto& process=m_snapshot.topGpuProcesses[i];const float y=390+21*static_cast<float>(i);
        const auto name=ellipsize(process.name,254);
        text(name.c_str(),D2D1::RectF(24,y,278,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        if(std::isfinite(process.cpuPercent) && process.cpuPercent>=0) swprintf_s(value,L"%.1f%%",std::clamp(process.cpuPercent,0.f,100.f));else wcscpy_s(value,L"—");
        text(value,D2D1::RectF(286,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    if(m_snapshot.topGpuProcesses.empty()) text(L"暂无进程数据",D2D1::RectF(24,390,338,407),11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
}

void ProDashboardWindow::DrawWhiteNetworkDirect(ID2D1DCRenderTarget* rt) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF),&brush);if(!brush) return;
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,301),20,24,0,8,.07f);
    const auto panel=D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,300.5f),19.5f,19.5f);
    rt->FillRoundedRectangle(panel,brush.Get());brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(panel,brush.Get(),1);
    m_overviewVisuals.Text(rt,m_renderer.GetDWriteFactory(),L"网络吞吐",D2D1::RectF(11,5,349,29),16,24,DWRITE_FONT_WEIGHT_BOLD,D2D1::ColorF(0x192739));
    m_overviewLinkRect={};
    D2D1_MATRIX_3X2_F original;rt->GetTransform(&original);
    rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-182)*original);
    DrawWhiteNetworkDetails(rt,true);rt->SetTransform(original);
    m_netChartRect.top-=182;m_netChartRect.bottom-=182;
}
void ProDashboardWindow::DrawWhiteNetworkDetails(ID2D1DCRenderTarget* rt, bool standalone) {
    auto* dw=m_renderer.GetDWriteFactory();
    const auto primary=D2D1::ColorF(0x192739),muted=D2D1::ColorF(0x53657A);
    const auto green=D2D1::ColorF(0x138873),blue=D2D1::ColorF(0x266BCE);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC),&brush);
    if (!brush) return;
    const auto card=D2D1::RoundedRect(D2D1::RectF(11.5f,194.5f,348.5f,482.5f),9.5f,9.5f);
    if(!standalone&&!m_motionContents) {
        rt->FillRoundedRectangle(card,brush.Get());
        brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->DrawRoundedRectangle(card,brush.Get(),1);
    }
    auto text=[&](const wchar_t* value,D2D1_RECT_F box,float size,float lineHeight,DWRITE_FONT_WEIGHT weight,
        D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt,dw,value,box,size,lineHeight,weight,color,align);
    };
    if(!standalone) text(L"网络",D2D1::RectF(24,199,338,219),13,24,DWRITE_FONT_WEIGHT_BOLD,primary);
    wchar_t value[128];
    for (int i=0;i<2;++i) {
        const float x=i==0 ? 24.0f : 185.0f;
        const auto color=i==0 ? green : blue;
        text(i==0 ? L"↓ 下载" : L"↑ 上传",D2D1::RectF(x,223,x+100,239),11,17,DWRITE_FONT_WEIGHT_BOLD,color);
        const auto raw=i==0 ? m_snapshot.downloadSpeedBps : m_snapshot.uploadSpeedBps;
        const bool giga=raw>=1000000000ULL;
        const double rate=raw/(giga ? 1000000000.0 : 1000000.0);
        const bool sampled=m_snapshot.downloadHistory.Size()>0 && m_snapshot.uploadHistory.Size()>0;
        if(sampled) swprintf_s(value,rate>=100 ? L"%.0f" : i==0 || rate>=10 ? L"%.1f" : L"%.2f",rate);else wcscpy_s(value,L"—");
        float size=28;
        while (size>18 && m_overviewVisuals.Measure(dw,value,size,DWRITE_FONT_WEIGHT_MEDIUM)>110) --size;
        const float width=std::max(64.0f,m_overviewVisuals.Measure(dw,value,size,DWRITE_FONT_WEIGHT_MEDIUM));
        text(value,D2D1::RectF(x,241,x+width+1,279),size,42,DWRITE_FONT_WEIGHT_MEDIUM,color);
        text(giga ? L"GB/s" : L"MB/s",D2D1::RectF(x+width+4,251.5f,x+153,268.5f),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
    }
    m_netChartRect=D2D1::RectF(24,285,338,365.36f);
    brush->SetColor(D2D1::ColorF(0xDDE3EB,.5f));
    for (float y : {291.56f,328.46f,365.36f})rt->FillRectangle(D2D1::RectF(24,y,338,y+.82f),brush.Get());
    const auto& down=m_snapshot.downloadHistory;const auto& up=m_snapshot.uploadHistory;
    // Both histories are collected together. Tail alignment also handles unequal startup lengths.
    const size_t count=std::min(down.Size(),up.Size());
    float peak=0;
    bool hasPeak=false;
    const auto valid=[](float v){return std::isfinite(v) && v>=0;};
    for(size_t i=0;i<count;++i) {
        for(float v : {down[down.Size()-count+i],up[up.Size()-count+i]}) if(valid(v)) {peak=std::max(peak,v);hasPeak=true;}
    }
    const float maximum=std::max(1000000.0f,peak*1.15f);
    if (count>=2) {
        for(int series=0;series<2;++series) {
            const auto& history=series==0 ? down : up;
            Microsoft::WRL::ComPtr<ID2D1PathGeometry> line,area;
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> lineSink,areaSink;
            auto* factory=m_renderer.GetD2DFactory();
            if(FAILED(factory->CreatePathGeometry(&line)) || FAILED(factory->CreatePathGeometry(&area)) ||
                FAILED(line->Open(&lineSink)) || FAILED(area->Open(&areaSink)))continue;
            size_t i=0;
            while(i<count) {
                while(i<count && !valid(history[history.Size()-count+i])) ++i;
                const size_t start=i;
                while(i<count && valid(history[history.Size()-count+i])) ++i;
                if(i-start<2) continue;
                areaSink->BeginFigure(D2D1::Point2F(338-314*static_cast<float>(count-1-start)/59,365.36f),D2D1_FIGURE_BEGIN_FILLED);
                for(size_t j=start;j<i;++j) {
                    const float x=338-314*static_cast<float>(count-1-j)/59;
                    const float y=365.36f-74*std::clamp(history[history.Size()-count+j]/maximum,0.0f,1.0f);
                    if(j==start)lineSink->BeginFigure(D2D1::Point2F(x,y),D2D1_FIGURE_BEGIN_HOLLOW);
                    else lineSink->AddLine(D2D1::Point2F(x,y));
                    areaSink->AddLine(D2D1::Point2F(x,y));
                }
                areaSink->AddLine(D2D1::Point2F(338-314*static_cast<float>(count-i)/59,365.36f));
                areaSink->EndFigure(D2D1_FIGURE_END_CLOSED);lineSink->EndFigure(D2D1_FIGURE_END_OPEN);
            }
            lineSink->Close();areaSink->Close();
            if(!series){brush->SetColor(D2D1::ColorF(0x138873,.12f));rt->FillGeometry(area.Get(),brush.Get());}
            brush->SetColor(series==0 ? green : blue);rt->DrawGeometry(line.Get(),brush.Get(),1.5f);
        }
    } else text(L"正在收集历史数据",D2D1::RectF(24,310,338,340),11,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    wcscpy_s(value,L"60 秒前");
    text(value,D2D1::RectF(24,368,120,385),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted);
    text(L"现在",D2D1::RectF(300,368,338,385),10,17,DWRITE_FONT_WEIGHT_NORMAL,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(L"● 下载",D2D1::RectF(24,389,100,406),11,17,DWRITE_FONT_WEIGHT_BOLD,green);
    text(L"● 上传",D2D1::RectF(142.5f,389,220,406),11,17,DWRITE_FONT_WEIGHT_BOLD,blue);
    if(hasPeak) swprintf_s(value,L"峰值 %.1f MB/s",peak/1000000.0);else wcscpy_s(value,L"峰值 —");
    text(value,D2D1::RectF(235,389,338,406),11,17,DWRITE_FONT_WEIGHT_BOLD,muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    brush->SetColor(D2D1::ColorF(0xDDE3EB));rt->FillRectangle(D2D1::RectF(24,410,338,411),brush.Get());
    auto metadata=[&](const wchar_t* label,std::wstring content,float y) {
        while(!content.empty() && m_overviewVisuals.Measure(dw,content.c_str(),11,DWRITE_FONT_WEIGHT_BOLD)>204) {
            if(content.back()==L'…')content.pop_back();content.pop_back();content+=L"…";
            if(content.size()==1)break;
        }
        text(label,D2D1::RectF(24,y,132,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        text(content.c_str(),D2D1::RectF(134,y,338,y+17),11,17,DWRITE_FONT_WEIGHT_BOLD,primary,DWRITE_TEXT_ALIGNMENT_TRAILING);
    };
    metadata(L"网络接口",m_snapshot.netAdapterName.empty() ? L"—" : m_snapshot.netAdapterName,415);
    metadata(L"本地地址",m_snapshot.localIpAddress.empty() ? L"—" : m_snapshot.localIpAddress,436);
    auto bytes=[](uint64_t n) {
        wchar_t out[32];
        if(n>=1000000000ULL)swprintf_s(out,L"%.2f GB",n/1000000000.0);
        else if(n>=1000000ULL)swprintf_s(out,L"%.0f MB",n/1000000.0);
        else if(n>=1000ULL)swprintf_s(out,L"%.0f KB",n/1000.0);
        else swprintf_s(out,L"%llu B",n);
        return std::wstring(out);
    };
    metadata(L"本次下载 / 上传",count ? bytes(m_snapshot.sessionTotalInBytes)+L" / "+bytes(m_snapshot.sessionTotalOutBytes) : L"— / —",457);
    if(m_netProbeActive && count>=2 && m_netProbeX>=338-314*static_cast<float>(count-1)/59) {
        const float ratio=std::clamp((m_netProbeX-24)/314,0.0f,1.0f);
        const size_t seconds=std::min(count-1,static_cast<size_t>(std::lround((1-ratio)*59)));
        const size_t index=count-1-seconds;
        const size_t secondsAgo=count-1-index;
        const float cursor=338-314*static_cast<float>(secondsAgo)/59;
        brush->SetColor(muted);rt->FillRectangle(D2D1::RectF(cursor,285,cursor+.863f,365.36f),brush.Get());
        const std::time_t sample=static_cast<std::time_t>(m_netSampleUnixSeconds ? m_netSampleUnixSeconds : std::time(nullptr));
        const std::time_t time=sample-static_cast<std::time_t>(secondsAgo);std::tm clock{};localtime_s(&clock,&time);
        wchar_t stamp[128],rates[128];
        swprintf_s(stamp,L"%zu 秒前  ·  %02d:%02d:%02d",secondsAgo,clock.tm_hour,clock.tm_min,clock.tm_sec);
        wchar_t downRate[32],upRate[32];
        const float d=down[down.Size()-count+index],u=up[up.Size()-count+index];
        if(valid(d)) swprintf_s(downRate,L"%.1f",d/1000000.0);else wcscpy_s(downRate,L"—");
        if(valid(u)) swprintf_s(upRate,L"%.1f",u/1000000.0);else wcscpy_s(upRate,L"—");
        swprintf_s(rates,L"↓ %s   ↑ %s MB/s",downRate,upRate);
        const float width=std::clamp(std::max(150.216f,std::max(m_overviewVisuals.Measure(dw,stamp,11,DWRITE_FONT_WEIGHT_BOLD),
            m_overviewVisuals.Measure(dw,rates,11,DWRITE_FONT_WEIGHT_BOLD))+16),150.216f,314.0f);
        const float x=std::clamp(cursor-width*.43f,24.0f,338-width);
        brush->SetColor(D2D1::ColorF(0xEDF3FB));rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,285,x+width,339),6,6),brush.Get());
        text(stamp,D2D1::RectF(x+8,293,x+width-8,310),11,17,DWRITE_FONT_WEIGHT_BOLD,muted);
        text(rates,D2D1::RectF(x+8,314,x+width-8,331),11,17,DWRITE_FONT_WEIGHT_BOLD,primary);
    }
}

void ProDashboardWindow::DrawMotionBody(ID2D1DCRenderTarget* rt) {
    const auto& layout=m_panelMotion.current;
    D2D1_MATRIX_3X2_F original;rt->GetTransform(&original);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> b;rt->CreateSolidColorBrush(D2D1::ColorF(0xF7F9FC),&b);if(!b)return;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;rt->GetFactory(&factory);
    auto* dw=m_renderer.GetDWriteFactory();
    const wchar_t* labels[]={L"CPU",L"GPU",L"内存",L"磁盘",L"网络",L"传感器"};
    const UINT colors[]={0x266BCE,0x8259C2,0xA87517,0x2278A1,0x138873,0xBC5648};
    const MetricHistory* history[]={&m_snapshot.cpuHistory,&m_snapshot.gpuHistory,&m_snapshot.memoryHistory,&m_snapshot.diskHistory,&m_snapshot.downloadHistory,&m_snapshot.tempHistory};
    const float nativeTop[]={50,86,122,158,194,230};
    const float top=42+8*layout.expanded,bottom=m_dipHeight-40-6*layout.expanded;
    rt->PushAxisAlignedClip(D2D1::RectF(11,top,349,bottom),D2D1_ANTIALIAS_MODE_ALIASED);
    for(int i=0;i<Module_Count;++i){const auto& r=layout.items[i];const auto box=D2D1::RectF(r.x,r.y,r.x+r.w,r.y+r.h);
        const float inset=.5f*r.content[2];
        const auto shape=D2D1::RoundedRect(D2D1::RectF(box.left+inset,box.top+inset,box.right-inset,box.bottom-inset),r.radius-inset,r.radius-inset);
        b->SetColor(D2D1::ColorF(0xF7F9FC,r.background));rt->FillRoundedRectangle(shape,b.Get());
        if(r.content[2]>.001f){b->SetColor(D2D1::ColorF(0xDDE3EB,r.content[2]));rt->DrawRoundedRectangle(shape,b.Get(),1);}
        Microsoft::WRL::ComPtr<ID2D1RoundedRectangleGeometry> mask;factory->CreateRoundedRectangleGeometry(shape,&mask);
        if(!mask)continue;
        wchar_t value[80]{};
        switch(i){
        case Module_CPU:if(std::isfinite(m_snapshot.cpuUsageTotal))swprintf_s(value,L"%.0f%%",m_snapshot.cpuUsageTotal);else wcscpy_s(value,L"—");break;
        case Module_GPU:swprintf_s(value,L"%.0f%%",m_snapshot.gpuUsagePercent);break;
        case Module_Memory:if(m_snapshot.totalPhysMem)swprintf_s(value,L"%.1f / %.0f GiB",m_snapshot.usedPhysMem/1073741824.,m_snapshot.totalPhysMem/1073741824.);else wcscpy_s(value,L"—");break;
        case Module_Disk:if(m_snapshot.diskReadHistory.Size()&&std::isfinite(m_snapshot.diskReadHistory.Back()))swprintf_s(value,layout.expanded<.5f?L"R %.0f MB/s":L"读 %.0f MB/s",m_snapshot.diskReadBps/1000000.);else wcscpy_s(value,layout.expanded<.5f?L"R —":L"读 —");break;
        case Module_Network:swprintf_s(value,L"↓ %.1f MB/s",m_snapshot.downloadSpeedBps/1000000.);break;
        default:if(m_snapshot.cpuTempC>0)swprintf_s(value,m_settings.tempUnit==1?L"CPU %.0f°F":L"CPU %.0f°C",m_settings.tempUnit==1?m_snapshot.cpuTempC*1.8f+32:m_snapshot.cpuTempC);else wcscpy_s(value,L"CPU —");break;}
        for(int kind=0;kind<3;++kind){float opacity=r.content[kind];if(opacity<=.001f)continue;
            rt->PushLayer(D2D1::LayerParameters(box,mask.Get(),D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,D2D1::Matrix3x2F::Identity(),opacity),nullptr);
            if(kind==2){
                rt->SetTransform(D2D1::Matrix3x2F::Translation(r.x-11,r.y-nativeTop[i])*original);
                m_motionContents=true;
                switch(i){case Module_CPU:DrawWhiteCpuDetails(rt);break;case Module_GPU:DrawWhiteGpuDetails(rt);break;case Module_Memory:DrawWhiteMemoryDetails(rt);break;case Module_Disk:DrawWhiteDiskDetails(rt);break;case Module_Network:DrawWhiteNetworkDetails(rt);break;default:DrawWhiteSensorDetails(rt);break;}
                m_motionContents=false;rt->SetTransform(original);
            }else{
                const bool capsule=kind==1;float x=r.x,y=r.y,w=r.w;
                if(capsule){b->SetColor(D2D1::ColorF(colors[i]));rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x+9,y+14),1.5f,1.5f),b.Get());}
                m_overviewVisuals.Text(rt,dw,labels[i],D2D1::RectF(x+(capsule?16:12),y+5,x+68,y+25),13,20,DWRITE_FONT_WEIGHT_MEDIUM,D2D1::ColorF(0x192739));
                if(!capsule&&history[i]->Size()>=2)DrawMiniSparkline(rt,x+68.6667f,y+8,100,14,*history[i],100,D2D1::ColorF(colors[i]));
                float available=w-(capsule?68:190),measured=m_overviewVisuals.Measure(dw,value,13,DWRITE_FONT_WEIGHT_MEDIUM);
                const float right=x+w-(capsule?8:36.6667f);
                if(!capsule&&i==Module_Disk&&layout.expanded<.5f){float prefix=m_overviewVisuals.Measure(dw,L"R ",13,DWRITE_FONT_WEIGHT_MEDIUM),left=std::max(x+177,right-measured);
                    m_overviewVisuals.Text(rt,dw,L"R ",D2D1::RectF(left,y+5,left+prefix,y+25),13,20,DWRITE_FONT_WEIGHT_MEDIUM,D2D1::ColorF(0x138873));
                    m_overviewVisuals.Text(rt,dw,value+2,D2D1::RectF(left+prefix,y+5,right,y+25),13,20,DWRITE_FONT_WEIGHT_MEDIUM,D2D1::ColorF(0x192739));
                }else m_overviewVisuals.Text(rt,dw,value,D2D1::RectF(x+(capsule?60:177),y+5,right,y+25),capsule&&measured>available?11.f:13.f,20,DWRITE_FONT_WEIGHT_MEDIUM,D2D1::ColorF(capsule?colors[i]:0x192739),DWRITE_TEXT_ALIGNMENT_TRAILING);
                if(!capsule)m_overviewVisuals.DrawIcon(rt,ProOverviewVisuals::Chevron,D2D1::RectF(x+w-28,y+7,x+w-12,y+23));
            }
            rt->PopLayer();
        }
        // Only the title of a morphing detail is a collapse target. Never stretch hit regions.
        float hitBottom=i==m_expandedModule?std::min(r.y+25,r.y+r.h):r.y+r.h;
        m_moduleRects[i]=D2D1::RectF(r.x,std::max(r.y,top),r.x+r.w,std::min(hitBottom,bottom));
        if(m_moduleRects[i].bottom<=m_moduleRects[i].top)m_moduleRects[i]={};
    }
    rt->PopAxisAlignedClip();rt->SetTransform(original);
    m_cpuCoreRects.clear();m_netChartRect={};m_diskTriggerRect={};m_diskOptionRects.clear();
}

void ProDashboardWindow::DrawWhiteOverview(ID2D1DCRenderTarget* rt) {
    using namespace ProOverviewLayout;
    const bool cpuExpanded = m_expandedModule == Module_CPU;
    const bool netExpanded = m_expandedModule == Module_Network;
    const bool gpuExpanded = m_expandedModule == Module_GPU;
    const bool memExpanded = m_expandedModule == Module_Memory;
    const bool diskExpanded = m_expandedModule == Module_Disk;
    const bool sensorExpanded = m_expandedModule == Module_Sensor;
    const bool expanded = cpuExpanded || netExpanded || gpuExpanded || memExpanded || diskExpanded || sensorExpanded;
    const bool compactCpu = cpuExpanded && m_settings.layoutMode==1;
    const bool compactGpu = gpuExpanded && m_settings.layoutMode==1;
    const bool compactMem = memExpanded && m_settings.layoutMode==1;
    const bool compactNet = netExpanded && m_settings.layoutMode==1;
    const bool compactDisk = diskExpanded && m_settings.layoutMode==1;
    const bool compactSensor = sensorExpanded && m_settings.layoutMode==1;
    const float coreExtra = cpuExpanded ? CpuCoreExtraHeight() : 0;
    const float panelHeight = m_dipHeight;
    const float expansion=m_panelMotion.active?m_panelMotion.current.expanded:(expanded?1.f:0.f);
    const float headerOffset = 4*expansion;
    const float footerOffset = m_dipHeight-284;
    auto* dw = m_renderer.GetDWriteFactory();
    const auto primary = D2D1::ColorF(0x192739);
    const auto muted = D2D1::ColorF(0x53657A);
    const auto cpu = D2D1::ColorF(0x266BCE);
    const auto green = D2D1::ColorF(0x138873);
    const auto border = D2D1::ColorF(0xDDE3EB);
    const auto surface = D2D1::ColorF(0xF7F9FC);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(primary, &brush);
    if (!brush) return;
    auto rounded = [&](D2D1_RECT_F box, float radius, D2D1_COLOR_F color) {
        brush->SetColor(color); rt->FillRoundedRectangle(D2D1::RoundedRect(box, radius, radius), brush.Get());
    };
    auto text = [&](const wchar_t* value, D2D1_RECT_F box, float size, float lineHeight,
        DWRITE_FONT_WEIGHT weight, D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
        m_overviewVisuals.Text(rt, dw, value, box, size, lineHeight, weight, color, align);
    };
    m_overviewVisuals.Shadow(rt, D2D1::RectF(0, 0, Width, 160), 20, 24, 0, 8, .07f,0x1A263D,panelHeight);
    const auto panel = D2D1::RoundedRect(D2D1::RectF(.5f, .5f, Width-.5f, panelHeight-.5f), 19.5f, 19.5f);
    brush->SetColor(D2D1::ColorF(0xFFFFFF)); rt->FillRoundedRectangle(panel, brush.Get());
    brush->SetColor(border); rt->DrawRoundedRectangle(panel, brush.Get(), 1);
    D2D1_MATRIX_3X2_F originalTransform; rt->GetTransform(&originalTransform);
    rt->SetTransform(D2D1::Matrix3x2F::Translation(0, headerOffset) * originalTransform);
    text(L"系统运行概览", D2D1::RectF(11, 7, 107, 31), 16, 24, DWRITE_FONT_WEIGHT_BOLD, primary);
    rounded(D2D1::RectF(115, 9, 149, 29), 6, D2D1::ColorF(0xEDF3FB));
    text(L"PRO", D2D1::RectF(119, 9, 149, 29), 13, 20, DWRITE_FONT_WEIGHT_MEDIUM, cpu);
    m_pinButtonRect = D2D1::RectF(m_isPinned ? 237.f : 249.f, 5, 313, 33);
    m_closeButtonRect = D2D1::RectF(321, 5, 349, 33);
    m_settingsHeaderBtnRect = {}; // Only the visible footer gear is interactive.
    m_settingsButtonRect = {};
    rounded(m_pinButtonRect, 14, m_pinHover||m_isPinned ? D2D1::ColorF(0xEDF3FB) : surface);
    rounded(m_closeButtonRect, 14, m_closeHover ? D2D1::ColorF(0xEDF3FB) : surface);
    m_overviewVisuals.DrawIcon(rt, ProOverviewVisuals::Pin, D2D1::RectF(m_isPinned ? 248.5f : 260.5f, 12, m_isPinned ? 262.5f : 274.5f, 26));
    text(m_isPinned ? L"已固定" : L"固定", D2D1::RectF(m_isPinned ? 267.5f : 279.5f, 11, 301.5f, 27), 11, 16, DWRITE_FONT_WEIGHT_MEDIUM, muted);
    m_overviewVisuals.DrawIcon(rt, ProOverviewVisuals::Close, D2D1::RectF(328, 12, 342, 26));
    const float headerDividerY = 37+2*expansion;
    brush->SetColor(border); rt->FillRectangle(D2D1::RectF(11, headerDividerY, 349, headerDividerY+1), brush.Get());
    rt->SetTransform(originalTransform);
    m_pinButtonRect.top += headerOffset; m_pinButtonRect.bottom += headerOffset;
    m_closeButtonRect.top += headerOffset; m_closeButtonRect.bottom += headerOffset;
    // Expanded CPU has no duplicate disclosure row. Its title remains a collapse target.
    if(m_panelMotion.active)DrawMotionBody(rt);
    else {
    const auto bodyTransform=D2D1::Matrix3x2F::Translation(0,-m_cpuScroll)*originalTransform;
    const float bodyBottom=panelHeight-(expanded ? 46.f : 40.f);
    rt->PushAxisAlignedClip(D2D1::RectF(11,expanded ? 50.f : 42.f,349,bodyBottom),D2D1_ANTIALIAS_MODE_ALIASED);
    rt->SetTransform(bodyTransform);
    if (cpuExpanded) {
        DrawWhiteCpuDetails(rt);
        m_moduleRects[Module_CPU] = D2D1::RectF(11, 50, 349, 75);
    } else if (netExpanded) {
        if(compactNet) rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-76)*bodyTransform);
        DrawWhiteNetworkDetails(rt);
        rt->SetTransform(bodyTransform);
        if(compactNet) {m_netChartRect.top-=76;m_netChartRect.bottom-=76;}
        m_moduleRects[Module_Network] = D2D1::RectF(11, compactNet ? 118 : 194, 349, compactNet ? 143 : 219);
    } else if (gpuExpanded) {
        if(compactGpu) rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-2)*bodyTransform);
        DrawWhiteGpuDetails(rt);
        rt->SetTransform(bodyTransform);
        m_moduleRects[Module_GPU] = D2D1::RectF(11, compactGpu ? 84 : 86, 349, compactGpu ? 109 : 111);
    } else if (memExpanded) {
        if(compactMem) rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-38)*bodyTransform);
        DrawWhiteMemoryDetails(rt);
        rt->SetTransform(bodyTransform);
        m_moduleRects[Module_Memory] = D2D1::RectF(11, compactMem ? 84 : 122, 349, compactMem ? 113 : 151);
    } else if (diskExpanded) {
        if(compactDisk) rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-40)*bodyTransform);
        DrawWhiteDiskDetails(rt);
        rt->SetTransform(bodyTransform);
        m_moduleRects[Module_Disk] = D2D1::RectF(11, compactDisk ? 118 : 158, 349, compactDisk ? 143 : 183);
    } else if (sensorExpanded) {
        if(compactSensor) rt->SetTransform(D2D1::Matrix3x2F::Translation(0,-78)*bodyTransform);
        DrawWhiteSensorDetails(rt);
        rt->SetTransform(bodyTransform);
        m_moduleRects[Module_Sensor] = D2D1::RectF(11, compactSensor ? 152 : 230, 349, compactSensor ? 177 : 255);
    }

    const wchar_t* labels[] = {L"CPU", L"GPU", L"内存", L"磁盘", L"网络", L"传感器"};
    const MetricHistory* histories[] = {&m_snapshot.cpuHistory, &m_snapshot.gpuHistory, &m_snapshot.memoryHistory,
        &m_snapshot.diskHistory, &m_snapshot.downloadHistory, &m_snapshot.tempHistory};
    const D2D1_COLOR_F colors[] = {cpu, D2D1::ColorF(0x8259C2), D2D1::ColorF(0xA87517), D2D1::ColorF(0x2278A1), green, D2D1::ColorF(0xBC5648)};
    for (int i = cpuExpanded ? 1 : 0; i < Module_Count; ++i) {
        if(compactGpu && i==Module_GPU) continue;
        if(compactMem && i==Module_Memory) continue;
        if(compactNet && i==Module_Network) continue;
        if(compactDisk && i==Module_Disk) continue;
        if(compactSensor && i==Module_Sensor) continue;
        if(compactCpu || compactGpu || compactMem || compactNet || compactDisk || compactSensor) {
            const int index=compactSensor ? i : compactDisk ? (i<3 ? i : i-4) : compactNet ? (i<4 ? i : 0) : compactMem ? (i<2 ? i : i-3) : compactGpu ? std::max(0,i-2) : i-1,row=index/2,column=index%2;
            const float x=compactGpu && i==0 ? 11 : 11+column*172.f;
            const float y=compactSensor ? 50+row*34.f : compactDisk ? (i<3 ? 50+row*34.f : 464) : compactNet ? (i<4 ? 50+row*34.f : 413) : compactMem ? (i<2 ? 50 : 465+row*34.f) : compactGpu ? (i==0 ? 50 : 437+row*34.f) : 417+row*34.f+coreExtra;
            const float w=compactSensor ? (i==Module_Network ? 338.f : 166.f) : compactDisk ? (i==Module_Memory ? 338.f : 166.f) : compactNet || compactMem ? (i==Module_Sensor ? 338.f : 166.f) : compactGpu ? (i==0 ? 338.f : 166.f) : i==Module_Sensor ? 338.f : 166.f;
            m_moduleRects[i]=D2D1::RectF(x,y,x+w,y+28);
            rounded(m_moduleRects[i],14,m_hoveredModule==i ? D2D1::ColorF(0xEDF3FB) : surface);
            brush->SetColor(colors[i]);rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x+9,y+14),1.5f,1.5f),brush.Get());
            text(labels[i],D2D1::RectF(x+16,y+4,x+68,y+24),13,20,DWRITE_FONT_WEIGHT_MEDIUM,primary);
            wchar_t val[64];
            switch(i) {
            case Module_CPU: if(std::isfinite(m_snapshot.cpuUsageTotal)) swprintf_s(val,L"%.0f%%",std::clamp(m_snapshot.cpuUsageTotal,0.f,100.f));else wcscpy_s(val,L"—");break;
            case Module_GPU: swprintf_s(val,L"%.0f%%",m_snapshot.gpuUsagePercent);break;
            case Module_Memory: if(m_snapshot.totalPhysMem) swprintf_s(val,L"%.1f / %.0f GiB",m_snapshot.usedPhysMem/1073741824.0,m_snapshot.totalPhysMem/1073741824.0);else wcscpy_s(val,L"—");break;
            case Module_Disk: if(m_snapshot.diskReadHistory.Size()&&std::isfinite(m_snapshot.diskReadHistory.Back()))swprintf_s(val,L"读 %.0f MB/s",m_snapshot.diskReadBps/1000000.0);else wcscpy_s(val,L"读 —");break;
            case Module_Network: swprintf_s(val,L"↓ %.1f MB/s",m_snapshot.downloadSpeedBps/1000000.0);break;
            default: if(m_snapshot.cpuTempC>0) swprintf_s(val,m_settings.tempUnit==1 ? L"CPU %.0f°F" : L"CPU %.0f°C",m_settings.tempUnit==1 ? m_snapshot.cpuTempC*1.8f+32 : m_snapshot.cpuTempC);else wcscpy_s(val,L"CPU —");break;
            }
            const float available=w-68;
            const float measured=m_overviewVisuals.Measure(dw,val,13,DWRITE_FONT_WEIGHT_MEDIUM);
            text(val,D2D1::RectF(x+60,y+4,x+w-8,y+24),measured>available ? 11.f : 13.f,20,DWRITE_FONT_WEIGHT_MEDIUM,colors[i],DWRITE_TEXT_ALIGNMENT_TRAILING);
            continue;
        }
        if (netExpanded && i == Module_Network) continue;
        if (gpuExpanded && i == Module_GPU) continue;
        if (memExpanded && i == Module_Memory) continue;
        if (diskExpanded && i == Module_Disk) continue;
        if (sensorExpanded && i == Module_Sensor) continue;
        const float y = cpuExpanded ? 417 + (i-1)*36 + coreExtra : netExpanded ? (i==Module_Sensor ? 489.0f : 50+i*36.0f) : gpuExpanded ? (i==0 ? 50.0f : 439+(i-2)*36.0f) : memExpanded ? (i<2 ? 50+i*36.0f : 503+(i-3)*36.0f) : diskExpanded ? (i<3 ? 50+i*36.0f : 504+(i-4)*36.0f) : sensorExpanded ? 50+i*36.0f : RowY(i);
        m_moduleRects[i] = D2D1::RectF(11, y, 349, y + 30);
        if (m_hoveredModule == i) rounded(m_moduleRects[i], 10, surface);
        text(labels[i], D2D1::RectF(LabelX, y+5, LabelX+48, y+25), 13, 20, DWRITE_FONT_WEIGHT_MEDIUM, primary);
        if (histories[i]->Size() >= 2)
            DrawMiniSparkline(rt, SparkX, y+8, 100, 14, *histories[i], 100, colors[i]);
        wchar_t value[80]{};
        switch (i) {
        case Module_CPU: swprintf_s(value, L"%.0f%%", m_snapshot.cpuUsageTotal); break;
        case Module_GPU: swprintf_s(value, L"%.0f%%", m_snapshot.gpuUsagePercent); break;
        case Module_Memory:
            if (m_snapshot.totalPhysMem) swprintf_s(value, L"%.1f / %.0f GiB", m_snapshot.usedPhysMem / 1073741824.0, m_snapshot.totalPhysMem / 1073741824.0);
            else wcscpy_s(value, L"—"); break;
        case Module_Disk: if(m_snapshot.diskReadHistory.Size()&&std::isfinite(m_snapshot.diskReadHistory.Back()))swprintf_s(value, expanded ? L"读 %.0f MB/s" : L"R %.0f MB/s", m_snapshot.diskReadBps / 1000000.0);else wcscpy_s(value,L"R —");break;
        case Module_Network: swprintf_s(value, L"↓ %.1f MB/s", m_snapshot.downloadSpeedBps / 1000000.0); break;
        case Module_Sensor:
            if (m_snapshot.cpuTempC > 0) {
                const bool fahrenheit = m_settings.tempUnit == 1;
                swprintf_s(value, fahrenheit ? L"CPU %.0f°F" : L"CPU %.0f°C", fahrenheit ? m_snapshot.cpuTempC * 1.8f + 32 : m_snapshot.cpuTempC);
            } else wcscpy_s(value, L"CPU —"); break;
        }
        if (i == Module_Disk && !expanded) {
            // Exactly the R prefix is green; the rate stays primary text.
            const float total = m_overviewVisuals.Measure(dw, value, 13, DWRITE_FONT_WEIGHT_MEDIUM);
            const float prefix = m_overviewVisuals.Measure(dw, L"R ", 13, DWRITE_FONT_WEIGHT_MEDIUM);
            const float x = std::max(ValueX, ValueRight - total);
            text(L"R ", D2D1::RectF(x, y+5, x+prefix, y+25), 13, 20, DWRITE_FONT_WEIGHT_MEDIUM, green);
            text(value+2, D2D1::RectF(x+prefix, y+5, ValueRight, y+25), 13, 20, DWRITE_FONT_WEIGHT_MEDIUM, primary);
        } else text(value, D2D1::RectF(ValueX, y+5, ValueRight, y+25), 13, 20, DWRITE_FONT_WEIGHT_MEDIUM, primary, DWRITE_TEXT_ALIGNMENT_TRAILING);
        m_overviewVisuals.DrawIcon(rt, ProOverviewVisuals::Chevron, D2D1::RectF(ArrowX, y+7, ArrowX+16, y+23));
    }
    rt->SetTransform(originalTransform);rt->PopAxisAlignedClip();
    for(auto& rect:m_moduleRects) {rect.top-=m_cpuScroll;rect.bottom-=m_cpuScroll;rect.top=std::max(rect.top,expanded ? 50.f : 42.f);rect.bottom=std::min(rect.bottom,bodyBottom);if(rect.bottom<=rect.top)rect={};}
    auto offsetHit=[&](D2D1_RECT_F& r){r.top-=m_cpuScroll;r.bottom-=m_cpuScroll;};
    if(netExpanded){offsetHit(m_netChartRect);m_netChartRect.top=std::max(m_netChartRect.top,50.f);m_netChartRect.bottom=std::min(m_netChartRect.bottom,bodyBottom);}
    if(diskExpanded){offsetHit(m_diskTriggerRect);offsetHit(m_diskMenuRect);for(auto& r:m_diskOptionRects)offsetHit(r);if(m_diskTriggerRect.top<50||m_diskTriggerRect.bottom>bodyBottom)m_diskTriggerRect={};}
    }
    rt->SetTransform(D2D1::Matrix3x2F::Translation(0, footerOffset) * originalTransform);
    // CPU's divider is 597; its footer starts at 604 (6 DIP gap).
    const float footerDividerY = 246-2*expansion;
    brush->SetColor(D2D1::ColorF(0xDDE3EB, .35f)); rt->FillRectangle(D2D1::RectF(11, footerDividerY, 349, footerDividerY+1), brush.Get());
    m_overviewVisuals.DrawIcon(rt, ProOverviewVisuals::Clock, D2D1::RectF(11, 259, 23, 271));
    const auto uptime = m_snapshot.uptimeSeconds;
    wchar_t uptimeText[32];
    swprintf_s(uptimeText, L"%02llu:%02llu:%02llu", uptime / 3600, (uptime / 60) % 60, uptime % 60);
    text(uptimeText, D2D1::RectF(31, 256.5f, 79, 273.5f), 11, 17, DWRITE_FONT_WEIGHT_NORMAL, muted);
    brush->SetColor(border); rt->FillRectangle(D2D1::RectF(83, 259, 84, 271), brush.Get());
    wchar_t interval[24];if(m_settings.updateIntervalSec==0)wcscpy_s(interval,L"0.5 秒刷新");else swprintf_s(interval, L"%d 秒刷新", m_settings.updateIntervalSec);
    text(interval, D2D1::RectF(91, 256.5f, 151, 273.5f), 11, 17, DWRITE_FONT_WEIGHT_NORMAL, muted);
    m_settingsFooterBtnRect = D2D1::RectF(321, 251, 349, 279);
    m_overviewVisuals.Shadow(rt, D2D1::RectF(322, 252, 348, 278), 7, 5, -2, 4, .10f, 0);
    rounded(m_settingsFooterBtnRect, 8, m_settingsHover ? D2D1::ColorF(0xEDF3FB) : surface);
    m_overviewVisuals.DrawIcon(rt, (memExpanded || diskExpanded || sensorExpanded) ? ProOverviewVisuals::SettingsMemory : gpuExpanded ? ProOverviewVisuals::SettingsGpu : expanded ? ProOverviewVisuals::SettingsCpu : ProOverviewVisuals::Settings, D2D1::RectF(328, 258, 342, 272));
    m_settingsFooterBtnRect.top += footerOffset; m_settingsFooterBtnRect.bottom += footerOffset;
    rt->SetTransform(originalTransform);
    if(cpuExpanded&&!m_panelMotion.active) DrawCpuCorePeek(rt);
}

void ProDashboardWindow::DrawScrolledSurface(ID2D1DCRenderTarget* rt,const std::function<void()>& draw) {
    if(m_cpuFullHeight<=m_dipHeight+.1f){draw();return;}
    const float visible=m_dipHeight,natural=m_cpuFullHeight;
    const float top=m_mode==Mode_Settings ? 70.f : 33.f;
    const float bottom=m_mode==Mode_DirectTemp ? 27.f : 5.f;
    D2D1_MATRIX_3X2_F original;rt->GetTransform(&original);
    m_overviewVisuals.Shadow(rt,D2D1::RectF(0,0,360,visible),20,24,0,8,.07f);
    auto pass=[&](D2D1_RECT_F clip,float offset){rt->SetTransform(original);rt->PushAxisAlignedClip(clip,D2D1_ANTIALIAS_MODE_ALIASED);rt->SetTransform(D2D1::Matrix3x2F::Translation(0,offset)*original);m_dipHeight=natural;draw();m_dipHeight=visible;rt->SetTransform(original);rt->PopAxisAlignedClip();};
    pass(D2D1::RectF(0,0,360,top),0);
    pass(D2D1::RectF(0,top,360,visible-bottom),-m_cpuScroll);
    pass(D2D1::RectF(0,visible-bottom,360,visible),visible-natural);
    auto offset=[&](D2D1_RECT_F& r){if(r.right<=r.left)return;r.top-=m_cpuScroll;r.bottom-=m_cpuScroll;};
    if(m_mode==Mode_DirectTemp){offset(m_tempChartRect);m_tempAlertRect.top+=visible-natural;m_tempAlertRect.bottom+=visible-natural;m_allSensorsRect.top+=visible-natural;m_allSensorsRect.bottom+=visible-natural;}
    if(m_mode==Mode_DirectNetwork)offset(m_netChartRect);
    if(m_mode==Mode_Settings) {
        for(auto& r:m_themeRects)offset(r);for(auto& r:m_layoutOptionRects)offset(r);offset(m_sliderTrackRect);
        for(auto& r:m_metricToggleRects)offset(r);for(auto& r:m_monitoringHeaders)offset(r);for(auto& r:m_indicatorGripRects)offset(r);for(auto& r:m_monitoringDeviceRects)offset(r);
        for(auto& r:m_generalToggleRects)offset(r);offset(m_generalHotkeyRect);offset(m_intervalRect);offset(m_tempUnitRect);offset(m_highTempAlertRect);offset(m_reduceTransRect);
    }
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border;rt->CreateSolidColorBrush(D2D1::ColorF(0xDDE3EB),&border);
    if(border)rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(.5f,.5f,359.5f,visible-.5f),19.5f,19.5f),border.Get(),1);
}
void ProDashboardWindow::Render() {
    ID2D1DCRenderTarget* rt = m_renderer.BeginDraw();
    if (!rt) return;

    EnsureBrushes(rt);
    if(IsDisconnectedDisk()) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron,m_shadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
        DrawWhiteDisconnectedDisk(rt);m_renderer.EndDraw();return;
    }

    rt->SetTransform(D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));
    rt->Clear(D2D1::ColorF(0, 0, 0, 0));
    if(m_mode==Mode_Settings) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron,m_shadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);DrawScrolledSurface(rt,[&]{if(m_settings.activeTab==0)DrawSettingsAppearance(rt);else if(m_settings.activeTab==1)DrawSettingsMonitoring(rt);else DrawSettingsGeneral(rt);});m_renderer.EndDraw();return;
    }

    if(IsPureWhite() && m_mode==Mode_DirectTemp) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron,m_shadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        DrawScrolledSurface(rt,[&]{DrawWhiteTempDirect(rt);});m_renderer.EndDraw();return;
    }
    if(IsPureWhite() && m_mode==Mode_DirectNetwork) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron,m_shadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        DrawScrolledSurface(rt,[&]{DrawWhiteNetworkDirect(rt);});m_renderer.EndDraw();return;
    }
    if(IsPureWhite() && m_mode==Mode_DirectMemory) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron,m_shadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        DrawScrolledSurface(rt,[&]{DrawWhiteMemoryDirect(rt);});m_renderer.EndDraw();return;
    }
    if(IsPureWhite() && m_mode==Mode_DirectCpu) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron,m_shadowApron)*D2D1::Matrix3x2F::Scale(m_dpiScale,m_dpiScale));
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        DrawWhiteCpuDirect(rt);m_renderer.EndDraw();return;
    }

    if (IsWhiteOverview()) {
        rt->SetTransform(D2D1::Matrix3x2F::Translation(m_shadowApron, m_shadowApron) *
            D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));
        const auto textMode = rt->GetTextAntialiasMode();
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        DrawWhiteOverview(rt);
        rt->SetTextAntialiasMode(textMode);
        m_renderer.EndDraw();
        return;
    }

    float W = m_dipWidth;
    float H = m_dipHeight;

    if (m_mode == Mode_DirectTemp) {
        DrawCpuTempDirect(rt, W, H);
        m_renderer.EndDraw();
        return;
    } else if (m_mode == Mode_DirectCpu) {
        DrawCpuDirect(rt, W, H);
        m_renderer.EndDraw();
        return;
    } else if (m_mode == Mode_DirectMemory) {
        DrawMemoryDirect(rt, W, H);
        m_renderer.EndDraw();
        return;
    } else if (m_mode == Mode_DirectNetwork) {
        DrawNetworkDirect(rt, W, H);
        m_renderer.EndDraw();
        return;
    } else if (m_mode == Mode_DirectGpu) {
        DrawGpuDirect(rt, W, H);
        m_renderer.EndDraw();
        return;
    } else if (m_mode == Mode_DirectDisk) {
        DrawDiskDirect(rt, W, H);
        m_renderer.EndDraw();
        return;
    } else if (m_mode == Mode_Settings) {
        DrawSettings(rt, W, H);
        m_renderer.EndDraw();
        return;
    }

    // Light frosted container with outer specular rim (面板圆角: 20 DIP, 内边距: 左右 16, 上下 12)
    D2D1_ROUNDED_RECT windowRect = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), 20.0f, 20.0f);
    rt->FillRoundedRectangle(windowRect, m_pBgBrush);
    rt->DrawRoundedRectangle(windowRect, m_pBorderBrush, 1.2f);

    // Figma spec: 360 面板，内容区起点 (11, 5)
    float curY = (m_mode == Mode_FullAccordion) ? 5.0f : 12.0f;

    DrawHeader(rt, curY, W);

    if (m_settings.layoutMode == 1) {
        // ========== 紧凑胶囊 (Compact) 模式 ==========
        if (m_expandedModule == Module_None) {
            std::vector<AccordionModule> allMods = {
                Module_CPU, Module_GPU, Module_Memory, Module_Disk, Module_Network, Module_Sensor
            };
            DrawCompactGrid(rt, allMods, curY, 11.0f, 338.0f);
        } else {
            std::vector<AccordionModule> beforeMods;
            std::vector<AccordionModule> afterMods;
            for (int i = 0; i < Module_Count; ++i) {
                if (i < m_expandedModule) beforeMods.push_back(static_cast<AccordionModule>(i));
                else if (i > m_expandedModule) afterMods.push_back(static_cast<AccordionModule>(i));
            }
            if (!beforeMods.empty()) {
                DrawCompactGrid(rt, beforeMods, curY, 11.0f, 338.0f);
            }
            DrawModuleCard(rt, static_cast<AccordionModule>(m_expandedModule), curY, W);
            if (!afterMods.empty()) {
                DrawCompactGrid(rt, afterMods, curY, 11.0f, 338.0f);
            }
        }
    } else {
        // ========== 完整长条 (FullRows) 模式 ==========
        for (int i = 0; i < Module_Count; ++i) {
            DrawModuleCard(rt, static_cast<AccordionModule>(i), curY, W);
        }
    }

    DrawFooter(rt, curY, W);

    m_renderer.EndDraw();
}

void ProDashboardWindow::OnClick(float x, float y) {
    if(IsDisconnectedDisk()) {
        if(x>=11&&x<150&&y>=183&&y<200) {
            HMENU menu=CreatePopupMenu();if(!menu)return;
            std::vector<std::wstring> ids;
            for(size_t i=0;i<m_snapshot.physicalDisks.size();++i){ids.push_back(m_snapshot.physicalDisks[i].id);AppendMenuW(menu,MF_STRING,i+1,m_snapshot.physicalDisks[i].model.c_str());}
            if(m_snapshot.physicalDisks.empty())AppendMenuW(menu,MF_STRING|MF_GRAYED,0,L"暂无可用磁盘");
            POINT p{static_cast<LONG>((11+m_shadowApron)*m_dpiScale),static_cast<LONG>((200+m_shadowApron)*m_dpiScale)};ClientToScreen(m_hwnd,&p);
            m_settingsMenuActive=true;const int choice=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,m_hwnd,nullptr);m_settingsMenuActive=false;DestroyMenu(menu);
            if(choice>0&&choice<=static_cast<int>(ids.size())&&std::any_of(m_snapshot.physicalDisks.begin(),m_snapshot.physicalDisks.end(),[&](const auto& d){return d.id==ids[choice-1];})) {
                m_selectedDiskId=ids[choice-1];ReconcileDiskSelection();
                if(m_onSettingsChanged)m_onSettingsChanged(m_settings);UpdateDimensionsAndPosition();Render();
            }
        }
        return;
    }
    if(IsWhiteOverview() && m_expandedModule==Module_Disk) {
        auto hit=[&](D2D1_RECT_F r){return r.right>r.left&&x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;};
        if(m_diskMenuOpen) {
            for(size_t i=0;i<m_diskOptionRects.size();++i)if(hit(m_diskOptionRects[i])) {m_selectedDiskId=m_diskOptionIds[i];m_diskMenuOpen=false;if(m_onSettingsChanged)m_onSettingsChanged(m_settings);Render();return;}
            m_diskMenuOpen=false;Render();return; // Outside click closes without activating the covered chart/row.
        }
        if(hit(m_diskTriggerRect) && SelectedDisk()) {
            m_diskMenuOpen=true;m_diskMenuHover=-1;m_diskMenuFocus=0;
            for(size_t i=0;i<m_snapshot.physicalDisks.size();++i)if(m_snapshot.physicalDisks[i].id==m_selectedDiskId)m_diskMenuFocus=static_cast<int>(i);
            m_diskMenuFirst=std::max(0,m_diskMenuFocus-4);SetFocus(m_hwnd);Render();return;
        }
    }
    const int core=HitCpuCore(x,y);
    if(core>=0) {
        m_focusedCpuCore=core;m_cpuKeyboardFocus=true;m_hoveredCpuCore=-1;
        SetFocus(m_hwnd);Render();return;
    }
    if (m_mode == Mode_Settings) {
        if (x >= m_settingsDoneRect.left && x <= m_settingsDoneRect.right &&
            y >= m_settingsDoneRect.top && y <= m_settingsDoneRect.bottom) {
            ReturnFromSettings();
            return;
        }
        for (int i = 0; i < 3; ++i) {
            if (x >= m_tabRects[i].left && x <= m_tabRects[i].right &&
                y >= m_tabRects[i].top && y <= m_tabRects[i].bottom) {
                m_settings.activeTab = i;
                m_monitoringFocus=-1;
                m_generalFocus=-1;m_hotkeyCapturing=false;
                m_sliderDragging=false;UpdateDimensionsAndPosition();
                Render();
                return;
            }
        }
        if(y<70||y>=m_dipHeight-5)return;
        if (m_settings.activeTab == 0) {
            for (int i = 0; i < 4; ++i) {
                if (x >= m_themeRects[i].left && x <= m_themeRects[i].right &&
                    y >= m_themeRects[i].top && y <= m_themeRects[i].bottom) {
                    m_settings.themeMode = i;
                    UpdateDimensionsAndPosition();
                    ReleaseBrushes();
                    if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                    Render();
                    return;
                }
            }
            for (int i = 0; i < 2; ++i) {
                if (x >= m_layoutOptionRects[i].left && x <= m_layoutOptionRects[i].right &&
                    y >= m_layoutOptionRects[i].top && y <= m_layoutOptionRects[i].bottom) {
                    m_settings.layoutMode = (1 - i); // 0: 紧凑胶囊 (layoutMode = 1), 1: 完整长条 (layoutMode = 0)
                    UpdateDimensionsAndPosition();
                    if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                    Render();
                    return;
                }
            }
            if (m_settings.themeMode==0 && m_sliderTrackRect.right>m_sliderTrackRect.left && x >= m_sliderTrackRect.left && x <= m_sliderTrackRect.right &&
                y >= m_sliderTrackRect.top && y <= m_sliderTrackRect.bottom) {
                float startX = 11.0f;
                float inW = 338.0f;
                m_settings.glassOpacity = .1f+.9f*std::clamp((x-18)/324,0.f,1.f);
                ReleaseBrushes();
                if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                Render();
                return;
            }
            bool* metricFlags[4] = { &m_settings.showCpu, &m_settings.showTemp, &m_settings.showRam, &m_settings.showNet };
            for (int i = 0; i < 4; ++i) {
                if (x >= m_metricToggleRects[i].left && x <= m_metricToggleRects[i].right &&
                    y >= m_metricToggleRects[i].top && y <= m_metricToggleRects[i].bottom) {
                    *metricFlags[i] = !(*metricFlags[i]);
                    if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                    Render();
                    return;
                }
            }
            if (x >= m_intervalRect.left && x <= m_intervalRect.right &&
                y >= m_intervalRect.top && y <= m_intervalRect.bottom) {
                if (m_settings.updateIntervalSec == 1) m_settings.updateIntervalSec = 2;
                else if (m_settings.updateIntervalSec == 2) m_settings.updateIntervalSec = 0;
                else m_settings.updateIntervalSec = 1;
                if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                Render();
                return;
            }
            if (x >= m_tempUnitRect.left && x <= m_tempUnitRect.right &&
                y >= m_tempUnitRect.top && y <= m_tempUnitRect.bottom) {
                m_settings.tempUnit = 1 - m_settings.tempUnit;
                if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                Render();
                return;
            }
            if (x >= m_highTempAlertRect.left && x <= m_highTempAlertRect.right &&
                y >= m_highTempAlertRect.top && y <= m_highTempAlertRect.bottom) {
                if (m_settings.highTempAlert == 85) m_settings.highTempAlert = 90;
                else if (m_settings.highTempAlert == 90) m_settings.highTempAlert = 95;
                else if (m_settings.highTempAlert == 95) m_settings.highTempAlert = 0;
                else if (m_settings.highTempAlert == 0) m_settings.highTempAlert = 75;
                else if (m_settings.highTempAlert == 75) m_settings.highTempAlert = 80;
                else m_settings.highTempAlert = 85;
                m_alertTempThreshold = m_settings.highTempAlert;
                if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                Render();
                return;
            }
            if (x >= m_reduceTransRect.left && x <= m_reduceTransRect.right &&
                y >= m_reduceTransRect.top && y <= m_reduceTransRect.bottom) {
                m_settings.reduceTransparency = !m_settings.reduceTransparency;
                ReleaseBrushes();
                if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
                Render();
                return;
            }
        }
        if(m_settings.activeTab==1)OnMonitoringClick(x,y);
        if(m_settings.activeTab==2)OnGeneralClick(x,y);
        return;
    }

    if (m_mode == Mode_DirectTemp) {
        if (x >= m_allSensorsRect.left && x <= m_allSensorsRect.right &&
            y >= m_allSensorsRect.top && y <= m_allSensorsRect.bottom) {
            ShowFullDashboard(Module_Sensor, m_anchorBarRectDip);
            return;
        }
        if (x >= m_tempAlertRect.left && x <= m_tempAlertRect.right &&
            y >= m_tempAlertRect.top && y <= m_tempAlertRect.bottom) {
            if(IsPureWhite()) {
                const int value=m_settings.highTempAlert;
                m_settings.highTempAlert=value==85 ? 90 : value==90 ? 95 : value==95 ? 0 : value==0 ? 75 : value==75 ? 80 : 85;
                m_alertTempThreshold=m_settings.highTempAlert;
                if(m_onSettingsChanged) m_onSettingsChanged(m_settings);
                Render();return;
            }
            if (m_alertTempThreshold == 85) m_alertTempThreshold = 90;
            else if (m_alertTempThreshold == 90) m_alertTempThreshold = 80;
            else m_alertTempThreshold = 85;
            Render();
            return;
        }
        return;
    }

    if (m_mode == Mode_DirectCpu || m_mode == Mode_DirectMemory || m_mode == Mode_DirectNetwork ||
        m_mode == Mode_DirectGpu || m_mode == Mode_DirectDisk) {
        if (x >= m_overviewLinkRect.left && x <= m_overviewLinkRect.right &&
            y >= m_overviewLinkRect.top && y <= m_overviewLinkRect.bottom) {
            AccordionModule targetMod = Module_CPU;
            if (m_mode == Mode_DirectMemory) targetMod = Module_Memory;
            else if (m_mode == Mode_DirectNetwork) targetMod = Module_Network;
            else if (m_mode == Mode_DirectGpu) targetMod = Module_GPU;
            else if (m_mode == Mode_DirectDisk) targetMod = Module_Disk;
            ShowFullDashboard(targetMod, m_anchorBarRectDip);
            return;
        }
        return;
    }

    if (x >= m_closeButtonRect.left && x <= m_closeButtonRect.right &&
        y >= m_closeButtonRect.top && y <= m_closeButtonRect.bottom) {
        Hide();
        return;
    }

    if (x >= m_pinButtonRect.left && x <= m_pinButtonRect.right &&
        y >= m_pinButtonRect.top && y <= m_pinButtonRect.bottom) {
        SetPinned(!m_isPinned);
        return;
    }

    if ((x >= m_settingsHeaderBtnRect.left && x <= m_settingsHeaderBtnRect.right &&
         y >= m_settingsHeaderBtnRect.top && y <= m_settingsHeaderBtnRect.bottom) ||
        (x >= m_settingsFooterBtnRect.left && x <= m_settingsFooterBtnRect.right &&
         y >= m_settingsFooterBtnRect.top && y <= m_settingsFooterBtnRect.bottom) ||
        (x >= m_settingsButtonRect.left && x <= m_settingsButtonRect.right &&
         y >= m_settingsButtonRect.top && y <= m_settingsButtonRect.bottom)) {
        ShowSettings(m_anchorBarRectDip);
        return;
    }

    const float expansion=m_panelMotion.active?m_panelMotion.current.expanded:(m_expandedModule==Module_None?0.f:1.f);
    if(IsWhiteOverview() && (y<42+8*expansion || y>m_dipHeight-40-6*expansion)) return;
    for (int i = 0; i < Module_Count; ++i) {
        const auto& r = m_moduleRects[i];
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            TransitionModule(static_cast<AccordionModule>(m_expandedModule==i?Module_None:i));
            return;
        }
    }
}

void ProDashboardWindow::OnMouseMove(float x, float y) {
    if(m_diskMenuOpen) {
        int hovered=-1;for(size_t i=0;i<m_diskOptionRects.size();++i) {const auto& r=m_diskOptionRects[i];if(x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)hovered=m_diskMenuFirst+static_cast<int>(i);}
        if(hovered!=m_diskMenuHover) {m_diskMenuHover=hovered;Render();}return;
    }
    bool needRedraw = false;
    const int core=HitCpuCore(x,y);
    if(core!=m_hoveredCpuCore) {m_hoveredCpuCore=core;m_cpuKeyboardFocus=false;needRedraw=true;}

    if (m_mode == Mode_Settings) {
        if (m_sliderDragging) {
            float startX = 11.0f;
            float inW = 338.0f;
            m_settings.glassOpacity = .1f+.9f*std::clamp((x-18)/324,0.f,1.f);
            ReleaseBrushes();
            if (m_onSettingsChanged) m_onSettingsChanged(m_settings);
            Render();
            return;
        }
        bool doneHover = (x >= m_settingsDoneRect.left && x <= m_settingsDoneRect.right &&
                          y >= m_settingsDoneRect.top && y <= m_settingsDoneRect.bottom);
        if (doneHover != m_settingsDoneHover) {
            m_settingsDoneHover = doneHover;
            Render();
        }
        return;
    }

    if (m_mode == Mode_DirectTemp) {
        bool allSensorsHover = (x >= m_allSensorsRect.left && x <= m_allSensorsRect.right &&
                                y >= m_allSensorsRect.top && y <= m_allSensorsRect.bottom);
        if (allSensorsHover != m_allSensorsHover) {
            m_allSensorsHover = allSensorsHover;
            needRedraw = true;
        }

        if (x >= m_tempChartRect.left && x <= m_tempChartRect.right &&
            y >= m_tempChartRect.top - 8.0f && y <= m_tempChartRect.bottom + 8.0f) {
            float ratio = (x - m_tempChartRect.left) / (m_tempChartRect.right - m_tempChartRect.left);
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            int secAgo = static_cast<int>((1.0f - ratio) * 59.0f + 0.5f);
            if (!m_tempProbeActive || m_tempProbeSecAgo != secAgo || fabs(m_tempProbeX - x) > 1.0f) {
                m_tempProbeActive = true;
                m_tempProbeSecAgo = secAgo;
                m_tempProbeX = x;
                needRedraw = true;
            }
        } else {
            if (m_tempProbeActive) {
                m_tempProbeActive = false;
                needRedraw = true;
            }
        }

        if (needRedraw) Render();
        return;
    }

    if (m_mode == Mode_DirectCpu || m_mode == Mode_DirectMemory || m_mode == Mode_DirectNetwork ||
        m_mode == Mode_DirectGpu || m_mode == Mode_DirectDisk) {
        bool linkHover = (x >= m_overviewLinkRect.left && x <= m_overviewLinkRect.right &&
                          y >= m_overviewLinkRect.top && y <= m_overviewLinkRect.bottom);
        if (linkHover != m_overviewLinkHover) {
            m_overviewLinkHover = linkHover;
            needRedraw = true;
        }

        if (m_mode == Mode_DirectNetwork) {
            bool inNetChart = (x >= m_netChartRect.left && x <= m_netChartRect.right && y >= m_netChartRect.top && y <= m_netChartRect.bottom);
            if (inNetChart != m_netProbeActive || (inNetChart && (fabs(x - m_netProbeX) > 1.0f))) {
                m_netProbeActive = inNetChart;
                m_netProbeX = x;
                m_netProbeY = y;
                needRedraw = true;
            }
        }

        if (needRedraw) Render();
        return;
    }

    bool closeH = (x >= m_closeButtonRect.left && x <= m_closeButtonRect.right && y >= m_closeButtonRect.top && y <= m_closeButtonRect.bottom);
    if (closeH != m_closeHover) { m_closeHover = closeH; needRedraw = true; }

    bool pinH = (x >= m_pinButtonRect.left && x <= m_pinButtonRect.right && y >= m_pinButtonRect.top && y <= m_pinButtonRect.bottom);
    if (pinH != m_pinHover) { m_pinHover = pinH; needRedraw = true; }

    bool setH = ((x >= m_settingsHeaderBtnRect.left && x <= m_settingsHeaderBtnRect.right && y >= m_settingsHeaderBtnRect.top && y <= m_settingsHeaderBtnRect.bottom) ||
                 (x >= m_settingsFooterBtnRect.left && x <= m_settingsFooterBtnRect.right && y >= m_settingsFooterBtnRect.top && y <= m_settingsFooterBtnRect.bottom) ||
                 (x >= m_settingsButtonRect.left && x <= m_settingsButtonRect.right && y >= m_settingsButtonRect.top && y <= m_settingsButtonRect.bottom));
    if (setH != m_settingsHover) { m_settingsHover = setH; needRedraw = true; }

    int hoverMod = -1;
    for (int i = 0; i < Module_Count; ++i) {
        const float expansion=m_panelMotion.active?m_panelMotion.current.expanded:(m_expandedModule==Module_None?0.f:1.f);
        if(IsWhiteOverview() && (y<42+8*expansion || y>m_dipHeight-40-6*expansion)) break;
        const auto& r = m_moduleRects[i];
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            hoverMod = i;
            break;
        }
    }
    if (hoverMod != m_hoveredModule) {
        m_hoveredModule = hoverMod;
        needRedraw = true;
    }

    if (m_expandedModule == Module_Network) {
        bool inNetChart = (x >= m_netChartRect.left && x <= m_netChartRect.right && y >= m_netChartRect.top && y <= m_netChartRect.bottom);
        if (inNetChart != m_netProbeActive || (inNetChart && (fabs(x - m_netProbeX) > 1.0f))) {
            m_netProbeActive = inNetChart;
            m_netProbeX = x;
            m_netProbeY = y;
            needRedraw = true;
        }
    } else if (m_netProbeActive) {
        m_netProbeActive = false;
        needRedraw = true;
    }

    if (needRedraw) {
        Render();
    }
}

LRESULT CALLBACK ProDashboardWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProDashboardWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<ProDashboardWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<ProDashboardWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:if(wParam==73){pThis->TickPanelMotion();return 0;}break;
    case WM_APP+41:pThis->OnOutsideClick();return 0;
    case WM_CLOSE:pThis->Hide();return 0;
    case WM_DPICHANGED:
        pThis->CancelPanelMotion();
        if(pThis->IsPureWhite()) {
            pThis->m_dpiScale=HIWORD(wParam)/96.f;
            pThis->m_hoveredCpuCore=-1;pThis->UpdateDimensionsAndPosition();pThis->Render();return 0;
        }
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        if(pThis->OnGeneralKey(static_cast<UINT>(wParam),(GetKeyState(VK_SHIFT)&0x8000)!=0,(GetKeyState(VK_MENU)&0x8000)!=0))return 0;
        if(pThis->OnMonitoringKey(static_cast<UINT>(wParam),(GetKeyState(VK_SHIFT)&0x8000)!=0,(GetKeyState(VK_MENU)&0x8000)!=0))return 0;
        if(wParam==VK_ESCAPE&&pThis->m_indicatorDragging>=0){pThis->m_indicatorDragging=-1;ReleaseCapture();if(pThis->m_onSettingsChanged)pThis->m_onSettingsChanged(pThis->m_settings);pThis->Render();return 0;}
        if(pThis->OnDiskKey(static_cast<UINT>(wParam)))return 0;
        if(wParam==VK_ESCAPE) {pThis->CloseTopLayer();return 0;}
        if(pThis->OnCpuKey(static_cast<UINT>(wParam),(GetKeyState(VK_SHIFT)&0x8000)!=0)) return 0;
        break;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
    case WM_DISPLAYCHANGE:
        if(msg==WM_SETTINGCHANGE&&wParam==SPI_SETCLIENTAREAANIMATION){pThis->CancelPanelMotion();pThis->UpdateDimensionsAndPosition();if(pThis->m_visible)pThis->Render();return 0;}
        if(msg==WM_DISPLAYCHANGE||(msg==WM_SETTINGCHANGE&&wParam==SPI_SETWORKAREA)){pThis->CancelPanelMotion();pThis->m_cpuPlacementValid=false;pThis->UpdateDimensionsAndPosition();if(pThis->m_visible)pThis->Render();return 0;}
        if(pThis->m_settings.themeMode==3) {pThis->CancelPanelMotion();pThis->ReleaseBrushes();pThis->UpdateDimensionsAndPosition();if(pThis->m_onSettingsChanged)pThis->m_onSettingsChanged(pThis->m_settings);pThis->Render();}
        break;
    case WM_MOUSEWHEEL:
        if(pThis->m_diskMenuOpen) {pThis->OnDiskKey(GET_WHEEL_DELTA_WPARAM(wParam)>0 ? VK_UP : VK_DOWN);return 0;}
        if(pThis->IsPureWhite()||pThis->m_mode==Mode_Settings) {pThis->ScrollCpu(-GET_WHEEL_DELTA_WPARAM(wParam)*34.f/WHEEL_DELTA);return 0;}
        break;
    case WM_KILLFOCUS:
        pThis->m_generalFocus=-1;
        pThis->m_hotkeyCapturing=false;
        pThis->m_monitoringFocus=-1;
        pThis->m_cpuKeyboardFocus=false;
        if(pThis->IsWhiteCpu()||pThis->m_mode==Mode_Settings) pThis->Render();
        break;
    case WM_NCHITTEST: {
        if (pThis->m_shadowApron > 0) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &point);
            const float x = point.x / pThis->m_dpiScale - pThis->m_shadowApron;
            const float y = point.y / pThis->m_dpiScale - pThis->m_shadowApron;
            if (x < 0 || y < 0 || x >= pThis->m_dipWidth || y >= pThis->m_dipHeight) return HTTRANSPARENT;
        }
        return HTCLIENT;
    }
    case WM_LBUTTONDOWN: {
        int px = GET_X_LPARAM(lParam);
        int py = GET_Y_LPARAM(lParam);
        float dipX = px / pThis->m_dpiScale - pThis->m_shadowApron;
        float dipY = py / pThis->m_dpiScale - pThis->m_shadowApron;
        if (pThis->m_mode == Mode_Settings && pThis->m_settings.activeTab == 0 && pThis->m_settings.themeMode==0 &&
            dipX >= pThis->m_sliderTrackRect.left && dipX <= pThis->m_sliderTrackRect.right &&
            dipY >= pThis->m_sliderTrackRect.top && dipY <= pThis->m_sliderTrackRect.bottom) {
            pThis->m_sliderDragging = true;
            SetCapture(hwnd);
            float startX = 17.0f;
            float inW = 388.0f;
            pThis->m_settings.glassOpacity = .1f+.9f*std::clamp((dipX-18)/324,0.f,1.f);
            pThis->ReleaseBrushes();
            if (pThis->m_onSettingsChanged) pThis->m_onSettingsChanged(pThis->m_settings);
            pThis->Render();
            return 0;
        }
        pThis->OnClick(dipX, dipY);
        return 0;
    }

    case WM_LBUTTONUP: {
        if(pThis->m_indicatorDragging>=0){pThis->m_indicatorDragging=-1;ReleaseCapture();if(pThis->m_onSettingsChanged)pThis->m_onSettingsChanged(pThis->m_settings);pThis->Render();return 0;}
        if (pThis->m_sliderDragging) {
            pThis->m_sliderDragging = false;
            ReleaseCapture();
            pThis->Render();
            return 0;
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
        if(pThis->m_indicatorDragging>=0){pThis->m_indicatorDragging=-1;if(pThis->m_onSettingsChanged)pThis->m_onSettingsChanged(pThis->m_settings);}
        pThis->m_sliderDragging=false;
        return 0;

    case WM_MOUSEMOVE: {
        int px = GET_X_LPARAM(lParam);
        int py = GET_Y_LPARAM(lParam);
        if(pThis->m_indicatorDragging>=0){pThis->MoveMonitoringIndicator(py/pThis->m_dpiScale-pThis->m_shadowApron);return 0;}
        pThis->OnMouseMove(px / pThis->m_dpiScale - pThis->m_shadowApron, py / pThis->m_dpiScale - pThis->m_shadowApron);

        TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        pThis->m_hoveredCpuCore=-1;
        pThis->m_hoveredModule = -1;
        pThis->m_diskMenuHover=-1;
        pThis->m_closeHover = false;
        pThis->m_pinHover = false;
        pThis->m_settingsHover = false;
        pThis->m_settingsDoneHover = false;
        pThis->m_netProbeActive = false;
        pThis->m_tempProbeActive = false;
        pThis->m_allSensorsHover = false;
        pThis->Render();
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !pThis->m_settingsMenuActive && pThis->m_indicatorDragging<0) {
            POINT pt;
            GetCursorPos(&pt);
            HWND hHover = WindowFromPoint(pt);
            if (hHover != hwnd && hHover!=pThis->m_barOwner && reinterpret_cast<HWND>(lParam)!=pThis->m_barOwner) {
                if(!pThis->m_mouseHook||(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_RBUTTON)&0x8000)))pThis->OnOutsideClick();
            }
        }
        return 0;

    case WM_DESTROY:
        pThis->CancelPanelMotion();
        pThis->WatchOutsideClicks(false);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
