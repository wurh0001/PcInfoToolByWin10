// 项目头文件：主监控条窗口及其运行逻辑。
#include "ui/MiniBarWindow.h"
// Windows 基础 API：窗口、消息循环、COM 和错误提示框。
#include <windows.h>

// Windows GUI 程序入口：负责初始化系统环境，并启动主监控窗口。
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 使用每显示器 DPI 感知模式，让窗口在不同缩放比例的显示器上保持正确尺寸。
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 初始化当前线程的 COM 环境，Direct2D、DirectWrite 等组件会使用它。
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // MiniBarWindow 负责创建监控条、启动采集线程和处理 Windows 消息。
    MiniBarWindow app;
    if (app.Create()) {
        // 进入消息循环，程序会一直运行到收到退出消息。
        app.Run();
    } else {
        // 创建窗口或初始化 Direct2D 失败时，显示错误提示。
        MessageBoxW(NULL, L"无法初始化监控部件或 Direct2D 渲染环境。", L"PcInfoTool", MB_ICONERROR);
    }

    // 消息循环结束后释放 COM 环境。
    CoUninitialize();
    return 0;
}
