/*
 * 模块说明：联想 WMI 只读查询
 * 建立 ROOT\WMI 连接并查找 LENOVO_GAMEZONE_DATA 实例，通过方法返回 Data 字段。
 * BSTR、VARIANT 和 COM 接口各自有对应的释放方法；不能把它们都当普通 C++ 指针 delete。
 * 成功缓存约 2 秒，失败退避约 30 秒；这些等待和查询时间也影响实际刷新节奏。
 */
#include "core/collectors/NativeLenovoSensors.h"
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {
/**
 * @brief COM BSTR (Basic String) 的 RAII 安全封装器
 * 
 * 原理说明（新手必读）：
 * Windows COM 接口中的字符串标准类型是 BSTR，它在分配时会在字符串开头前置 4 字节的长度前缀。
 * 普通 delete 或 free 释放 BSTR 会破坏堆内存，必须调用 SysFreeString()。
 * 此结构体确保作用域结束时自动安全释放 BSTR，避免此包装对象所持字符串因遗漏释放而泄漏；不保证整个模块不存在其他泄漏。
 */
struct Bstr {
    BSTR value;
    explicit Bstr(const wchar_t* text) : value(SysAllocString(text)) {}
    ~Bstr() { SysFreeString(value); }
};
}

/**
 * @brief 构造函数：初始化多线程 COM 套间 (COINIT_MULTITHREADED)
 * 采集线程独占自己的 COM 套间，避免 UI 线程与采集线程产生上下文切换开销。
 */
NativeLenovoSensors::NativeLenovoSensors() {
    comStatus = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comOwned = SUCCEEDED(comStatus);
}

/**
 * @brief 析构函数：释放 WMI 服务代理接口并注销当前线程的 COM 套间
 */
NativeLenovoSensors::~NativeLenovoSensors() {
    services.Reset();
    if (comOwned) CoUninitialize();
}

/**
 * @brief 连接到联想笔记本特定的 WMI ACPI 硬件命名空间 (ROOT\\WMI)
 * 
 * 核心原理（新手必读）：
 * 1. 为什么不用通用驱动？
 *    很多笔记本品牌（如联想拯救者/小新系列）在主板 BIOS/EC (Embedded Controller) 中
 *    注册了 ACPI 硬件接口，并向 Windows WMI 暴露了 `ROOT\WMI` 命名空间下的 `LENOVO_GAMEZONE_DATA` 类。
 * 2. 安全性（只读模式）：
 *    软件只调用其提供的查询函数（GetCPUTemp、GetFanCount、GetFan1Speed 等），
 *    绝对不写入控制风扇调速的寄存器，不主动调整风扇、频率或温控策略；接口支持情况仍取决于机型和驱动。
 * 3. CoSetProxyBlanket：
 *    WMI 连接通常需要设置 RPC 安全覆盖层，赋予查询代理身份模拟级别 (RPC_C_IMP_LEVEL_IMPERSONATE)。
 */
HRESULT NativeLenovoSensors::Connect() {
    if (FAILED(comStatus)) return comStatus;

    Microsoft::WRL::ComPtr<IWbemLocator> locator;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator));
    if (FAILED(hr)) return hr;

    Bstr ns(L"ROOT\\WMI");
    hr = locator->ConnectServer(ns.value, nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, nullptr, &services);
    if (FAILED(hr)) return hr;

    // 配置 WMI 安全套接层级别
    hr = CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        services.Reset();
        return hr;
    }

    // 执行 WQL 查询定位联想硬件驱动实例对象
    Bstr language(L"WQL"), query(L"SELECT * FROM LENOVO_GAMEZONE_DATA");
    Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
    hr = services->ExecQuery(language.value, query.value, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    if (FAILED(hr)) {
        services.Reset();
        return hr;
    }

    Microsoft::WRL::ComPtr<IWbemClassObject> instance;
    ULONG count = 0;
    hr = enumerator->Next(1000, 1, &instance, &count);
    if (FAILED(hr) || !count) {
        services.Reset();
        return FAILED(hr) ? hr : WBEM_E_NOT_FOUND;
    }

    // 提取该硬件实例在 WMI 中的唯一绝对路径 (__PATH)，后续调用方法时使用
    VARIANT path;
    VariantInit(&path);
    hr = instance->Get(L"__PATH", 0, &path, nullptr, nullptr);
    if (SUCCEEDED(hr) && path.vt == VT_BSTR) {
        devicePath = path.bstrVal;
    } else {
        hr = WBEM_E_INVALID_OBJECT;
    }
    VariantClear(&path);
    if (FAILED(hr)) services.Reset();
    return hr;
}

/**
 * @brief 执行 WMI ACPI 硬件查询方法（例如 "GetCPUTemp" 或 "GetFan1Speed"）
 * 
 * 原理说明（新手必读）：
 * 1. ExecMethod：向设备路径同步触发 EC 传感器查询指令。
 * 2. 校验返回值与有效数据字段：
 *    - ReturnValue 校验方法执行是否成功。
 *    - Data 字段存储返回的 32 位无符号整数（如摄氏度数值或风扇 RPM 转速）。
 */
// method 是 OEM 方法名，value 是输出参数；方法返回值与 HRESULT 是两层状态。
// VARIANT 先初始化再清理，数据可能是字符串、整数等不同类型，读取前必须检查 vt。
HRESULT NativeLenovoSensors::Read(const wchar_t* method, uint32_t& value) {
    Bstr path(devicePath.c_str()), name(method);
    Microsoft::WRL::ComPtr<IWbemCallResult> call;
    HRESULT hr = services->ExecMethod(path.value, name.value, WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, nullptr, nullptr, &call);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWbemClassObject> result;
    hr = call->GetResultObject(1000, &result);
    if (hr != WBEM_S_NO_ERROR || !result) return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_TIMEOUT);

    VARIANT status;
    VariantInit(&status);
    const HRESULT statusHr = result->Get(L"ReturnValue", 0, &status, nullptr, nullptr);

    // CIM 规范与某些 WMI 桥接驱动可能省略 ReturnValue，做兼容性判断
    const bool success = (statusHr == WBEM_E_NOT_FOUND) ||
        (SUCCEEDED(statusHr) && ((status.vt == VT_BOOL && status.boolVal != VARIANT_FALSE) ||
        ((status.vt == VT_I4 || status.vt == VT_UI4) && status.ulVal == 1)));
    VariantClear(&status);
    if (!success) return E_FAIL;

    VARIANT data;
    VariantInit(&data);
    hr = result->Get(L"Data", 0, &data, nullptr, nullptr);
    if (SUCCEEDED(hr) && (data.vt == VT_UI4 || data.vt == VT_I4) && data.lVal >= 0) {
        value = data.ulVal;
    } else {
        hr = WBEM_E_TYPE_MISMATCH;
    }
    VariantClear(&data);
    return hr;
}

/**
 * @brief 周期性采样一次联想笔记本风扇与 CPU 核心温度
 * 
 * 异常降频保护机制（新手必读）：
 * 若当前机型并非联想笔记本或 WMI 连接失败，下次尝试间隔自动延长到 30 秒 (nextAttempt)，
 * 避免在台式机或其他品牌笔记本上每秒无意义重试占用系统 CPU 资源。
 */
// 在缓存有效期内直接返回 cached；过期后查询，失败释放连接并延迟下一次尝试。
NativeLenovoSensors::Reading NativeLenovoSensors::Sample() {
    const auto now = GetTickCount64();
    if (now < nextAttempt) return cached;

    Reading r;
    HRESULT hr = services ? S_OK : Connect();
    if (SUCCEEDED(hr)) {
        // 读取风扇总数与各自转速
        uint32_t count = 0;
        hr = Read(L"GetFanCount", count);
        if (SUCCEEDED(hr) && count > 0 && count <= 8) {
            r.fanCount = static_cast<int>(count);
            uint32_t rpm = 0;
            if (SUCCEEDED(Read(L"GetFan1Speed", rpm)) && rpm <= 20000) r.fan1 = static_cast<int>(rpm);
            if (count >= 2 && SUCCEEDED(Read(L"GetFan2Speed", rpm)) && rpm <= 20000) r.fan2 = static_cast<int>(rpm);
        }

        // 读取 CPU 芯片摄氏度
        uint32_t temperature = 0;
        if (SUCCEEDED(Read(L"GetCPUTemp", temperature)) && temperature > 0 && temperature <= 150) {
            r.cpuTemperature = static_cast<float>(temperature);
        }
    }

    r.status = hr;
    r.sampleTime = GetTickCount64();
    cached = r;
    // 成功则 2 秒后可再次刷新，失败则退避 30 秒
    nextAttempt = r.sampleTime + (FAILED(hr) ? 30000 : 2000);
    if (FAILED(hr)) {
        services.Reset();
        devicePath.clear();
    }
    return cached;
}
