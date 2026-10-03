#pragma once
#include <vector>
#include <array>
#include <string>
#include <cstdint>

struct DiskInfo {
    std::wstring drive;
    std::wstring label;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
};

struct ProcessUsageInfo {
    std::wstring name;
    float cpuPercent = 0.0f;
};

struct ProcessMemoryInfo {
    std::wstring name;
    uint64_t memBytes = 0;
};

// 固定 60 点零堆分配环形缓冲区 (0 堆内存分配，0 内存重分配碎片)
struct MetricHistory {
    static constexpr size_t kCapacity = 60;
    std::array<float, kCapacity> data{};
    size_t count = 0;
    size_t head = 0; // 下一个写入位置

    void Push(float val) {
        data[head] = val;
        head = (head + 1) % kCapacity;
        if (count < kCapacity) {
            ++count;
        }
    }

    size_t Size() const { return count; }
    bool Empty() const { return count == 0; }

    // 按时间先后顺序访问：index 0 是最早的点，index count-1 是最新点
    float operator[](size_t index) const {
        if (count < kCapacity) {
            return data[index];
        }
        return data[(head + index) % kCapacity];
    }

    float Back() const {
        if (count == 0) return 0.0f;
        size_t lastIdx = (head + kCapacity - 1) % kCapacity;
        return data[lastIdx];
    }
};

struct MetricsSnapshot {
    // CPU
    float cpuUsageTotal = 0.0f;
    float cpuUserPercent = 0.0f;
    float cpuKernelPercent = 0.0f;
    float cpuIdlePercent = 0.0f;
    float cpuFreqGHz = 0.0f;
    float cpuTempC = 0.0f;
    std::wstring cpuBrand;
    std::vector<float> cpuCoreUsage;
    MetricHistory cpuHistory; // last 60 points (RingBuffer)
    std::vector<ProcessUsageInfo> topProcesses; // Top 3-4 CPU processes

    // Memory
    float memoryUsagePercent = 0.0f;
    uint64_t totalPhysMem = 0;
    uint64_t availPhysMem = 0;
    uint64_t usedPhysMem = 0;
    uint64_t memInUseBytes = 0;          // 正在使用
    uint64_t memStandbyCacheBytes = 0;   // 待机缓存
    uint64_t memFreeBytes = 0;           // 空闲
    uint64_t memCommittedBytes = 0;      // 已提交
    uint64_t memCommitLimitBytes = 0;    // 提交限制
    MetricHistory memoryHistory;         // last 60 points (RingBuffer)
    std::vector<ProcessMemoryInfo> topMemoryProcesses;

    // GPU
    float gpuUsagePercent = 0.0f;
    float gpuTempC = 0.0f;
    MetricHistory gpuHistory; // last 60 points (RingBuffer)
    std::wstring gpuName;               // e.g. L"NVIDIA GeForce RTX 4060"
    float gpuFreqGHz = 0.0f;            // e.g. 2.46 GHz
    float gpuPowerWatts = 0.0f;         // e.g. 42 W
    uint64_t gpuVramTotalBytes = 0;     // e.g. 8.0 GiB
    uint64_t gpuVramUsedBytes = 0;      // e.g. 2.3 GiB
    float gpu3dLoadPercent = 0.0f;      // 3D 渲染
    float gpuVideoDecodePercent = 0.0f; // 视频解码
    float gpuVideoEncodePercent = 0.0f; // 视频编码
    std::vector<ProcessUsageInfo> topGpuProcesses;

    // Network
    uint64_t uploadSpeedBps = 0;
    uint64_t downloadSpeedBps = 0;
    MetricHistory downloadHistory; // last 60 points (RingBuffer)
    MetricHistory uploadHistory;   // last 60 points (RingBuffer)
    uint64_t sessionTotalInBytes = 0;
    uint64_t sessionTotalOutBytes = 0;
    std::wstring netAdapterName;
    std::wstring localIpAddress;

    // Disk
    std::vector<DiskInfo> disks;
    uint64_t diskReadBps = 0;
    uint64_t diskWriteBps = 0;
    MetricHistory diskHistory;      // last 60 points (RingBuffer)
    MetricHistory diskReadHistory;  // last 60 points (RingBuffer)
    MetricHistory diskWriteHistory; // last 60 points (RingBuffer)
    std::wstring diskModelName;     // e.g. L"Samsung 980 PRO"
    std::wstring diskBusType;       // e.g. L"1 TB • NVMe"
    float diskActivePercent = 0.0f; // 活动时间 (e.g. 12%)
    float diskResponseTimeMs = 0.0f;// 平均响应时间 (e.g. 0.8 ms)

    // Sensor / Temperature
    MetricHistory tempHistory;     // last 60 points (RingBuffer)
    float ssdTempC = 41.0f;
    float cpuPowerWatts = 32.0f;
    int cpuFanRpm = 1240;
    int caseFanRpm = -1;           // -1 means unavailable (—)
    float tempMinC = 46.0f;
    float tempMaxC = 64.0f;

    // System
    uint64_t uptimeSeconds = 0;
};
