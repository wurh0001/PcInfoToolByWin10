/*
 * 模块说明：只读窗口的输入法关联控制
 * UI 无文本输入控件时，在 COM 和建窗之前禁用线程 IME，并解除各窗口 IME 上下文。
 * 普通键盘导航与快捷键仍使用窗口键盘消息；若未来加入文字输入，需要重新评估此策略。
 */
#pragma once
#include <windows.h>
#include <imm.h>

#pragma comment(lib, "imm32.lib")

// 用于没有文字输入控件的 UI 线程，在 COM 初始化和窗口创建之前调用。
// 键盘导航和快捷键捕获依然通过普通按键消息处理，不依赖 IME。
/**
 * 禁用当前只读 UI 线程的 IME，在 COM 初始化和建窗前调用。
 */
inline bool DisableReadOnlyThreadIme() {
    return ImmDisableIME(GetCurrentThreadId()) != FALSE;
}

/**
 * 解除指定 HWND 的输入法上下文关联，返回 Windows API 的成功状态。
 */
inline bool DisableReadOnlyWindowIme(HWND window) {
    return ImmAssociateContextEx(window, nullptr, 0) != FALSE;
}
