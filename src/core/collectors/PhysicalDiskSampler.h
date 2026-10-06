#pragma once
#include <windows.h>
#include <winioctl.h>
#include "core/metrics/MetricsSnapshot.h"
#include <unordered_map>

class PhysicalDiskSampler {
public:
    std::vector<PhysicalDiskInfo> Sample(const std::vector<DiskInfo>& volumes);
private:
    struct Device { PhysicalDiskInfo info; std::wstring path; };
    struct Baseline { DISK_PERFORMANCE counters{}; uint64_t ticks=0; bool valid=false; MetricHistory reads,writes; };
    std::vector<Device> devices;
    std::unordered_map<std::wstring,Baseline> baselines;
    uint64_t inventoryTicks=0;
    void RefreshInventory();
};
