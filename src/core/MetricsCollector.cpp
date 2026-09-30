// Winsock 基础类型和 TCP/IP 辅助 API。
#include <winsock2.h>
#include <ws2tcpip.h>
// Windows 基础系统 API 和系统信息结构。
#include <windows.h>
// IP Helper API：读取网络适配器的流量和状态。
#include <iphlpapi.h>
// 网络 I/O API：提供网络接口表等底层网络结构。
#include <netioapi.h>
// Performance Data Helper：读取 CPU、GPU 等性能计数器。
#include <pdh.h>
// PDH 错误码和消息定义。
#include <pdhmsg.h>

// 项目数据采集器的类声明和 MetricsSnapshot 类型。
#include "MetricsCollector.h"

// 后台线程和线程同步。
#include <thread>
#include <atomic>
#include <mutex>
// 指标历史记录使用的动态数组。
#include <vector>
// 后台采样间隔使用的时间工具。
#include <chrono>
// 限制指标范围时使用 std::clamp。
#include <algorithm>

// 链接 IP Helper API 和 PDH 库，提供网络与性能计数器的实现。
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")

// NtQuerySystemInformation 返回的 NT 状态码及成功判断宏。
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;
    LARGE_INTEGER KernelTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER DpcTime;
    LARGE_INTEGER InterruptTime;
    ULONG InterruptCount;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

typedef NTSTATUS(NTAPI* PFN_NtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

static PFN_NtQuerySystemInformation s_pfnNtQuerySystemInformation = nullptr;
static bool s_ntInit = false;

// 延迟获取 ntdll 中的 NtQuerySystemInformation，避免程序启动时强依赖导入符号。
static void EnsureNtLoaded() {
    if (!s_ntInit) {
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            s_pfnNtQuerySystemInformation = reinterpret_cast<PFN_NtQuerySystemInformation>(
                GetProcAddress(hNtdll, "NtQuerySystemInformation")
            );
        }
        s_ntInit = true;
    }
}

struct CoreTimeRecord {
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
};

struct MetricsCollector::Impl {
    std::thread workerThread;
    std::atomic<bool> running{false};
    std::mutex mutex;

    MetricsSnapshot snapshot;
    int coreCount = 0;

    // Per-core tracking via NtQuerySystemInformation
    std::vector<CoreTimeRecord> prevCoreTimes;
    bool hasPrevCoreTimes = false;

    // System Times tracking (universal fallback)
    uint64_t lastIdle = 0;
    uint64_t lastKernel = 0;
    uint64_t lastUser = 0;
    bool lastSystemTimeInit = false;

    // Network tracking
    uint64_t lastInBytes = 0;
    uint64_t lastOutBytes = 0;
    uint64_t lastNetTimestamp = 0; // ms

    // GPU tracking via Windows PDH (GPU Engine Utilization Percentage)
    PDH_HQUERY gpuQuery = NULL;
    PDH_HCOUNTER gpuCounter = NULL;
    bool gpuQueryInit = false;
    bool gpuQueryFailed = false;

    static constexpr size_t kMaxHistory = 60;

    void WorkerLoop();
    void UpdateCpu();
    void UpdateMemory();
    void UpdateGpu();
    void UpdateNetwork();
    void UpdateDisks();
    void InitGpuQuery();
};

// 初始化处理器数量、每核心历史数据，以及采集线程使用的内部状态。
MetricsCollector::MetricsCollector() : m_impl(std::make_unique<Impl>()) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_impl->coreCount = sysInfo.dwNumberOfProcessors;
    m_impl->snapshot.cpuCoreUsage.resize(m_impl->coreCount, 0.0f);
    m_impl->prevCoreTimes.resize(m_impl->coreCount);
}

// 析构前先停止后台线程，保证资源释放时没有线程仍在访问成员数据。
MetricsCollector::~MetricsCollector() {
    Stop();
}

// 启动唯一的后台采集线程。
void MetricsCollector::Start() {
    if (m_impl->running.exchange(true)) return;
    m_impl->workerThread = std::thread(&MetricsCollector::Impl::WorkerLoop, m_impl.get());
}

// 停止采集线程，并关闭已经创建的 GPU 性能查询。
void MetricsCollector::Stop() {
    if (!m_impl->running.exchange(false)) return;
    if (m_impl->workerThread.joinable()) {
        m_impl->workerThread.join();
    }
    if (m_impl->gpuQuery) {
        PdhCloseQuery(m_impl->gpuQuery);
        m_impl->gpuQuery = NULL;
        m_impl->gpuCounter = NULL;
        m_impl->gpuQueryInit = false;
    }
}

// 在线程锁保护下复制一份最新快照，供 UI 线程安全读取。
MetricsSnapshot MetricsCollector::GetLatestSnapshot() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->snapshot;
}

// 后台线程循环：依次采集 CPU、内存、GPU、网络和磁盘信息。
void MetricsCollector::Impl::WorkerLoop() {
    while (running) {
        UpdateCpu();
        UpdateMemory();
        UpdateGpu();
        UpdateNetwork();
        UpdateDisks();

        // Sample every 1000ms
        for (int i = 0; i < 10 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// 将 Windows FILETIME 的高低 32 位合并为一个 64 位计数值。
static uint64_t FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// 计算 CPU 总使用率和每核心使用率，并保存最近 60 个历史点。
void MetricsCollector::Impl::UpdateCpu() {
    float total = 0.0f;
    std::vector<float> cores(coreCount, 0.0f);
    bool gotData = false;

    EnsureNtLoaded();
    if (s_pfnNtQuerySystemInformation && coreCount > 0) {
        std::vector<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION> perfInfo(coreCount);
        ULONG retLen = 0;
        NTSTATUS status = s_pfnNtQuerySystemInformation(
            8 /* SystemProcessorPerformanceInformation */,
            perfInfo.data(),
            sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * static_cast<ULONG>(coreCount),
            &retLen
        );

        if (NT_SUCCESS(status)) {
            if (hasPrevCoreTimes) {
                float sumUsage = 0.0f;
                for (int i = 0; i < coreCount; ++i) {
                    uint64_t idleDiff = perfInfo[i].IdleTime.QuadPart - prevCoreTimes[i].idle;
                    uint64_t kernelDiff = perfInfo[i].KernelTime.QuadPart - prevCoreTimes[i].kernel;
                    uint64_t userDiff = perfInfo[i].UserTime.QuadPart - prevCoreTimes[i].user;
                    uint64_t totalDiff = kernelDiff + userDiff;

                    float coreUsage = 0.0f;
                    if (totalDiff > 0 && totalDiff >= idleDiff) {
                        coreUsage = (1.0f - static_cast<float>(idleDiff) / static_cast<float>(totalDiff)) * 100.0f;
                    }
                    coreUsage = std::clamp(coreUsage, 0.0f, 100.0f);
                    cores[i] = coreUsage;
                    sumUsage += coreUsage;

                    prevCoreTimes[i].idle = perfInfo[i].IdleTime.QuadPart;
                    prevCoreTimes[i].kernel = perfInfo[i].KernelTime.QuadPart;
                    prevCoreTimes[i].user = perfInfo[i].UserTime.QuadPart;
                }
                total = std::clamp(sumUsage / static_cast<float>(coreCount), 0.0f, 100.0f);
                gotData = true;
            } else {
                for (int i = 0; i < coreCount; ++i) {
                    prevCoreTimes[i].idle = perfInfo[i].IdleTime.QuadPart;
                    prevCoreTimes[i].kernel = perfInfo[i].KernelTime.QuadPart;
                    prevCoreTimes[i].user = perfInfo[i].UserTime.QuadPart;
                }
                hasPrevCoreTimes = true;
            }
        }
    }

    // High reliability fallback with GetSystemTimes if NT API didn't provide complete deltas yet
    if (!gotData) {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            uint64_t curIdle = FileTimeToUint64(idle);
            uint64_t curKernel = FileTimeToUint64(kernel);
            uint64_t curUser = FileTimeToUint64(user);

            if (lastSystemTimeInit) {
                uint64_t idleDiff = curIdle - lastIdle;
                uint64_t kernelDiff = curKernel - lastKernel;
                uint64_t userDiff = curUser - lastUser;
                uint64_t totalSys = kernelDiff + userDiff;
                if (totalSys > 0 && totalSys >= idleDiff) {
                    float usage = 100.0f - (static_cast<float>(idleDiff) * 100.0f / static_cast<float>(totalSys));
                    total = std::clamp(usage, 0.0f, 100.0f);
                    std::fill(cores.begin(), cores.end(), total);
                }
            }
            lastIdle = curIdle;
            lastKernel = curKernel;
            lastUser = curUser;
            lastSystemTimeInit = true;
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    snapshot.cpuUsageTotal = total;
    snapshot.cpuCoreUsage = cores;

    snapshot.cpuHistory.push_back(total);
    if (snapshot.cpuHistory.size() > kMaxHistory) {
        snapshot.cpuHistory.erase(snapshot.cpuHistory.begin());
    }
}

// 读取物理内存总量、可用量和当前使用率。
void MetricsCollector::Impl::UpdateMemory() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        std::lock_guard<std::mutex> lock(mutex);
        snapshot.memoryUsagePercent = static_cast<float>(memStatus.dwMemoryLoad);
        snapshot.totalPhysMem = memStatus.ullTotalPhys;
        snapshot.availPhysMem = memStatus.ullAvailPhys;
        snapshot.usedPhysMem = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
    }
}

// 初始化 Windows PDH GPU Engine 性能计数器。
void MetricsCollector::Impl::InitGpuQuery() {
    if (gpuQueryInit || gpuQueryFailed) return;

    PDH_STATUS status = PdhOpenQuery(NULL, 0, &gpuQuery);
    if (status != ERROR_SUCCESS) {
        gpuQueryFailed = true;
        return;
    }

    // Standard Windows GPU Engine 3D utilization counter wildcard
    status = PdhAddEnglishCounterW(gpuQuery, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &gpuCounter);
    if (status != ERROR_SUCCESS) {
        // Fallback: try all GPU Engine utilization instances
        status = PdhAddEnglishCounterW(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter);
    }

    if (status == ERROR_SUCCESS) {
        PdhCollectQueryData(gpuQuery); // Primer read
        gpuQueryInit = true;
    } else {
        PdhCloseQuery(gpuQuery);
        gpuQuery = NULL;
        gpuQueryFailed = true;
    }
}

// 读取 GPU Engine 使用率，并合并多个 3D 引擎实例的结果。
void MetricsCollector::Impl::UpdateGpu() {
    if (!gpuQueryInit && !gpuQueryFailed) {
        InitGpuQuery();
        return;
    }

    float gpuVal = 0.0f;
    if (gpuQueryInit && gpuQuery) {
        PDH_STATUS status = PdhCollectQueryData(gpuQuery);
        if (status == ERROR_SUCCESS) {
            DWORD bufferSize = 0;
            DWORD itemCount = 0;
            // First call to determine buffer size needed for wildcard instances
            status = PdhGetFormattedCounterArrayW(gpuCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, NULL);
            if (status == PDH_MORE_DATA && bufferSize > 0) {
                std::vector<BYTE> buffer(bufferSize);
                PDH_FMT_COUNTERVALUE_ITEM_W* pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
                status = PdhGetFormattedCounterArrayW(gpuCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, pItems);
                if (status == ERROR_SUCCESS) {
                    double sumEngines = 0.0;
                    for (DWORD i = 0; i < itemCount; ++i) {
                        if (pItems[i].FmtValue.CStatus == ERROR_SUCCESS && pItems[i].FmtValue.doubleValue > 0.0) {
                            sumEngines += pItems[i].FmtValue.doubleValue;
                        }
                    }
                    gpuVal = static_cast<float>(std::clamp(sumEngines, 0.0, 100.0));
                }
            } else {
                // If single instance counter
                PDH_FMT_COUNTERVALUE val;
                if (PdhGetFormattedCounterValue(gpuCounter, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS) {
                    if (val.CStatus == ERROR_SUCCESS) {
                        gpuVal = static_cast<float>(std::clamp(val.doubleValue, 0.0, 100.0));
                    }
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    snapshot.gpuUsagePercent = gpuVal;
}

// 读取活动网卡累计字节数，并根据时间差计算上传下载速度。
void MetricsCollector::Impl::UpdateNetwork() {
    PMIB_IF_TABLE2 pIfTable = NULL;
    if (GetIfTable2(&pIfTable) == NO_ERROR && pIfTable != NULL) {
        uint64_t totalIn = 0;
        uint64_t totalOut = 0;

        for (ULONG i = 0; i < pIfTable->NumEntries; ++i) {
            const MIB_IF_ROW2& row = pIfTable->Table[i];
            if (row.InterfaceAndOperStatusFlags.HardwareInterface &&
                row.OperStatus == IfOperStatusUp &&
                row.Type != IF_TYPE_SOFTWARE_LOOPBACK) {
                totalIn += row.InOctets;
                totalOut += row.OutOctets;
            }
        }
        FreeMibTable(pIfTable);

        uint64_t nowMs = GetTickCount64();
        if (lastNetTimestamp > 0 && nowMs > lastNetTimestamp) {
            double elapsedSec = static_cast<double>(nowMs - lastNetTimestamp) / 1000.0;
            if (elapsedSec > 0.05) {
                uint64_t inDiff = (totalIn >= lastInBytes) ? (totalIn - lastInBytes) : 0;
                uint64_t outDiff = (totalOut >= lastOutBytes) ? (totalOut - lastOutBytes) : 0;

                std::lock_guard<std::mutex> lock(mutex);
                snapshot.downloadSpeedBps = static_cast<uint64_t>(static_cast<double>(inDiff) / elapsedSec);
                snapshot.uploadSpeedBps = static_cast<uint64_t>(static_cast<double>(outDiff) / elapsedSec);

                snapshot.downloadHistory.push_back(static_cast<float>(snapshot.downloadSpeedBps));
                if (snapshot.downloadHistory.size() > kMaxHistory) {
                    snapshot.downloadHistory.erase(snapshot.downloadHistory.begin());
                }
            }
        }

        lastInBytes = totalIn;
        lastOutBytes = totalOut;
        lastNetTimestamp = nowMs;
    }
}

// 遍历本地固定磁盘，读取总容量和剩余容量。
void MetricsCollector::Impl::UpdateDisks() {
    std::vector<DiskInfo> disks;
    DWORD drivesMask = GetLogicalDrives();

    for (int i = 0; i < 26; ++i) {
        if (drivesMask & (1 << i)) {
            wchar_t rootPath[] = { static_cast<wchar_t>(L'A' + i), L':', L'\\', 0 };
            UINT type = GetDriveTypeW(rootPath);
            if (type == DRIVE_FIXED) {
                ULARGE_INTEGER freeBytesAvail, totalBytes, totalFreeBytes;
                if (GetDiskFreeSpaceExW(rootPath, &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
                    DiskInfo di;
                    di.drive = rootPath;
                    di.totalBytes = totalBytes.QuadPart;
                    di.freeBytes = totalFreeBytes.QuadPart;
                    disks.push_back(di);
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    snapshot.disks = std::move(disks);
}
