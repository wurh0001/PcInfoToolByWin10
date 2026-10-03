#pragma once
#include <windows.h>
#include "ProMiniBarWindow.h"
#include <memory>

class ProApp {
public:
    ProApp() = default;
    ~ProApp() = default;

    bool Create() {
        m_miniBar = std::make_unique<ProMiniBarWindow>();
        return m_miniBar->Create();
    }

    void Run() {
        if (m_miniBar) {
            m_miniBar->Run();
        }
    }

private:
    std::unique_ptr<ProMiniBarWindow> m_miniBar;
};
