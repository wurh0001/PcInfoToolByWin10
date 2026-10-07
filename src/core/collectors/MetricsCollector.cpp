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
#include <intrin.h>
#include <powerbase.h>

// 项目数据采集器的类声明和 MetricsSnapshot 类型。
#include "core/collectors/MetricsCollector.h"
#include "core/collectors/PhysicalDiskSampler.h"
#include "core/collectors/NativeGpuSensors.h"
#include "core/collectors/NativeLenovoSensors.h"
#include "core/collectors/NativeAmdTemperature.h"

// 后台线程和线程同步。
#include <thread>
#include <atomic>
#include <mutex>
// 指标历史记录使用的动态数组。
#include <vector>
#include <unordered_map>
// 后台采样间隔使用的时间工具。
#include <chrono>
// 限制指标范围时使用 std::clamp。
#include <algorithm>
#include <limits>
#include <dxgi.h>
#include <psapi.h>

// 链接 IP Helper API 和 PDH 库，提供网络与性能计数器的实现。
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "psapi.lib")

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

typedef struct _SYSTEM_PROCESS_INFORMATION_LOCAL {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    struct {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    } ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} SYSTEM_PROCESS_INFORMATION_LOCAL;

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

typedef struct _MY_PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
} MY_PROCESSOR_POWER_INFORMATION;

typedef LONG (NTAPI *PFN_CallNtPowerInformation)(
    POWER_INFORMATION_LEVEL InformationLevel,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength
);
static PFN_CallNtPowerInformation s_pfnCallNtPowerInformation = nullptr;
static bool s_powerLoaded = false;

static void EnsurePowerLoaded() {
    if (!s_powerLoaded) {
        HMODULE hPowr = LoadLibraryW(L"powrprof.dll");
        if (hPowr) {
            s_pfnCallNtPowerInformation = reinterpret_cast<PFN_CallNtPowerInformation>(
                GetProcAddress(hPowr, "CallNtPowerInformation")
            );
        }
        s_powerLoaded = true;
    }
}

static std::wstring QueryCpuBrand() {
    int cpuInfo[4] = { -1 };
    char brand[0x41] = { 0 };
    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];
    if (nExIds >= 0x80000004) {
        __cpuid((int*)brand, 0x80000002);
        __cpuid((int*)(brand + 16), 0x80000003);
        __cpuid((int*)(brand + 32), 0x80000004);
        char* p = brand;
        while (*p == ' ') p++;
        int wlen = MultiByteToWideChar(CP_ACP, 0, p, -1, NULL, 0);
        if (wlen > 0) {
            std::wstring res(wlen - 1, 0);
            MultiByteToWideChar(CP_ACP, 0, p, -1, res.data(), wlen);
            return res;
        }
    }
    return L"Processor";
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
    PhysicalDiskSampler physicalDiskSampler;
    NativeGpuSensors gpuSensors;
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

    // Process tracking
    std::unordered_map<DWORD, uint64_t> prevProcessTimes;
    uint64_t prevProcessTimestamp = 0;
    int processCycleCount = 0;

    // GPU tracking via Windows PDH (GPU Engine Utilization Percentage)
    std::atomic<bool> enableGpu{false};
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
    void UpdateNetworkInfo();
    void UpdateDisks();
    void UpdateProcesses();
    void InitGpuQuery();
    void CloseGpuQuery();
};

static void QueryGpuInfo(std::wstring& outName, uint64_t& outVram) {
    IDXGIFactory1* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pFactory))) && pFactory) {
        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; SUCCEEDED(pFactory->EnumAdapters1(i, &pAdapter)); ++i) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(pAdapter->GetDesc1(&desc))) {
                if (!(desc.Flags & 2 /* DXGI_ADAPTER_FLAG_SOFTWARE */)) {
                    if (outName.empty() || desc.DedicatedVideoMemory > outVram) {
                        outName = desc.Description;
                        outVram = desc.DedicatedVideoMemory;
                    }
                }
            }
            pAdapter->Release();
            pAdapter = nullptr;
        }
        pFactory->Release();
    }
    if (outName.empty()) {
        outName = L"显卡型号未提供";
    }
    if (outVram == 0) {
        outVram = 0;
    }
}

// 初始化处理器数量、每核心历史数据，以及采集线程使用的内部状态。
MetricsCollector::MetricsCollector() : m_impl(std::make_unique<Impl>()) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_impl->coreCount = sysInfo.dwNumberOfProcessors;
    m_impl->snapshot.cpuCoreUsage.resize(m_impl->coreCount, 0.0f);
    m_impl->prevCoreTimes.resize(m_impl->coreCount);
    m_impl->snapshot.cpuBrand = QueryCpuBrand();
    m_impl->UpdateNetworkInfo();
    QueryGpuInfo(m_impl->snapshot.gpuName, m_impl->snapshot.gpuVramTotalBytes);
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
    m_impl->CloseGpuQuery();
}

void MetricsCollector::SetGpuEnabled(bool enable) {
    if (m_impl) {
        m_impl->enableGpu.store(enable);
        if (!enable) {
            m_impl->CloseGpuQuery();
        }
    }
}

void MetricsCollector::Impl::CloseGpuQuery() {
    if (gpuQuery) {
        PdhCloseQuery(gpuQuery);
        gpuQuery = NULL;
        gpuCounter = NULL;
        gpuQueryInit = false;
        gpuQueryFailed = false;
    }
}

// 在线程锁保护下复制一份最新快照，供 UI 线程安全读取。
MetricsSnapshot MetricsCollector::GetLatestSnapshot() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->snapshot;
}

// 后台线程循环：依次采集 CPU、内存、GPU、网络和磁盘信息。
void MetricsCollector::Impl::WorkerLoop() {
    NativeLenovoSensors lenovoSensors; // COM resources are owned by this worker thread.
    NativeAmdTemperature amdTemperature;
    while (running) {
        UpdateCpu();
        UpdateMemory();
        if (enableGpu.load()) {
            UpdateGpu();
        }
        UpdateNetwork();
        UpdateDisks();
        // Independent of PDH initialization and of the currently visible detail page.
        const auto sensors=gpuSensors.Sample(snapshot.gpuName);
        const auto laptop=lenovoSensors.Sample();
        const auto amd=amdTemperature.Sample();
        {
            std::lock_guard<std::mutex> lock(mutex);
            snapshot.gpuTempC=sensors.temperature;snapshot.gpuPowerWatts=sensors.watts;snapshot.gpuFreqGHz=sensors.ghz;
            snapshot.gpuVramUsedBytes=sensors.used;
            snapshot.gpuVramTotalBytes=sensors.memoryValid ? sensors.total : 0;
            snapshot.laptopFanCount=laptop.fanCount;
            snapshot.cpuFanRpm=laptop.fan1;snapshot.caseFanRpm=laptop.fan2;
            snapshot.sensorPermissionRequired=laptop.status==WBEM_E_ACCESS_DENIED||laptop.status==E_ACCESSDENIED;
            snapshot.sensorProviderStatus=laptop.status;
            if(amd.temperature>0) {
                snapshot.temperatureStatus.Record(amd.temperature,snapshot.cpuTempC,snapshot.tempHistory,amd.sampleTime);
            } else if(laptop.cpuTemperature>0) {
                snapshot.temperatureStatus.Record(laptop.cpuTemperature,snapshot.cpuTempC,snapshot.tempHistory,laptop.sampleTime);
            } else if(snapshot.temperatureStatus.lastValidMs) {
                snapshot.temperatureStatus.Record(NAN,snapshot.cpuTempC,snapshot.tempHistory,laptop.sampleTime);
            } else {
                snapshot.temperatureStatus.state=MetricDataState::Unsupported;snapshot.cpuTempC=0;
            }
        }

        if (++processCycleCount >= 2) {
            processCycleCount = 0;
            UpdateProcesses();
        }

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
                uint64_t totalIdleDiff = 0;
                uint64_t totalKernelDiff = 0;
                uint64_t totalUserDiff = 0;

                for (int i = 0; i < coreCount; ++i) {
                    uint64_t idleDiff = perfInfo[i].IdleTime.QuadPart - prevCoreTimes[i].idle;
                    uint64_t kernelDiff = perfInfo[i].KernelTime.QuadPart - prevCoreTimes[i].kernel;
                    uint64_t userDiff = perfInfo[i].UserTime.QuadPart - prevCoreTimes[i].user;
                    uint64_t totalDiff = kernelDiff + userDiff;

                    totalIdleDiff += idleDiff;
                    totalKernelDiff += kernelDiff;
                    totalUserDiff += userDiff;

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

                uint64_t totalSysDiff = totalKernelDiff + totalUserDiff;
                if (totalSysDiff > 0) {
                    // In Windows NT API, KernelTime includes IdleTime!
                    // Actual kernel time spent executing OS code is (totalKernelDiff - totalIdleDiff)
                    uint64_t actualKernelDiff = (totalKernelDiff >= totalIdleDiff) ? (totalKernelDiff - totalIdleDiff) : 0;
                    std::lock_guard<std::mutex> lock(mutex);
                    snapshot.cpuUserPercent = std::clamp(static_cast<float>(totalUserDiff * 100.0 / totalSysDiff), 0.0f, 100.0f);
                    snapshot.cpuKernelPercent = std::clamp(static_cast<float>(actualKernelDiff * 100.0 / totalSysDiff), 0.0f, 100.0f);
                    snapshot.cpuIdlePercent = std::clamp(static_cast<float>(totalIdleDiff * 100.0 / totalSysDiff), 0.0f, 100.0f);
                }
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

                    uint64_t actualKernelDiff = (kernelDiff >= idleDiff) ? (kernelDiff - idleDiff) : 0;
                    std::lock_guard<std::mutex> lock(mutex);
                    snapshot.cpuUserPercent = std::clamp(static_cast<float>(userDiff * 100.0 / totalSys), 0.0f, 100.0f);
                    snapshot.cpuKernelPercent = std::clamp(static_cast<float>(actualKernelDiff * 100.0 / totalSys), 0.0f, 100.0f);
                    snapshot.cpuIdlePercent = std::clamp(static_cast<float>(idleDiff * 100.0 / totalSys), 0.0f, 100.0f);
                }
            }
            lastIdle = curIdle;
            lastKernel = curKernel;
            lastUser = curUser;
            lastSystemTimeInit = true;
        }
    }

    // Frequency & Temp
    EnsurePowerLoaded();
    float freqGHz = 0.0f;
    if (s_pfnCallNtPowerInformation && coreCount > 0) {
        std::vector<MY_PROCESSOR_POWER_INFORMATION> ppi(coreCount);
        NTSTATUS st = s_pfnCallNtPowerInformation(
            static_cast<POWER_INFORMATION_LEVEL>(11) /* ProcessorInformation */,
            NULL, 0,
            ppi.data(),
            sizeof(MY_PROCESSOR_POWER_INFORMATION) * static_cast<ULONG>(coreCount)
        );
        if (NT_SUCCESS(st) && ppi[0].CurrentMhz > 0) {
            uint64_t sumMhz = 0;
            for (int i = 0; i < coreCount; ++i) {
                sumMhz += ppi[i].CurrentMhz;
            }
            freqGHz = static_cast<float>(sumMhz) / (static_cast<float>(coreCount) * 1000.0f);
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    snapshot.cpuUsageTotal = total;
    snapshot.cpuCoreUsage = cores;
    snapshot.cpuHistory.Push(total);
    snapshot.cpuFreqGHz = freqGHz;
    // These sensors require a real hardware provider; CPU load is not temperature.
    snapshot.cpuPowerWatts = 0;
    snapshot.uptimeSeconds = GetTickCount64() / 1000;
}

// 读取物理内存组成（正在使用、待机缓存、空闲、已提交及限制）与使用率。
void MetricsCollector::Impl::UpdateMemory() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    bool hasStatus = GlobalMemoryStatusEx(&memStatus);

    PERFORMANCE_INFORMATION pi;
    pi.cb = sizeof(pi);
    bool hasPerf = GetPerformanceInfo(&pi, sizeof(pi));

    std::lock_guard<std::mutex> lock(mutex);
    if (hasPerf) {
        snapshot.memoryCompositionValid = false; // SystemCache is not the standby list.
        uint64_t pageSize = pi.PageSize;
        uint64_t totalPhys = pi.PhysicalTotal * pageSize;
        uint64_t availPhys = pi.PhysicalAvailable * pageSize;
        uint64_t cachePhys = pi.SystemCache * pageSize;
        uint64_t inUse = (totalPhys > availPhys) ? (totalPhys - availPhys) : 0;
        uint64_t freePhys = (availPhys > cachePhys) ? (availPhys - cachePhys) : 0;

        snapshot.totalPhysMem = totalPhys;
        snapshot.availPhysMem = availPhys;
        snapshot.usedPhysMem = inUse;
        snapshot.memInUseBytes = inUse;
        snapshot.memStandbyCacheBytes = cachePhys;
        snapshot.memFreeBytes = freePhys;
        snapshot.memCommittedBytes = pi.CommitTotal * pageSize;
        snapshot.memCommitLimitBytes = pi.CommitLimit * pageSize;

        snapshot.memoryUsagePercent = (totalPhys > 0) ? std::clamp(static_cast<float>(inUse * 100.0 / totalPhys), 0.0f, 100.0f) : 52.0f;
    } else if (hasStatus) {
        snapshot.memoryCompositionValid = false; // The cache/free split below is estimated.
        snapshot.totalPhysMem = memStatus.ullTotalPhys;
        snapshot.availPhysMem = memStatus.ullAvailPhys;
        snapshot.usedPhysMem = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
        snapshot.memInUseBytes = snapshot.usedPhysMem;
        snapshot.memStandbyCacheBytes = static_cast<uint64_t>(memStatus.ullAvailPhys * 0.38);
        snapshot.memFreeBytes = memStatus.ullAvailPhys - snapshot.memStandbyCacheBytes;
        snapshot.memCommittedBytes = memStatus.ullTotalPageFile - memStatus.ullAvailPageFile;
        snapshot.memCommitLimitBytes = memStatus.ullTotalPageFile;
        snapshot.memoryUsagePercent = static_cast<float>(memStatus.dwMemoryLoad);
    }
    snapshot.memoryHistory.Push(snapshot.memoryUsagePercent);
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
    snapshot.gpuHistory.Push(gpuVal);

    snapshot.gpu3dLoadPercent = gpuVal;
    snapshot.gpuVideoDecodePercent = std::numeric_limits<float>::quiet_NaN();
    snapshot.gpuVideoEncodePercent = std::numeric_limits<float>::quiet_NaN();
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

                snapshot.downloadHistory.Push(static_cast<float>(snapshot.downloadSpeedBps));
                snapshot.uploadHistory.Push(static_cast<float>(snapshot.uploadSpeedBps));

                snapshot.sessionTotalInBytes += inDiff;
                snapshot.sessionTotalOutBytes += outDiff;
            }
        }

        lastInBytes = totalIn;
        lastOutBytes = totalOut;
        lastNetTimestamp = nowMs;
    }
}

// 遍历本地固定磁盘，读取卷标签、总容量、剩余容量及实时读写指标。
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

                    wchar_t volName[MAX_PATH] = { 0 };
                    if (GetVolumeInformationW(rootPath, volName, MAX_PATH, NULL, NULL, NULL, NULL, 0) && wcslen(volName) > 0) {
                        wchar_t buf[MAX_PATH];
                        swprintf_s(buf, L"%s (%c:)", volName, static_cast<wchar_t>(L'A' + i));
                        di.label = buf;
                    } else {
                        wchar_t buf[64];
                        if (i == 2) swprintf_s(buf, L"系统 (%c:)", static_cast<wchar_t>(L'A' + i));
                        else swprintf_s(buf, L"数据 (%c:)", static_cast<wchar_t>(L'A' + i));
                        di.label = buf;
                    }

                    di.totalBytes = totalBytes.QuadPart;
                    di.freeBytes = totalFreeBytes.QuadPart;
                    disks.push_back(di);
                }
            }
        }
    }

    auto physical=physicalDiskSampler.Sample(disks);
    uint64_t read=0,write=0;bool valid=!physical.empty();
    for(const auto& disk:physical) {if(disk.throughputValid) {read+=disk.readBps;write+=disk.writeBps;}else valid=false;}
    std::lock_guard<std::mutex> lock(mutex);
    snapshot.disks=std::move(disks);snapshot.physicalDisks=std::move(physical);
    snapshot.ssdTempC=0;
    for(const auto& disk:snapshot.physicalDisks)if(disk.temperatureValid){snapshot.ssdTempC=disk.temperatureC;break;}
    snapshot.diskModelName.clear();snapshot.diskBusType.clear();snapshot.diskStatisticsValid=false;
    snapshot.diskReadBps=read;snapshot.diskWriteBps=write;
    const float r=valid ? static_cast<float>(read/1000000.0) : std::numeric_limits<float>::quiet_NaN();
    const float w=valid ? static_cast<float>(write/1000000.0) : std::numeric_limits<float>::quiet_NaN();
    snapshot.diskHistory.Push(r);snapshot.diskReadHistory.Push(r);snapshot.diskWriteHistory.Push(w);
}

void MetricsCollector::Impl::UpdateNetworkInfo() {
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG outBufLen = 15000;
    std::vector<BYTE> buffer(outBufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        dwRetVal = GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
            if (pCurr->OperStatus == IfOperStatusUp && pCurr->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                std::wstring friendlyName = pCurr->FriendlyName ? pCurr->FriendlyName : L"Ethernet";
                std::wstring ipStr = L"--";
                if (pCurr->FirstUnicastAddress) {
                    sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pCurr->FirstUnicastAddress->Address.lpSockaddr);
                    if (sa_in) {
                        char ipBuf[INET_ADDRSTRLEN] = { 0 };
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ipBuf, INET_ADDRSTRLEN);
                        wchar_t wIp[INET_ADDRSTRLEN] = { 0 };
                        MultiByteToWideChar(CP_ACP, 0, ipBuf, -1, wIp, INET_ADDRSTRLEN);
                        ipStr = wIp;
                    }
                }
                std::lock_guard<std::mutex> lock(mutex);
                snapshot.netAdapterName = friendlyName;
                snapshot.localIpAddress = ipStr;
                break;
            }
        }
    }
}

void MetricsCollector::Impl::UpdateProcesses() {
    EnsureNtLoaded();
    if (!s_pfnNtQuerySystemInformation) return;

    ULONG bufSize = 512 * 1024;
    std::vector<BYTE> buffer(bufSize);
    ULONG retLen = 0;

    NTSTATUS st = s_pfnNtQuerySystemInformation(5 /* SystemProcessInformation */, buffer.data(), bufSize, &retLen);
    if (st == static_cast<NTSTATUS>(0xC0000004) /* STATUS_INFO_LENGTH_MISMATCH */ && retLen > 0) {
        buffer.resize(retLen + 65536);
        bufSize = static_cast<ULONG>(buffer.size());
        st = s_pfnNtQuerySystemInformation(5, buffer.data(), bufSize, &retLen);
    }

    if (!NT_SUCCESS(st)) return;

    uint64_t nowMs = GetTickCount64();
    uint64_t elapsedMs = (prevProcessTimestamp > 0) ? (nowMs - prevProcessTimestamp) : 0;
    prevProcessTimestamp = nowMs;

    struct ProcEntry {
        std::wstring name;
        float cpu = 0.0f;
    };
    std::vector<ProcEntry> procs;

    std::unordered_map<DWORD, uint64_t> currentMap;
    std::unordered_map<std::wstring, uint64_t> memMap;
    BYTE* ptr = buffer.data();

    while (true) {
        SYSTEM_PROCESS_INFORMATION_LOCAL* pInfo = reinterpret_cast<SYSTEM_PROCESS_INFORMATION_LOCAL*>(ptr);
        DWORD pid = static_cast<DWORD>(reinterpret_cast<uintptr_t>(pInfo->UniqueProcessId));
        uint64_t totalProcTime = pInfo->KernelTime.QuadPart + pInfo->UserTime.QuadPart;
        currentMap[pid] = totalProcTime;

        if (pid != 0) {
            std::wstring procName;
            if (pInfo->ImageName.Buffer && pInfo->ImageName.Length > 0) {
                procName = std::wstring(pInfo->ImageName.Buffer, pInfo->ImageName.Length / sizeof(wchar_t));
            } else if (pid == 4) {
                procName = L"System";
            }

            std::wstring cleanName = procName;
            if (cleanName == L"chrome.exe") cleanName = L"Chrome";
            else if (cleanName == L"Code.exe") cleanName = L"Visual Studio Code";
            else if (cleanName == L"dwm.exe") cleanName = L"Desktop Window Manager";
            else if (cleanName == L"explorer.exe") cleanName = L"Windows 资源管理器";
            else if (cleanName == L"msedge.exe") cleanName = L"Microsoft Edge";
            else if (cleanName == L"Figma.exe") cleanName = L"Figma";
            else if (cleanName.size() > 4 && cleanName.substr(cleanName.size() - 4) == L".exe") {
                cleanName = cleanName.substr(0, cleanName.size() - 4);
            }

            if (!cleanName.empty() && pInfo->WorkingSetSize > 0) {
                memMap[cleanName] += pInfo->WorkingSetSize;
            }

            if (elapsedMs > 0) {
                auto it = prevProcessTimes.find(pid);
                if (it != prevProcessTimes.end()) {
                    uint64_t diffTime = (totalProcTime >= it->second) ? (totalProcTime - it->second) : 0;
                    // diffTime is in 100-nanosecond units (10,000 units = 1ms)
                    double cpuPercent = (static_cast<double>(diffTime) / (static_cast<double>(elapsedMs) * 10000.0 * (coreCount > 0 ? coreCount : 1))) * 100.0;
                    if (cpuPercent > 0.05) {
                        procs.push_back({ cleanName, static_cast<float>(cpuPercent) });
                    }
                }
            }
        }

        if (pInfo->NextEntryOffset == 0) break;
        ptr += pInfo->NextEntryOffset;
    }

    prevProcessTimes = std::move(currentMap);

    std::vector<ProcessUsageInfo> top3;
    if (!procs.empty()) {
        std::sort(procs.begin(), procs.end(), [](const ProcEntry& a, const ProcEntry& b) {
            return a.cpu > b.cpu;
        });

        for (size_t i = 0; i < procs.size() && top3.size() < 3; ++i) {
            top3.push_back({ procs[i].name, procs[i].cpu });
        }
    }

    std::vector<ProcessMemoryInfo> topMem;
    for (auto& kv : memMap) {
        topMem.push_back({ kv.first, kv.second });
    }
    std::sort(topMem.begin(), topMem.end(), [](const ProcessMemoryInfo& a, const ProcessMemoryInfo& b) {
        return a.memBytes > b.memBytes;
    });
    if (topMem.size() > 3) topMem.resize(3);

    std::lock_guard<std::mutex> lock(mutex);
    if (!top3.empty()) {
        snapshot.topProcesses = std::move(top3);
    }
    if (!topMem.empty()) {
        snapshot.topMemoryProcesses = std::move(topMem);
    }
}
