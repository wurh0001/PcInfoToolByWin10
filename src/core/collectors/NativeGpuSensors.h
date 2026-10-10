/*
 * 模块说明：NVIDIA NVML 传感器
 * 从系统目录动态加载显卡驱动提供的 NVML，匹配 DXGI 型号后读取温度、功耗、频率和显存。
 * 导出函数缺失或查询失败时保留默认无数据结果；是否有效还需看对应状态和字段。
 * 动态加载免去静态 SDK 依赖，但 DLL 和初始化缓存仍会计入进程内存。
 */
#pragma once

#include <windows.h>
#include <string>
#include <cstdint>
#include "core/diagnostics/PerformanceTrace.h"

/**
 * @brief NVIDIA GPU 显卡原生传感器采集器 (NativeGpuSensors)
 * 
 * 核心设计亮点（极客级绿色软件规范）：
 * 1. 绝对不安装、不打包任何第三方臃肿显卡 SDK，也不安装任何内核驱动服务！
 * 2. 纯动态延迟绑定 (Dynamic Delay-Load)：
 *    直接在 Windows 官方系统目录 (System32) 中寻找用户显卡驱动自带的 `nvml.dll` (NVIDIA Management Library)。
 * 3. 如果用户电脑是纯集显 (Intel / AMD)，或者未安装独显驱动，LoadLibrary 失败会优雅忽略，
 *    此路径返回默认读数，不主动弹窗；这里只实现 NVML，不代表 Intel/AMD 同等支持。
 */
class NativeGpuSensors {
    HMODULE library = nullptr;      ///< 动态加载的 nvml.dll 模块句柄
    bool initialized = false;       ///< NVML 子系统是否成功初始化 (nvmlInit_v2)
    using Device = void*;           ///< NVML 内部的 nvmlDevice_t 不透明设备句柄

    /**
     * @brief 动态查找并强转 NVML C-API 函数指针的辅助模板函数
     * @tparam T 函数指针类型
     * @param name DLL 中的导出的 C 函数符号名
     */
    template<class T> T Function(const char* name) const {
        return reinterpret_cast<T>(GetProcAddress(library, name));
    }

public:
    /// @brief GPU 传感器采样读数结果
    struct Reading {
        float temperature = 0;      ///< 显卡核心温度 (摄氏度 °C)
        float watts = 0;            ///< 显卡整板实时功耗 (瓦特 W)
        float ghz = 0;              ///< 显卡核心当前图形时钟频率 (GHz)
        uint64_t used = 0;          ///< 显存已使用字节数 (Byte)
        uint64_t total = 0;         ///< 显存总容量字节数 (Byte)
        bool memoryValid = false;   ///< 显存读数是否准确有效
    };

    /**
     * @brief 构造函数：尝试从系统 System32 路径加载显卡驱动提供的 nvml.dll 并初始化
     */
    NativeGpuSensors() {
        /**
         * 记录进程工作集、私有提交和句柄等检查点；资源估计量与进程总量不能相加。
         */
        pcinfo::diagnostics::MemoryEvent("gpu.nvml.begin");
        pcinfo::diagnostics::MemoryScope memory("gpu.nvml.ready");
        wchar_t system[MAX_PATH]{};
        // 获取系统目录 (如 C:\Windows\System32)
        if (!GetSystemDirectoryW(system, MAX_PATH)) return;

        // 仅在系统受信目录中查找 nvml.dll，防止 DLL 劫持安全漏洞
        library = LoadLibraryExW((std::wstring(system) + L"\\nvml.dll").c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (library) {
            auto init = Function<int(__cdecl*)()>("nvmlInit_v2");
            initialized = (init && init() == 0); // nvmlInit 返回 0 代表 NVML_SUCCESS
        }
    }

    /**
     * @brief 析构函数：逆向安全注销 NVML 环境并释放动态库
     */
    ~NativeGpuSensors() {
        if (initialized) {
            auto shutdown = Function<int(__cdecl*)()>("nvmlShutdown");
            if (shutdown) shutdown();
        }
        if (library) FreeLibrary(library);
    }

    /**
     * @brief 执行显卡传感器采样
     * 
     * @param adapterName 当前主 DXGI 显卡适配器名称 (如 L"NVIDIA GeForce RTX 4060 Laptop GPU")
     *                    严格按型号比对，绝不将独显的读数张冠李戴到核显上！
     * @return Reading 采集到的各项指标读数
     */
    Reading Sample(const std::wstring& adapterName) const {
        Reading r;
        if (!initialized) return r;

        // 获取显卡数量、显卡句柄和型号名称的 API 指针
        auto count = Function<int(__cdecl*)(unsigned*)>("nvmlDeviceGetCount_v2");
        auto handle = Function<int(__cdecl*)(unsigned, Device*)>("nvmlDeviceGetHandleByIndex_v2");
        auto name = Function<int(__cdecl*)(Device, char*, unsigned)>("nvmlDeviceGetName");
        unsigned n = 0;
        if (!count || !handle || !name || count(&n) != 0) return r;

        // 遍历所有已识别的 NVIDIA GPU 设备
        for (unsigned i = 0; i < n; ++i) {
            Device d = nullptr;
            char text[256]{};
            if (handle(i, &d) != 0 || name(d, text, sizeof(text)) != 0) continue;

            // 将 NVML 返回的 UTF-8 窄字符串显卡名称转为宽字符以进行匹配
            wchar_t wide[256]{};
            MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, 256);

            // 核心安全原则：仅在名称匹配时读取，严防多卡/核显混淆
            if (adapterName != wide) continue;

            // 1. 读取 GPU 温度 (nvmlDeviceGetTemperature)
            unsigned value = 0;
            auto temp = Function<int(__cdecl*)(Device, unsigned, unsigned*)>("nvmlDeviceGetTemperature");
            if (temp && temp(d, 0, &value) == 0 && value <= 200) {
                r.temperature = static_cast<float>(value);
            }

            // 2. 读取整板功耗 (nvmlDeviceGetPowerUsage，单位是毫瓦 mW，除以 1000 得到 W)
            auto power = Function<int(__cdecl*)(Device, unsigned*)>("nvmlDeviceGetPowerUsage");
            if (power && power(d, &value) == 0) {
                r.watts = value / 1000.f;
            }

            // 3. 读取图形时钟频率 (nvmlDeviceGetClockInfo，单位是 MHz，除以 1000 得到 GHz)
            auto clock = Function<int(__cdecl*)(Device, unsigned, unsigned*)>("nvmlDeviceGetClockInfo");
            if (clock && clock(d, 0, &value) == 0) {
                r.ghz = value / 1000.f;
            }

            // 4. 读取显存使用情况 (nvmlDeviceGetMemoryInfo)
            struct Memory { unsigned long long total, free, used; } memory{};
            auto mem = Function<int(__cdecl*)(Device, Memory*)>("nvmlDeviceGetMemoryInfo");
            if (mem && mem(d, &memory) == 0 && memory.used <= memory.total) {
                r.used = memory.used;
                r.total = memory.total;
                r.memoryValid = true;
            }
            break; // 已经匹配到目标设备，退出循环
        }
        return r;
    }
};
