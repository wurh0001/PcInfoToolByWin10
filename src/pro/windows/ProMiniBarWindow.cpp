#include "pro/windows/ProMiniBarWindow.h"
#include "pro/windows/ProDashboardWindow.h"
#include "pro/windows/ProMiniBarTooltipWindow.h"
#include "ui/platform/DpiHelper.h"
#include "ui/platform/AcrylicHelper.h"
#include "core/config/ConfigManager.h"
#include <windowsx.h>
#include <cwchar>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <dwmapi.h>

static const wchar_t* kProMiniBarClassName = L"PcInfoToolProMiniBarClass";

ProMiniBarWindow::ProMiniBarWindow() {
    m_pDashboard = new ProDashboardWindow();
    m_pTooltip = new ProMiniBarTooltipWindow();

    if (m_pDashboard) {
        const auto appearance=ConfigManager::LoadProAppearance();
        m_pDashboard->SetAppearance(appearance.themeMode,appearance.layoutMode,appearance.glassOpacity);
        const auto monitoring=ConfigManager::LoadProMonitoring();
        m_pDashboard->SetMonitoring(monitoring.visible,monitoring.order,monitoring.interval,monitoring.unit,monitoring.alert,monitoring.diskId);
        m_pDashboard->SetGeneral(ConfigManager::LoadProGeneral());
        m_pDashboard->SetOnGeneralChange([this](ProGeneralSettings& value,std::wstring& error){return ApplyGeneralChange(value,error);});
        m_pDashboard->SetOnSettingsChanged([this](const ProDashboardWindow::SettingsState& s) {
            const bool appearanceSaved=ConfigManager::SaveProAppearance({s.themeMode,s.layoutMode,s.glassOpacity});
            const bool monitoringSaved=ConfigManager::SaveProMonitoring({{s.showCpu,s.showTemp,s.showRam,s.showNet},s.indicatorOrder,s.updateIntervalSec,s.tempUnit,s.highTempAlert,m_pDashboard->GetSelectedDiskId()});
            const bool generalSaved=ConfigManager::SaveProGeneral(s.general);
            m_pDashboard->SetAppearanceSaveFailed(!appearanceSaved||!monitoringSaved||!generalSaved);
            if(m_hwnd)SetTimer(m_hwnd,1,s.updateIntervalSec==0?500:s.updateIntervalSec*1000,nullptr);
            m_hoveredSegment=-1;m_chevronHover=false;if(m_pTooltip)m_pTooltip->Hide();
            ReleaseBrushes();
            if (m_pTooltip) {
                m_pTooltip->SetPureWhite(m_pDashboard->IsPureWhite());
                m_pTooltip->SetOpacity(m_pDashboard->GetTooltipBgAlpha());
            }
            Render();
            PollFullscreen();
        });
    }
}

ProMiniBarWindow::~ProMiniBarWindow() {
    SavePosition();m_hotkeyBinding.Clear();
    m_collector.Stop();
    ReleaseBrushes();
    if (m_pTooltip) {
        delete m_pTooltip;
        m_pTooltip = nullptr;
    }
    if (m_pDashboard) {
        delete m_pDashboard;
        m_pDashboard = nullptr;
    }
}

void ProMiniBarWindow::ReleaseBrushes() {
    auto SafeRelease = [](auto** pp) {
        if (*pp) { (*pp)->Release(); *pp = nullptr; }
    };
    SafeRelease(&m_pBgBrush);
    SafeRelease(&m_pBorderBrush);
    SafeRelease(&m_pSepBrush);
    SafeRelease(&m_pTextPrimary);
    SafeRelease(&m_pTextSecondary);
    SafeRelease(&m_pTextMuted);
    SafeRelease(&m_pCpuBlue);
    SafeRelease(&m_pRamAmber);
    SafeRelease(&m_pNetGreen);
    SafeRelease(&m_pNetSky);
    SafeRelease(&m_pHoverBrush);
    SafeRelease(&m_pHoverPillBg);
    SafeRelease(&m_pHoverPillBorder);
}

void ProMiniBarWindow::EnsureBrushes(ID2D1DCRenderTarget* rt) {
    if (m_pBgBrush) return;

    // Theme palette matching Figma tokens
    bool isPureWhite = m_pDashboard ? m_pDashboard->IsPureWhite() : false;
    if (isPureWhite) {
        // 纯白主题: 主背景 #FFFFFF alpha 1.0, 描边/分割线 #DDE3EB 100%
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_pBgBrush);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &m_pBorderBrush); // #DDE3EB
        rt->CreateSolidColorBrush(D2D1::ColorF(0.867f, 0.890f, 0.922f, 1.0f), &m_pSepBrush);    // #DDE3EB
    } else {
        float bgAlpha = m_pDashboard ? m_pDashboard->GetMiniBarBgAlpha() : 0.44f;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.929f, 0.945f, 0.973f, bgAlpha), &m_pBgBrush);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.85f), &m_pBorderBrush);
        rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.75f), &m_pSepBrush);
    }

    rt->CreateSolidColorBrush(D2D1::ColorF(0.098f, 0.153f, 0.224f, 1.0f), &m_pTextPrimary);    // #192739
    rt->CreateSolidColorBrush(D2D1::ColorF(0.325f, 0.396f, 0.478f, 1.0f), &m_pTextSecondary);  // #53657A
    rt->CreateSolidColorBrush(D2D1::ColorF(0.392f, 0.455f, 0.545f, 1.0f), &m_pTextMuted);      // #64748B

    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pCpuBlue);        // #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f), &m_pRamAmber);       // #A87517
    rt->CreateSolidColorBrush(D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f), &m_pNetGreen);       // #138873
    rt->CreateSolidColorBrush(D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f), &m_pNetSky);         // #266BCE
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.60f), &m_pHoverBrush);

    // Segment hover white pill
    rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &m_pHoverPillBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.80f), &m_pHoverPillBorder);
}

RECT ProMiniBarWindow::GetWindowRectDip() const {
    RECT rc = { 0, 0, 0, 0 };
    if (m_hwnd) {
        GetWindowRect(m_hwnd, &rc);
        rc.left = static_cast<LONG>(rc.left / m_dpiScale + kShadowApron);
        rc.top = static_cast<LONG>(rc.top / m_dpiScale + kShadowApron);
        rc.right = rc.left + static_cast<LONG>(m_dipWidth);
        rc.bottom = rc.top + static_cast<LONG>(m_dipHeight);
    }
    return rc;
}

bool ProMiniBarWindow::Create() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = kProMiniBarClassName;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hDesk = GetDesktopWindow();
    m_dpiScale = DpiHelper::GetScaleForWindow(hDesk);
    m_pixelWidth = static_cast<int>((m_dipWidth + 2 * kShadowApron) * m_dpiScale + 0.5f);
    m_pixelHeight = static_cast<int>((m_dipHeight + 2 * kShadowApron) * m_dpiScale + 0.5f);

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenW = workArea.right - workArea.left;
    int posX = workArea.left + (screenW - m_pixelWidth) / 2;
    int posY = workArea.top + static_cast<int>((12 - kShadowApron) * m_dpiScale);
    const auto& general=m_pDashboard->GetSettings().general;
    if(general.rememberPosition&&general.positionValid){
        RECT saved{general.positionX,general.positionY,general.positionX+static_cast<LONG>(380.f*general.savedDpi/96),general.positionY+static_cast<LONG>(32.f*general.savedDpi/96)};
        HMONITOR monitor=MonitorFromRect(&saved,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(MONITORINFO)};
        if(GetMonitorInfoW(monitor,&info)){
            HMODULE module=LoadLibraryExW(L"shcore.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(module){using GetMonitorDpi=HRESULT(WINAPI*)(HMONITOR,int,UINT*,UINT*);auto fn=reinterpret_cast<GetMonitorDpi>(GetProcAddress(module,"GetDpiForMonitor"));UINT x=96,y=96;if(fn&&SUCCEEDED(fn(monitor,0,&x,&y)))m_dpiScale=x/96.f;FreeLibrary(module);}
            m_pixelWidth=static_cast<int>(std::lround((380+2*kShadowApron)*m_dpiScale));m_pixelHeight=static_cast<int>(std::lround((32+2*kShadowApron)*m_dpiScale));
            const auto point=ProWindowBehavior::ClampVisiblePosition({general.positionX,general.positionY},{static_cast<LONG>(std::lround(380*m_dpiScale)),static_cast<LONG>(std::lround(32*m_dpiScale))},info.rcWork,static_cast<int>(8*m_dpiScale));
            posX=point.x-static_cast<int>(std::lround(kShadowApron*m_dpiScale));posY=point.y-static_cast<int>(std::lround(kShadowApron*m_dpiScale));
        }
    }

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | (general.alwaysOnTop?WS_EX_TOPMOST:0),
        kProMiniBarClassName,
        L"PcInfoTool-Pro MiniBar",
        WS_POPUP,
        posX, posY, m_pixelWidth, m_pixelHeight,
        NULL, NULL, GetModuleHandleW(NULL), this
    );

    if (!m_hwnd) return false;

    if (!m_renderer.Initialize(m_hwnd, m_pixelWidth, m_pixelHeight)) {
        return false;
    }

    if (m_pDashboard) {
        m_pDashboard->Create();
        m_pDashboard->SetBarOwner(m_hwnd);m_pDashboard->SetAnchorDpi(m_dpiScale);
    }
    if (m_pTooltip) {
        m_pTooltip->Create(m_hwnd);
        m_pTooltip->SetAlwaysOnTop(general.alwaysOnTop);
        if (m_pDashboard) {
            m_pTooltip->SetPureWhite(m_pDashboard->IsPureWhite());
            m_pTooltip->SetOpacity(m_pDashboard->GetTooltipBgAlpha());
        }
    }

    m_collector.SetGpuEnabled(true);
    m_collector.Start();
    SetTimer(m_hwnd,1,m_pDashboard->GetSettings().updateIntervalSec==0?500:m_pDashboard->GetSettings().updateIntervalSec*1000,nullptr);
    SetTimer(m_hwnd,3,250,nullptr);
    std::wstring hotkeyError;if(!m_hotkeyBinding.Set(m_hwnd,general.hotkeyModifiers,general.hotkeyKey,hotkeyError))m_pDashboard->SetGeneralError(hotkeyError);

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    OnTimer();

    if (m_pDashboard) {
        m_pDashboard->ShowNear(GetWindowRectDip());
    }
    m_positionDirty=true;
    PollFullscreen();

    return true;
}

bool ProMiniBarWindow::ApplyGeneralChange(ProGeneralSettings& value,std::wstring& error) {
    const auto& old=m_pDashboard->GetSettings().general;
    if(value.hotkeyKey!=old.hotkeyKey||value.hotkeyModifiers!=old.hotkeyModifiers){if(!m_hotkeyBinding.Set(m_hwnd,value.hotkeyModifiers,value.hotkeyKey,error))return false;}
    if(value.autoStart!=old.autoStart&&!ConfigManager::SetProAutoStart(value.autoStart)){error=L"开机启动设置失败，请检查系统权限。";return false;}
    if(value.alwaysOnTop!=old.alwaysOnTop&&m_hwnd){
        HWND z=value.alwaysOnTop?HWND_TOPMOST:HWND_NOTOPMOST;
        const bool barOk=SetWindowPos(m_hwnd,z,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=0;
        const auto panel=m_pDashboard->GetHwnd();const bool panelOk=!panel||SetWindowPos(panel,z,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=0;
        if(!barOk||!panelOk){HWND previous=old.alwaysOnTop?HWND_TOPMOST:HWND_NOTOPMOST;SetWindowPos(m_hwnd,previous,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);if(panel)SetWindowPos(panel,previous,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);error=L"窗口置顶设置失败，请重试。";return false;}
        if(m_pTooltip)m_pTooltip->SetAlwaysOnTop(value.alwaysOnTop);
    }
    if(value.rememberPosition){CapturePosition(value);}else value.positionValid=false;
    error.clear();return true;
}
void ProMiniBarWindow::CapturePosition(ProGeneralSettings& value) const {
    if(!m_hwnd)return;RECT rect{};if(!GetWindowRect(m_hwnd,&rect))return;
    value.positionX=rect.left+static_cast<int>(std::lround(kShadowApron*m_dpiScale));value.positionY=rect.top+static_cast<int>(std::lround(kShadowApron*m_dpiScale));value.savedDpi=static_cast<int>(std::lround(m_dpiScale*96));value.positionValid=true;
}
void ProMiniBarWindow::SavePosition() {
    if(!m_hwnd||!m_pDashboard)return;auto value=m_pDashboard->GetSettings().general;
    if(!value.rememberPosition||!m_positionDirty)return;CapturePosition(value);m_pDashboard->SetGeneral(value);
    const bool saved=ConfigManager::SaveProGeneral(value);m_pDashboard->SetAppearanceSaveFailed(!saved);m_positionDirty=!saved;
}
void ProMiniBarWindow::ApplyVisibility() {
    if(!m_hwnd)return;const bool show=!m_userHidden&&!m_fullscreenHidden;
    if(!show){
        if(IsWindowVisible(m_hwnd)){if(m_pDashboard&&m_pDashboard->IsVisible()){m_restoreDashboard=m_fullscreenHidden&&!m_userHidden;m_pDashboard->Hide();}if(m_pTooltip)m_pTooltip->Hide();KillTimer(m_hwnd,2);ShowWindow(m_hwnd,SW_HIDE);}
    }else if(!IsWindowVisible(m_hwnd)){
        ShowWindow(m_hwnd,SW_SHOWNOACTIVATE);
        if(m_restoreDashboard&&m_pDashboard){m_pDashboard->ShowDirectModule(m_pDashboard->GetMode(),GetWindowRectDip());}
        m_restoreDashboard=false;
    }
}
void ProMiniBarWindow::PollFullscreen() {
    if(!m_hwnd||!m_pDashboard)return;
    const HWND foreground=GetForegroundWindow();if(foreground!=m_fullscreenOverride)m_fullscreenOverride=nullptr;
    bool fullscreen=false;
    if(m_pDashboard->GetSettings().general.hideInFullscreen&&foreground&&foreground!=m_fullscreenOverride&&IsWindowVisible(foreground)&&!IsIconic(foreground)){
        DWORD process=0;GetWindowThreadProcessId(foreground,&process);wchar_t name[128];GetClassNameW(foreground,name,128);
        const bool shell=wcscmp(name,L"Progman")==0||wcscmp(name,L"WorkerW")==0||wcscmp(name,L"Shell_TrayWnd")==0||wcscmp(name,L"Shell_SecondaryTrayWnd")==0;
        if(process!=GetCurrentProcessId()&&!shell){RECT bar{},window{};GetWindowRect(m_hwnd,&bar);HMONITOR monitor=MonitorFromRect(&bar,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(MONITORINFO)};
            if(GetMonitorInfoW(monitor,&info)&&GetWindowRect(foreground,&window)){
                DwmGetWindowAttribute(foreground,DWMWA_EXTENDED_FRAME_BOUNDS,&window,sizeof(window));
                // Ordinary maximized captioned windows aren't fullscreen, even with an auto-hidden taskbar.
                const bool maximized=IsZoomed(foreground)&&(GetWindowLongPtrW(foreground,GWL_STYLE)&WS_CAPTION);
                fullscreen=!maximized&&ProWindowBehavior::CoversMonitor(window,info.rcMonitor);
            }
        }
    }
    m_fullscreenHidden=fullscreen;ApplyVisibility();
}
void ProMiniBarWindow::ToggleAppVisibility() {
    if(!m_hwnd)return;
    if(m_userHidden||m_fullscreenHidden){m_userHidden=false;m_fullscreenOverride=GetForegroundWindow();m_fullscreenHidden=false;m_restoreDashboard=false;}
    else {m_userHidden=true;m_restoreDashboard=false;}
    ApplyVisibility();
}

void ProMiniBarWindow::Run() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void ProMiniBarWindow::OnTimer() {
    m_snapshot = m_collector.GetLatestSnapshot();
    if (m_pDashboard && m_pDashboard->IsVisible()) {
        m_pDashboard->UpdateSnapshot(m_snapshot);
    }
    if (m_pTooltip && m_pTooltip->IsVisible() && m_hoveredSegment >= 0 && m_hoveredSegment <= 3) {
        ShowHoverPeek();
    }
    Render();
}

void ProMiniBarWindow::ShowHoverPeek() {
    if (!m_pTooltip) return;
    if (m_isDragging || m_hoveredSegment < 0 || m_hoveredSegment > 3 || !m_pDashboard || m_pDashboard->IsVisible()) {
        m_pTooltip->Hide(); return;
    }
    RECT bar{}; GetWindowRect(m_hwnd, &bar);
    bar.left += static_cast<LONG>(std::lround(kShadowApron * m_dpiScale));
    bar.top += static_cast<LONG>(std::lround(kShadowApron * m_dpiScale));
    bar.right = bar.left + static_cast<LONG>(std::lround(m_dipWidth * m_dpiScale));
    bar.bottom = bar.top + static_cast<LONG>(std::lround(m_dipHeight * m_dpiScale));
    m_pTooltip->SetTempUnit(m_pDashboard->GetSettings().tempUnit);
    m_pTooltip->Show(m_hoveredSegment, bar, m_snapshot, m_dpiScale);
}

void ProMiniBarWindow::ToggleDashboard() {
    if (m_pTooltip) {
        m_pTooltip->Hide();
    }
    if (!m_pDashboard) return;
    if (m_pDashboard->IsVisible() && m_pDashboard->GetMode() == ProDashboardWindow::Mode_FullAccordion) {
        m_pDashboard->Hide();
    } else {
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowFullDashboard(m_pDashboard->GetExpandedModule(), GetWindowRectDip());
    }
    Render();
}

void ProMiniBarWindow::OnSegmentClick(int segmentIndex) {
    if (m_pTooltip) {
        m_pTooltip->Hide();
    }
    if (!m_pDashboard) return;

    switch (segmentIndex) {
    case 0: // CPU -> Direct entry into dedicated CPU card (ONLY CPU info, no other modules!)
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectCpu, GetWindowRectDip());
        break;
    case 1: // Sensor / Temp -> Direct entry into "02 CPU 温度" card matching Figma
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectTemp, GetWindowRectDip());
        break;
    case 2: // RAM -> Direct entry into dedicated Memory card
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectMemory, GetWindowRectDip());
        break;
    case 3: // Network -> Direct entry into dedicated Network card
        m_pDashboard->UpdateSnapshot(m_snapshot);
        m_pDashboard->ShowDirectModule(ProDashboardWindow::Mode_DirectNetwork, GetWindowRectDip());
        break;
    case 4: // Chevron toggle button -> Full multi-module dashboard overview!
    default:
        ToggleDashboard();
        break;
    }
    Render();
}

void ProMiniBarWindow::DrawCpuIcon(ID2D1DCRenderTarget* rt, float x, float y, ID2D1Brush* brush) {
    // 🌡️ Thermometer vector matching Figma
    float cx = x + 3.5f;
    float topY = y + 1.0f;
    float stemBottomY = y + 8.5f;
    float bulbCy = y + 10.5f;
    float bulbR = 3.2f;

    // Outer glass stem (rounded rectangle)
    D2D1_ROUNDED_RECT stemOutline = D2D1::RoundedRect(D2D1::RectF(cx - 1.6f, topY, cx + 1.6f, stemBottomY + 0.5f), 1.6f, 1.6f);
    rt->DrawRoundedRectangle(stemOutline, brush, 1.0f);

    // Bulb outline & fill
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, bulbCy), bulbR, bulbR), brush);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, bulbCy), bulbR, bulbR), brush, 1.0f);

    // Inner mercury column
    rt->DrawLine(D2D1::Point2F(cx, topY + 2.5f), D2D1::Point2F(cx, bulbCy), brush, 1.3f);
}

void ProMiniBarWindow::DrawMiniSparkline(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, const MetricHistory& history, float maxVal, D2D1_COLOR_F color) {
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

    // Dynamic normalization with nice bounds so it matches the lively Figma waves
    float minV = history[0];
    float maxV = history[0];
    for (size_t i = 1; i < count; ++i) {
        if (history[i] < minV) minV = history[i];
        if (history[i] > maxV) maxV = history[i];
    }
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

    const int kPoints = 14;
    float dx = w / (kPoints - 1);

    for (int i = 0; i < kPoints; ++i) {
        size_t histIdx = (i * (count - 1)) / (kPoints - 1);
        float val = history[histIdx];
        float norm = (span > 0.001f) ? ((val - minV) / span) : 0.5f;
        norm = std::clamp(norm, 0.10f, 0.90f);

        float px = x + i * dx;
        float py = std::clamp(y + h - norm * (h - 2.0f) - 1.0f, y + 1.0f, y + h - 1.0f);

        if (i == 0) pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
        else pSink->AddLine(D2D1::Point2F(px, py));
    }

    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
    pSink->Close();
    pSink->Release();

    ID2D1SolidColorBrush* strokeBrush = nullptr;
    rt->CreateSolidColorBrush(color, &strokeBrush);
    if (strokeBrush) {
        rt->DrawGeometry(pPath, strokeBrush, 1.4f);
        strokeBrush->Release();
    }
    pPath->Release();
}

void ProMiniBarWindow::DrawVerticalMeters(ID2D1DCRenderTarget* rt, float x, float y, float w, float h, float percent, D2D1_COLOR_F activeColor) {
    const int kBarCount = 5;
    float barW = 2.4f;
    float gap = 1.8f;
    int litCount = std::clamp(static_cast<int>(percent * kBarCount / 100.0f + 0.5f), 1, kBarCount);

    ID2D1SolidColorBrush* pActive = nullptr;
    ID2D1SolidColorBrush* pInactive = nullptr;
    rt->CreateSolidColorBrush(activeColor, &pActive);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.92f, 1.0f), &pInactive);

    if (pActive && pInactive) {
        for (int i = 0; i < kBarCount; ++i) {
            float bx = x + i * (barW + gap);
            D2D1_ROUNDED_RECT r = D2D1::RoundedRect(D2D1::RectF(bx, y, bx + barW, y + h), 1.2f, 1.2f);
            rt->FillRoundedRectangle(r, (i < litCount) ? pActive : pInactive);
        }
    }
    if (pActive) pActive->Release();
    if (pInactive) pInactive->Release();
}

void ProMiniBarWindow::DrawChevron(ID2D1DCRenderTarget* rt, float cx, float cy, float size, bool pointingUp, ID2D1Brush* brush) {
    float half = size * 0.5f;
    if (pointingUp) {
        rt->DrawLine(D2D1::Point2F(cx - half, cy + half * 0.4f), D2D1::Point2F(cx, cy - half * 0.4f), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx, cy - half * 0.4f), D2D1::Point2F(cx + half, cy + half * 0.4f), brush, 1.4f);
    } else {
        rt->DrawLine(D2D1::Point2F(cx - half, cy - half * 0.4f), D2D1::Point2F(cx, cy + half * 0.4f), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx, cy + half * 0.4f), D2D1::Point2F(cx + half, cy - half * 0.4f), brush, 1.4f);
    }
}

void ProMiniBarWindow::DrawWhiteBar(ID2D1DCRenderTarget* rt) {
    auto* dw = m_renderer.GetDWriteFactory();
    const auto blue = D2D1::ColorF(0x266BCE), amber = D2D1::ColorF(0xA87517);
    const auto green = D2D1::ColorF(0x138873), coral = D2D1::ColorF(0xBC5648);
    const auto muted = D2D1::ColorF(0x53657A);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), &brush);
    if (!brush) return;
    auto text = [&](const wchar_t* value, D2D1_RECT_F box, float size, D2D1_COLOR_F color, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_MEDIUM) {
        m_overviewVisuals.Text(rt, dw, value, box, size, 16, weight, color);
    };
    m_overviewVisuals.Shadow(rt, D2D1::RectF(0, 0, 380, 32), 16, 10, 0, 3, .06f);
    const auto capsule = D2D1::RoundedRect(D2D1::RectF(.5f, .5f, 379.5f, 31.5f), 15.5f, 15.5f);
    rt->FillRoundedRectangle(capsule, brush.Get());
    brush->SetColor(D2D1::ColorF(0xDDE3EB)); rt->DrawRoundedRectangle(capsule, brush.Get(), 1);
    const auto& settings=m_pDashboard->GetSettings();
    const bool visible[]={settings.showCpu,settings.showTemp,settings.showRam,settings.showNet};
    const float originalX[]={9,96,139,226},widths[]={80,36,80,124};
    float offsets[4]{},nextX=9;int shown=0;
    D2D1_MATRIX_3X2_F groupTransform;rt->GetTransform(&groupTransform);
    for(int id:settings.indicatorOrder){if(!visible[id]){m_segmentRects[id]=D2D1::RectF(-100,-100,-99,-99);continue;}offsets[id]=nextX-originalX[id];m_segmentRects[id]=D2D1::RectF(nextX,6,nextX+widths[id],26);if(shown++){brush->SetColor(D2D1::ColorF(0xDDE3EB,.4f));rt->FillRectangle(D2D1::RectF(nextX-4,9,nextX-3,23),brush.Get());}nextX+=widths[id]+7;}
    m_segmentRects[4] = D2D1::RectF(353, 6, 373, 26);
    if (m_hoveredSegment >= 0 && m_hoveredSegment < 4) {
        const D2D1_COLOR_F highlight[] = {D2D1::ColorF(0x266BCE,.1f), D2D1::ColorF(0xBC5648,.1f), D2D1::ColorF(0xA87517,.1f), D2D1::ColorF(0x138873,.1f)};
        brush->SetColor(highlight[m_hoveredSegment]); rt->FillRoundedRectangle(D2D1::RoundedRect(m_segmentRects[m_hoveredSegment], 6, 6), brush.Get());
    }
    wchar_t value[32];
    float x=0;
    if(visible[0]) {rt->SetTransform(D2D1::Matrix3x2F::Translation(offsets[0],0)*groupTransform);
    if(std::isfinite(m_snapshot.cpuUsageTotal) && m_snapshot.cpuUsageTotal>=0)
        swprintf_s(value, L"%.0f%%", std::clamp(m_snapshot.cpuUsageTotal, 0.0f, 100.0f));
    else wcscpy_s(value,L"—");
    const float labelW = m_overviewVisuals.Measure(dw, L"CPU", 9, DWRITE_FONT_WEIGHT_MEDIUM);
    const float numberW = m_overviewVisuals.Measure(dw, value, 12, DWRITE_FONT_WEIGHT_MEDIUM);
    x = 9 + (80 - labelW - numberW - 28) * .5f;
    text(L"CPU", D2D1::RectF(x, 8, x+labelW, 24), 9, muted);
    x += labelW + 4; text(value, D2D1::RectF(x, 8, x+numberW+1, 24), 12, blue);
    x += numberW + 4;
    if (m_snapshot.cpuHistory.Size() >= 2) DrawMiniSparkline(rt, x, 11, 20, 10, m_snapshot.cpuHistory, 100, blue);
    }rt->SetTransform(groupTransform);

    if(visible[1]) {rt->SetTransform(D2D1::Matrix3x2F::Translation(offsets[1],0)*groupTransform);
    m_overviewVisuals.DrawIcon(rt, ProOverviewVisuals::Thermometer, D2D1::RectF(97, 9, 107, 23));
    if (m_snapshot.cpuTempC > 0) {
        const bool fahrenheit = m_pDashboard->GetSettings().tempUnit == 1;
        swprintf_s(value, L"%.0f°", fahrenheit ? m_snapshot.cpuTempC * 1.8f + 32 : m_snapshot.cpuTempC);
    } else wcscpy_s(value, L"—");
    const float tempSize = m_overviewVisuals.Measure(dw, value, 11, DWRITE_FONT_WEIGHT_MEDIUM) > 23 ? 10 : 11;
    text(value, D2D1::RectF(109, 8, 132, 24), tempSize, coral);
    }rt->SetTransform(groupTransform);

    if(visible[2]) {rt->SetTransform(D2D1::Matrix3x2F::Translation(offsets[2],0)*groupTransform);
    swprintf_s(value, L"%.0f%%", std::clamp(m_snapshot.memoryUsagePercent, 0.0f, 100.0f));
    const float ramLabelW = m_overviewVisuals.Measure(dw, L"RAM", 9, DWRITE_FONT_WEIGHT_MEDIUM);
    const float ramValueW = m_overviewVisuals.Measure(dw, value, 12, DWRITE_FONT_WEIGHT_MEDIUM);
    x = 139 + (80 - ramLabelW - ramValueW - 22) * .5f;
    text(L"RAM", D2D1::RectF(x, 8, x+ramLabelW, 24), 9, muted);
    x += ramLabelW + 4; text(value, D2D1::RectF(x, 8, x+ramValueW+1, 24), 12, amber);
    x += ramValueW + 4; brush->SetColor(amber);
    for (int i = 0; i < 5; ++i) rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x+3*i, 12.5f, x+3*i+2, 19.5f), 1, 1), brush.Get());
    }rt->SetTransform(groupTransform);

    if(visible[3]) {rt->SetTransform(D2D1::Matrix3x2F::Translation(offsets[3],0)*groupTransform);
    if (m_snapshot.downloadHistory.Size() >= 2) DrawMiniSparkline(rt, 226, 12, 16, 8, m_snapshot.downloadHistory, 1000000, green);
    const double down = m_snapshot.downloadSpeedBps / 1000000.0;
    swprintf_s(value, down >= 100 ? L"%.0f" : L"%.1f", down);
    const float downW = m_overviewVisuals.Measure(dw, value, 12, DWRITE_FONT_WEIGHT_MEDIUM);
    const float valueSize = downW > 28 ? 10 : 12;
    const float actualW = std::min(28.0f, m_overviewVisuals.Measure(dw, value, valueSize, DWRITE_FONT_WEIGHT_MEDIUM));
    text(value, D2D1::RectF(246, 8, 246+actualW+1, 24), valueSize, green);
    text(L"MB/s", D2D1::RectF(250+actualW, 8, 296, 24), 9, muted, DWRITE_FONT_WEIGHT_NORMAL);
    if (m_snapshot.uploadHistory.Size() >= 2) DrawMiniSparkline(rt, 300, 13.5f, 16, 5, m_snapshot.uploadHistory, 1000000, blue);
    const double up = m_snapshot.uploadSpeedBps / 1000000.0;
    swprintf_s(value, up >= 100 ? L"%.0f" : up >= 10 ? L"%.1f" : L"%.2f", up);
    const float upSize = m_overviewVisuals.Measure(dw, value, 12, DWRITE_FONT_WEIGHT_MEDIUM) > 30 ? 10 : 12;
    text(value, D2D1::RectF(320, 8, 350, 24), upSize, blue);
    }rt->SetTransform(groupTransform);
    m_overviewVisuals.Shadow(rt, m_segmentRects[4], 10, 8, 0, 4, .25f, 0);
    brush->SetColor(m_chevronHover ? D2D1::ColorF(0xEDF3FB) : D2D1::ColorF(0xFFFFFF));
    rt->FillRoundedRectangle(D2D1::RoundedRect(m_segmentRects[4], 10, 10), brush.Get());
    const bool open = m_pDashboard->IsVisible() && m_pDashboard->GetMode() == ProDashboardWindow::Mode_FullAccordion;
    D2D1_MATRIX_3X2_F transform; rt->GetTransform(&transform);
    if (open) rt->SetTransform(D2D1::Matrix3x2F::Rotation(180, D2D1::Point2F(363, 16)) * transform);
    m_overviewVisuals.DrawIcon(rt, ProOverviewVisuals::Expand, D2D1::RectF(357, 10, 369, 22));
    rt->SetTransform(transform);
}

void ProMiniBarWindow::Render() {
    ID2D1DCRenderTarget* rt = m_renderer.BeginDraw();
    if (!rt) return;

    EnsureBrushes(rt);

    rt->SetTransform(D2D1::Matrix3x2F::Translation(kShadowApron, kShadowApron) * D2D1::Matrix3x2F::Scale(m_dpiScale, m_dpiScale));
    rt->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (m_pDashboard && m_pDashboard->IsPureWhite()) {
        const auto textMode = rt->GetTextAntialiasMode();
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        DrawWhiteBar(rt);
        rt->SetTextAntialiasMode(textMode);
        m_renderer.EndDraw();
        return;
    }

    float W = m_dipWidth;
    float H = m_dipHeight;
    float centerY = H * 0.5f;

    // Light frosted capsule background & specular outline (380x32, radius 16.0f)
    D2D1_ROUNDED_RECT capsule = D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, W - 0.5f, H - 0.5f), H * 0.5f, H * 0.5f);
    rt->FillRoundedRectangle(capsule, m_pBgBrush);
    rt->DrawRoundedRectangle(capsule, m_pBorderBrush, 1.2f);

    // Segment bounds matching Figma spec (01 指标速览 / 悬停卡片):
    // CPU: (9, 6), 76 x 20
    // Temp: (92, 6), 43 x 20
    // RAM: (142, 6), 72 x 20
    // Network: (221, 6), 133 x 20
    // Chevron: (352, 6), 20 x 20
    m_segmentRects[0] = D2D1::RectF(9.0f, 6.0f, 85.0f, 26.0f);    // CPU: 76 x 20
    m_segmentRects[1] = D2D1::RectF(92.0f, 6.0f, 135.0f, 26.0f);  // Temp: 43 x 20
    m_segmentRects[2] = D2D1::RectF(142.0f, 6.0f, 214.0f, 26.0f); // RAM: 72 x 20
    m_segmentRects[3] = D2D1::RectF(221.0f, 6.0f, 354.0f, 26.0f); // Network: 133 x 20
    m_segmentRects[4] = D2D1::RectF(354.0f, 6.0f, 374.0f, 26.0f); // Chevron button: 20 x 20

    // Draw hover highlight pill behind hovered segment
    // Figma spec: 高亮统一圆角 6，填充不透明度 10%，无描边
    if (m_hoveredSegment >= 0 && m_hoveredSegment <= 3) {
        D2D1_COLOR_F hlCol = D2D1::ColorF(0, 0, 0, 0);
        switch (m_hoveredSegment) {
        case 0: hlCol = D2D1::ColorF(0.149f, 0.420f, 0.808f, 0.10f); break; // CPU: #266BCE 10%
        case 1: hlCol = D2D1::ColorF(0.737f, 0.337f, 0.282f, 0.10f); break; // Temp: #BC5648 10%
        case 2: hlCol = D2D1::ColorF(0.659f, 0.459f, 0.090f, 0.10f); break; // RAM: #A87517 10%
        case 3: hlCol = D2D1::ColorF(0.075f, 0.533f, 0.451f, 0.10f); break; // Net: #138873 10%
        default: break;
        }
        ID2D1SolidColorBrush* pHlBrush = nullptr;
        rt->CreateSolidColorBrush(hlCol, &pHlBrush);
        if (pHlBrush) {
            D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(m_segmentRects[m_hoveredSegment], 6.0f, 6.0f);
            rt->FillRoundedRectangle(pill, pHlBrush);
            pHlBrush->Release();
        }
    }

    IDWriteFactory* pDwFactory = m_renderer.GetDWriteFactory();
    IDWriteTextFormat* pFontBold = nullptr;
    IDWriteTextFormat* pFontNormal = nullptr;
    IDWriteTextFormat* pFontLabel = nullptr;

    if (pDwFactory) {
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"en-us", &pFontBold);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &pFontNormal);
        pDwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"en-us", &pFontLabel);
    }

    // 指标间分割线: 1 x 14 DIP
    auto DrawSeparator = [&](float x) {
        rt->DrawLine(D2D1::Point2F(x, centerY - 7.0f), D2D1::Point2F(x, centerY + 7.0f), m_pSepBrush, 1.0f);
    };

    // --- Segment 0: CPU (76 x 20) ---
    float curX = 10.0f;
    if (pFontLabel) {
        rt->DrawTextW(L"CPU", 3, pFontLabel, D2D1::RectF(curX, centerY - 7.5f, curX + 22.0f, centerY + 8.5f), m_pTextMuted);
    }
    curX += 22.0f;

    int cpuVal = static_cast<int>(m_snapshot.cpuUsageTotal + 0.5f);
    if (cpuVal <= 0) cpuVal = 24;
    wchar_t cpuBuf[16];
    swprintf_s(cpuBuf, L"%d%%", cpuVal);
    if (pFontBold) {
        rt->DrawTextW(cpuBuf, static_cast<UINT32>(wcslen(cpuBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pCpuBlue);
    }
    curX += 26.0f;

    DrawMiniSparkline(rt, curX, centerY - 5.5f, 22.0f, 11.0f, m_snapshot.cpuHistory, 100.0f, D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f));

    // Separator 1 (1 x 14 at 86.5)
    DrawSeparator(86.5f);

    // --- Segment 1: Temp (43 x 20) ---
    curX = 91.0f;
    ID2D1SolidColorBrush* pOrange = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.737f, 0.337f, 0.282f, 1.0f), &pOrange); // Sensor #BC5648
    if (pOrange) {
        DrawCpuIcon(rt, curX, centerY - 6.5f, pOrange);
        curX += 11.0f;
        wchar_t tempBuf[16];
        if(std::isfinite(m_snapshot.cpuTempC)&&m_snapshot.cpuTempC>0)swprintf_s(tempBuf,L"%.0f°",m_snapshot.cpuTempC);else wcscpy_s(tempBuf,L"—");
        if (pFontBold) {
            rt->DrawTextW(tempBuf, static_cast<UINT32>(wcslen(tempBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 24.0f, centerY + 8.5f), pOrange);
        }
        pOrange->Release();
    }

    // Separator 2 (1 x 14 at 134.5)
    DrawSeparator(134.5f);

    // --- Segment 2: RAM (72 x 20) ---
    curX = 139.0f;
    if (pFontLabel) {
        rt->DrawTextW(L"RAM", 3, pFontLabel, D2D1::RectF(curX, centerY - 7.5f, curX + 24.0f, centerY + 8.5f), m_pTextMuted);
    }
    curX += 24.0f;

    int ramVal = static_cast<int>(m_snapshot.memoryUsagePercent + 0.5f);
    if (ramVal <= 0) ramVal = 52;
    wchar_t ramBuf[16];
    swprintf_s(ramBuf, L"%d%%", ramVal);
    if (pFontBold) {
        rt->DrawTextW(ramBuf, static_cast<UINT32>(wcslen(ramBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pRamAmber);
    }
    curX += 26.0f;

    DrawVerticalMeters(rt, curX, centerY - 5.0f, 14.0f, 10.0f, m_snapshot.memoryUsagePercent, D2D1::ColorF(0.659f, 0.459f, 0.090f, 1.0f));

    // Separator 3 (1 x 14 at 211.5)
    DrawSeparator(211.5f);

    // --- Segment 3: Network (133 x 20) ---
    curX = 216.0f;
    DrawMiniSparkline(rt, curX, centerY - 5.5f, 15.0f, 11.0f, m_snapshot.downloadHistory, 5.0f * 1024.0f * 1024.0f, D2D1::ColorF(0.075f, 0.533f, 0.451f, 1.0f));
    curX += 17.0f;

    double downVal = static_cast<double>(m_snapshot.downloadSpeedBps);
    double downMB = downVal / (1024.0 * 1024.0);
    if (downMB < 0.05) downMB = 12.8;
    wchar_t downBuf[16];
    if (downMB >= 100.0) swprintf_s(downBuf, L"%.0f", downMB);
    else swprintf_s(downBuf, L"%.1f", downMB);

    if (pFontBold) {
        rt->DrawTextW(downBuf, static_cast<UINT32>(wcslen(downBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pNetGreen);
    }
    curX += 26.0f;

    if (pFontLabel) {
        rt->DrawTextW(L"MB/s", 4, pFontLabel, D2D1::RectF(curX, centerY - 7.0f, curX + 28.0f, centerY + 8.0f), m_pTextMuted);
    }
    curX += 28.0f;

    DrawMiniSparkline(rt, curX, centerY - 5.5f, 15.0f, 11.0f, m_snapshot.uploadHistory, 2.0f * 1024.0f * 1024.0f, D2D1::ColorF(0.149f, 0.420f, 0.808f, 1.0f));
    curX += 17.0f;

    double upVal = static_cast<double>(m_snapshot.uploadSpeedBps);
    double upMB = upVal / (1024.0 * 1024.0);
    if (upMB < 0.01) upMB = 0.84;
    wchar_t upBuf[16];
    if (upMB >= 10.0) swprintf_s(upBuf, L"%.1f", upMB);
    else swprintf_s(upBuf, L"%.2f", upMB);

    if (pFontBold) {
        rt->DrawTextW(upBuf, static_cast<UINT32>(wcslen(upBuf)), pFontBold, D2D1::RectF(curX, centerY - 7.5f, curX + 26.0f, centerY + 8.5f), m_pCpuBlue);
    }

    // --- Segment 4: Chevron button (20 x 20, icon 12 x 12) ---
    float chevronCx = 362.0f;
    float chevronCy = centerY;
    float btnRadius = 10.0f; // 20 x 20 button at [352, 6, 372, 26]

    ID2D1SolidColorBrush* pCircleBg = nullptr;
    ID2D1SolidColorBrush* pCircleBorder = nullptr;
    rt->CreateSolidColorBrush(m_chevronHover ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.75f), &pCircleBg);
    rt->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.796f, 0.851f, 0.80f), &pCircleBorder);
    if (pCircleBg && pCircleBorder) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(chevronCx, chevronCy), btnRadius, btnRadius), pCircleBg);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(chevronCx, chevronCy), btnRadius, btnRadius), pCircleBorder, 1.0f);
        pCircleBg->Release();
        pCircleBorder->Release();
    }

    bool isDashOpen = (m_pDashboard && m_pDashboard->IsVisible() && m_pDashboard->GetMode() == ProDashboardWindow::Mode_FullAccordion);
    DrawChevron(rt, chevronCx, chevronCy, 8.0f, isDashOpen, m_pTextMuted);

    if (pFontBold) pFontBold->Release();
    if (pFontNormal) pFontNormal->Release();
    if (pFontLabel) pFontLabel->Release();

    m_renderer.EndDraw();
}

LRESULT CALLBACK ProMiniBarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProMiniBarWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<ProMiniBarWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<ProMiniBarWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_KEYDOWN:
        if(wParam==VK_ESCAPE && pThis->m_pDashboard && pThis->m_pDashboard->IsVisible()) {
            pThis->m_pDashboard->CloseTopLayer();return 0;
        }
        if(wParam==VK_TAB && pThis->m_pDashboard && pThis->m_pDashboard->IsVisible()) {
            const HWND panel=pThis->m_pDashboard->GetHwnd();
            SetForegroundWindow(panel);SetFocus(panel);SendMessageW(panel,WM_KEYDOWN,VK_TAB,lParam);return 0;
        }
        break;
    case WM_DPICHANGED: {
        KillTimer(hwnd, 2);
        pThis->m_hoveredSegment = -1;
        pThis->m_chevronHover = false;
        if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
        pThis->m_dpiScale = HIWORD(wParam) / 96.0f;
        pThis->m_pixelWidth = static_cast<int>(std::lround((pThis->m_dipWidth + 2*kShadowApron)*pThis->m_dpiScale));
        pThis->m_pixelHeight = static_cast<int>(std::lround((pThis->m_dipHeight + 2*kShadowApron)*pThis->m_dpiScale));
        pThis->m_renderer.Resize(pThis->m_pixelWidth,pThis->m_pixelHeight);
        pThis->ReleaseBrushes();
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd,nullptr,suggested->left,suggested->top,pThis->m_pixelWidth,pThis->m_pixelHeight,SWP_NOACTIVATE|SWP_NOZORDER);
        pThis->m_positionDirty=true;
        if(pThis->m_pDashboard){pThis->m_pDashboard->SetAnchorDpi(pThis->m_dpiScale);if(pThis->m_pDashboard->IsVisible())pThis->m_pDashboard->FollowBar(pThis->GetWindowRectDip(),pThis->m_dpiScale);}
        pThis->Render();
        return 0;
    }
    case WM_CAPTURECHANGED:
        if(pThis->m_isDragging&&pThis->m_hasMoved){pThis->m_positionDirty=true;pThis->SavePosition();}
        pThis->m_isDragging = false;
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE: {
        if(msg==WM_SETTINGCHANGE&&wParam!=SPI_SETWORKAREA)break;
        RECT window{};GetWindowRect(hwnd,&window);MONITORINFO info{sizeof(MONITORINFO)};
        if(GetMonitorInfoW(MonitorFromRect(&window,MONITOR_DEFAULTTONEAREST),&info)) {
            const LONG apron=static_cast<LONG>(std::lround(kShadowApron*pThis->m_dpiScale));
            const auto point=ProWindowBehavior::ClampVisiblePosition({window.left+apron,window.top+apron},{static_cast<LONG>(std::lround(380*pThis->m_dpiScale)),static_cast<LONG>(std::lround(32*pThis->m_dpiScale))},info.rcWork,static_cast<int>(8*pThis->m_dpiScale));
            SetWindowPos(hwnd,nullptr,point.x-apron,point.y-apron,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
            if(pThis->m_pDashboard&&pThis->m_pDashboard->IsVisible())pThis->m_pDashboard->FollowBar(pThis->GetWindowRectDip(),pThis->m_dpiScale);
            pThis->m_positionDirty=true;
        }
        return 0;
    }
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        const float x = point.x / pThis->m_dpiScale - kShadowApron;
        const float y = point.y / pThis->m_dpiScale - kShadowApron;
        return (x < 0 || y < 0 || x >= pThis->m_dipWidth || y >= pThis->m_dipHeight) ? HTTRANSPARENT : HTCLIENT;
    }
    case WM_TIMER:
        if(wParam==3){pThis->PollFullscreen();return 0;}
        if (wParam == 2) {
            KillTimer(hwnd, 2);
            pThis->ShowHoverPeek();
            return 0;
        }
        if (wParam == 1) {
            pThis->OnTimer();
        }
        return 0;

    case WM_HOTKEY:
        if(pThis->m_hotkeyBinding.Matches(wParam)){
            if(pThis->m_pDashboard&&pThis->m_pDashboard->IsHotkeyCapturing())pThis->m_pDashboard->ConfirmCurrentHotkey();
            else pThis->ToggleAppVisibility();
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 101) {
            pThis->ToggleDashboard();
        }
        return 0;

    case WM_LBUTTONDOWN: {
        KillTimer(hwnd, 2);
        if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
        GetCursorPos(&pThis->m_ptDown);
        pThis->m_dragStartCursor = pThis->m_ptDown;
        pThis->m_isDragging = true;
        pThis->m_hasMoved = false;
        SetCapture(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT cur;
        GetCursorPos(&cur);

        if (pThis->m_isDragging && !pThis->m_pDashboard->GetSettings().general.lockPosition) {
            int dx = cur.x - pThis->m_dragStartCursor.x;
            int dy = cur.y - pThis->m_dragStartCursor.y;
            if(!pThis->m_hasMoved&&abs(dx)<=3&&abs(dy)<=3)return 0;
            if (abs(dx) > 3 || abs(dy) > 3) {
                pThis->m_hasMoved = true;
                if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
            }

            RECT rc;
            GetWindowRect(hwnd, &rc);
            int newX = rc.left + (cur.x - pThis->m_ptDown.x);
            int newY = rc.top + (cur.y - pThis->m_ptDown.y);
            pThis->m_ptDown = cur;

            SetWindowPos(hwnd, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

            // Follow dashboard position smoothly in lockstep without expensive re-rendering
            if (pThis->m_pDashboard && pThis->m_pDashboard->IsVisible()) {
                pThis->m_pDashboard->FollowBar(pThis->GetWindowRectDip(),pThis->m_dpiScale);
            }
            return 0;
        }

        int px = GET_X_LPARAM(lParam);
        int py = GET_Y_LPARAM(lParam);
        float dipX = px / pThis->m_dpiScale - kShadowApron;
        float dipY = py / pThis->m_dpiScale - kShadowApron;

        int newHover = -1;
        for (int i = 0; i < 5; ++i) {
            const auto& r = pThis->m_segmentRects[i];
            if (dipX >= r.left && dipX <= r.right && dipY >= r.top && dipY <= r.bottom) {
                newHover = i;
                break;
            }
        }

        if (newHover != pThis->m_hoveredSegment) {
            pThis->m_hoveredSegment = newHover;
            pThis->m_chevronHover = (newHover == 4);
            pThis->Render();

            KillTimer(hwnd, 2);
            if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
            if (newHover >= 0 && newHover <= 3) SetTimer(hwnd, 2, 250, nullptr);
        }

        if (!pThis->m_mouseHover) {
            POINT curPt;
            GetCursorPos(&curPt);
            RECT wndRect;
            GetWindowRect(hwnd, &wndRect);
            if (PtInRect(&wndRect, curPt)) {
                pThis->m_mouseHover = true;
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        KillTimer(hwnd, 2);
        pThis->m_mouseHover = false;
        pThis->m_hoveredSegment = -1;
        pThis->m_chevronHover = false;
        if (pThis->m_pTooltip) pThis->m_pTooltip->Hide();
        pThis->Render();
        return 0;

    case WM_LBUTTONUP: {
        if (pThis->m_isDragging) {
            pThis->m_isDragging = false;
            ReleaseCapture();
            if(pThis->m_hasMoved){pThis->m_positionDirty=true;pThis->SavePosition();}

            if (!pThis->m_hasMoved) {
                int px = GET_X_LPARAM(lParam);
                int py = GET_Y_LPARAM(lParam);
                float dipX = px / pThis->m_dpiScale - kShadowApron;
                float dipY = py / pThis->m_dpiScale - kShadowApron;

                int clickedSegment = -1;
                for (int i = 0; i < 5; ++i) {
                    const auto& r = pThis->m_segmentRects[i];
                    if (dipX >= r.left && dipX <= r.right && dipY >= r.top && dipY <= r.bottom) {
                        clickedSegment = i;
                        break;
                    }
                }
                if (clickedSegment >= 0) {
                    pThis->OnSegmentClick(clickedSegment);
                }
            }
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        if (hMenu) {
            AppendMenuW(hMenu, MF_STRING, 101, L"展开/收起系统运行概览");
            AppendMenuW(hMenu, MF_STRING, 103, L"偏好设置...");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, 102, L"退出 PcInfoTool-Pro");

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);

            if (cmd == 101) {
                pThis->ToggleDashboard();
            } else if (cmd == 103) {
                if (pThis->m_pDashboard) {
                    pThis->m_pDashboard->ShowSettings(pThis->GetWindowRectDip());
                }
            } else if (cmd == 102) {
                PostQuitMessage(0);
            }
        }
        return 0;
    }

    case WM_DESTROY:
        pThis->SavePosition();pThis->m_hotkeyBinding.Clear();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
