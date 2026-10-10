/*
 * 模块说明：按需开启的性能日志
 * PCINFO_PERF_TRACE 指定覆盖写入的日志路径；作用域记录实际经过时间和当前线程 CPU 时间。
 * 嵌套作用域会重叠，不能简单相加；进程内存检查点是总量，不是该函数独占分配量。
 * 未开启时跳过计时和写文件，但仍有状态检查成本，不能称为严格零开销。
 */
#pragma once

#include <windows.h>
#include <cstdio>
#include <share.h>
#include <mutex>
#include <map>
#include <algorithm>
#include <filesystem>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

/**
 * @brief 内部诊断性能追踪命名空间 (pcinfo::diagnostics)
 * 
 * 新手必读说明：
 * 这是一个按需启用的性能分析跟踪器 (Opt-in Performance Profiler)。
 * 未开启日志时跳过计时和输出，仍有状态检查及首次初始化的少量开销。
 * 只有当开发者在系统或终端中设置了环境变量 `PCINFO_PERF_TRACE="C:\\path\\perf.log"` 时，
 * 它才会开启高精度计时器 (QueryPerformanceCounter)，记录各个函数与局部作用域的 CPU 耗时与墙上时间 (Wall Time)，
 * 帮助开发者定位 Direct2D 渲染或性能采样过程中的掉帧和毫秒级卡顿。
 */
namespace pcinfo::diagnostics {

/**
 * @brief 全局跟踪日志与统计聚合管理器
 */
struct Trace {
    /// @brief 针对某个具名作用域 (Scope) 的统计数据
    struct Stats {
        unsigned count = 0;     ///< 调用总次数
        double wall = 0;        ///< 墙上物理时间总耗时 (毫秒)
        double cpu = 0;         ///< 线程 CPU 时间总耗时 (毫秒)
        double maxWall = 0;     ///< 单次最大墙上耗时 (毫秒)
        double maxCpu = 0;      ///< 单次最大 CPU 耗时 (毫秒)
    };

    FILE* file = nullptr;                   ///< 跟踪日志输出文件句柄
    std::mutex mutex;                       ///< 互斥锁，保护多线程并发统计聚合
    // key 按指针索引，不复制名字内容；现有调用传字符串字面量，需确保名字生命周期足够长。
    std::map<const char*, Stats> stats;     ///< 按作用域名字索引的性能统计字典
    ULONGLONG last = GetTickCount64();      ///< 上次刷新文件的时间戳

    /**
     * @brief 构造函数：检查环境变量 PCINFO_PERF_TRACE 并按需创建输出日志文件
     */
    Trace() {
        wchar_t path[32768];
        const DWORD length = GetEnvironmentVariableW(L"PCINFO_PERF_TRACE", path, 32768);
        if (length && length < 32768) {
            std::error_code error;
            const auto parent = std::filesystem::path(path).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent, error);
            if (!error) file = _wfsopen(path, L"w", _SH_DENYWR);
        }
        if (file) {
            std::fprintf(file, "# pid=%lu; interval summaries, wall/cpu in ms; nested scopes overlap\n", GetCurrentProcessId());
            std::fflush(file);
        }
    }

    /**
     * @brief 定期将内存中累积的性能统计刷新输出到磁盘文件
     */
    void Flush() {
        SYSTEMTIME time;
        GetLocalTime(&time);
        for (const auto& [name, s] : stats)
            std::fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu scope=%s calls=%u wall_ms=%.3f cpu_ms=%.3f max_wall_ms=%.3f max_cpu_ms=%.3f\n",
                time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                GetCurrentProcessId(), name, s.count, s.wall, s.cpu, s.maxWall, s.maxCpu);
        stats.clear();
        std::fflush(file);
    }

    ~Trace() {
        if (file) {
            Flush();
            std::fclose(file);
        }
    }
};

/// @brief 获取单例性能跟踪实例
/**
 * 返回函数局部 static 跟踪器；首次使用时初始化，以后复用同一实例。
 */
inline Trace& GetTrace() {
    static Trace trace;
    return trace;
}

// 工作集是驻留物理页面，PrivateUsage 是私有提交量，两者不是同一口径。
// 这里记录整个进程的检查点，不能把前后差值直接当成某个线程或函数的独占分配。
/**
 * 记录进程工作集、私有提交和句柄等检查点；资源估计量与进程总量不能相加。
 */
inline void MemoryEvent(const char* label, size_t bytes = 0, size_t count = 0) {
    auto& trace = GetTrace();
    if (!trace.file) return;
    PROCESS_MEMORY_COUNTERS_EX memory{sizeof(memory)};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory))) return;
    DWORD handles = 0; GetProcessHandleCount(GetCurrentProcess(), &handles);
    SYSTEMTIME time; GetLocalTime(&time);
    std::lock_guard lock(trace.mutex);
    std::fprintf(trace.file,"%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu memory=%s working_set_bytes=%llu private_bytes=%llu resource_bytes=%llu resource_count=%llu handles=%lu gdi=%lu user=%lu\n",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
        GetCurrentProcessId(),label,static_cast<unsigned long long>(memory.WorkingSetSize),static_cast<unsigned long long>(memory.PrivateUsage),
        static_cast<unsigned long long>(bytes),static_cast<unsigned long long>(count),handles,
        GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS));
    std::fflush(trace.file);
}

class MemoryScope {
    const char* label;
public:
    explicit MemoryScope(const char* name) : label(name) {}
    ~MemoryScope() { if(label)MemoryEvent(label); }
};

/**
 * 记录当前加载的模块名，用于检查哪些 DLL 在某个阶段进入进程。
 */
inline void ModulesEvent(const char* label) {
    auto& trace=GetTrace();if(!trace.file)return;
    HMODULE modules[512];DWORD bytes=0;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes))return;
    std::lock_guard lock(trace.mutex);
    for(size_t i=0;i<std::min<size_t>(bytes/sizeof(HMODULE),512);++i){
        char name[MAX_PATH]{};
        if(GetModuleBaseNameA(GetCurrentProcess(),modules[i],name,MAX_PATH))
            std::fprintf(trace.file,"# pid=%lu modules=%s name=%s\n",GetCurrentProcessId(),label,name);
    }
    std::fflush(trace.file);
}

/**
 * @brief 获取当前线程在内核与用户态累积消耗的 CPU 时间 (精确到毫秒)
 */
inline double ThreadCpu() {
    FILETIME created, exited, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return 0;
    const auto value = [](FILETIME t) {
        return (static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return (value(kernel) + value(user)) / 10000.0;
}

/**
 * @brief RAII 作用域自动耗时计时器 (Scope)
 * 
 * 用法极为简单：在需要测量的函数或代码块开头写一行 `pcinfo::diagnostics::Scope scope("FunctionName");` 即可。
 * 构造时自动打点开始，析构（退出代码块）时自动计算差值并记录。
 */
class Scope {
    const char* name;
    Trace* trace;
    LARGE_INTEGER start{};
    double cpu = 0;
public:
    explicit Scope(const char* label) : name(label), trace(&GetTrace()) {
        if (trace->file) {
            QueryPerformanceCounter(&start);
            cpu = ThreadCpu();
        }
    }
    ~Scope() {
        if (!trace->file) return;
        // 实际经过时间包含等待，线程 CPU 时间不含睡眠，也不统计图形库其他工作线程。
        // 内外嵌套 Scope 可能同时统计同一段执行时间，不能把它们简单相加。
        const double used = ThreadCpu() - cpu;
        LARGE_INTEGER end, frequency;
        QueryPerformanceCounter(&end);
        QueryPerformanceFrequency(&frequency);
        const double elapsed = 1000.0 * (end.QuadPart - start.QuadPart) / frequency.QuadPart;
        std::lock_guard lock(trace->mutex);
        auto& s=trace->stats[name];++s.count;s.wall+=elapsed;s.cpu+=used;
        s.maxWall=std::max(s.maxWall,elapsed);s.maxCpu=std::max(s.maxCpu,used);
        const auto now=GetTickCount64();
        if(now-trace->last>=1000){trace->Flush();trace->last=now;}
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
