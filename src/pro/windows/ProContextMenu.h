/*
 * 模块说明：Pro 自绘右键菜单接口
 * Track 返回选中的命令 ID，取消返回 0；命令执行由主悬浮条负责。
 * HitTest/Navigate 可独立测试；Draw 可用于离屏截图检查。
 */
#pragma once
#include "ui/rendering/LayeredD2DRenderer.h"
#include "pro/rendering/ProOverviewVisuals.h"

// Pro-local Figma menu; Lite's menu and typography remain unchanged.
class ProContextMenu {
public:
    static constexpr float Width=220, Height=126, Apron=24;
    /**
     * 将菜单局部坐标转换为行索引；边距和分隔区域不属于可点击行。
     */
    static int HitTest(float x,float y);
    /**
     * 按方向在有效菜单行间循环移动焦点，处理初始未选择状态。
     */
    static int Navigate(int current,int direction);
    /**
     * 定位并显示菜单，运行局部消息循环直到选择或取消；返回命令 ID，0 表示取消。
     * @param owner 所属窗口，用于焦点、消息或窗口关系。
     */
    int Track(HWND owner,POINT anchor,bool panelOpen,bool dark=false);
    /**
     * 绘制菜单当前状态，选择的执行动作由外层处理。
     * @param factory 借用的文字工厂，用于创建格式或布局。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void Draw(ID2D1RenderTarget* target,IDWriteFactory* factory,bool panelOpen,int hovered=-1,int pressed=-1,bool dark=false);
private:
    /**
     * Win32 窗口过程：按消息类型分发交互；用窗口用户数据关联 C++ 实例。
     */
    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    /**
     * 按当前状态绘制并提交一帧；数据来自已保存快照，绘图不等于重新采集硬件。
     * 绘图坐标受当前变换/裁剪影响；本函数不是硬件采集入口。
     */
    void Render();
    HWND window=nullptr;
    LayeredD2DRenderer renderer;
    ProOverviewVisuals visuals;
    float scale=1;
    bool open=false,darkTheme=false,tracking=false;
    int hovered=-1,pressed=-1,command=0;
};
