/*
 * 模块说明：Pro 常规设置的数据约定
 * 集中保存置顶、锁定、全屏隐藏、动画、快捷键和记忆位置，供配置与窗口共用。
 * 键码与修饰键是 Win32 数值，不是展示文字；保存位置还要记录 DPI 便于恢复。
 */
#pragma once

#include <windows.h>

/**
 * @brief Pro 专业版通用行为设置结构体 (ProGeneralSettings)
 * 
 * 存储用户在“偏好设置 -> 常规”选项卡中配置的各项系统级行为参数。
 * 注意：面板展开钉住 (Pinning) 属于运行态会话状态，不写入此结构体。
 */
struct ProGeneralSettings {
    bool autoStart = false;         ///< 是否开机自启动（注册到 Windows CurrentVersion\Run）
    bool alwaysOnTop = true;        ///< 是否总在最前（WS_EX_TOPMOST）
    bool lockPosition = false;      ///< 是否锁定位置（锁定后禁止鼠标拖拽移动）
    bool hideInFullscreen = true;   ///< 全屏游戏或看视频时自动隐藏悬浮条（避免画面遮挡）
    bool rememberPosition = true;   ///< 退出时是否记忆屏幕坐标（保存到 config.ini）
    bool reduceMotion = false;      ///< 减少动效模式（关闭布局过渡动画，适合低功耗或偏好瞬切的用户）
    UINT hotkeyModifiers = MOD_CONTROL | MOD_ALT; ///< 全局唤醒快捷键修饰键（默认 Ctrl + Alt）
    UINT hotkeyKey = 'I';           ///< 全局唤醒快捷键主键（默认字母 'I'，即 Ctrl + Alt + I）
    bool positionValid = false;     ///< 保存的屏幕坐标是否合法有效
    int positionX = 0;              ///< 记忆的屏幕水平绝对坐标 X (像素)
    int positionY = 0;              ///< 记忆的屏幕垂直绝对坐标 Y (像素)
    int savedDpi = 96;              ///< 保存该坐标时屏幕的 DPI（用于跨分辨率/不同 DPI 迁移时自动按比例校正）
};

/**
 * @brief 校验 Pro 版热键组合是否合法安全
 * 
 * 为什么需要严格校验热键？
 * 本项目要求热键包含 Ctrl 或 Alt，减少普通按键被注册为全局热键后影响其他程序输入的情况。
 * 
 * @param modifiers 修饰键掩码 (MOD_CONTROL, MOD_ALT, MOD_SHIFT, MOD_WIN)
 * @param key       虚拟键码 (Virtual Key Code)
 * @return true 合法有效；false 无效或存在潜在按键拦截风险
 */
inline bool IsValidProHotkey(UINT modifiers, UINT key) {
    // 如果没有指定键码，只有在修饰键也为空（即禁用热键）时才合法
    if (key == 0) return modifiers == 0;

    // 只允许这四种标准的 Windows 修饰键组合
    const UINT allowed = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN;
    if (modifiers & ~allowed) return false;

    // 安全约束：必须至少包含 Ctrl 或 Alt 之一（防止用户误绑定单按 Shift 导致打字冲突）
    if (!(modifiers & (MOD_CONTROL | MOD_ALT))) return false;

    // 主键必须在合理范围内：A-Z 字母键、0-9 数字键、或者 F1-F24 功能键
    const bool isLetter = (key >= 'A' && key <= 'Z');
    const bool isDigit  = (key >= '0' && key <= '9');
    const bool isFKey   = (key >= VK_F1 && key <= VK_F24);

    return isLetter || isDigit || isFKey;
}
