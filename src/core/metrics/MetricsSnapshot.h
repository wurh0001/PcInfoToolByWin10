/*
 * 模块说明：采集结果的数据结构
 * 包含 CPU、内存、GPU、网络和磁盘数据，UI 读取快照而不直接依赖硬件查询对象。
 * MetricHistory 用固定数组保存最近 60 点；vector/string 持有动态数据，其复制可能申请堆内存。
 */
#pragma once

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include "core/metrics/MetricDataState.h"

/**
 * @brief 逻辑驱动器/分区卷信息 (DiskInfo)
 * 对应 Windows 系统中的“逻辑盘符”（如 C:\、D:\）
 */
struct DiskInfo {
    std::wstring drive;         ///< 盘符名称，例如 L"C:" 或 L"D:"
    std::wstring label;         ///< 卷标名称，例如 L"系统" 或 L"Data"
    uint64_t totalBytes = 0;    ///< 分区总容量 (字节数 Byte)
    uint64_t freeBytes = 0;     ///< 分区剩余可用容量 (字节数 Byte)
};

/**
 * @brief 进程 CPU 占用信息 (ProcessUsageInfo)
 * 用于面板中展示“占用 CPU 最高的前 3~4 个进程”
 */
struct ProcessUsageInfo {
    std::wstring name;          ///< 进程映像名称，例如 L"Chrome.exe"
    float cpuPercent = 0.0f;    ///< 全局归一化后的 CPU 占用率 (0.0% ~ 100.0%)
};

/**
 * @brief 进程物理内存占用信息 (ProcessMemoryInfo)
 * 用于面板中展示“占用内存最高的进程”
 */
struct ProcessMemoryInfo {
    std::wstring name;          ///< 进程映像名称，例如 L"Photoshop.exe"
    uint64_t memBytes = 0;      ///< 进程工作集/物理内存常驻字节数 (Byte)
};

/**
 * @brief 固定 60 点零堆分配环形历史缓冲区 (MetricHistory)
 * 
 * 极客级性能设计的核心实现：
 * 1. 为什么不用 std::vector？std::vector 在插入时如果超出容量会发生动态内存扩容 (realloc)，
 *    造成堆内存碎片和不必要的 CPU 开销。
 * 2. 环形队列 (RingBuffer) 底层使用编译期固定大小的 std::array<float, 60>，
 *    数组保存在所属对象内部，不单独申请堆内存；所属对象本身仍可能在堆上。
 *    写入操作量不随历史长度增加，称为时间复杂度 O(1)。
 * 3. 保存最近 60 次采样，实际覆盖时长取决于采样间隔，不保证严格 60 秒。
 */
struct MetricHistory {
    static constexpr size_t kCapacity = 60;     ///< 容量恒定为 60（存储最近 60 次采样）
    std::array<float, kCapacity> data{};        ///< 固定尺寸内联数组
    size_t count = 0;                           ///< 当前已记录的数据点数量 (0 <= count <= 60)
    size_t head = 0;                            ///< 下一次写入数据的游标位置 (循环推进)

    /**
     * @brief 压入一个新的历史数据点
     * @param val 采样数值（支持 NaN 代表断点）
     */
    void Push(float val) {
        data[head] = val;
        head = (head + 1) % kCapacity;          // 环形推进，满了就覆盖最老的数据
        if (count < kCapacity) {
            ++count;
        }
    }

    /// @brief 返回当前包含的有效数据点数量
    size_t Size() const { return count; }

    /// @brief 历史记录是否为空
    bool Empty() const { return count == 0; }

    /**
     * @brief 按时间先后逻辑顺序访问数据点
     * 
     * 重载下标操作符 []：
     * - index = 0：代表历史上“最早/最旧”的点（波形图最左侧）
     * - index = count - 1：代表“最新”的点（波形图最右侧当前点）
     */
    float operator[](size_t index) const {
        if (count < kCapacity) {
            return data[index];
        }
        // 当环形缓冲区蓄满时，head 指向的位置恰好是最旧的点，加上偏移 index 取模即可
        return data[(head + index) % kCapacity];
    }

    /// @brief 获取最新压入的一个数据点
    /**
     * 取得最新历史点；空历史的返回值由实现约定，调用方先检查 Empty 更明确。
     */
    float Back() const {
        if (count == 0) return 0.0f;
        size_t lastIdx = (head + kCapacity - 1) % kCapacity;
        return data[lastIdx];
    }
};

/**
 * @brief 物理磁盘硬件详细信息 (PhysicalDiskInfo)
 * 对应机箱内真正插在主板上的固态硬盘 (NVMe SSD) 或机械硬盘 (HDD)，而非逻辑盘符
 */
struct PhysicalDiskInfo {
    std::wstring id;                        ///< 即插即用设备实例路径 (PnP Device ID)，唯一标识物理硬件
    uint32_t number = 0;                    ///< 物理磁盘编号（如“磁盘 0”、“磁盘 1”）
    std::wstring model;                     ///< 硬盘型号名称，例如 L"Samsung SSD 980 PRO 1TB"
    std::wstring busType;                   ///< 传输接口总线类型，例如 L"NVMe"、L"SATA"、L"USB"
    uint64_t capacityBytes = 0;             ///< 物理硬盘总容量 (Byte)
    uint64_t readBps = 0;                   ///< 当前实时读取速率 (Byte/s 字节每秒)
    uint64_t writeBps = 0;                  ///< 当前实时写入速率 (Byte/s 字节每秒)
    bool throughputValid = false;           ///< 吞吐量读数是否有效可用
    bool statisticsValid = false;           ///< 活动时间与响应时间是否为真实硬件采样（非占位符）
    float temperatureC = 0;                 ///< 硬盘物理温度 (摄氏度 °C)
    bool temperatureValid = false;          ///< 硬盘温度传感器是否支持并成功读取
    float activePercent = 0;                ///< 磁盘活动时间百分比 (0.0% ~ 100.0%)
    float responseTimeMs = 0;               ///< 平均 I/O 响应时间 (毫秒 ms)
    MetricHistory readHistory;              ///< 最近 60 秒读取速率折线历史
    MetricHistory writeHistory;             ///< 最近 60 秒写入速率折线历史
    std::vector<DiskInfo> volumes;          ///< 该物理硬盘上包含的所有逻辑分区 (例如 C:, D:)
    std::wstring status;                    ///< 硬件运行健康状态描述
};

/**
 * @brief 全局硬件监控核心快照模型 (MetricsSnapshot)
 * 
 * 核心设计模式：数据与渲染彻底解耦（单向数据流）
 * 1. 后台采集线程循环调用底层 API，再休眠约 1 秒，逐项更新此结构体；
 * 2. 界面通过 GetLatestSnapshot 短暂加锁取得副本，绘制副本时不需要锁住采集器；
 * 3. 避免任何 UI 绘制函数直接去调用慢速的 Windows 性能计数器 API。
 */
struct MetricsSnapshot {
    // ==========================================
    // 1. CPU (中央处理器) 监控指标
    // ==========================================
    float cpuUsageTotal = 0.0f;             ///< CPU 整体总利用率 (0.0% ~ 100.0%)
    float cpuUserPercent = 0.0f;            ///< 用户态程序耗时百分比 (User Time)
    float cpuKernelPercent = 0.0f;          ///< 系统内核态耗时百分比 (Kernel / System Time)
    float cpuIdlePercent = 0.0f;            ///< 空闲百分比 (Idle Time)
    float cpuFreqGHz = 0.0f;                ///< CPU 当前运行主频 (GHz，例如 3.80 GHz)
    float cpuTempC = 0.0f;                  ///< CPU 核心/封装温度 (摄氏度 °C)
    std::wstring cpuBrand;                  ///< CPU 品牌与型号全称，例如 L"13th Gen Intel Core i7-13700H"
    std::vector<float> cpuCoreUsage;        ///< 每个独立逻辑核心的实时占用率列表（支持 8核/16核/32核等）
    MetricHistory cpuHistory;               ///< 最近 60 秒 CPU 总体负载波形历史
    std::vector<ProcessUsageInfo> topProcesses; ///< 占用 CPU 最高的前几名进程

    // ==========================================
    // 2. 物理内存 (RAM) 监控指标
    // ==========================================
    float memoryUsagePercent = 0.0f;        ///< 物理内存已使用百分比 (0.0% ~ 100.0%)
    uint64_t totalPhysMem = 0;              ///< 系统物理内存总量 (Byte)
    uint64_t availPhysMem = 0;              ///< 剩余可用物理内存 (Byte)
    uint64_t usedPhysMem = 0;               ///< 正在使用的物理内存 (Byte)
    uint64_t memInUseBytes = 0;             ///< 活跃使用内存 (In-Use，被进程、驱动实际占用的内存)
    bool memoryCompositionValid = false;    ///< 内存细分构成是否由内核真实测得（非估算）
    uint64_t memStandbyCacheBytes = 0;      ///< 备用/待机缓存内存 (Standby Cache，包含预读和页缓存)
    uint64_t memFreeBytes = 0;              ///< 纯未分配空闲内存 (Free)
    uint64_t memCommittedBytes = 0;         ///< 已提交虚拟内存总量 (Committed)
    uint64_t memCommitLimitBytes = 0;       ///< 提交限制额度 (Commit Limit，物理内存 + 页面文件上限)
    MetricHistory memoryHistory;            ///< 最近 60 秒内存占用波形历史
    std::vector<ProcessMemoryInfo> topMemoryProcesses; ///< 占用物理内存最高的前几名进程

    // ==========================================
    // 3. GPU (图形显卡) 监控指标
    // ==========================================
    float gpuUsagePercent = 0.0f;           ///< GPU 核心总体负载率 (0.0% ~ 100.0%)
    float gpuTempC = 0.0f;                  ///< GPU 核心温度 (摄氏度 °C，NVML 真实读取)
    MetricHistory gpuHistory;               ///< 最近 60 秒 GPU 负载波形历史
    std::wstring gpuName;                   ///< 显卡设备型号，例如 L"NVIDIA GeForce RTX 4060 Laptop GPU"
    float gpuFreqGHz = 0.0f;                ///< GPU 核心实时图形时钟频率 (GHz)
    float gpuPowerWatts = 0.0f;             ///< GPU 板卡实时功耗 (瓦特 W)
    uint64_t gpuVramTotalBytes = 0;         ///< 显存 (VRAM) 总容量 (Byte，例如 8.0 GiB)
    uint64_t gpuVramUsedBytes = 0;          ///< 显存已使用容量 (Byte，例如 2.3 GiB)
    float gpu3dLoadPercent = 0.0f;          ///< GPU 3D 渲染引擎负载 (0.0% ~ 100.0%)
    float gpuVideoDecodePercent = 0.0f;     ///< GPU 硬件视频解码引擎负载 (Decode)
    float gpuVideoEncodePercent = 0.0f;     ///< GPU 硬件视频编码引擎负载 (Encode)
    std::vector<ProcessUsageInfo> topGpuProcesses; ///< 占用 GPU 算力最高的前几名进程

    // ==========================================
    // 4. 网络 (Network) 监控指标
    // ==========================================
    uint64_t uploadSpeedBps = 0;            ///< 当前网络实时上传速率 (Byte/s 字节每秒)
    uint64_t downloadSpeedBps = 0;          ///< 当前网络实时下载速率 (Byte/s 字节每秒)
    MetricHistory downloadHistory;          ///< 最近 60 秒下载速率波形历史 (独立自适应动态量程)
    MetricHistory uploadHistory;            ///< 最近 60 秒上传速率波形历史
    uint64_t sessionTotalInBytes = 0;       ///< 本次运行期间累计下载总流量 (Byte)
    uint64_t sessionTotalOutBytes = 0;      ///< 本次运行期间累计上传总流量 (Byte)
    std::wstring netAdapterName;            ///< 当前活跃网络适配器网卡名称 (例如 Wi-Fi / 以太网)
    std::wstring localIpAddress;            ///< 本地 IPv4 地址 (例如 L"192.168.1.100")

    // ==========================================
    // 5. 存储与磁盘 (Disk) 监控指标
    // ==========================================
    std::vector<DiskInfo> disks;             ///< 本地所有逻辑分区卷列表 (C:, D: 等)
    std::vector<PhysicalDiskInfo> physicalDisks; ///< 本机所有物理硬盘设备详细信息列表
    uint64_t diskReadBps = 0;               ///< 主磁盘当前读取吞吐量 (Byte/s)
    uint64_t diskWriteBps = 0;              ///< 主磁盘当前写入吞吐量 (Byte/s)
    MetricHistory diskHistory;              ///< 最近 60 秒总吞吐历史 (读+写)
    MetricHistory diskReadHistory;          ///< 最近 60 秒独立读取历史
    MetricHistory diskWriteHistory;         ///< 最近 60 秒独立写入历史
    std::wstring diskModelName;             ///< 当前选中主磁盘的型号 (例如 L"Samsung 980 PRO")
    std::wstring diskBusType;               ///< 当前选中主磁盘的接口类型 (例如 L"1 TB • NVMe")
    bool diskStatisticsValid = false;       ///< 磁盘 I/O 统计指标是否来自内核真实性能计数器
    float diskActivePercent = 0.0f;         ///< 磁盘活动时间占比 (0.0% ~ 100.0%)
    float diskResponseTimeMs = 0.0f;        ///< 磁盘平均响应延迟时间 (ms)

    // ==========================================
    // 6. 传感器与风扇 (Sensor / Fan) 监控指标
    // ==========================================
    MetricHistory tempHistory;              ///< 最近 60 秒主硬件温度历史曲线
    MetricSampleStatus temperatureStatus;   ///< 温度数据采样状态机 (跟踪 Ready / Stale / Unsupported)
    float ssdTempC = 0.0f;                  ///< 固态硬盘温度 (摄氏度 °C)
    float cpuPowerWatts = 0.0f;             ///< CPU 实时封装功耗 (W，若硬件未开放驱动则为 0)
    int cpuFanRpm = -1;                     ///< CPU 散热风扇转速 (RPM，-1 代表该机型不支持读取)
    int caseFanRpm = -1;                    ///< 机箱/辅助风扇转速 (RPM，-1 代表不可用)
    int laptopFanCount = 0;                 ///< 笔记本/OEM 原生探测到的风扇总数
    bool sensorPermissionRequired = false;  ///< 是否需要管理员权限才能访问特定底层传感器
    int32_t sensorProviderStatus = 0;       ///< 底层原生驱动返回的 HRESULT 诊断状态码
    float tempMinC = 0.0f;                  ///< 观察期内的最低温度记录值
    float tempMaxC = 0.0f;                  ///< 观察期内的最高温度记录值

    // ==========================================
    // 7. 系统综合信息 (System)
    // ==========================================
    uint64_t uptimeSeconds = 0;             ///< Windows 操作系统本次开机持续运行秒数 (Uptime)
};
