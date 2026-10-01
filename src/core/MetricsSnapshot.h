#pragma once
#include <vector>
#include <array>
#include <string>
#include <cstdint>

struct DiskInfo {
    std::wstring drive;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
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
    std::vector<float> cpuCoreUsage;
    MetricHistory cpuHistory; // last 60 points (RingBuffer)

    // Memory
    float memoryUsagePercent = 0.0f;
    uint64_t totalPhysMem = 0;
    uint64_t availPhysMem = 0;
    uint64_t usedPhysMem = 0;
    MetricHistory memoryHistory; // last 60 points (RingBuffer)

    // GPU
    float gpuUsagePercent = 0.0f;
    MetricHistory gpuHistory; // last 60 points (RingBuffer)

    // Network
    uint64_t uploadSpeedBps = 0;
    uint64_t downloadSpeedBps = 0;
    MetricHistory downloadHistory; // last 60 points (RingBuffer)
    MetricHistory uploadHistory;   // last 60 points (RingBuffer)

    // Disk
    std::vector<DiskInfo> disks;
};
