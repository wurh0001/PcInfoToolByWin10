/*
 * 模块说明：物理磁盘采样接口
 * 保存设备清单及上次累计性能计数，将卷信息关联到实际磁盘。
 * 盘符不是物理设备：一块盘可含多个卷；设备 ID 用来保持选择和历史的连续性。
 */
#pragma once

#include <windows.h>
#include <winioctl.h>
#include "core/metrics/MetricsSnapshot.h"
#include <unordered_map>

/**
 * @brief 物理磁盘硬件级采样器 (PhysicalDiskSampler)
 * 
 * 为什么不用 GetDiskFreeSpaceEx？
 * GetDiskFreeSpaceEx 只能查询逻辑分区 (C: 盘、D: 盘) 的容量，它不知道这几个分区是不是在同一块物理 NVMe 固态硬盘上，
 * 也无法获取硬件实时的物理读取速度 (B/s)、写入速度 (B/s)、I/O 平均响应延迟 (ms) 和活动时间百分比。
 * 
 * 本采样器通过 Windows 底层 DeviceIoControl 发送控制码：
 * 1. IOCTL_STORAGE_QUERY_PROPERTY：查询物理硬盘真实型号 (如 Samsung 980 PRO) 与总线协议 (NVMe / SATA)；
 * 2. IOCTL_DISK_PERFORMANCE：读取 Windows 物理磁盘性能计数器 (读取字节增量、写入字节增量、响应时间)；
 * 3. StorageDeviceTemperatureProperty：读取物理硬盘内部温控传感器。
 */
class PhysicalDiskSampler {
public:
    /**
     * @brief 周期性执行物理磁盘性能采样
     * @param volumes 已经探测到的本地逻辑盘符列表 (如 C:, D:)，用于与物理磁盘进行映射关联
     * @return std::vector<PhysicalDiskInfo> 本机所有物理磁盘的最新性能指标列表
     */
    std::vector<PhysicalDiskInfo> Sample(const std::vector<DiskInfo>& volumes);

private:
    /// @brief 内部记录的物理硬件设备基础标识
    struct Device {
        PhysicalDiskInfo info;  ///< 物理硬盘静态信息
        std::wstring path;      ///< 符号链接或设备句柄路径 (如 \\.\PhysicalDrive0)
    };

    /// @brief 上一次采样的基准快照（用于做差值计算实时速率）
    struct Baseline {
        DISK_PERFORMANCE counters{};    ///< Windows 底层物理磁盘性能计数器原始结构
        uint64_t ticks = 0;             ///< 上次采样时的高精度时间戳
        bool valid = false;             ///< 基准点是否已建立
        MetricHistory reads;            ///< 该磁盘历史读取波形
        MetricHistory writes;           ///< 该磁盘历史写入波形
    };

    std::vector<Device> devices;        ///< 当前系统中已发现的物理磁盘设备列表
    std::unordered_map<std::wstring, Baseline> baselines; ///< 按设备 ID 索引的历史基准线字典
    uint64_t inventoryTicks = 0;        ///< 上次扫描系统物理磁盘资产列表的时间戳

    /**
     * @brief 重新扫描系统硬件资产（每隔一段时间刷新一次，以支持热插拔 U 盘和移动固态硬盘）
     */
    void RefreshInventory();
};
