/*
 * 模块说明：共享采集器的公开接口
 * Lite 和 Pro 复用同一采集实现；头文件通过 Impl 前置声明隐藏系统 API 细节。
 * Start/Stop 控制后台线程，GetLatestSnapshot 加锁后复制数据供 UI 使用。
 */
#pragma once

#include "core/metrics/MetricsSnapshot.h"
#include <memory>

/**
 * @brief 硬件指标采集调度器 (MetricsCollector)
 * 
 * 架构设计重点（新手必读）：
 * 1. 为什么采用 Pimpl 模式（Pointer to Implementation）？
 *    Windows 硬件采集涉及极其庞杂的底层头文件，如 PDH 性能计数器、IPHLPAPI 网络接口、
 *    WbemIdl (WMI) 组件、DirectX/DXGI 显卡接口等。如果全写在头文件里，所有包含它的源文件
 *    编译速度会极慢，并且会污染全局命名空间。
 *    因此头文件只声明一个前置声明结构体 `struct Impl;` 和一个智能指针 `m_impl`，
 *    所有复杂的实现和 Windows 内部头文件完全隐藏在 `MetricsCollector.cpp` 中。
 * 
 * 2. 线程模型：
 *    MetricsCollector 内部管理一个专属的低优先级后台工作线程 (Worker Thread)，
 *    每轮调用系统 API 采样后休眠约 1 秒，并安全地更新快照；
 *    主 UI 线程调用 `GetLatestSnapshot()`，短暂加锁并复制数据，不在 UI 线程执行硬件查询。
 */
class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector();

    /**
     * @brief 启动后台硬件采集工作线程
     * 线程启动后，将定期轮询 CPU、内存、网络、磁盘与 GPU 等性能接口
     */
    void Start();

    /**
     * @brief 停止并安全退出后台采集线程
     * 会等待后台线程处理完当前采样周期后优雅退出，避免强杀线程造成句柄泄漏
     */
    void Stop();

    /**
     * @brief 动态启用或禁用 GPU 硬件采集
     * 当用户折叠 GPU 模块或者不需要 GPU 监控时，可停用以进一步降低开销
     * @param enable true 开启；false 关闭
     */
    void SetGpuEnabled(bool enable);

    /**
     * @brief 获取当前最新的全局硬件数据快照（短暂加锁后返回副本）
     * @return MetricsSnapshot 最新的硬件读数快照拷贝
     */
    MetricsSnapshot GetLatestSnapshot();

private:
    struct Impl;                        ///< 前置声明具体实现类（隐藏具体平台细节）
    std::unique_ptr<Impl> m_impl;       ///< 指向具体实现对象的独占智能指针
};
