/*
 * 模块说明：Lite 程序入口
 * 配置进程 DPI 感知和 UI 线程 COM 环境，再创建主悬浮条并进入消息循环。
 * 局部窗口对象离开作用域时销毁；COM 初始化与反初始化需要成对。
 */
// Lite 轻量版主监控条窗口及其运行逻辑头文件
#include "lite/windows/MiniBarWindow.h"
// Windows 基础 API：窗口、DPI、消息循环、COM 和错误提示框
#include <windows.h>

/**
 * @brief Windows GUI 应用程序统一入口函数 (Unicode 宽字符版本 - Lite 经典版)
 * 
 * @param hInstance     当前应用程序实例句柄
 * @param hPrevInstance 先前实例句柄（Win32 环境下始终为 NULL）
 * @param pCmdLine      Unicode 命令行参数字符串
 * @param nCmdShow      主窗口默认显示控制标志
 * @return 进程退出码 (int)
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 启用每显示器 DPI 感知模式 (Per-Monitor V2)：
    // 让悬浮条与展开面板在不同缩放比例 (100%、125%、150%、200%) 的高分屏与跨显示器场景下保持清晰与尺寸精准
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 初始化当前线程的 COM 环境（单线程套间 STA 模式）：
    // Direct2D 渲染器、DirectWrite 文本引擎与 Windows 系统底层组件依赖此上下文
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // 实例化 Lite 经典版主监控条窗口对象：
    // 负责创建无边框分层透明窗口、启动后台低开销采集线程与处理 Windows 交互消息
    MiniBarWindow app;
    if (app.Create()) {
        // 进入主消息循环，程序持续运行直到用户通过右键菜单等方式触发退出
        app.Run();
    } else {
        // 创建窗口或初始化 Direct2D 硬件加速渲染管线失败时，弹出系统错误提示框
        MessageBoxW(NULL, L"无法初始化监控部件或 Direct2D 渲染环境。", L"PcInfoTool", MB_ICONERROR);
    }

    // 消息循环结束后，反初始化并释放当前线程持有的 COM 资源
    CoUninitialize();
    return 0;
}
