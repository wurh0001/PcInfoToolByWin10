/*
 * 模块说明：AMD 主温度适配器接口
 * 仅启用验证过的 AMD Family 0x17 / Model 0x60；通过已安装的 PawnIO 读取主温度。
 * 对象独占 DLL、设备和互斥锁句柄，禁止复制，避免重复释放同一个资源。
 */
#pragma once

#include <windows.h>
#include <cstdint>

/**
 * @brief AMD 锐龙处理器原生温度传感器适配器 (NativeAmdTemperature)
 * 
 * 核心原理解析（新手必读）：
 * 1. AMD 移动端与桌面端锐龙处理器（如 Renoir / Zen2 / Zen3 / Zen4 架构）：
 *    Windows 官方 WMI 不提供 CPU 真实温度，通常需要通过 PCI 物理总线与 SMU (System Management Unit) 协处理器通信。
 * 2. 安全边界：
 *    - 仅在 Worker 后台线程中调用；
 *    - 严格通过系统互斥量 (Mutex) 保护 PCI 总线并发访问，防止与主板其他传感器冲突；
 *    - 只在验证型号上启用；初始化/读取失败返回错误。此限制不能保证任意驱动环境无系统故障。
 */
class NativeAmdTemperature {
public:
    /// @brief AMD CPU 温度采样结果结构
    struct Reading {
        float temperature = 0;      ///< 探测到的 CPU 封装温度 (摄氏度 °C)
        HRESULT status = E_PENDING; ///< 采样状态码
        uint64_t sampleTime = 0;    ///< 采样时间戳 (毫秒)
    };

    NativeAmdTemperature() = default;
    ~NativeAmdTemperature();

    // 禁用拷贝与赋值
    NativeAmdTemperature(const NativeAmdTemperature&) = delete;
    NativeAmdTemperature& operator=(const NativeAmdTemperature&) = delete;

    /**
     * @brief 执行一次 AMD 处理器温度采样
     * @return Reading 最新的温度读数与状态
     */
    Reading Sample();

private:
    // 底层通信函数签名
    using Execute = HRESULT(WINAPI*)(HANDLE, PCSTR, const ULONG64*, SIZE_T, PULONG64, SIZE_T, PSIZE_T);
    using Close   = HRESULT(WINAPI*)(HANDLE);

    /**
     * 验证 CPU 和模块并打开 PawnIO 设备，返回初始化状态 HRESULT。
     */
    HRESULT Initialize();           ///< 初始化设备驱动句柄与总线互斥锁
    void Reset();                   ///< 重置并释放句柄资源

    HMODULE library = nullptr;      ///< 动态通信模块
    HANDLE device = nullptr;        ///< 硬件通信设备句柄
    HANDLE pciMutex = nullptr;      ///< PCI 总线全局安全互斥锁
    Execute execute = nullptr;      ///< SMU 读写调用函数指针
    Close close = nullptr;          ///< 句柄关闭函数指针
    uint64_t nextAttempt = 0;       ///< 失败后的下次尝试时间戳 (避免重复报错)
    HRESULT lastStatus = E_PENDING; ///< 上次采样的状态记录
};
