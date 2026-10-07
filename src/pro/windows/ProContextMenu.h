#pragma once
#include "ui/rendering/LayeredD2DRenderer.h"
#include "pro/rendering/ProOverviewVisuals.h"

// Pro-local Figma menu; Lite's menu and typography remain unchanged.
class ProContextMenu {
public:
    static constexpr float Width=220, Height=126, Apron=24;
    static int HitTest(float x,float y);
    static int Navigate(int current,int direction);
    int Track(HWND owner,POINT anchor,bool panelOpen,bool dark=false);
    void Draw(ID2D1RenderTarget* target,IDWriteFactory* factory,bool panelOpen,int hovered=-1,int pressed=-1,bool dark=false);
private:
    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    void Render();
    HWND window=nullptr;
    LayeredD2DRenderer renderer;
    ProOverviewVisuals visuals;
    float scale=1;
    bool open=false,darkTheme=false,tracking=false;
    int hovered=-1,pressed=-1,command=0;
};
