/*
 * 模块说明：硬件采集调度与实现
 * 后台依次查询 CPU、内存、网络、磁盘和传感器；UI 不在绘图过程中执行这些查询。
 * 累计计数需与上轮相减才能得到利用率或速率；快照写入用 mutex 保护。
 * 构造和首次查询会加载系统/厂商组件，内存成本不只是 snapshot 中的字段。
 */
// Winsock 基础类型和网络通信套接字 API
#include <winsock2.h>
#include <ws2tcpip.h>
// 内部性能跟踪与排障头文件
#include "core/diagnostics/PerformanceTrace.h"
// Windows 核心基础系统 API
#include <windows.h>
// IP Helper API：读取网卡网络适配器的实时流量、速率与状态
#include <iphlpapi.h>
// 网络 I/O 底层 API：提供网络接口表等原生内核网络结构
#include <netioapi.h>
// Performance Data Helper (PDH)：Windows 性能计数器框架，用于精准读取 GPU 3D 引擎利用率
#include <pdh.h>
#include <pdhmsg.h>
// 处理器内置 CPUID 汇编指令头文件，用于直接查询 CPU 真实型号字符串
#include <intrin.h>
// Windows 电源子系统 API，用于查询 CPU 实时运行主频
#include <powerbase.h>

// 业务核心：采集器声明与各子硬件传感器适配器
#include "core/collectors/MetricsCollector.h"
#include "core/collectors/PhysicalDiskSampler.h"
#include "core/collectors/NativeGpuSensors.h"
#include "core/collectors/NativeLenovoSensors.h"
#include "core/collectors/NativeAmdTemperature.h"

// C++ 标准库多线程与同步原语
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <limits>
// DirectX 图形基础设施 (DXGI)：枚举显卡型号、独立显存容量
#include <dxgi.h>
// 进程状态接口 (PSAPI)
#include <psapi.h>

// 显式链接系统动态库，避免在项目配置中手动配置输入依赖
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "psapi.lib")

// Windows NT 内部原生调用状态码定义 (ntstatus.h)
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

/**
 * @brief Windows 内核处理器性能信息结构体
 * 对应 NtQuerySystemInformation 的 SystemProcessorPerformanceInformation (8 号调用)
 * 能够一次性纳秒级获取所有 CPU 逻辑核心的空闲时间、内核时间与用户态时间
 */
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
    LARGE_INTEGER IdleTime;         ///< 核心处于空闲睡眠状态的时间 (100纳秒单位)
    LARGE_INTEGER KernelTime;       ///< 核心处于内核态 (包含 IdleTime) 的时间
    LARGE_INTEGER UserTime;         ///< 核心处于用户态执行程序的时间
    LARGE_INTEGER DpcTime;          ///< 延迟过程调用 (DPC) 耗时
    LARGE_INTEGER InterruptTime;    ///< 硬件中断处理耗时
    ULONG InterruptCount;           ///< 硬件中断发生次数
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;

/**
 * @brief Windows 内核进程列表信息局部定义
 * 对应 NtQuerySystemInformation 的 SystemProcessInformation (5 号调用)
 * 相比慢速的 Toolhelp32Snapshot，单次调用即可捕获系统几百个进程的 CPU 时间与内存工作集，开销低两个数量级
 */
typedef struct _SYSTEM_PROCESS_INFORMATION_LOCAL {
    ULONG NextEntryOffset;          ///< 链表中下一个进程节点的偏移量 (0 代表最后一个)
    ULONG NumberOfThreads;          ///< 该进程拥有的线程总数
    LARGE_INTEGER WorkingSetPrivateSize; ///< 私有工作集物理内存
    ULONG HardFaultCount;           ///< 硬页错误发生次数
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;            ///< 累计 CPU 周期数
    LARGE_INTEGER CreateTime;       ///< 进程启动时间
    LARGE_INTEGER UserTime;         ///< 进程消耗的用户态时间
    LARGE_INTEGER KernelTime;       ///< 进程消耗的内核态时间
    struct {
        USHORT Length;              ///< 字符串长度 (字节)
        USHORT MaximumLength;
        PWSTR Buffer;               ///< 进程可执行文件名宽字符缓冲区 (例如 L"chrome.exe")
    } ImageName;
    LONG BasePriority;              ///< 进程基础优先级
    HANDLE UniqueProcessId;         ///< 进程 ID (PID)
    HANDLE InheritedFromUniqueProcessId; ///< 父进程 ID
    ULONG HandleCount;              ///< 进程打开的系统句柄总数
    ULONG SessionId;                ///< Windows 用户会话 ID
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;          ///< 进程物理内存工作集常驻大小 (字节 Byte)
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

// NtQuerySystemInformation 函数指针类型
typedef NTSTATUS(NTAPI* PFN_NtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

static PFN_NtQuerySystemInformation s_pfnNtQuerySystemInformation = nullptr;
static bool s_ntInit = false;

/**
 * @brief 动态延迟加载 ntdll.dll 中的内核接口 NtQuerySystemInformation
 * 避免在编译期静态链接未公开符号，提高在不同 Windows 10/11 补丁版本下的兼容性
 */
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

/**
 * @brief Windows 电源子系统处理器电源状态定义
 * 用于查询 CPU 当前运行实时主频 (CurrentMhz)
 */
typedef struct _MY_PROCESSOR_POWER_INFORMATION {
    ULONG Number;                   ///< 核心编号
    ULONG MaxMhz;                   ///< 标称最大频率 (MHz)
    ULONG CurrentMhz;               ///< 当前运行主频 (MHz)
    ULONG MhzLimit;                 ///< 功耗墙限制频率
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

/**
 * @brief 动态加载 powrprof.dll 中的 CallNtPowerInformation
 */
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

/**
 * @brief 通过 CPUID 指令查询 CPU 的品牌字符串（例如 "AMD Ryzen 7 5800H" 或 "12th Gen Intel Core i7-12700H"）
 * 
 * 原理说明（新手必读）：
 * x86/x64 架构的 CPU 提供了一条专门的汇编指令 __cpuid()。
 * 传入功能号 0x80000002, 0x80000003, 0x80000004 时，CPU 内部的微代码会直接返回
 * 3 组各 16 字节的 ASCII 文本（存放在 EAX, EBX, ECX, EDX 寄存器中），拼接起来共 48 字节，
 * 即为 CPU 厂商烧录在芯片内部的完整商品型号名称。
 */
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
        while (*p == ' ') p++; // 去除开头的冗余空格
        int wlen = MultiByteToWideChar(CP_ACP, 0, p, -1, NULL, 0);
        if (wlen > 0) {
            std::wstring res(wlen - 1, 0);
            MultiByteToWideChar(CP_ACP, 0, p, -1, res.data(), wlen);
            return res;
        }
    }
    return L"Processor";
}

/**
 * @brief 保存单个 CPU 逻辑核心上一轮采样时间戳的辅助结构体
 * 包含空闲时间 (idle)、内核态时间 (kernel) 和用户态时间 (user)
 */
struct CoreTimeRecord {
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
};

/**
 * @brief MetricsCollector 的内部实现类 (Pimpl 结构体)
 * 封装了所有采集逻辑、操作系统句柄、历史状态缓存以及后台线程。
 */
struct MetricsCollector::Impl {
    std::thread workerThread;                   ///< 独立的后台工作线程，定时循环采样系统指标
    std::atomic<bool> running{false};           ///< 控制采集线程生命周期的原子标志位
    std::mutex mutex;                           ///< 保护 snapshot 读写的互斥锁，防止 UI 线程和采集线程发生数据竞争

    MetricsSnapshot snapshot;                   ///< 内存快照单一真实源，存储最新一轮的所有硬件指标
    PhysicalDiskSampler physicalDiskSampler;    ///< 物理磁盘采集器（通过 Windows DeviceIoControl 获取读写吞吐与温度）
    NativeGpuSensors gpuSensors;                ///< 原生 GPU 传感器（NVML / 显卡驱动接口）
    int coreCount = 0;                          ///< CPU 逻辑核心总数

    // 单核使用率追踪状态（通过底层 NtQuerySystemInformation）
    std::vector<CoreTimeRecord> prevCoreTimes;  ///< 保存每个核心上一轮采样的 CPU 周期时间
    bool hasPrevCoreTimes = false;              ///< 是否已完成首次初始化采样（首轮只能存基准，第二轮才能算差值）

    // 全局系统时间追踪状态（标准 Win32 GetSystemTimes 保底方案）
    uint64_t lastIdle = 0;
    uint64_t lastKernel = 0;
    uint64_t lastUser = 0;
    bool lastSystemTimeInit = false;

    // 网络收发速率计算基准
    uint64_t lastInBytes = 0;                   ///< 上一次采样时所有网卡的累计下载字节数
    uint64_t lastOutBytes = 0;                  ///< 上一次采样时所有网卡的累计上传字节数
    uint64_t lastNetTimestamp = 0;              ///< 上一次网络采样时的系统毫秒时间戳 (GetTickCount64)

    // 进程 CPU 消耗计算基准
    std::unordered_map<DWORD, uint64_t> prevProcessTimes; ///< 进程 PID -> 上次记录的 CPU 运行时间 (100ns)
    uint64_t prevProcessTimestamp = 0;                    ///< 上次采样进程列表时的时间戳
    int processCycleCount = 0;                            ///< 降采样计数器（进程枚举较为耗时，每 2 秒才采集一次）

    // Windows 性能计数器 (PDH) 采集 GPU 使用率相关状态
    std::atomic<bool> enableGpu{false};         ///< 是否开启 GPU 轮询开关（当 UI 切换到 GPU 详情页时才激活）
    PDH_HQUERY gpuQuery = NULL;                 ///< PDH 查询句柄
    PDH_HCOUNTER gpuCounter = NULL;             ///< GPU 3D 引擎使用率计数器句柄
    bool gpuQueryInit = false;                  ///< PDH 查询是否已成功打开
    bool gpuQueryFailed = false;                ///< 若当前系统不支持或损坏，避免重复抛错

    static constexpr size_t kMaxHistory = 60;   ///< 历史折线图采样点容量（默认 60 秒）

    // 内部模块化采集子函数
    void WorkerLoop();                          ///< 后台线程的主执行循环体
    void UpdateCpu();                           ///< 采集 CPU 总负荷、单核负荷、主频及用户/内核态占比
    void UpdateMemory();                        ///< 采集物理内存已用、空闲、备用缓存及已提交虚拟内存
    void UpdateGpu();                           ///< 采集 GPU 3D 引擎负载
    void UpdateNetwork();                       ///< 采集网络实时上下行速率及累计吞吐
    void UpdateNetworkInfo();                   ///< 采集当前活动网卡型号名称及本机 IP 地址
    void UpdateDisks();                         ///< 采集逻辑盘符剩余空间及物理磁盘 IOPS、吞吐、温度
    void UpdateProcesses();                     ///< 采集占用 CPU 和内存最高的前几个前台/后台进程
    void InitGpuQuery();                        ///< 初始化 PDH 性能计数器查询
    void CloseGpuQuery();                       ///< 释放 PDH 资源
};

/**
 * @brief 通过 DirectX 图形基础设施 (DXGI) 查询独立显卡/核心显卡的型号名称与显存 (VRAM) 大小
 * 
 * 原理说明（新手必读）：
 * DirectX 图形基础设施 (DXGI) 是 Windows 图形驱动的标准接口。
 * 1. CreateDXGIFactory1：创建一个工厂对象，用于枚举系统中的所有图形适配器 (Adapter)。
 * 2. EnumAdapters1：遍历系统中的每张显卡（可能同时有集成核显与独立独显）。
 * 3. 过滤纯软件虚拟驱动（DXGI_ADAPTER_FLAG_SOFTWARE，例如远程桌面或软件模拟驱动）。
 * 4. 择优选择显存最大的一张显卡作为主显卡，提取型号字符串与 DedicatedVideoMemory（独立显存字节数）。
 */
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

/**
 * @brief 构造函数：初始化硬件环境与基准指标
 * 1. 查询 CPU 逻辑核心总数并预分配单核容器
 * 2. 查询 CPU 型号名称 (CPUID)
 * 3. 静态枚举一次活动网卡 IP 与适配器信息
 * 4. 枚举 DXGI 主显卡型号与显存容量
 */
// make_unique 在堆上创建 Impl，unique_ptr 独占它；后台线程尚未启动。
// 构造会读取基础设备信息，成员传感器构造也可能加载厂商 DLL，所以构造并非零成本。
MetricsCollector::MetricsCollector() : m_impl(std::make_unique<Impl>()) {
    pcinfo::diagnostics::MemoryEvent("collector.objects.ready");
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_impl->coreCount = sysInfo.dwNumberOfProcessors;
    m_impl->snapshot.cpuCoreUsage.resize(m_impl->coreCount, 0.0f);
    m_impl->prevCoreTimes.resize(m_impl->coreCount);
    m_impl->snapshot.cpuBrand = QueryCpuBrand();
    m_impl->UpdateNetworkInfo();
    pcinfo::diagnostics::MemoryEvent("collector.network-info.ready");
    QueryGpuInfo(m_impl->snapshot.gpuName, m_impl->snapshot.gpuVramTotalBytes);
    pcinfo::diagnostics::MemoryEvent("collector.dxgi-info.ready");
}

/**
 * @brief 析构函数：确保安全退出后台采集线程，释放句柄资源，避免悬挂指针
 */
// 先 Stop 等待线程结束；随后 unique_ptr 自动析构 Impl 和各采集成员。
MetricsCollector::~MetricsCollector() {
    Stop();
}

/**
 * @brief 启动后台采集线程
 * 使用 atomic exchange 保证并发安全性，如果已经在运行则直接返回
 */
// exchange 原子地把 running 设为 true 并返回旧值，避免重复启动第二条采集线程。
void MetricsCollector::Start() {
    if (m_impl->running.exchange(true)) return;
    m_impl->workerThread = std::thread(&MetricsCollector::Impl::WorkerLoop, m_impl.get());
}

/**
 * @brief 停止后台采集线程
 * 将 running 标志位置为 false，并等待线程安全结束 (join)，关闭 PDH 句柄
 */
void MetricsCollector::Stop() {
    if (!m_impl->running.exchange(false)) return;
    if (m_impl->workerThread.joinable()) {
        // join 等待线程真正结束，确保 Impl 成员不会在线程仍访问时被销毁。
        m_impl->workerThread.join();
    }
    m_impl->CloseGpuQuery();
}

/**
 * @brief 动态启用/禁用 GPU 轮询
 * 考虑到 Windows PDH 轮询 GPU 会带来微量的 CPU 唤醒开销，仅在 UI 展开 GPU 监控时开启
 */
// 此开关控制 PDH GPU 利用率查询；它不等于卸载 NVML，也不关闭所有 GPU 传感器读取。
void MetricsCollector::SetGpuEnabled(bool enable) {
    if (m_impl) {
        m_impl->enableGpu.store(enable);
        if (!enable) {
            m_impl->CloseGpuQuery();
        }
    }
}

/**
 * @brief 关闭并重置 PDH 性能计数器查询句柄
 */
void MetricsCollector::Impl::CloseGpuQuery() {
    if (gpuQuery) {
        PdhCloseQuery(gpuQuery);
        gpuQuery = NULL;
        gpuCounter = NULL;
        gpuQueryInit = false;
        gpuQueryFailed = false;
    }
}

/**
 * @brief 获取最新一轮硬件快照的副本（线程安全）
 * UI 渲染线程每帧或每秒调用此函数，互斥锁加锁后快速执行一次拷贝，保证 UI 不会读到半更新的脏数据
 */
// lock_guard 离开作用域自动解锁，称为 RAII：资源释放跟随对象生命周期。
// 返回数据副本，UI 不直接共享可变数据；string/vector 的复制仍可能申请内存。
// 互斥锁保护读写安全，不保证所有指标来自同一瞬间。
MetricsSnapshot MetricsCollector::GetLatestSnapshot() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->snapshot;
}

/**
 * @brief 后台采集线程的主循环体 (Worker Loop)
 * 
 * 核心设计（新手必读）：
 * 1. 独立线程拥有专属局部 COM 对象：例如联想 WMI 传感器对象在采集线程内创建，
 *    避免跨线程套间 (Apartment) 调用的性能锁开销与死锁隐患。
 * 2. 管道式采样流程：依次采集 CPU、内存、GPU（可选）、网络、磁盘。
 * 3. 硬件温度/风扇扩展传感器（GPU/联想OEM/AMD SMU）采样并记录性能耗时跟踪 (diagnostics::Scope)。
 * 4. 聚合数据写入 Snapshot：通过 std::lock_guard 批量更新写入。
 * 5. 节奏控制：采样后休眠约 1000ms，实际周期包括查询耗时；分段休眠方便检查退出标志。
 */
void MetricsCollector::Impl::WorkerLoop() {
    // 局部传感器的生命周期覆盖整段采样循环，避免每轮重复连接 WMI。
    NativeLenovoSensors lenovoSensors; // COM 传感器归采集线程独占
    NativeAmdTemperature amdTemperature;
    bool firstMemoryCycle=pcinfo::diagnostics::GetTrace().file!=nullptr;
    const auto memory=[&](const char* label){if(firstMemoryCycle)pcinfo::diagnostics::MemoryEvent(label);};
    while (running) {
        UpdateCpu();
        memory("collector.cpu.ready");
        UpdateMemory();
        memory("collector.memory.ready");
        // 原子 load 安全读取 UI 更新的开关；只跳过这一段 PDH 采样。
        if (enableGpu.load()) {
            UpdateGpu();
            memory("collector.gpu.ready");
        }
        UpdateNetwork();
        memory("collector.network.ready");
        UpdateDisks();
        memory("collector.disks.ready");

        // 扩展硬件传感器独立采样（通过 scope 记录每一项的微秒级耗时）
        const auto sensors = [&] {
            pcinfo::diagnostics::Scope scope("sensor.Gpu");
            return gpuSensors.Sample(snapshot.gpuName);
        }();
        memory("collector.gpu-sensors.ready");
        const auto laptop = [&] {
            pcinfo::diagnostics::Scope scope("sensor.Lenovo");
            return lenovoSensors.Sample();
        }();
        memory("collector.lenovo.ready");
        const auto amd = [&] {
            pcinfo::diagnostics::Scope scope("sensor.PawnIO");
            return amdTemperature.Sample();
        }();
        memory("collector.pawnio.ready");
        firstMemoryCycle=false;

        {
            // 加锁将本轮采集的所有传感器数据合并写入快照
            std::lock_guard<std::mutex> lock(mutex);
            snapshot.gpuTempC = sensors.temperature;
            snapshot.gpuPowerWatts = sensors.watts;
            snapshot.gpuFreqGHz = sensors.ghz;
            snapshot.gpuVramUsedBytes = sensors.used;
            snapshot.gpuVramTotalBytes = sensors.memoryValid ? sensors.total : 0;
            snapshot.laptopFanCount = laptop.fanCount;
            snapshot.cpuFanRpm = laptop.fan1;
            snapshot.caseFanRpm = laptop.fan2;
            snapshot.sensorPermissionRequired = (laptop.status == WBEM_E_ACCESS_DENIED || laptop.status == E_ACCESSDENIED);
            snapshot.sensorProviderStatus = laptop.status;

            // CPU 温度来源仲裁：AMD SMU 硬件直读优先，其次是联想 OEM ACPI，最后才进入不支持/NAN 抖动保护状态
            if (amd.temperature > 0) {
                snapshot.temperatureStatus.Record(amd.temperature, snapshot.cpuTempC, snapshot.tempHistory, amd.sampleTime);
            } else if (laptop.cpuTemperature > 0) {
                snapshot.temperatureStatus.Record(laptop.cpuTemperature, snapshot.cpuTempC, snapshot.tempHistory, laptop.sampleTime);
            } else if (snapshot.temperatureStatus.lastValidMs) {
                snapshot.temperatureStatus.Record(NAN, snapshot.cpuTempC, snapshot.tempHistory, laptop.sampleTime);
            } else {
                snapshot.temperatureStatus.state = MetricDataState::Unsupported;
                snapshot.cpuTempC = 0;
            }
        }

        // 进程列表采样较为耗时，执行降采样：每两轮（2秒）更新一次
        if (++processCycleCount >= 2) {
            processCycleCount = 0;
            UpdateProcesses();
        }

        // 采样间隔 1000ms：细分为 10 次 100ms 休眠，方便检查退出标志；正在执行的系统查询仍需等待完成
        for (int i = 0; i < 10 && running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// // 将 Windows FILETIME 的高低 32 位合并为一个 64 位计数值（100 纳秒单位）
/**
 * 合并 FILETIME 高低 32 位为 64 位计数，单位保持 100ns。
 */
static uint64_t FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

/**
 * @brief 采集 CPU 综合使用率、单核独立使用率、用户态/内核态划分，以及当前运行主频
 * 
 * 核心原理（新手必读）：
 * 1. 为什么不用简单的 GetSystemTimes？
 *    GetSystemTimes 只能拿到全系统合并的总 CPU 时间，无法拿到每个独立 CPU 逻辑核心（如 16 线程）的负荷。
 * 2. NtQuerySystemInformation(SystemProcessorPerformanceInformation = 8)：
 *    Windows NT 内核原生接口，能一次性返回一个包含 N 个核心的结构体数组。
 * 3. 极其重要的坑点（Windows NT 经典设计）：
 *    在 Windows 内核返回的数据中，KernelTime（内核时间）是包含了 IdleTime（CPU 空闲时间）的！
 *    因此，真正执行操作系统代码的纯内核态时间是：actualKernel = KernelTime - IdleTime。
 *    而总时间 totalTime = (KernelTime - IdleTime) + UserTime + IdleTime = KernelTime + UserTime。
 *    CPU 使用率 = (1.0 - IdleTime / totalTime) * 100%。
 * 4. 保底方案 (Fallback)：若某些受限环境阻止调用 NT API，则自动降级使用 Win32 的 GetSystemTimes。
 * 5. 主频采集：通过 powrprof.dll 的 CallNtPowerInformation(ProcessorInformation = 11)，
 *    查询每个核心当前的 CurrentMhz，取平均值转为 GHz。
 */
void MetricsCollector::Impl::UpdateCpu() {
    pcinfo::diagnostics::Scope perfScope("collect.Cpu");
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

                // 遍历每个逻辑核心，计算两次采样之间的时间差值 (Delta)
                for (int i = 0; i < coreCount; ++i) {
                    uint64_t idleDiff = perfInfo[i].IdleTime.QuadPart - prevCoreTimes[i].idle;
                    uint64_t kernelDiff = perfInfo[i].KernelTime.QuadPart - prevCoreTimes[i].kernel;
                    uint64_t userDiff = perfInfo[i].UserTime.QuadPart - prevCoreTimes[i].user;
                    uint64_t totalDiff = kernelDiff + userDiff; // 注意：Windows 中 kernelDiff 已包含 idleDiff

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

                    // 记录本轮采样时间，作为下一轮计算的基准
                    prevCoreTimes[i].idle = perfInfo[i].IdleTime.QuadPart;
                    prevCoreTimes[i].kernel = perfInfo[i].KernelTime.QuadPart;
                    prevCoreTimes[i].user = perfInfo[i].UserTime.QuadPart;
                }
                total = std::clamp(sumUsage / static_cast<float>(coreCount), 0.0f, 100.0f);
                gotData = true;

                uint64_t totalSysDiff = totalKernelDiff + totalUserDiff;
                if (totalSysDiff > 0) {
                    // 扣除空闲时间，得到真正的纯操作系统内核态开销
                    uint64_t actualKernelDiff = (totalKernelDiff >= totalIdleDiff) ? (totalKernelDiff - totalIdleDiff) : 0;
                    std::lock_guard<std::mutex> lock(mutex);
                    snapshot.cpuUserPercent = std::clamp(static_cast<float>(totalUserDiff * 100.0 / totalSysDiff), 0.0f, 100.0f);
                    snapshot.cpuKernelPercent = std::clamp(static_cast<float>(actualKernelDiff * 100.0 / totalSysDiff), 0.0f, 100.0f);
                    snapshot.cpuIdlePercent = std::clamp(static_cast<float>(totalIdleDiff * 100.0 / totalSysDiff), 0.0f, 100.0f);
                }
            } else {
                // 第一轮采样：仅记录初始时间基准，不计算使用率（避免冷启动计算出现除以零或错误尖峰）
                for (int i = 0; i < coreCount; ++i) {
                    prevCoreTimes[i].idle = perfInfo[i].IdleTime.QuadPart;
                    prevCoreTimes[i].kernel = perfInfo[i].KernelTime.QuadPart;
                    prevCoreTimes[i].user = perfInfo[i].UserTime.QuadPart;
                }
                hasPrevCoreTimes = true;
            }
        }
    }

    // 保底方案：若 NT API 调用失败，使用标准 Win32 GetSystemTimes 计算全系统平均负载
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

/**
 * @brief 读取物理内存与虚拟内存指标（已用、可用、系统缓存、已提交、提交上限等）
 * 
 * 核心原理（新手必读）：
 * 1. GlobalMemoryStatusEx 与 GetPerformanceInfo 的区别：
 *    - GlobalMemoryStatusEx：简单易用，返回总物理内存和可用物理内存，但无法区分可用内存中
 *      有多少是被系统作为文件缓存（Standby Cache），有多少是真正未被映射的干净空闲页（Free）。
 *    - GetPerformanceInfo (psapi.dll)：返回底层的系统分页指标（以系统 PageCount 为单位，通常 1 页 = 4KB）。
 *      通过它可以精确获取 SystemCache（系统文件缓存）、CommitTotal（全系统已提交虚拟内存页）以及
 *      CommitLimit（当前物理内存 + 虚拟内存分页文件的总上限）。
 * 2. 严谨性说明：
 *    Win32 的 SystemCache 仅反映文件缓存，不包含所有待机内存页。因此 snapshot.memoryCompositionValid
 *    标记为 false 提醒调用方这属于估算划分，但物理总内存与在用内存是完全精确的。
 */
void MetricsCollector::Impl::UpdateMemory() {
    pcinfo::diagnostics::Scope perfScope("collect.Memory");
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

/**
 * @brief 初始化 Windows PDH (Performance Data Helper) GPU 引擎性能计数器
 * 
 * 核心原理（新手必读）：
 * Windows 10/11 内置了 GPU 引擎的性能计数器，格式如：
 * "\\GPU Engine(pid_1234_luid_0x0000_phys_0_eng_0_engtype_3D)\\Utilization Percentage"
 * 使用通配符 (*engtype_3D) 可以让 Windows 自动匹配出所有进程当前在 3D 渲染引擎上的负载。
 * 若通配符匹配失败，则降级尝试所有 GPU 引擎 (*)。
 */
void MetricsCollector::Impl::InitGpuQuery() {
    if (gpuQueryInit || gpuQueryFailed) return;

    PDH_STATUS status = PdhOpenQuery(NULL, 0, &gpuQuery);
    if (status != ERROR_SUCCESS) {
        gpuQueryFailed = true;
        return;
    }

    // 添加 3D 渲染引擎通配符计数器
    status = PdhAddEnglishCounterW(gpuQuery, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &gpuCounter);
    if (status != ERROR_SUCCESS) {
        // 保底：若指定 3D 引擎失败，则尝试匹配所有 GPU Engine 实例
        status = PdhAddEnglishCounterW(gpuQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpuCounter);
    }

    if (status == ERROR_SUCCESS) {
        PdhCollectQueryData(gpuQuery); // 第一次采样作为基准底数 (Primer read)
        gpuQueryInit = true;
    } else {
        PdhCloseQuery(gpuQuery);
        gpuQuery = NULL;
        gpuQueryFailed = true;
    }
}

/**
 * @brief 采集 GPU 3D 引擎使用率，并累加所有活跃 3D 渲染进程的百分比
 * 
 * 核心原理（新手必读）：
 * 1. PdhCollectQueryData：刷新并采集一次计数器数据。
 * 2. 通配符数组展开 (PdhGetFormattedCounterArrayW)：
 *    因为可能同时有多个前台/后台应用（例如游戏 + 浏览器）在使用显卡，
 *    通配符会返回一个包含多个项的数组。我们首先以 NULL 缓冲区调用一次获取所需内存大小 (bufferSize)，
 *    随后分配 vector<BYTE> 并再次调用解析出每个引擎实例的使用率 doubleValue。
 * 3. 将所有引擎的使用率累加求和，并 clamp 在 0% ~ 100% 之间。
 */
void MetricsCollector::Impl::UpdateGpu() {
    pcinfo::diagnostics::Scope perfScope("collect.Gpu");
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
            // 第一次调用：探测通配符匹配到的所有实例需要的缓冲区尺寸
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
                // 如果是单个非通配符实例
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

/**
 * @brief 采集物理网卡累计收发字节数，根据时间差量 (Delta) 计算实时上下行网络流速 (Bps)
 * 
 * 核心原理（新手必读）：
 * 1. 为什么不用旧版的 GetIfTable？
 *    GetIfTable 返回的字节计数字段为 32 位整数 (4GB 上限)，在千兆宽带下一个大文件下载几分钟就会发生
 *    整数溢出归零 (Wrap-around)。
 *    现代 Windows 采用 GetIfTable2()，返回 64 位整型的 InOctets / OutOctets，永不溢出。
 * 2. 网卡过滤规则：
 *    - HardwareInterface 为 true：排除虚拟网卡、VPN 虚拟适配器、虚拟机网卡。
 *    - OperStatus == IfOperStatusUp：仅统计当前已插网线/已连 Wi-Fi 的连接状态网卡。
 *    - Type != IF_TYPE_SOFTWARE_LOOPBACK：排除本机 127.0.0.1 回环通信。
 * 3. 速率计算公式：
 *    流速 (Bytes/s) = (当前总字节数 - 上次总字节数) / 采样流逝秒数 (elapsedSec)。
 */
void MetricsCollector::Impl::UpdateNetwork() {
    pcinfo::diagnostics::Scope perfScope("collect.Network");
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
/**
 * @brief 枚举本地逻辑驱动器分区与物理磁盘，采集可用空间、分区卷标及物理磁盘吞吐/温度
 * 
 * 核心原理（新手必读）：
 * 1. 逻辑卷与物理磁盘分离设计：
 *    - 用户看到的通常是逻辑盘符（C:、D:、E:），一个物理固态硬盘可以被划分成多个逻辑分区。
 *    - GetLogicalDrives()：返回一个 32 位的驱动器位掩码（第 0 位为 A:，第 2 位为 C:）。
 *    - GetDriveTypeW == DRIVE_FIXED：只保留本地内置硬盘，忽略可移动 U 盘、虚拟光驱或网络共享映射盘。
 *    - GetDiskFreeSpaceExW：快速无阻塞地获取该分区的总容量 (totalBytes) 和用户可用空间 (freeBytes)。
 * 2. 物理磁盘读写速率与温度采样：
 *    - 将枚举出的逻辑分区传递给 physicalDiskSampler.Sample(disks)。
 *    - 物理磁盘采样器会通过 DeviceIoControl 底层命令查询物理磁盘的实际读写 Bps 以及 NVMe/SATA 芯片温度。
 */
void MetricsCollector::Impl::UpdateDisks() {
    pcinfo::diagnostics::Scope perfScope("collect.Disks");
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

    // 委派给物理磁盘采样器获取各物理硬盘的吞吐与温度
    auto physical = physicalDiskSampler.Sample(disks);
    uint64_t read = 0, write = 0;
    bool valid = !physical.empty();
    for (const auto& disk : physical) {
        if (disk.throughputValid) {
            read += disk.readBps;
            write += disk.writeBps;
        } else {
            valid = false;
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    snapshot.disks = std::move(disks);
    snapshot.physicalDisks = std::move(physical);
    snapshot.ssdTempC = 0;
    for (const auto& disk : snapshot.physicalDisks) {
        if (disk.temperatureValid) {
            snapshot.ssdTempC = disk.temperatureC;
            break;
        }
    }
    snapshot.diskModelName.clear();
    snapshot.diskBusType.clear();
    snapshot.diskStatisticsValid = false;
    snapshot.diskReadBps = read;
    snapshot.diskWriteBps = write;
    const float r = valid ? static_cast<float>(read / 1000000.0) : std::numeric_limits<float>::quiet_NaN();
    const float w = valid ? static_cast<float>(write / 1000000.0) : std::numeric_limits<float>::quiet_NaN();
    snapshot.diskHistory.Push(r);
    snapshot.diskReadHistory.Push(r);
    snapshot.diskWriteHistory.Push(w);
}

/**
 * @brief 采集活动网卡的友好名称（如 "WLAN" 或 "以太网"）及本机内网 IP 地址
 * 
 * 原理说明（新手必读）：
 * 使用 IP 帮助程序 API (Iphlpapi.dll) 中的 GetAdaptersAddresses()。
 * 遍历已连接且非环回的网卡，从其 FirstUnicastAddress 链表中提取首个单播 IPv4 地址，
 * 并将其格式化为标准的点分十进制字符串（如 192.168.1.100）。
 */
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

/**
 * @brief 采集系统全量进程列表，计算各进程 CPU 占用率与内存驻留集 (WorkingSet)，排序提取 Top3
 * 
 * 核心原理（新手必读）：
 * 1. 为什么不用 CreateToolhelp32Snapshot？
 *    Toolhelp32 快照在系统进程多时耗时可达数十毫秒，开销很大。
 *    而 NtQuerySystemInformation(SystemProcessInformation = 5) 是 Windows 任务管理器底层
 *    同款系统调用，仅需一次内核切换就能把所有进程、线程及其精确执行时间以单链表格式全部 Dump 到内存。
 * 2. 进程 CPU 使用率计算：
 *    - 每个进程结构体提供 KernelTime 与 UserTime（100 纳秒单位）。
 *    - 计算两次采样之间该进程消耗的 CPU 时间差值 (diffTime)。
 *    - 公式：cpuPercent = (diffTime / (elapsedMs * 10,000 * coreCount)) * 100%。
 * 3. 内存聚合与多进程同名合并：
 *    现代浏览器（Chrome、Edge）和大型开发软件（VS Code）都是多进程架构。
 *    我们用 memMap 按友好应用名称累加 WorkingSetSize（工作集物理内存），以便向用户展示该软件总共占用的物理内存。
 */
void MetricsCollector::Impl::UpdateProcesses() {
    pcinfo::diagnostics::Scope perfScope("collect.Processes");
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

    // 遍历内核返回的单向链表
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

            // 对常见知名软件名称进行友好美化（避免显示为长条冷冰冰的 xxx.exe）
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

            // 统计该进程的物理工作集内存并按友好名称聚合
            if (!cleanName.empty() && pInfo->WorkingSetSize > 0) {
                memMap[cleanName] += pInfo->WorkingSetSize;
            }

            // 计算 CPU 占用百分比
            if (elapsedMs > 0) {
                auto it = prevProcessTimes.find(pid);
                if (it != prevProcessTimes.end()) {
                    uint64_t diffTime = (totalProcTime >= it->second) ? (totalProcTime - it->second) : 0;
                    // diffTime 单位是 100 纳秒（10,000 单位 = 1 毫秒）
                    double cpuPercent = (static_cast<double>(diffTime) / (static_cast<double>(elapsedMs) * 10000.0 * (coreCount > 0 ? coreCount : 1))) * 100.0;
                    if (cpuPercent > 0.05) {
                        procs.push_back({ cleanName, static_cast<float>(cpuPercent) });
                    }
                }
            }
        }

        if (pInfo->NextEntryOffset == 0) break; // 链表末尾
        ptr += pInfo->NextEntryOffset;
    }

    prevProcessTimes = std::move(currentMap);

    // 按 CPU 占用率降序排序，提取 Top 3
    std::vector<ProcessUsageInfo> top3;
    if (!procs.empty()) {
        std::sort(procs.begin(), procs.end(), [](const ProcEntry& a, const ProcEntry& b) {
            return a.cpu > b.cpu;
        });

        for (size_t i = 0; i < procs.size() && top3.size() < 3; ++i) {
            top3.push_back({ procs[i].name, procs[i].cpu });
        }
    }

    // 按物理内存占用降序排序，提取 Top 3
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
