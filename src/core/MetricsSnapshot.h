#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct DiskInfo {
    std::wstring drive;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
};

struct MetricsSnapshot {
    // CPU
    float cpuUsageTotal = 0.0f;
    std::vector<float> cpuCoreUsage;
    std::vector<float> cpuHistory; // last 60 points

    // Memory
    float memoryUsagePercent = 0.0f;
    uint64_t totalPhysMem = 0;
    uint64_t availPhysMem = 0;
    uint64_t usedPhysMem = 0;
    std::vector<float> memoryHistory; // last 60 points

    // GPU
    float gpuUsagePercent = 0.0f;
    std::vector<float> gpuHistory; // last 60 points

    // Network
    uint64_t uploadSpeedBps = 0;
    uint64_t downloadSpeedBps = 0;
    std::vector<float> downloadHistory; // last 60 points
    std::vector<float> uploadHistory;   // last 60 points

    // Disk
    std::vector<DiskInfo> disks;
};
