#include "pro/windows/ProContextMenu.h"
#include "pro/interaction/ProWindowBehavior.h"
#include <wincodec.h>
#include <filesystem>
#include <cstdio>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static int scenario=0;
static void CALLBACK Drive(HWND owner,UINT,UINT_PTR timer,DWORD){
    const HWND menu=FindWindowW(L"PcInfoProContextMenu",nullptr);
    if(!menu)return;KillTimer(owner,timer);
    if(scenario==0){PostMessageW(menu,WM_KEYDOWN,VK_DOWN,0);PostMessageW(menu,WM_KEYDOWN,VK_DOWN,0);PostMessageW(menu,WM_KEYDOWN,VK_RETURN,0);}
    else if(scenario==1)PostMessageW(menu,WM_KEYDOWN,VK_ESCAPE,0);
    else if(scenario==2)PostMessageW(menu,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(-4,-4));
    else if(scenario==3){const int x=static_cast<int>(ProContextMenu::Apron+50),y=static_cast<int>(ProContextMenu::Apron+22);PostMessageW(menu,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));PostMessageW(menu,WM_LBUTTONUP,0,MAKELPARAM(x,y));}
    else {PostMessageW(menu,WM_KEYDOWN,VK_UP,0);PostMessageW(menu,WM_KEYDOWN,VK_RETURN,0);}
}
int wmain(int argc,wchar_t** argv){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try {
        Check(argc==2,"output directory required");std::filesystem::create_directories(argv[1]);
        Check(ProContextMenu::HitTest(16,8)==0&&ProContextMenu::HitTest(16,40)==1&&ProContextMenu::HitTest(16,85)==-1&&ProContextMenu::HitTest(16,86)==2,"row geometry");
        Check(ProContextMenu::HitTest(7,20)==-1&&ProContextMenu::HitTest(212,20)==-1,"padding hit target");
        Check(ProContextMenu::Navigate(-1,1)==0&&ProContextMenu::Navigate(-1,-1)==2&&ProContextMenu::Navigate(2,1)==0,"keyboard wrapping");
        const POINT clamped=ProWindowBehavior::ClampVisiblePosition({-5,900},{220,126},{-1920,0,0,1080},8);
        Check(clamped.x==-228&&clamped.y==900,"negative monitor clamp");
        ComPtr<ID2D1Factory> factory;ComPtr<IDWriteFactory> write;D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf());
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(write.GetAddressOf()));
        for(float scale:{1.f,1.5f,2.f}){
            const int w=static_cast<int>(1020*scale),h=static_cast<int>(186*scale);
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            void* pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);Check(dib&&pixels,"DIB");const auto old=SelectObject(dc,dib);
            ComPtr<ID2D1DCRenderTarget> rt;auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
            Check(SUCCEEDED(factory->CreateDCRenderTarget(&properties,&rt)),"render target");RECT bounds{0,0,w,h};rt->BindDC(dc,&bounds);rt->BeginDraw();rt->Clear(D2D1::ColorF(0xf2f4f7));rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            ProContextMenu menu;
            for(int i=0;i<4;++i){rt->SetTransform(D2D1::Matrix3x2F::Translation(24+i*252.f,24)*D2D1::Matrix3x2F::Scale(scale,scale));menu.Draw(rt.Get(),write.Get(),i==1,i-1);}
            rt->SetTransform(D2D1::Matrix3x2F::Identity());
            ProOverviewVisuals assets;for(int id=217;id<=219;++id)Check(assets.DrawIcon(rt.Get(),static_cast<ProOverviewVisuals::Icon>(id),D2D1::RectF(-32,-32,-16,-16)),"missing menu asset");
            Check(SUCCEEDED(rt->EndDraw()),"draw failed");
            ComPtr<IWICImagingFactory> wic;ComPtr<IWICBitmap> bitmap;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
            Check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic))),"WIC");wic->CreateBitmapFromMemory(w,h,GUID_WICPixelFormat32bppPBGRA,w*4,w*h*4,static_cast<BYTE*>(pixels),&bitmap);wic->CreateStream(&stream);
            const auto output=std::filesystem::path(argv[1])/(L"context-menu-"+std::to_wstring(static_cast<int>(scale*100))+L".png");Check(SUCCEEDED(stream->InitializeFromFilename(output.c_str(),GENERIC_WRITE)),"output");
            wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);encoder->CreateNewFrame(&frame,nullptr);frame->Initialize(nullptr);frame->SetSize(w,h);WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;frame->SetPixelFormat(&format);Check(SUCCEEDED(frame->WriteSource(bitmap.Get(),nullptr))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit()),"PNG");
            rt.Reset();SelectObject(dc,old);DeleteObject(dib);DeleteDC(dc);
        }
        // Exercise only this test's own windows, never the user's running app.
        HWND owner=CreateWindowExW(0,L"STATIC",L"Menu test owner",WS_POPUP,50,50,100,100,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(owner!=nullptr,"owner window");ShowWindow(owner,SW_SHOW);
        for(scenario=0;scenario<5;++scenario){SetTimer(owner,1,100,Drive);ProContextMenu menu;const int chosen=menu.Track(owner,{200,200},false);Check(chosen==(scenario==0?103:scenario==3?101:scenario==4?102:0),"native menu action");KillTimer(owner,1);}
        DestroyWindow(owner);std::puts("PASS context menu: Figma assets, 3 DPI scales, hit testing, placement, keyboard, mouse, cancel and repeat-open");
    } catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());CoUninitialize();return 1;}
    CoUninitialize();return 0;
}
