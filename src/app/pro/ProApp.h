/*
 * 模块说明：Pro 应用生命周期
 * unique_ptr 独占主悬浮条对象；Create 创建窗口，Run 接收并分发系统消息。
 * 智能指针管理 C++ 对象，窗口内部仍需分别释放 HWND、COM 和 GDI 资源。
 */
#pragma once

// Windows 基础 API：窗口管理与消息机制
#include <windows.h>
// Pro 专业版主悬浮条窗口类定义
#include "pro/windows/ProMiniBarWindow.h"
// C++ 标准库智能指针
#include <memory>

/**
 * @brief ProApp - PcInfoTool Pro 专业版应用程序封装类
 * 
 * 职责：
 * 1. 负责管理专业版主悬浮条窗口 (ProMiniBarWindow) 的生命周期；
 * 2. 隔离 WinMain 入口与底层具体窗口交互逻辑；
 * 3. 驱动 Windows UI 消息循环直至应用退出。
 */
class ProApp {
public:
    ProApp() = default;
    ~ProApp() = default;

    /**
     * @brief 初始化并创建专业版主窗口
     * @return true 创建成功；false 创建失败（例如 Direct2D 环境初始化失败等）
     */
    bool Create() {
        m_miniBar = std::make_unique<ProMiniBarWindow>();
        return m_miniBar->Create();
    }

    /**
     * @brief 进入应用程序主消息循环
     * 阻塞运行，直到收到 WM_QUIT 退出消息
     */
    void Run() {
        if (m_miniBar) {
            m_miniBar->Run();
        }
    }

private:
    // 持有主悬浮条窗口的独占智能指针，保证对象在生命周期结束时正确析构释放资源
    std::unique_ptr<ProMiniBarWindow> m_miniBar;
};
