#include "pro/windows/ProDashboardWindow.h"
#include "pro/windows/ProMiniBarWindow.h"
#include "pro/windows/ProMiniBarTooltipWindow.h"
#include "core/collectors/MetricsCollector.h"
#include "core/config/ConfigManager.h"
#include <thread>
#include <chrono>
#include <wincodec.h>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <stdexcept>
#include <limits>
#include <filesystem>

using Microsoft::WRL::ComPtr;
static void Check(bool success, const char* message) { if (!success) throw std::runtime_error(message); }

// Render production drawing functions into our own DIB, not a desktop/window capture.
// Example data exists exclusively in this optional QA target.
struct ProOverviewRenderTest {
    static void MotionRules() {
        using namespace ProPanelMotion;
        Check(Ease(0)==0&&Ease(1)==1&&Ease(.25f)>.5f,"bezier endpoints/easing incorrect");
        float previous=0;for(int ms=0;ms<=300;++ms){float value=Ease(ms/300.f);Check(value>=previous&&value<=1,"motion easing overshoots or reverses");previous=value;}
        ProDashboardWindow p;Check(p.Create(),"motion renderer init failed");p.m_settings.themeMode=1;p.m_snapshot=Example();
        for(int layout:{0,1}){p.m_settings.layoutMode=layout;p.m_expandedModule=-1;p.UpdateDimensionsAndPosition();const auto overview=p.MotionLayout();
            for(int module=0;module<6;++module){p.m_expandedModule=module;p.UpdateDimensionsAndPosition();const auto detail=p.MotionLayout();
                Check(detail.items[module].content[2]==1&&detail.items[module].h>280,"expanded module is not a single detail container");
                Transition t;t.Start(overview,detail,1000);t.Tick(1075);const auto current=t.current;t.Start(current,overview,1075);
                Check(t.current.height==current.height&&t.current.items[module].y==current.items[module].y,"retarget jumped at restart");
                t.Tick(1375);Check(!t.active&&t.current.height==overview.height,"transition did not settle exactly");
                for(int ms:{0,75,150,225,300}){auto frame=Mix(overview,detail,ms/300.f);for(const auto& r:frame.items){Check(r.w>=166&&r.h>=28&&std::isfinite(r.y),"invalid interpolated module bounds");Check(r.content[0]+r.content[1]+r.content[2]<=1.001f,"content crossfade duplicates full-opacity layers");}
                    for(float dpi:{1.f,1.25f,1.5f,2.f})for(bool above:{false,true}){RECT work{0,0,LONG(1920*dpi),LONG(1080*dpi)},bar{LONG(400*dpi),LONG((above?800:40)*dpi),LONG(780*dpi),LONG((above?832:72)*dpi)};auto place=ProWindowBehavior::PlacePanel(bar,work,frame.height,360,dpi,true,above);Check(place.above==above&&std::abs((above?place.y+place.height*dpi:place.y)-(above?bar.top-16*dpi:bar.bottom+16*dpi))<=1.1f,"animation frame moved its anchor");}
                }
            }
        }
        p.m_visible=true;p.m_settings.general.reduceMotion=true;p.TransitionModule(ProDashboardWindow::Module_CPU);Check(!p.m_panelMotion.active,"reduce motion did not skip transition");
        p.m_settings.general.reduceMotion=false;p.m_panelMotion.Start(p.MotionLayout(),p.MotionLayout(),GetTickCount64());p.Hide();Check(!p.m_panelMotion.active,"close left animation running");
        p.m_visible=true;p.m_expandedModule=-1;p.UpdateDimensionsAndPosition();auto start=p.MotionLayout();p.m_expandedModule=0;p.UpdateDimensionsAndPosition();auto finish=p.MotionLayout();
        p.Render();p.m_expandedModule=-1;p.UpdateDimensionsAndPosition();p.Render();p.m_expandedModule=0;
        const auto started=GetTickCount64();p.m_panelMotion.Start(start,finish,started);Check(SetTimer(p.GetHwnd(),73,16,nullptr)!=0,"motion timer installation failed");
        auto updated=p.m_snapshot;updated.cpuUsageTotal=32;p.UpdateSnapshot(updated);Check(p.m_panelMotion.active&&p.m_panelMotion.started==started&&p.m_snapshot.cpuUsageTotal==32,"data refresh restarted or cancelled motion");
        int observed=0;float height=start.height;
        while(p.m_panelMotion.active&&GetTickCount64()-started<1500){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);if(p.m_panelMotion.current.height!=height){++observed;height=p.m_panelMotion.current.height;}}
            MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        }
        if(p.m_panelMotion.active||observed<2||p.m_dipHeight!=finish.height)std::printf("motion timer diagnostic: active=%d observed=%d actual=%.1f target=%.1f elapsed=%llu\n",p.m_panelMotion.active,observed,p.m_dipHeight,finish.height,GetTickCount64()-started);
        Check(!p.m_panelMotion.active&&observed>=2&&p.m_dipHeight==finish.height,"native timer did not animate and settle");
        Check(GetTickCount64()-started>=300&&p.m_expandedModule==0,"native timer ended too early or lost target");std::printf("Native timer: %d changing frames, %llu ms including final render\n",observed,GetTickCount64()-started);p.Hide();
        p.m_visible=true;p.m_expandedModule=0;p.UpdateDimensionsAndPosition();const auto cpu=p.MotionLayout();
        p.m_expandedModule=-1;p.UpdateDimensionsAndPosition();const auto overview=p.MotionLayout();
        p.m_expandedModule=0;p.m_panelMotion.Start(cpu,overview,GetTickCount64()-75);p.m_panelMotion.Tick(GetTickCount64());const auto interrupted=p.m_panelMotion.current;
        p.TransitionModule(ProDashboardWindow::Module_GPU);
        if(p.m_panelMotion.active)Check(std::abs(p.m_panelMotion.from.height-interrupted.height)<5&&p.m_panelMotion.to.items[1].content[2]==1,"production quick click did not use current frame");
        p.CancelPanelMotion();p.m_expandedModule=0;p.m_settings.layoutMode=0;p.UpdateDimensionsAndPosition();p.m_selectedDiskId=L"motion-stable-device";
        p.m_settingsReturnValid=true;p.m_settingsReturnMode=ProDashboardWindow::Mode_FullAccordion;p.m_settingsReturnModule=0;p.m_settingsReturnLayoutMode=0;p.m_settingsReturnGeometry=p.MotionLayout();
        p.m_mode=ProDashboardWindow::Mode_Settings;p.m_settings.layoutMode=1;p.ReturnFromSettings();
        Check(p.m_mode==ProDashboardWindow::Mode_FullAccordion&&p.m_expandedModule==0&&p.m_selectedDiskId==L"motion-stable-device","settings return lost module or device");
        if(p.m_panelMotion.active)Check(p.m_panelMotion.from.items[1].content[0]==1&&p.m_panelMotion.to.items[1].content[1]==1,"layout preference did not morph rows to capsules");p.Hide();
        DestroyWindow(p.GetHwnd());std::puts("PASS motion: bezier, six modules/two layouts, interruption, reduce motion, close, settings return");
    }
    static void PlacementRules() {
        for(float dpi:{1.f,1.25f,1.5f,2.f}) {
            RECT work{-1920,0,0,static_cast<LONG>(800*dpi)};
            RECT top{-1300,static_cast<LONG>(40*dpi),static_cast<LONG>(-1300+380*dpi),static_cast<LONG>(72*dpi)};
            auto down=ProWindowBehavior::PlacePanel(top,work,284,360,dpi,false,false);
            Check(!down.above&&std::abs(down.y-88*dpi)<=1,"first down anchor wrong");
            for(float h:{373.f,557.f,637.f,651.f,900.f}) {auto next=ProWindowBehavior::PlacePanel(top,work,h,360,dpi,true,down.above);Check(!next.above&&next.y==down.y&&next.y+next.height*dpi<=work.bottom-8*dpi+1,"module switch flips down direction or overflows");}
            RECT lower=top;lower.top=static_cast<LONG>(700*dpi);lower.bottom=static_cast<LONG>(732*dpi);
            auto up=ProWindowBehavior::PlacePanel(lower,work,284,360,dpi,false,false);Check(up.above,"near-bottom open did not select up");
            auto larger=ProWindowBehavior::PlacePanel(lower,work,637,360,dpi,true,up.above);Check(larger.above&&std::abs(larger.y+larger.height*dpi-(lower.top-16*dpi))<=1,"up bottom anchor moved");
            RECT limitedWork=work;limitedWork.bottom=static_cast<LONG>(516*dpi);auto limited=ProWindowBehavior::PlacePanel(top,limitedWork,637,360,dpi,true,false);Check(std::abs(limited.height-420)<=1,"420-DIP work-area limit wrong");
            RECT edge=top;edge.left=-2000;edge.right=-2000+static_cast<LONG>(380*dpi);auto clamp=ProWindowBehavior::PlacePanel(edge,work,284,360,dpi,false,false);Check(clamp.x>=work.left+8*dpi-1,"negative monitor x not clamped");
        }
        ProDashboardWindow p;Check(p.Create(),"placement renderer init failed");p.m_snapshot=Example();p.m_visible=true;p.m_isPinned=true;p.m_expandedModule=ProDashboardWindow::Module_CPU;
        p.OnOutsideClick();Check(p.m_visible,"pinned panel dismissed outside");p.m_diskMenuOpen=true;p.CloseTopLayer();Check(!p.m_diskMenuOpen&&p.m_visible,"Esc failed menu-first close");
        p.m_mode=ProDashboardWindow::Mode_Settings;p.m_settingsReturnValid=true;p.m_settingsReturnMode=ProDashboardWindow::Mode_FullAccordion;p.m_settingsReturnModule=ProDashboardWindow::Module_CPU;p.m_cpuPlacementValid=true;
        p.CloseTopLayer();Check(p.m_visible&&p.m_mode==ProDashboardWindow::Mode_FullAccordion&&p.m_expandedModule==ProDashboardWindow::Module_CPU&&p.m_cpuPlacementValid,"settings did not restore parent layer or lost placement lock");
        p.CloseTopLayer();Check(!p.m_visible&&!p.m_cpuPlacementValid&&p.m_isPinned,"Esc failed to close pinned panel");
        p.m_visible=true;p.m_isPinned=false;p.OnOutsideClick();Check(!p.m_visible,"unpinned panel failed outside close");
        DestroyWindow(p.GetHwnd());std::puts("PASS placement: four DPI, fixed direction/anchors, negative monitor, limited viewport, layered closes and pin");
    }
    static void NativeGeneral() {
        struct OwnedWindow {HWND hwnd=nullptr;~OwnedWindow(){if(hwnd)DestroyWindow(hwnd);}};
        OwnedWindow host{CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,L"STATIC",L"PCInfo general test",WS_POPUP,30000,30000,200,50,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
        OwnedWindow peer{CreateWindowExW(0,L"STATIC",L"PCInfo hotkey test",0,0,0,0,0,HWND_MESSAGE,nullptr,GetModuleHandleW(nullptr),nullptr)};
        Check(host.hwnd&&peer.hwnd,"native general test windows failed");
        {
            ProWindowBehavior::HotkeyBinding binding,holder;std::wstring error;const UINT mods=MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN;
            UINT first=0,second=0;for(UINT key=VK_F13;key<=VK_F24;++key)if(binding.Set(host.hwnd,mods,key,error)){first=key;break;}
            Check(first!=0,"no available fixture shortcut");for(UINT key=first+1;key<=VK_F24;++key)if(holder.Set(peer.hwnd,mods,key,error)){second=key;break;}
            Check(second!=0,"no available conflict fixture shortcut");
            Check(!binding.Set(host.hwnd,mods,second,error)&&binding.Key()==first&&!error.empty(),"native conflict destroyed working shortcut");holder.Clear();Check(binding.Set(host.hwnd,mods,second,error)&&binding.Key()==second,"replacement shortcut failed after conflict released");Check(binding.Set(host.hwnd,0,0,error)&&binding.Key()==0,"clearing native shortcut failed");
        }
        ProMiniBarWindow bar;bar.m_pDashboard->m_settings={};bar.m_pDashboard->m_isPinned=true;bar.m_hwnd=host.hwnd;
        // No real application window or config is touched. The fixture windows remain hidden.
        try {
            auto g=bar.m_pDashboard->GetSettings().general;std::wstring error;g.alwaysOnTop=false;Check(bar.ApplyGeneralChange(g,error)&&!(GetWindowLongPtrW(host.hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)&&bar.m_pDashboard->IsPinned(),"unpinning z-order affected panel pinning");bar.m_pDashboard->SetGeneral(g);g.alwaysOnTop=true;Check(bar.ApplyGeneralChange(g,error)&&(GetWindowLongPtrW(host.hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST),"restoring z-order failed");bar.m_pDashboard->SetGeneral(g);
            SetWindowLongPtrW(host.hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&bar));POINT cursor{};GetCursorPos(&cursor);bar.m_ptDown={cursor.x-64,cursor.y};bar.m_dragStartCursor=bar.m_ptDown;bar.m_isDragging=true;bar.m_pDashboard->m_settings.general.lockPosition=true;RECT before{},after{};GetWindowRect(host.hwnd,&before);ProMiniBarWindow::WndProc(host.hwnd,WM_MOUSEMOVE,0,MAKELPARAM(-100,-100));GetWindowRect(host.hwnd,&after);Check(EqualRect(&before,&after)&&!bar.m_hasMoved,"locked minibar moved");bar.m_pDashboard->m_settings.general.lockPosition=false;ProMiniBarWindow::WndProc(host.hwnd,WM_MOUSEMOVE,0,MAKELPARAM(-100,-100));GetWindowRect(host.hwnd,&after);Check(!EqualRect(&before,&after)&&bar.m_hasMoved,"unlocked minibar failed to move");
            g=bar.m_pDashboard->GetSettings().general;bar.CapturePosition(g);Check(g.positionValid&&g.positionX==after.left+18&&g.savedDpi==96,"physical visible position capture incorrect");
        } catch(...){bar.m_hwnd=nullptr;throw;}
        bar.m_hwnd=nullptr;std::puts("PASS native general: hotkey conflict/replacement, z-order, pin independence, drag lock, position capture");
    }
    static MetricsSnapshot Example() {
        MetricsSnapshot s;
        s.cpuUsageTotal = 24; s.gpuUsagePercent = 18; s.memoryUsagePercent = 52;
        s.totalPhysMem = 32ULL * 1024 * 1024 * 1024;
        s.usedPhysMem = static_cast<uint64_t>(16.6 * 1073741824.0);
        s.diskReadBps = 42000000; s.downloadSpeedBps = 12800000; s.uploadSpeedBps = 840000;
        s.cpuTempC = 58; s.uptimeSeconds = 3 * 3600 + 42 * 60 + 18;
        s.cpuFanRpm=1240;s.cpuPowerWatts=32;s.ssdTempC=41; // Reference-only fixture, never production defaults.
        const float values[] = {22,25,21,31,28,40,31,34,23,29,26,39,32,34};
        for (float v : values) {
            s.cpuHistory.Push(v); s.gpuHistory.Push(v); s.memoryHistory.Push(v);
            s.diskHistory.Push(v);s.diskReadHistory.Push(v);s.diskWriteHistory.Push(v*.2f); s.downloadHistory.Push(v); s.uploadHistory.Push(v); s.tempHistory.Push(v);
        }
        return s;
    }
    static void Render(float scale, const wchar_t* output, bool cpuExpanded = false, size_t cores = 16, bool networkExpanded=false,float probe=212.4f,bool gpuExpanded=false,bool missingGpu=false,bool memoryExpanded=false,bool missingMemory=false,bool diskExpanded=false,bool missingDisk=false,int sensorMode=0,int hover=-1,int hoverVariant=0,const std::wstring& cpuState=L"") {
        ProDashboardWindow panel;
        Check(panel.Create(), "hidden renderer initialization failed");
        panel.m_snapshot = Example(); panel.m_hoveredModule = 0; // Figma CPU hover variant.
        if (cpuExpanded) {
            panel.m_snapshot.cpuFreqGHz=4.82f; panel.m_snapshot.cpuBrand=L"AMD Ryzen 7 7700";
            panel.m_snapshot.cpuUserPercent=18; panel.m_snapshot.cpuKernelPercent=6; panel.m_snapshot.cpuIdlePercent=76;
            const float loads[]={8,15,22,34,52,12,20,31,44,10,18,29,40,16,23,24};
            for (size_t i=0;i<cores;++i) panel.m_snapshot.cpuCoreUsage.push_back(loads[i%16]);
            panel.m_snapshot.topProcesses={{L"Chrome",8.2f},{L"Visual Studio Code",5.4f},{L"Desktop Window Manager",2.6f}};
            panel.m_expandedModule=ProDashboardWindow::Module_CPU;
            panel.m_snapshot.cpuHistory={};
            for(int i=0;i<60;++i) panel.m_snapshot.cpuHistory.Push(24+12*std::sin(i*.63f)+8*std::cos(i*.3f));
            if(cpuState.find(L"compact")!=std::wstring::npos) panel.m_settings.layoutMode=1;
            if(cpuState.find(L"direct")!=std::wstring::npos) panel.m_mode=ProDashboardWindow::Mode_DirectCpu;
            if(cpuState.find(L"focus")!=std::wstring::npos) {panel.m_focusedCpuCore=3;panel.m_cpuKeyboardFocus=true;}
            if(cpuState.find(L"hover")!=std::wstring::npos) panel.m_hoveredCpuCore=3;
            if(cpuState.find(L"missing")!=std::wstring::npos) {
                auto& s=panel.m_snapshot;s.cpuCoreUsage.clear();s.cpuHistory={};s.topProcesses.clear();s.cpuBrand.clear();
                s.cpuUsageTotal=std::numeric_limits<float>::quiet_NaN();s.cpuFreqGHz=0;s.cpuTempC=0;
            }
            if(cpuState.find(L"wide")!=std::wstring::npos) {
                auto& s=panel.m_snapshot;s.cpuUsageTotal=100;s.cpuFreqGHz=16.25f;s.cpuTempC=100;panel.m_settings.tempUnit=1;
                s.cpuBrand=L"Intel(R) Very Long Processor Model Name Which Must Be Ellipsized";
                s.topProcesses[0].name=L"Very long executable process name which must not overlap its utilization value";
                if(s.cpuCoreUsage.size()>3) s.cpuCoreUsage[3]=100;
            }
            if(cpuState.find(L"gaps")!=std::wstring::npos) {auto& hist=panel.m_snapshot.cpuHistory;hist={};hist.Push(0);hist.Push(24);hist.Push(std::numeric_limits<float>::quiet_NaN());hist.Push(18);hist.Push(0);}
            panel.UpdateDimensionsAndPosition();
            if(cpuState.find(L"viewport")!=std::wstring::npos) {panel.m_dipHeight=400;panel.m_cpuScroll=90;}
        }
        if(networkExpanded) {
            panel.m_hoveredModule=-1;
            panel.m_snapshot.netAdapterName=L"Wi-Fi 6 · 5 GHz";
            panel.m_snapshot.localIpAddress=L"192.168.1.108";
            panel.m_snapshot.sessionTotalInBytes=2460000000ULL;panel.m_snapshot.sessionTotalOutBytes=186000000;
            panel.m_snapshot.downloadHistory={};panel.m_snapshot.uploadHistory={};
            for(size_t i=0;i<cores;++i) {
                panel.m_snapshot.downloadHistory.Push(static_cast<float>(1000000*(10+6*std::sin(i*.85)+4*std::cos(i*1.9))));
                panel.m_snapshot.uploadHistory.Push(static_cast<float>(1000000*(2+1.5*std::sin(i*.64))));
            }
            std::tm clock{};clock.tm_year=124;clock.tm_mon=0;clock.tm_mday=1;clock.tm_hour=14;clock.tm_min=33;
            panel.m_netSampleUnixSeconds=static_cast<uint64_t>(std::mktime(&clock));
            panel.m_netProbeActive=probe>=0;panel.m_netProbeX=probe;
            if(cpuState.find(L"compact")!=std::wstring::npos) panel.m_settings.layoutMode=1;
            if(cpuState.find(L"pinned")!=std::wstring::npos) panel.m_isPinned=true;
            if(cpuState.find(L"direct")!=std::wstring::npos) panel.m_mode=ProDashboardWindow::Mode_DirectNetwork;
            if(cpuState.find(L"missing")!=std::wstring::npos) {
                auto& s=panel.m_snapshot;s.downloadHistory={};s.uploadHistory={};s.netAdapterName.clear();s.localIpAddress.clear();
                s.downloadSpeedBps=s.uploadSpeedBps=s.sessionTotalInBytes=s.sessionTotalOutBytes=0;
            }
            if(cpuState.find(L"wide")!=std::wstring::npos) {
                auto& s=panel.m_snapshot;s.downloadSpeedBps=123456000000ULL;s.uploadSpeedBps=9999000000ULL;
                s.netAdapterName=L"Very long network adapter model and connection description which must be ellipsized";
                s.localIpAddress=L"2001:0db8:1234:5678:90ab:cdef:1234:5678";
                s.sessionTotalInBytes=s.sessionTotalOutBytes=999999000000ULL;
            }
            if(cpuState.find(L"gaps")!=std::wstring::npos) {
                auto& s=panel.m_snapshot;s.downloadHistory={};s.uploadHistory={};
                for(float v : {0.f,24000000.f,std::numeric_limits<float>::quiet_NaN(),18000000.f,0.f}) s.downloadHistory.Push(v);
                for(float v : {0.f,1200000.f,1000000.f,std::numeric_limits<float>::quiet_NaN(),0.f}) s.uploadHistory.Push(v);
                panel.m_netProbeX=338-314.f*2/59;
            }
            panel.m_expandedModule=ProDashboardWindow::Module_Network;
            panel.UpdateDimensionsAndPosition();
        }
        ProMiniBarWindow bar;
        bar.m_pDashboard->m_settings=ProDashboardWindow::SettingsState{}; // Fixtures must not depend on the user's saved monitoring configuration.
        if(sensorMode) {
            panel.m_hoveredModule=-1;auto& s=panel.m_snapshot;
            s.tempHistory={};s.gpuTempC=46;s.cpuFreqGHz=4.82f;s.gpuPowerWatts=42;
            const float temps[]={46,49,47,52,51,54,50,53,48,51,49,54,64,57,59,55,58};
            for(float t : temps) s.tempHistory.Push(t);
            if(sensorMode==2) {s.tempHistory={};s.cpuTempC=0;s.gpuTempC=0;s.ssdTempC=0;s.cpuPowerWatts=0;s.gpuPowerWatts=0;s.cpuFreqGHz=0;s.cpuFanRpm=-1;}
            if(sensorMode==3) panel.m_settings.tempUnit=1;
            if(sensorMode==4) {s.tempHistory.Push(0);s.tempHistory.Push(57);s.tempHistory.Push(0);s.tempHistory.Push(61);s.tempHistory.Push(58);s.cpuFanRpm=0;s.caseFanRpm=0;}
            if(cpuState.find(L"compact")!=std::wstring::npos) panel.m_settings.layoutMode=1;
            if(cpuState.find(L"pinned")!=std::wstring::npos) panel.m_isPinned=true;
            if(cpuState.find(L"fahrenheit")!=std::wstring::npos) panel.m_settings.tempUnit=1;
            if(cpuState.find(L"wide")!=std::wstring::npos) {
                s.cpuTempC=200;s.gpuTempC=100;s.ssdTempC=90;panel.m_settings.tempUnit=1;
                s.tempHistory.Push(100);s.tempHistory.Push(200);s.cpuPowerWatts=999;s.gpuPowerWatts=1234;s.cpuFreqGHz=16.25f;s.cpuFanRpm=25000;s.caseFanRpm=10000;
            }
            if(cpuState.find(L"invalid")!=std::wstring::npos) {
                s.cpuTempC=s.gpuTempC=std::numeric_limits<float>::quiet_NaN();s.ssdTempC=-1;
                s.tempHistory.Push(std::numeric_limits<float>::quiet_NaN());s.tempHistory.Push(300);
                s.cpuPowerWatts=s.gpuPowerWatts=s.cpuFreqGHz=std::numeric_limits<float>::quiet_NaN();
                s.cpuFanRpm=s.caseFanRpm=-1;
            }
            if(cpuState.find(L"zero")!=std::wstring::npos) s.cpuFanRpm=s.caseFanRpm=0;
            if(cpuState.find(L"lenovo")!=std::wstring::npos) {
                s.laptopFanCount=2;s.cpuFanRpm=s.caseFanRpm=2300;s.cpuTempC=0;s.tempHistory={};s.cpuPowerWatts=0;
                s.sensorPermissionRequired=cpuState.find(L"permission")!=std::wstring::npos;
                if(s.sensorPermissionRequired){s.laptopFanCount=0;s.cpuFanRpm=s.caseFanRpm=-1;}
            }
            if(cpuState.rfind(L"temperature",0)==0) {
                panel.m_mode=ProDashboardWindow::Mode_DirectTemp;s.cpuBrand=L"AMD Ryzen 7 7700";
                if(cpuState.find(L"invalid")==std::wstring::npos && cpuState.find(L"wide")==std::wstring::npos) s.cpuPowerWatts=sensorMode==2 ? 0 : 32;
                if(cpuState.find(L"wide")!=std::wstring::npos) s.cpuBrand=L"Very long processor model name which must be ellipsized";
                if(cpuState.find(L"missing")!=std::wstring::npos) s.cpuBrand.clear();
                if(cpuState.find(L"hot")!=std::wstring::npos) s.cpuTempC=95;
                if(cpuState.find(L"off")!=std::wstring::npos) panel.m_settings.highTempAlert=0;
                if(cpuState.find(L"interval")!=std::wstring::npos) panel.m_settings.updateIntervalSec=2;
                if(cpuState.find(L"probe")!=std::wstring::npos) {panel.m_tempProbeActive=true;panel.m_tempProbeSecAgo=4;}
                if(cpuState.find(L"left")!=std::wstring::npos) {panel.m_tempProbeActive=true;panel.m_tempProbeSecAgo=59;}
                if(cpuState.find(L"right")!=std::wstring::npos) {panel.m_tempProbeActive=true;panel.m_tempProbeSecAgo=0;}
            }
            panel.m_expandedModule=ProDashboardWindow::Module_Sensor;panel.UpdateDimensionsAndPosition();
        }
        if(diskExpanded) {
            panel.m_hoveredModule=-1;auto& s=panel.m_snapshot;
            s.diskWriteBps=8700000;s.diskActivePercent=12;s.diskResponseTimeMs=.8f;
            s.diskStatisticsValid=!missingDisk;
            if(!missingDisk) {
                s.disks={{L"C:",L"系统",476ULL*1073741824,164ULL*1073741824},{L"D:",L"数据",455ULL*1073741824,95ULL*1073741824}};
                for(int i=0;i<60;++i) {s.diskReadHistory.Push(20+15*std::sin(i*.8f));s.diskWriteHistory.Push(8+5*std::sin(i*.6f));}
            }
            if(cpuState.find(L"compact")!=std::wstring::npos) panel.m_settings.layoutMode=1;
            if(cpuState.find(L"pinned")!=std::wstring::npos) panel.m_isPinned=true;
            if(cpuState.find(L"wide")!=std::wstring::npos) {
                s.diskReadBps=123456000000ULL;s.diskWriteBps=9999000000ULL;
                s.disks[0]={L"C:",L"Very long volume label which must not overlap capacity readings",999999ULL*1073741824,123456ULL*1073741824};
            }
            if(cpuState.find(L"unsupported")!=std::wstring::npos) s.diskStatisticsValid=false;
            if(cpuState.find(L"one")!=std::wstring::npos) s.disks.resize(1);
            if(cpuState.find(L"many")!=std::wstring::npos) s.disks.push_back({L"E:",L"备份",1024ULL*1073741824,512ULL*1073741824});
            if(cpuState.find(L"full")!=std::wstring::npos) {s.disks[0].freeBytes=0;s.disks[1].freeBytes=s.disks[1].totalBytes;}
            if(cpuState.find(L"invalid")!=std::wstring::npos) {s.disks[0].freeBytes=s.disks[0].totalBytes+1;s.disks[1].totalBytes=0;}
            if(cpuState.find(L"gaps")!=std::wstring::npos) {
                s.diskReadHistory={};s.diskWriteHistory={};
                for(float v : {0.f,24.f,std::numeric_limits<float>::quiet_NaN(),18.f,0.f}) s.diskReadHistory.Push(v);
                for(float v : {0.f,1.2f,1.f,std::numeric_limits<float>::quiet_NaN(),0.f}) s.diskWriteHistory.Push(v);
            }
            if(cpuState.find(L"selector")!=std::wstring::npos) {
                const wchar_t* ids[]={L"fixture-device-a",L"fixture-device-b",L"fixture-device-c"};
                const wchar_t* models[]={L"Samsung 980 PRO",L"WD Blue SN580",L"Seagate BarraCuda"};
                for(int i=0;i<3;++i) {PhysicalDiskInfo d;d.id=ids[i];d.number=i;d.model=models[i];d.busType=i==2 ? L"SATA" : L"NVMe";d.capacityBytes=i==2 ? 2000000000000ULL : 1000000000000ULL;d.throughputValid=d.statisticsValid=true;d.readBps=(42+i*20)*1000000ULL;d.writeBps=(8+i*4)*1000000ULL;d.activePercent=12+i*10;d.responseTimeMs=.8f+i;
                    d.readHistory=s.diskReadHistory;d.writeHistory=s.diskWriteHistory;for(int j=0;j<i*2;++j) {d.readHistory.Push(10+i*15);d.writeHistory.Push(i*5);}
                    if(i==0)d.volumes=s.disks;else d.volumes={{i==1 ? L"E:" : L"F:",L"数据",900ULL*1073741824,(200ULL+i*100)*1073741824}};
                    s.physicalDisks.push_back(std::move(d));
                }
                panel.m_selectedDiskId=ids[0];
                if(cpuState.find(L"second")!=std::wstring::npos)panel.m_selectedDiskId=ids[1];
                if(cpuState.find(L"single")!=std::wstring::npos)s.physicalDisks.resize(1);
                if(cpuState.find(L"many")!=std::wstring::npos)for(int i=3;i<9;++i) {auto d=s.physicalDisks.back();d.id=L"fixture-extra-"+std::to_wstring(i);d.number=i;s.physicalDisks.push_back(d);}
                if(cpuState.find(L"long")!=std::wstring::npos) {s.physicalDisks[0].model=L"Very long disk model name that must be truncated without covering the checkmark";}
                if(cpuState.find(L"unavailable")!=std::wstring::npos) {auto& d=s.physicalDisks[0];d.throughputValid=d.statisticsValid=false;d.readHistory={};d.writeHistory={};d.status=L"统计未提供（权限或设备不支持）";d.volumes.clear();}
                if(cpuState.find(L"reorder")!=std::wstring::npos) {panel.m_selectedDiskId=ids[1];std::swap(s.physicalDisks[0],s.physicalDisks[1]);}
                if(cpuState.find(L"removed")!=std::wstring::npos)s.physicalDisks.erase(s.physicalDisks.begin());
                if(cpuState.find(L"empty")!=std::wstring::npos)s.physicalDisks.clear();
                panel.ReconcileDiskSelection();
                panel.m_diskMenuOpen=cpuState.find(L"open")!=std::wstring::npos && !s.physicalDisks.empty();
            }
            panel.m_expandedModule=ProDashboardWindow::Module_Disk;panel.UpdateDimensionsAndPosition();
        }
        if(memoryExpanded) {
            panel.m_hoveredModule=-1;
            auto& s=panel.m_snapshot;
            if(missingMemory) {s.totalPhysMem=0;s.usedPhysMem=0;s.memoryHistory={};}
            else {
                s.memInUseBytes=s.usedPhysMem;s.memStandbyCacheBytes=static_cast<uint64_t>(5.7*1073741824);
                s.memoryCompositionValid=true;
                s.memFreeBytes=s.totalPhysMem-s.memInUseBytes-s.memStandbyCacheBytes;
                s.availPhysMem=s.totalPhysMem-s.usedPhysMem;
                s.memCommittedBytes=static_cast<uint64_t>(22.1*1073741824);s.memCommitLimitBytes=48ULL*1073741824;
                s.topMemoryProcesses={{L"Chrome",static_cast<uint64_t>(3.2*1073741824)},{L"Visual Studio Code",static_cast<uint64_t>(1.4*1073741824)},{L"Figma",static_cast<uint64_t>(.9*1073741824)}};
            }
            if(cpuState.find(L"compact")!=std::wstring::npos) panel.m_settings.layoutMode=1;
            if(cpuState.find(L"pinned")!=std::wstring::npos) panel.m_isPinned=true;
            if(cpuState.find(L"direct")!=std::wstring::npos) panel.m_mode=ProDashboardWindow::Mode_DirectMemory;
            if(!missingMemory) {s.memoryHistory={};for(int i=0;i<60;++i) s.memoryHistory.Push(52+12*std::sin(i*.63f));}
            if(cpuState.find(L"wide")!=std::wstring::npos) {
                s.totalPhysMem=2048ULL*1073741824;s.usedPhysMem=1536ULL*1073741824;s.memInUseBytes=s.usedPhysMem;
                s.memStandbyCacheBytes=256ULL*1073741824;s.memFreeBytes=256ULL*1073741824;s.availPhysMem=512ULL*1073741824;
                s.topMemoryProcesses[0]={L"Very long executable process name which must not overlap memory reading",1024ULL*1073741824};
            }
            if(cpuState.find(L"full")!=std::wstring::npos) {s.usedPhysMem=s.memInUseBytes=s.totalPhysMem;s.memFreeBytes=s.memStandbyCacheBytes=s.availPhysMem=0;}
            if(cpuState.find(L"unsupported")!=std::wstring::npos) s.memoryCompositionValid=false;
            if(cpuState.find(L"gaps")!=std::wstring::npos) {s.memoryHistory={};s.memoryHistory.Push(0);s.memoryHistory.Push(24);s.memoryHistory.Push(std::numeric_limits<float>::quiet_NaN());s.memoryHistory.Push(52);s.memoryHistory.Push(0);}
            panel.m_expandedModule=ProDashboardWindow::Module_Memory;
            panel.UpdateDimensionsAndPosition();
        }
        if(gpuExpanded) {
            panel.m_hoveredModule=-1;
            panel.m_snapshot.gpuName=missingGpu ? L"" : L"NVIDIA GeForce RTX 4060";
            panel.m_snapshot.gpuTempC=missingGpu ? 0 : 46;
            panel.m_snapshot.gpuFreqGHz=missingGpu ? 0 : 2.46f;
            panel.m_snapshot.gpuPowerWatts=missingGpu ? 0 : 42;
            panel.m_snapshot.gpuVramTotalBytes=missingGpu ? 0 : 8ULL*1073741824;
            panel.m_snapshot.gpuVramUsedBytes=missingGpu ? 0 : static_cast<uint64_t>(2.3*1073741824);
            panel.m_snapshot.gpu3dLoadPercent=18;panel.m_snapshot.gpuVideoDecodePercent=6;
            if(!missingGpu) panel.m_snapshot.topGpuProcesses={{L"Chrome",8.4f},{L"Desktop Window Manager",4.6f}};
            else panel.m_snapshot.gpuHistory={};
            if(cpuState.find(L"compact")!=std::wstring::npos) panel.m_settings.layoutMode=1;
            if(cpuState.find(L"pinned")!=std::wstring::npos) panel.m_isPinned=true;
            if(missingGpu) {
                auto& s=panel.m_snapshot;s.gpuUsagePercent=std::numeric_limits<float>::quiet_NaN();
                s.gpu3dLoadPercent=s.gpuVideoDecodePercent=s.gpuVideoEncodePercent=std::numeric_limits<float>::quiet_NaN();
            } else {
                panel.m_snapshot.gpuHistory={};
                for(int i=0;i<60;++i) panel.m_snapshot.gpuHistory.Push(18+12*std::sin(i*.63f)+5*std::cos(i*.3f));
            }
            if(cpuState.find(L"wide")!=std::wstring::npos) {
                auto& s=panel.m_snapshot;s.gpuUsagePercent=100;s.gpuTempC=100;panel.m_settings.tempUnit=1;
                s.gpuName=L"NVIDIA Very Long Graphics Adapter Model Name Which Must Be Ellipsized";
                s.topGpuProcesses[0].name=L"Very long process name which must not overlap its GPU percentage";
            }
            if(cpuState.find(L"gaps")!=std::wstring::npos) {
                auto& h=panel.m_snapshot.gpuHistory;h={};h.Push(0);h.Push(24);h.Push(std::numeric_limits<float>::quiet_NaN());h.Push(18);h.Push(0);
            }
            panel.m_expandedModule=ProDashboardWindow::Module_GPU;
            panel.UpdateDimensionsAndPosition();
        }
        Check(bar.m_renderer.Initialize(panel.GetHwnd(), 1, 1), "bar text factory initialization failed");
        bar.m_snapshot = panel.m_snapshot;
        bar.m_pDashboard->m_settings.tempUnit=panel.m_settings.tempUnit;
        ProMiniBarTooltipWindow peek;
        if(hover>=0) {
            Check(peek.Create(panel.GetHwnd()),"peek renderer initialization failed");
            peek.SetPureWhite(true);peek.m_segmentIndex=hover;peek.ConfigureSize(hover);
            peek.m_snapshot=bar.m_snapshot;peek.m_snapshot.cpuFreqGHz=4.82f;
            peek.m_snapshot.tempHistory.Push(64);bar.m_hoveredSegment=hover;
            if(hoverVariant==1) {peek.m_snapshot={};peek.m_snapshot.cpuFreqGHz=0;peek.m_snapshot.tempHistory={};}
            if(hoverVariant==2) {peek.m_snapshot.usedPhysMem=peek.m_snapshot.totalPhysMem;peek.m_snapshot.cpuUsageTotal=100;peek.m_snapshot.downloadSpeedBps=123456000;peek.m_snapshot.uploadSpeedBps=123456000;}
            if(hoverVariant==3) {peek.SetTempUnit(1);bar.m_pDashboard->m_settings.tempUnit=1;}
            wchar_t left[64],right[64];peek.FormatValues(left,right);
            if(hoverVariant==1) {
                Check(std::wstring(left)==(hover==0 ? L"0%" : hover==3 ? L"0.0" : L"—"),"zero/missing primary fabricated");
                Check(std::wstring(right)==(hover==3 ? L"0.00" : L"—"),"zero/missing secondary fabricated");
            }
            if(hoverVariant==2 && hover==2) Check(std::wstring(right)==L"0.0 GiB","full memory must have zero available");
            if(hoverVariant==3) Check(std::wstring(left)==L"136°F" && std::wstring(right)==L"147°F","Fahrenheit or peak conversion failed");
            if(hover==3 && hoverVariant==0) Check(std::wstring(left)==L"12.8" && std::wstring(right)==L"0.84","network must use decimal MB/s");
            if(hover==1 && hoverVariant==0) Check(std::wstring(right)==L"64°C","peak must come from valid history");
            const float widths[]{154,136,185,144};Check(peek.m_dipWidth==widths[hover] && peek.m_dipHeight==48,"peek geometry changed");
            const RECT work{-1920,-100,0,980};
            const POINT bottom=peek.Place({-400,900,-20,932},work,1,216,4,144,48);
            Check(bottom.x+24+144<=-8 && bottom.y+24+48<900,"peek bottom/right avoidance failed");
            const POINT top=peek.Place({-2000,-200,-1620,-168},work,1,0,4,154,48);
            Check(top.x+24>=work.left+8 && top.y+24>=work.top+8,"peek negative origin avoidance failed");
        }
        const bool appearance=cpuState.rfind(L"appearance",0)==0;
        const bool monitoring=cpuState.rfind(L"monitoring",0)==0;
        const bool general=cpuState.rfind(L"general",0)==0;
        if(general){panel.m_mode=ProDashboardWindow::Mode_Settings;panel.m_settings.activeTab=2;panel.m_settings.general={};panel.m_settings.layoutMode=cpuState.find(L"compact")!=std::wstring::npos?1:0;
            if(cpuState.find(L"all-on")!=std::wstring::npos){auto& g=panel.m_settings.general;g.autoStart=g.alwaysOnTop=g.lockPosition=g.hideInFullscreen=g.rememberPosition=g.reduceMotion=true;}
            if(cpuState.find(L"all-off")!=std::wstring::npos){auto& g=panel.m_settings.general;g.autoStart=g.alwaysOnTop=g.lockPosition=g.hideInFullscreen=g.rememberPosition=g.reduceMotion=false;g.hotkeyModifiers=g.hotkeyKey=0;}
            if(cpuState.find(L"dark")!=std::wstring::npos)panel.m_settings.themeMode=2;
            if(cpuState.find(L"hotkey-edit")!=std::wstring::npos)panel.m_hotkeyCapturing=true;
            if(cpuState.find(L"hotkey-error")!=std::wstring::npos)panel.m_generalError=L"快捷键已被占用，请换一个组合。";
            if(cpuState.find(L"save-failed")!=std::wstring::npos)panel.m_appearanceSaveFailed=true;
            panel.UpdateDimensionsAndPosition();
        }
        if(monitoring){panel.m_mode=ProDashboardWindow::Mode_Settings;panel.m_settings.activeTab=1;panel.m_settings.layoutMode=cpuState.find(L"compact")!=std::wstring::npos?1:0;panel.m_monitoringGroup=cpuState.find(L"indicators")!=std::wstring::npos?0:cpuState.find(L"display")!=std::wstring::npos?1:-1;panel.m_snapshot.gpuName=L"RTX 4060";panel.m_snapshot.netAdapterName=L"Wi-Fi";PhysicalDiskInfo d;d.id=L"monitoring-disk-a";d.model=L"Samsung 980 PRO";panel.m_snapshot.physicalDisks={d};d.id=L"monitoring-disk-b";d.number=1;d.model=L"WD Blue SN580";panel.m_snapshot.physicalDisks.push_back(d);panel.m_selectedDiskId=L"monitoring-disk-a";if(cpuState.find(L"fahrenheit")!=std::wstring::npos){panel.m_settings.tempUnit=1;panel.m_settings.updateIntervalSec=0;panel.m_settings.highTempAlert=0;}if(cpuState.find(L"hidden")!=std::wstring::npos){panel.m_settings.indicatorOrder={3,1,0,2};panel.m_settings.showCpu=false;}panel.UpdateDimensionsAndPosition();}
        if(appearance) {
            panel.m_mode=ProDashboardWindow::Mode_Settings;panel.m_settings.activeTab=0;
            panel.m_settings.layoutMode=cpuState.find(L"compact")!=std::wstring::npos ? 1 : 0;
            panel.m_settings.themeMode=cpuState.find(L"glass")!=std::wstring::npos ? 0 : cpuState.find(L"dark")!=std::wstring::npos ? 2 : cpuState.find(L"system")!=std::wstring::npos ? 3 : 1;
            if(cpuState.find(L"minimum")!=std::wstring::npos)panel.m_settings.glassOpacity=.1f;
            if(cpuState.find(L"maximum")!=std::wstring::npos)panel.m_settings.glassOpacity=1;
            if(cpuState.find(L"save-failed")!=std::wstring::npos)panel.m_appearanceSaveFailed=true;
            panel.UpdateDimensionsAndPosition();
        }
        const bool cpuDirect=cpuExpanded && panel.m_mode==ProDashboardWindow::Mode_DirectCpu;
        if(monitoring)bar.m_pDashboard->m_settings=panel.m_settings;
        const bool tempDirect=panel.m_mode==ProDashboardWindow::Mode_DirectTemp;
        const bool dataState=cpuState.rfind(L"state-",0)==0;
        const bool disconnected=dataState&&cpuState.find(L"disconnected")!=std::wstring::npos;
        if(dataState) {
            auto& s=panel.m_snapshot;s.cpuBrand=L"AMD Ryzen 7 7700";s.cpuFreqGHz=4.82f;s.tempHistory={};s.cpuTempC=58;
            if(cpuState==L"state-reading"||cpuState==L"state-unsupported")s.cpuTempC=0;
            else for(int i=0;i<(cpuState==L"state-partial" ? 12 : 60);++i)s.tempHistory.Push(54+4*std::sin(i*.6f));
            if(cpuState==L"state-reading")s.cpuPowerWatts=s.cpuFreqGHz=0,s.cpuFanRpm=-1;
            if(cpuState==L"state-unsupported")s.temperatureStatus.state=MetricDataState::Unsupported;
            if(cpuState==L"state-stale") {s.temperatureStatus.state=MetricDataState::Stale;s.temperatureStatus.lastValidMs=GetTickCount64()-8000;}
            panel.m_mode=disconnected ? ProDashboardWindow::Mode_FullAccordion : ProDashboardWindow::Mode_DirectTemp;
            if(disconnected){panel.m_expandedModule=ProDashboardWindow::Module_Disk;panel.m_diskDisconnected=true;panel.m_lastDiskModel=L"Samsung 980 PRO";}
            panel.UpdateDimensionsAndPosition();
            MetricSampleStatus status;MetricHistory hist;float last=0;status.Record(58,last,hist,1000);status.Record(NAN,last,hist,2000);
            Check(last==58&&status.state==MetricDataState::Stale,"failed sample replaced last value");status.Record(60,last,hist,10000);
            Check(hist.Size()==10&&!std::isfinite(hist[8])&&hist[9]==60,"recovery fabricated missing samples");
            status.Record(99,last,hist,10000);Check(last==60&&hist.Size()==10,"UI poll duplicated sample");
            Check(status.Resolve(true,2,14000)==MetricDataState::Stale,"stale timeout failed");
            Check(panel.m_dipHeight==(disconnected ? 205 : 289),"state height differs from Figma");
            if(disconnected) {
                auto connected=panel.m_snapshot;PhysicalDiskInfo d;d.id=L"fixture-state-disk";d.model=L"Samsung 980 PRO";connected.physicalDisks={d};panel.m_selectedDiskId=d.id;panel.UpdateSnapshot(connected);
                Check(!panel.m_diskDisconnected,"connected disk marked disconnected");
                auto removed=connected;removed.physicalDisks.clear();panel.UpdateSnapshot(removed);
                Check(panel.IsDisconnectedDisk()&&panel.m_dipHeight==205&&panel.m_selectedDiskId==d.id,"removal lost device identity or wrong state height");
                panel.UpdateSnapshot(connected);Check(!panel.m_diskDisconnected&&panel.SelectedDisk(),"same-device reconnect did not recover");
                panel.UpdateSnapshot(removed);
            }
        }
        const bool memDirect=memoryExpanded && panel.m_mode==ProDashboardWindow::Mode_DirectMemory;
        const bool netDirect=networkExpanded && panel.m_mode==ProDashboardWindow::Mode_DirectNetwork;
        const bool cpuCompact=cpuExpanded && panel.m_settings.layoutMode==1;
        const bool limited=cpuState.find(L"limited")!=std::wstring::npos;
        if(limited){panel.m_cpuFullHeight=panel.m_dipHeight;panel.m_dipHeight=420;if(cpuState.find(L"scrolled")!=std::wstring::npos)panel.m_cpuScroll=panel.m_cpuFullHeight-panel.m_dipHeight;}
        const auto motionAt=cpuState.find(L"motion-");const bool motion=motionAt!=std::wstring::npos;
        float motionCanvas=0;
        if(motion){const int target=panel.m_expandedModule;const auto targetLayout=panel.MotionLayout();panel.m_expandedModule=cpuState.find(L"switch")!=std::wstring::npos?ProDashboardWindow::Module_CPU:ProDashboardWindow::Module_None;panel.UpdateDimensionsAndPosition();if(limited)panel.m_dipHeight=std::min(panel.m_dipHeight,420.f);auto source=panel.MotionLayout();panel.m_expandedModule=target;panel.UpdateDimensionsAndPosition();
            const bool close=cpuState.find(L"close")!=std::wstring::npos;const auto from=close?targetLayout:source,to=close?source:targetLayout;panel.m_expandedModule=close?-1:target;
            panel.m_panelMotion.Start(from,to,1000);panel.m_panelMotion.Tick(1000+_wtoi(cpuState.substr(motionAt+7).c_str()));panel.m_dipHeight=panel.m_panelMotion.current.height;motionCanvas=std::max(from.height,to.height)+76;
        }
        const int w = static_cast<int>(420 * scale), h = static_cast<int>((motion?motionCanvas:limited || dataState || general || appearance || monitoring ? panel.m_dipHeight+76 : hover>=0 ? 140 : cpuExpanded ? panel.m_dipHeight+76 : networkExpanded ? panel.m_dipHeight+76 : gpuExpanded ? 699 : memoryExpanded ? panel.m_dipHeight+76 : diskExpanded ? panel.m_dipHeight+76 : sensorMode ? panel.m_dipHeight+76 : 360) * scale);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* pixels = nullptr;
        HDC dc = CreateCompatibleDC(nullptr);
        HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        Check(dib && pixels, "offscreen DIB failed");
        auto old = SelectObject(dc, dib);
        ComPtr<ID2D1DCRenderTarget> rt;
        auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        Check(SUCCEEDED(panel.m_renderer.GetD2DFactory()->CreateDCRenderTarget(&properties, &rt)), "D2D target failed");
        RECT bounds{0,0,w,h}; rt->BindDC(dc, &bounds); rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0xF2F4F7)); rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        const auto dpi = D2D1::Matrix3x2F::Scale(scale, scale);
        rt->SetTransform(D2D1::Matrix3x2F::Translation(20, 12) * dpi);
        bar.DrawWhiteBar(rt.Get());
        if(hover>=0) {
            const float offsets[]{0,44,90,216};
            rt->SetTransform(D2D1::Matrix3x2F::Translation(-1000,-1000)*dpi);
            if(dataState){if(disconnected)panel.DrawWhiteDisconnectedDisk(rt.Get());else panel.DrawWhiteTempDirect(rt.Get());}else if(general)panel.DrawSettingsGeneral(rt.Get());else if(monitoring)panel.DrawSettingsMonitoring(rt.Get());else if(appearance) panel.DrawSettingsAppearance(rt.Get());else if(tempDirect) panel.DrawWhiteTempDirect(rt.Get());else if(cpuDirect) panel.DrawWhiteCpuDirect(rt.Get());else if(memDirect) panel.DrawWhiteMemoryDirect(rt.Get());else if(netDirect) panel.DrawWhiteNetworkDirect(rt.Get());else panel.DrawWhiteOverview(rt.Get());
            rt->SetTransform(D2D1::Matrix3x2F::Translation(20+offsets[hover],hover==1 ? 48.5f : 48.f)*dpi);
            peek.DrawWhitePeek(rt.Get());
        } else {
            rt->SetTransform(D2D1::Matrix3x2F::Translation(30, 60) * dpi);
            if(dataState){if(disconnected)panel.DrawWhiteDisconnectedDisk(rt.Get());else panel.DrawWhiteTempDirect(rt.Get());}else if(general)panel.DrawSettingsGeneral(rt.Get());else if(monitoring)panel.DrawSettingsMonitoring(rt.Get());else if(appearance) panel.DrawSettingsAppearance(rt.Get());else if(tempDirect) panel.DrawWhiteTempDirect(rt.Get());else if(cpuDirect) panel.DrawWhiteCpuDirect(rt.Get());else if(memDirect) panel.DrawWhiteMemoryDirect(rt.Get());else if(netDirect) panel.DrawWhiteNetworkDirect(rt.Get());else panel.DrawWhiteOverview(rt.Get());
        }
        // Missing embedded assets must fail validation, not silently produce blank controls.
        rt->SetTransform(D2D1::Matrix3x2F::Translation(-1000,-1000)*dpi);
        for (int id = 201; id <= 216; ++id)
            Check(panel.m_overviewVisuals.DrawIcon(rt.Get(), static_cast<ProOverviewVisuals::Icon>(id), D2D1::RectF(-50,-50,-36,-36)), "missing/invalid icon resource");
        Check(SUCCEEDED(rt->EndDraw()), "D2D drawing failed");
        if(!motion) {
        const float extra=cpuExpanded ? panel.CpuCoreExtraHeight() : 0;
        if(!dataState && !general && !monitoring && !appearance && !tempDirect && !cpuDirect && !memDirect && !netDirect) Check(panel.m_closeButtonRect.left == 321 && panel.m_settingsFooterBtnRect.top == (cpuExpanded || networkExpanded || gpuExpanded || memoryExpanded || diskExpanded || sensorMode ? panel.m_dipHeight-33 : 251), "control geometry changed");
        if(general){
            Check(panel.m_dipHeight==260&&panel.m_shadowApron==40&&panel.m_generalHotkeyRect.top==218,"general dimensions incorrect");
            for(int i=0;i<6;++i)Check(panel.m_generalToggleRects[i].top==84+i*22&&panel.m_generalToggleRects[i].right==349,"general toggle geometry incorrect");
            bool notified=false;panel.m_onSettingsChanged=[&](const auto&){notified=true;};
            panel.m_hotkeyCapturing=false;panel.m_generalFocus=-1;Check(panel.OnGeneralKey(VK_TAB,false,false)&&panel.m_generalFocus==0,"general Tab focus failed");panel.m_generalFocus=-1;Check(panel.OnGeneralKey(VK_TAB,true,false)&&panel.m_generalFocus==10,"general Shift+Tab focus failed");
            const auto previous=panel.m_settings.general;panel.OnClick(330,138);Check(notified&&panel.m_settings.general.lockPosition!=previous.lockPosition&&panel.m_isPinned==false,"position lock changed pin state or failed notification");
            panel.m_onGeneralChange=[](auto&,std::wstring& error){error=L"模拟权限失败";return false;};const auto before=panel.m_settings.general;notified=false;panel.OnClick(330,94);Check(panel.m_settings.general.autoStart==before.autoStart&&!notified&&!panel.m_generalError.empty(),"startup failure did not preserve state");
            panel.m_onGeneralChange={};panel.OnClick(250,226);Check(panel.m_hotkeyCapturing,"shortcut edit did not start");Check(panel.OnGeneralKey(VK_ESCAPE,false,false)&&!panel.m_hotkeyCapturing,"shortcut Esc did not cancel edit");
            Check(!panel.CommitGeneralHotkey(0,'J'),"unsafe unmodified hotkey accepted");Check(panel.CommitGeneralHotkey(MOD_CONTROL|MOD_ALT|MOD_SHIFT,VK_F12)&&panel.m_settings.general.hotkeyKey==VK_F12,"shortcut commit failed");
            const auto shortcut=panel.m_settings.general;panel.m_onGeneralChange=[](auto&,std::wstring& error){error=L"快捷键已被占用，请换一个组合。";return false;};Check(!panel.CommitGeneralHotkey(MOD_CONTROL|MOD_ALT,'K')&&panel.m_settings.general.hotkeyKey==shortcut.hotkeyKey,"conflict replaced previous shortcut");panel.m_onGeneralChange={};
            const auto clamped=ProWindowBehavior::ClampVisiblePosition({-4000,2000},{570,48},{-1920,-200,0,880},12);Check(clamped.x==-1908&&clamped.y==820,"saved negative-monitor position not clamped");
            Check(ProWindowBehavior::CoversMonitor({-1920,0,0,1080},{-1920,0,0,1080})&&!ProWindowBehavior::CoversMonitor({0,0,1920,1040},{0,0,1920,1080}),"fullscreen confused with maximized work area");
            wchar_t temporary[MAX_PATH];auto directory=std::filesystem::path(output).parent_path();Check(GetTempFileNameW(directory.c_str(),L"pcg",0,temporary)!=0,"general fixture creation failed");ProGeneralSettings config;config.alwaysOnTop=false;config.lockPosition=true;config.hideInFullscreen=false;config.reduceMotion=true;config.positionValid=true;config.positionX=-1720;config.positionY=120;config.savedDpi=144;config.hotkeyModifiers=MOD_CONTROL|MOD_SHIFT;config.hotkeyKey=VK_F12;
            WritePrivateProfileStringW(L"Unrelated",L"Keep",L"yes",temporary);Check(ConfigManager::SaveProGeneral(config,temporary),"general config save failed");const auto restored=ConfigManager::LoadProGeneral(temporary);wchar_t keep[8];GetPrivateProfileStringW(L"Unrelated",L"Keep",L"",keep,8,temporary);
            Check(!restored.alwaysOnTop&&restored.lockPosition&&!restored.hideInFullscreen&&restored.reduceMotion&&restored.positionValid&&restored.positionX==-1720&&restored.savedDpi==144&&restored.hotkeyKey==VK_F12&&std::wstring(keep)==L"yes","general config roundtrip failed");WritePrivateProfileStringW(L"ProGeneral",L"HotkeyModifiers",L"-1",temporary);WritePrivateProfileStringW(L"ProGeneral",L"RememberPosition",L"0",temporary);const auto sanitized=ConfigManager::LoadProGeneral(temporary);Check(sanitized.hotkeyKey=='I'&&sanitized.hotkeyModifiers==(MOD_CONTROL|MOD_ALT)&&!sanitized.positionValid,"general invalid config not sanitized");DeleteFileW(temporary);
        }
        if(monitoring){
            Check(panel.m_shadowApron==40&&panel.m_dipHeight==(panel.m_monitoringGroup==0?319:panel.m_monitoringGroup==1?278:212),"monitoring dimensions incorrect");
            Check(panel.m_monitoringHeaders[0].top==72&&panel.m_monitoringDeviceRects[0].top==panel.m_dipHeight-67,"monitoring geometry incorrect");
            const auto savedOrder=panel.m_settings.indicatorOrder;
            Check(panel.OnMonitoringKey(VK_TAB,false,false)&&panel.m_monitoringFocus==-10,"monitoring keyboard focus failed");
            if(panel.m_monitoringGroup==0){panel.m_monitoringFocus=2;auto order=panel.m_settings.indicatorOrder;Check(panel.OnMonitoringKey(VK_DOWN,false,true),"keyboard reorder not handled");panel.m_settings.indicatorOrder=order;panel.m_monitoringFocus=-1;}
            if(cpuState.find(L"hidden")!=std::wstring::npos)Check(bar.m_segmentRects[0].left<0&&bar.m_segmentRects[3].left==9,"bar did not hide or reorder groups");
            bool notified=false;panel.m_onSettingsChanged=[&](const auto&){notified=true;};
            if(panel.m_monitoringGroup==0){const bool before=panel.m_settings.showTemp;const auto r=panel.m_metricToggleRects[1];panel.OnClick((r.left+r.right)/2,(r.top+r.bottom)/2);Check(notified&&panel.m_settings.showTemp!=before,"indicator toggle not applied");panel.m_settings.showTemp=before;}
            panel.ApplyMonitoringChoice(0,2);panel.ApplyMonitoringChoice(1,1);panel.ApplyMonitoringChoice(2,90);panel.ApplyMonitoringChoice(3,1);
            Check(notified&&panel.m_settings.updateIntervalSec==2&&panel.m_settings.tempUnit==1&&panel.m_settings.highTempAlert==90&&panel.m_alertTempThreshold==90&&panel.m_selectedDiskId==L"monitoring-disk-b","monitoring selection not applied");
            notified=false;panel.ApplyMonitoringChoice(2,101);Check(!notified&&panel.m_settings.highTempAlert==90,"invalid monitoring choice accepted");
            panel.OnClick(20,80);Check(panel.m_monitoringGroup== (cpuState.find(L"indicators")!=std::wstring::npos?-1:0),"indicator accordion failed");
            panel.m_monitoringGroup=0;panel.UpdateDimensionsAndPosition();panel.m_monitoringHeaders[1]=D2D1::RectF(11,218,349,248);panel.OnClick(20,230);Check(panel.m_monitoringGroup==1&&panel.m_dipHeight==278&&panel.m_settings.indicatorOrder==savedOrder,"accordion lost state or allowed both open");
            panel.m_monitoringGroup=0;panel.m_settings.indicatorOrder={0,1,2,3};panel.m_indicatorDragging=0;panel.MoveMonitoringIndicator(198);Check(panel.m_settings.indicatorOrder==std::array<int,4>{1,2,3,0},"indicator drag ordering failed");panel.m_indicatorDragging=-1;
            wchar_t temporary[MAX_PATH];const auto directory=std::filesystem::path(output).parent_path();Check(GetTempFileNameW(directory.c_str(),L"pcm",0,temporary)!=0,"monitoring fixture creation failed");
            ConfigManager::ProMonitoring config{{false,true,false,true},{3,1,0,2},0,1,0,L"fixture-id"};Check(ConfigManager::SaveProMonitoring(config,temporary),"monitoring config save failed");const auto restored=ConfigManager::LoadProMonitoring(temporary);Check(restored.visible==config.visible&&restored.order==config.order&&restored.interval==0&&restored.unit==1&&restored.alert==0&&restored.diskId==config.diskId,"monitoring config roundtrip failed");WritePrivateProfileStringW(L"ProMonitoring",L"Order0",L"2",temporary);WritePrivateProfileStringW(L"ProMonitoring",L"AlertCelsius",L"101",temporary);const auto sanitized=ConfigManager::LoadProMonitoring(temporary);Check(sanitized.order==std::array<int,4>{0,1,2,3}&&sanitized.alert==85,"invalid config not sanitized");DeleteFileW(temporary);
        }
        if(appearance) {
            wchar_t temporary[MAX_PATH];const auto directory=std::filesystem::path(output).parent_path();
            Check(GetTempFileNameW(directory.c_str(),L"pca",0,temporary)!=0,"configuration fixture creation failed");
            WritePrivateProfileStringW(L"Unrelated",L"Keep",L"yes",temporary);
            Check(ConfigManager::SaveProAppearance({3,1,.73f},temporary),"appearance config write failed");
            const auto saved=ConfigManager::LoadProAppearance(temporary);wchar_t untouched[8];GetPrivateProfileStringW(L"Unrelated",L"Keep",L"",untouched,8,temporary);
            Check(saved.themeMode==3&&saved.layoutMode==1&&std::abs(saved.glassOpacity-.73f)<.001f&&std::wstring(untouched)==L"yes","appearance config roundtrip or section preservation failed");
            DeleteFileW(temporary); // Only the file just created by GetTempFileNameW.
            Check(panel.m_dipHeight==(panel.m_settings.themeMode==0 ? 259 : 201),"appearance height changed");
            Check(panel.m_shadowApron==40 && panel.m_tabRects[0].top==33 && panel.m_themeRects[0].top==89 && panel.m_layoutOptionRects[0].top==146,"appearance geometry changed");
            Check((panel.m_sliderTrackRect.right>panel.m_sliderTrackRect.left)==(panel.m_settings.themeMode==0),"opacity shown for wrong theme");
            const float before=panel.m_settings.glassOpacity;bool changed=false;
            panel.m_onSettingsChanged=[&](const auto&){changed=true;};
            panel.OnClick(190,160);Check(changed&&panel.m_settings.layoutMode==0,"full rows selection not notified");
            changed=false;panel.OnClick(20,160);Check(changed&&panel.m_settings.layoutMode==1,"compact selection not notified");
            panel.OnClick(110,100);Check(panel.m_settings.themeMode==1&&panel.m_dipHeight==201&&panel.m_settings.glassOpacity==before,"white changed saved opacity");
            changed=false;panel.OnClick(180,236);Check(!changed&&panel.m_settings.glassOpacity==before,"hidden opacity remains clickable");
            panel.OnClick(20,100);Check(panel.m_settings.themeMode==0&&panel.m_dipHeight==259,"glass expansion failed");
            panel.OnClick(18,236);Check(std::abs(panel.m_settings.glassOpacity-.1f)<.001f,"minimum opacity incorrect");
            panel.OnClick(342,236);Check(std::abs(panel.m_settings.glassOpacity-1.f)<.001f,"maximum opacity incorrect");
            panel.OnClick(270,45);Check(panel.m_settings.activeTab==2&&panel.m_dipHeight==260,"tab selection did not resize");
        }
        if(sensorMode && !tempDirect) {Check(panel.IsWhiteOverview(),"sensor not using calibrated renderer");Check(panel.m_dipHeight==(panel.m_settings.layoutMode==1 ? 586 : 664),"sensor panel height changed");}
        if(tempDirect) {
            Check(panel.m_dipHeight==289 && panel.m_shadowApron==40,"temperature panel geometry changed");
            Check(panel.m_tempChartRect.left==11 && panel.m_tempChartRect.right==349 && panel.m_tempChartRect.top==100,"temperature chart hit changed");
            Check(panel.m_tempAlertRect.top==267 && panel.m_allSensorsRect.bottom==284,"temperature actions changed");
            if(cpuState.find(L"off")!=std::wstring::npos) {
                bool changed=false;panel.m_onSettingsChanged=[&](const auto& settings) {changed=settings.highTempAlert==75;};
                panel.OnClick(20,275);Check(changed && panel.m_settings.highTempAlert==75,"temperature threshold not persisted");
            }
        }
        if(diskExpanded) {Check(panel.IsWhiteOverview(),"disk not using calibrated renderer");Check(panel.m_dipHeight==(panel.m_settings.layoutMode==1 ? 538 : 616),"disk panel height changed");}
        if(diskExpanded && cpuState.find(L"selector")!=std::wstring::npos) {
            const auto* selected=panel.SelectedDisk();
            Check(selected || panel.m_snapshot.physicalDisks.empty(),"selection lost");
            if(cpuState.find(L"reorder")!=std::wstring::npos)Check(selected&&selected->id==L"fixture-device-b","reorder changed selected identity");
            if(cpuState.find(L"removed")!=std::wstring::npos)Check(selected&&selected->id==L"fixture-device-b","removed device not reconciled");
            if(panel.m_diskMenuOpen) {
                Check(panel.m_diskMenuRect.right-panel.m_diskMenuRect.left==256,"menu width changed");
                Check(panel.m_diskMenuRect.top==(panel.m_settings.layoutMode==1 ? 171 : 211),"menu position changed");
                Check(panel.m_diskOptionRects.size()==std::min<size_t>(5,panel.m_snapshot.physicalDisks.size()),"menu row count changed");
                Check(panel.m_diskOptionRects[0].bottom-panel.m_diskOptionRects[0].top==38,"menu row height changed");
                if(panel.m_snapshot.physicalDisks.size()>1) {
                    const auto id=panel.m_diskOptionIds[1];const auto box=panel.m_diskOptionRects[1];panel.OnClick(box.left+8,box.top+10);
                    Check(!panel.m_diskMenuOpen&&panel.SelectedDisk()->id==id,"mouse selection failed");
                    Check(panel.SelectedDisk()->volumes[0].drive==L"E:" || cpuState.find(L"reorder")!=std::wstring::npos || cpuState.find(L"removed")!=std::wstring::npos,"volume did not switch");
                }
                panel.m_diskMenuOpen=true;Check(panel.OnDiskKey(VK_ESCAPE)&&!panel.m_diskMenuOpen,"Escape failed to dismiss menu first");
                panel.m_diskMenuOpen=true;Check(panel.OnDiskKey(VK_END),"End navigation failed");const auto id=panel.m_snapshot.physicalDisks.back().id;
                Check(panel.OnDiskKey(VK_RETURN)&&panel.SelectedDisk()->id==id&&!panel.m_diskMenuOpen,"keyboard selection failed");
                panel.m_diskMenuOpen=true;panel.OnClick(350,500);Check(!panel.m_diskMenuOpen&&panel.m_expandedModule==ProDashboardWindow::Module_Disk,"outside click changed module");
            }
        }
        if(memoryExpanded) Check(panel.m_dipHeight==(memDirect ? 387 : panel.m_settings.layoutMode==1 ? 573 : 651),"memory panel height changed");
        if(gpuExpanded) {Check(panel.IsWhiteOverview(),"GPU does not use calibrated renderer");if(!limited)Check(panel.m_dipHeight==(panel.m_settings.layoutMode==1 ? 545 : 623),"GPU panel height changed");}
        if (cpuExpanded) {
            if(!limited&&cpuState.find(L"viewport")==std::wstring::npos) Check(panel.m_dipHeight==(cpuDirect ? 373 : cpuCompact ? 559 : 637)+extra,"CPU panel height changed");
            if(!cpuDirect && panel.m_cpuScroll==0) Check(panel.m_moduleRects[0].top==50 && panel.m_moduleRects[0].bottom==75,"CPU title collapse target changed");
            Check(panel.m_cpuCoreRects.size()==panel.m_snapshot.cpuCoreUsage.size(),"core count fabricated or lost");
            for(size_t i=0;i<panel.m_cpuCoreRects.size();++i) {
                const auto& r=panel.m_cpuCoreRects[i];Check(r.right-r.left==24 && r.bottom-r.top==28,"bucket dimensions changed");
                Check(r.left>=23 && r.right<=338,"bucket outside content");
                if(i%8) Check(r.left-panel.m_cpuCoreRects[i-1].right==12,"bucket row gap changed");
            }
            if(!panel.m_cpuCoreRects.empty()) {
                const auto first=panel.m_cpuCoreRects[0];
                if(first.top>=50) Check(panel.HitCpuCore(first.left+12,first.top+14)==0,"core hit test misses center");
                panel.m_focusedCpuCore=-1;Check(panel.OnCpuKey(VK_TAB,false) && panel.m_focusedCpuCore==0,"Tab focus failed");
                Check(panel.OnCpuKey(VK_TAB,true) && panel.m_focusedCpuCore==static_cast<int>(panel.m_cpuCoreRects.size())-1,"Shift+Tab focus failed");
                Check(panel.OnCpuKey(VK_HOME,false) && panel.m_focusedCpuCore==0,"Home focus failed");
                if(panel.m_cpuCoreRects.size()>8) Check(panel.OnCpuKey(VK_DOWN,false) && panel.m_focusedCpuCore==8,"vertical core navigation failed");
            } else Check(!panel.OnCpuKey(VK_TAB,false),"missing cores receive keyboard focus");
        }
        if(limited){Check(panel.m_dipHeight==420&&panel.m_settingsFooterBtnRect.top==387&&panel.m_pinButtonRect.top==9,"limited fixed shell geometry wrong");for(const auto& r:panel.m_moduleRects)if(r.right>r.left)Check(r.top>=50&&r.bottom<=374,"hidden module hit target leaks into fixed shell");}
        for (int i = limited || dataState || general || monitoring || appearance || tempDirect || cpuDirect || memDirect || netDirect ? 6 : cpuExpanded ? 1 : 0; i < 6; ++i) {
            const auto box = panel.m_moduleRects[i];
            if(cpuCompact) {Check(box.left==(i%2 ? 11 : 183) && box.top==417+(i-1)/2*34+extra-panel.m_cpuScroll && box.bottom-box.top==28,"CPU compact capsule geometry changed");continue;}
            if(cpuExpanded && cpuState.find(L"viewport")!=std::wstring::npos) continue;
            if(memoryExpanded && panel.m_settings.layoutMode==1) {
                if(i==2) Check(box.top==84 && box.bottom==113,"memory compact title hit changed");
                else Check(box.top==(i<2 ? 50 : 465+(i-3)/2*34) && box.left==((i<2 ? i : i-3)%2 ? 183 : 11) && box.bottom-box.top==28,"memory capsule geometry changed");
                continue;
            }
            if(networkExpanded && panel.m_settings.layoutMode==1) {
                if(i==4) Check(box.top==118 && box.bottom==143,"network compact title changed");
                else Check(box.top==(i<4 ? 50+i/2*34 : 413) && box.left==(i<4 && i%2 ? 183 : 11) && box.bottom-box.top==28,"network compact capsule changed");
                continue;
            }
            if(networkExpanded && i==4) {Check(box.top==194 && box.bottom==219,"network collapse target changed");continue;}
            if(gpuExpanded && panel.m_settings.layoutMode==1) {
                if(i==1) Check(box.top==84 && box.bottom==109,"GPU compact collapse target changed");
                else Check(box.top==(i==0 ? 50 : 437+(i-2)/2*34) && box.left==(i==0 || i%2==0 ? 11 : 183) && box.bottom-box.top==28,"GPU compact capsule geometry changed");
                Check(panel.m_dipHeight==545,"GPU compact height changed");continue;
            }
            if(gpuExpanded && i==1) {Check(box.top==86 && box.bottom==111,"GPU collapse target changed");continue;}
            if(memoryExpanded && i==2) {Check(box.top==122 && box.bottom==151,"memory collapse target changed");continue;}
            if(diskExpanded && panel.m_settings.layoutMode==1) {
                if(i==3) Check(box.top==118 && box.bottom==143,"disk compact title changed");
                else Check(box.top==(i<3 ? 50+i/2*34 : 464) && box.left==((i<3 ? i : i-4)%2 ? 183 : 11) && box.bottom-box.top==28,"disk compact capsule changed");
                continue;
            }
            if(diskExpanded && i==3) {Check(box.top==158 && box.bottom==183,"disk collapse target changed");continue;}
            if(sensorMode && panel.m_settings.layoutMode==1) {
                if(i==5) Check(box.top==152 && box.bottom==177,"sensor compact title changed");
                else Check(box.top==50+i/2*34 && box.left==(i%2 ? 183 : 11) && box.bottom-box.top==28 && box.right-box.left==(i==4 ? 338 : 166),"sensor capsule geometry changed");
                continue;
            }
            if(sensorMode && i==5) {Check(box.top==230 && box.bottom==255,"sensor collapse target changed");continue;}
            Check(box.left == 11 && box.right == 349 && box.top == (cpuExpanded ? 417+36*(i-1)+extra : networkExpanded ? (i==5 ? 489 : 50+36*i) : gpuExpanded ? (i==0 ? 50 : 439+36*(i-2)) : memoryExpanded ? (i<2 ? 50+36*i : 503+36*(i-3)) : diskExpanded ? (i<3 ? 50+36*i : 504+36*(i-4)) : sensorMode ? 50+36*i : 42+34*i) && box.bottom-box.top == 30, "row geometry changed");
        }
        if(networkExpanded) {
            Check(panel.m_dipHeight==(netDirect ? 301 : panel.m_settings.layoutMode==1 ? 487 : 565),"network panel height changed");
            Check(panel.m_netChartRect.left==24 && panel.m_netChartRect.top==(netDirect ? 103 : panel.m_settings.layoutMode==1 ? 209 : 285) && panel.m_netChartRect.right==338,"network chart hit geometry changed");
        }
        }
        if(motion){const float top=42+8*panel.m_panelMotion.current.expanded,bottom=panel.m_dipHeight-40-6*panel.m_panelMotion.current.expanded;for(const auto& r:panel.m_moduleRects)if(r.right>r.left)Check(r.top>=top-.01f&&r.bottom<=bottom+.01f,"animation hits escape visible body");}
        std::vector<D2D1_RECT_F> visibleSegments;for(const auto& r:bar.m_segmentRects)if(r.left>=0)visibleSegments.push_back(r);
        std::sort(visibleSegments.begin(),visibleSegments.end(),[](const auto& a,const auto& b){return a.left<b.left;});
        for(size_t i=1;i<visibleSegments.size();++i)Check(visibleSegments[i-1].right<visibleSegments[i].left,"bar hit targets overlap");
        auto* factory = panel.m_renderer.GetDWriteFactory();
        ComPtr<IDWriteFontCollection> fonts; factory->GetSystemFontCollection(&fonts);
        UINT32 index = 0; BOOL exists = FALSE; fonts->FindFamilyName(L"Noto Sans SC", &index, &exists);
        Check(exists, "Figma font Noto Sans SC is not installed (runtime uses fallback)");
        const float heading = panel.m_overviewVisuals.Measure(factory, L"系统运行概览", 16, DWRITE_FONT_WEIGHT_BOLD);
        Check(heading <= 96.1f, "heading exceeds Figma slot");
        const float value = panel.m_overviewVisuals.Measure(factory, L"16.6 / 32 GiB", 13, DWRITE_FONT_WEIGHT_MEDIUM);
        Check(value < 124, "memory value clips");
        if(cpuExpanded) Check(std::abs(panel.m_overviewVisuals.Measure(factory,L"111",12,DWRITE_FONT_WEIGHT_BOLD)-panel.m_overviewVisuals.Measure(factory,L"888",12,DWRITE_FONT_WEIGHT_BOLD))<.1f,"core digits must have equal advance");
        ComPtr<IWICImagingFactory> wic;
        ComPtr<IWICBitmap> bitmap; ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder; ComPtr<IWICBitmapFrameEncode> frame;
        Check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))), "WIC factory failed");
        Check(SUCCEEDED(wic->CreateBitmapFromMemory(w,h,GUID_WICPixelFormat32bppPBGRA,w*4,w*h*4,static_cast<BYTE*>(pixels),&bitmap)), "WIC bitmap failed");
        wic->CreateStream(&stream); Check(SUCCEEDED(stream->InitializeFromFilename(output, GENERIC_WRITE)), "output path failed");
        wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder); encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        encoder->CreateNewFrame(&frame, nullptr); frame->Initialize(nullptr); frame->SetSize(w,h);
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA; frame->SetPixelFormat(&format);
        Check(SUCCEEDED(frame->WriteSource(bitmap.Get(), nullptr)) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit()), "PNG encode failed");
        if(cpuExpanded) {
            panel.m_focusedCpuCore=8;panel.m_hoveredCpuCore=8;
            auto removed=panel.m_snapshot;removed.cpuCoreUsage.clear();
            panel.UpdateSnapshot(removed);
            Check(panel.m_focusedCpuCore==-1 && panel.m_hoveredCpuCore==-1 && !panel.OnCpuKey(VK_TAB,false),"removed core retains focus or hover");
        }
        rt.Reset(); SelectObject(dc, old); DeleteObject(dib); DeleteDC(dc);
        DestroyWindow(panel.GetHwnd());
        std::printf("PASS %s %.0f%%: layout, typography and 16 embedded icons (%zu %s)\n",general?"general":monitoring?"monitoring":appearance?"appearance":hover>=0 ? "hover peek" : sensorMode ? "sensor" : diskExpanded ? "disk" : memoryExpanded ? "memory" : gpuExpanded ? "GPU" : networkExpanded ? "network" : cpuExpanded ? "CPU" : "overview", scale*100,cores,networkExpanded ? "samples" : "cores");
    }
};

int wmain(int argc, wchar_t** argv) {
    if (argc < 3 || argc > 5) { std::puts("Usage: OverviewRenderTest <DPI scale> <output.png> [cpu|gpu|memory|disk|sensor|network|hover-cpu|hover-temp|hover-memory|hover-network] [count]; variants: *-missing, hover-*-wide, hover-temp-fahrenheit, sensor-fahrenheit, sensor-gaps, network-plain/left/right"); return 2; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const std::wstring mode=argc>=4 ? argv[3] : L"";
    const bool network=mode.rfind(L"network",0)==0;
    const float probe=mode==L"network-plain" ? -1 : mode==L"network-left" ? 24 : mode==L"network-right" ? 338 : 212.4f;
    const int sensorMode=(mode.rfind(L"sensor",0)==0 || mode.rfind(L"temperature",0)==0) ? (mode.find(L"missing")!=std::wstring::npos ? 2 : mode.find(L"fahrenheit")!=std::wstring::npos ? 3 : mode.find(L"gaps")!=std::wstring::npos ? 4 : 1) : 0;
    const int hover=mode.rfind(L"hover-cpu",0)==0 ? 0 : mode.rfind(L"hover-temp",0)==0 ? 1 : mode.rfind(L"hover-memory",0)==0 ? 2 : mode.rfind(L"hover-network",0)==0 ? 3 : -1;
    const int variant=mode.find(L"missing")!=std::wstring::npos ? 1 : mode.find(L"wide")!=std::wstring::npos ? 2 : mode.find(L"fahrenheit")!=std::wstring::npos ? 3 : 0;
    try {
        if(mode==L"general-native"){ProOverviewRenderTest::NativeGeneral();CoUninitialize();return 0;}
        if(mode==L"placement-native"){ProOverviewRenderTest::PlacementRules();CoUninitialize();return 0;}
        if(mode==L"motion-native"){ProOverviewRenderTest::MotionRules();CoUninitialize();return 0;}
        if(mode==L"state-live"||mode==L"lenovo-live") {
            MetricsCollector collector;collector.Start();std::this_thread::sleep_for(std::chrono::seconds(2));auto s=collector.GetLatestSnapshot();collector.Stop();
            Check(s.cpuPowerWatts==0,"collector synthesizes CPU power");
            if(s.cpuTempC==0)Check(s.tempHistory.Empty(),"invalid temperature creates history");
            std::printf("Native sensors: GPU %.1f C, %.2f W; disk %.1f C; CPU %.1f C; fans %d: %d / %d RPM; permission required %d\n",s.gpuTempC,s.gpuPowerWatts,s.ssdTempC,s.cpuTempC,s.laptopFanCount,s.cpuFanRpm,s.caseFanRpm,s.sensorPermissionRequired);
            if(mode==L"lenovo-live") {
                std::filesystem::create_directories(std::filesystem::path(argv[2]).parent_path());
                FILE* report=nullptr;_wfopen_s(&report,argv[2],L"w");Check(report!=nullptr,"cannot open native probe report");
                std::fprintf(report,"CPU %.1f C\nGPU %.1f C\nFans %d\nFan1 %d RPM\nFan2 %d RPM\nPermission required %d\nHRESULT %08x\n",s.cpuTempC,s.gpuTempC,s.laptopFanCount,s.cpuFanRpm,s.caseFanRpm,s.sensorPermissionRequired,static_cast<unsigned>(s.sensorProviderStatus));std::fclose(report);
                Check(s.laptopFanCount>0&&s.cpuFanRpm>=0,"Lenovo native fan query failed");
            }
            std::puts("PASS live sensor state: real provider values only");CoUninitialize();return 0;
        }
        if(mode==L"disk-live") {
            MetricsCollector collector;collector.Start();std::this_thread::sleep_for(std::chrono::seconds(4));const auto snapshot=collector.GetLatestSnapshot();collector.Stop();
            size_t valid=0,mapped=0;for(const auto& disk:snapshot.physicalDisks) {if(disk.throughputValid)++valid;mapped+=disk.volumes.size();Check(!disk.id.empty(),"device ID missing");Check(disk.readHistory.Size()==disk.writeHistory.Size(),"histories desynchronized");}
            std::printf("Live disk check: %zu devices, %zu valid throughput, %zu mapped volumes\n",snapshot.physicalDisks.size(),valid,mapped);CoUninitialize();return 0;
        }
        wchar_t executable[MAX_PATH];Check(GetModuleFileNameW(nullptr,executable,MAX_PATH)!=0,"executable path unavailable");
        auto output=std::filesystem::absolute(argv[2]).lexically_normal();
        const auto release=std::filesystem::path(executable).parent_path();
        if(output.parent_path()==release) {
            const auto category=mode.rfind(L"memory",0)==0 ? L"memory" : mode.rfind(L"gpu",0)==0 ? L"gpu" : mode.rfind(L"cpu",0)==0 ? L"cpu" : hover>=0 ? L"hover" : L"regression";
            output=release.parent_path()/L"visual-tests"/category/output.filename();
        }
        std::filesystem::create_directories(output.parent_path());
        std::wprintf(L"Image: %ls\n",output.c_str());
        ProOverviewRenderTest::Render(static_cast<float>(_wtof(argv[1])), output.c_str(),mode.rfind(L"cpu",0)==0,argc==5 ? static_cast<size_t>(_wtoi(argv[4])) : network ? 60 : 16,network,probe,mode.rfind(L"gpu",0)==0,mode.find(L"missing")!=std::wstring::npos && mode.rfind(L"gpu",0)==0,mode.rfind(L"memory",0)==0,mode.find(L"missing")!=std::wstring::npos && mode.rfind(L"memory",0)==0,mode.rfind(L"disk",0)==0,mode.find(L"missing")!=std::wstring::npos && mode.rfind(L"disk",0)==0,sensorMode,hover,variant,mode);
    }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); CoUninitialize(); return 1; }
    CoUninitialize(); return 0;
}
