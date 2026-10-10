/*
 * 模块说明：Pro 程序入口
 * 先关闭只读 UI 线程的输入法关联，再设置 DPI、COM 并启动 ProApp。
 * 入口负责应用级初始化；具体采集、绘制和交互交给窗口类。
 */
// Pro 专业版应用程序类头文件
#include "app/pro/ProApp.h"
// Windows 基础 API：窗口、DPI、错误处理
#include <windows.h>
// COM 组件初始化接口（Direct2D、DirectWrite、WIC 必需）
#include <objbase.h>
#include "ui/platform/ReadOnlyInput.h"

/**
 * @brief Windows GUI 应用程序统一入口函数 (Unicode 宽字符版本)
 * 
 * @param hInstance     当前应用程序实例句柄
 * @param hPrevInstance 先前实例句柄（Win32 环境下始终为 NULL）
 * @param pCmdLine      Unicode 命令行参数字符串
 * @param nCmdShow      主窗口默认显示控制标志
 * @return 进程退出码 (int)
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Pro 控件使用普通按键消息，没有接收输入法组合文本的输入框。
    // 在 COM 可能创建辅助窗口之前，先关闭此 UI 线程的 IME 关联。
    DisableReadOnlyThreadIme();
    // 启用 Windows 10/11 Per-Monitor DPI Aware V2 模式：
    // 确保窗口在多显示器混合 DPI、缩放切换及拖拽跨屏时，文字与几何图元均能保持清晰且缩放比例计算精准
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 初始化当前线程的 COM 环境（单线程套间 STA 模式）：
    // 此线程通过 CoCreateInstance 创建 WIC 等 COM 组件，需要先建立 COM 环境
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // 实例化 Pro 专业版应用管理对象并启动
    ProApp app;
    if (app.Create()) {
        // 进入 UI 消息循环，阻塞直到收到退出指令
        app.Run();
    } else {
        // 创建主窗口或初始化 Direct2D 渲染环境失败时，弹出系统级告警对话框
        MessageBoxW(NULL, L"无法初始化 PcInfoTool-Pro 专业版或 Direct2D 渲染环境。", L"PcInfoTool-Pro", MB_ICONERROR);
    }

    // 应用程序退出，关闭并清理当前线程的 COM 资源
    CoUninitialize();
    return 0;
}
