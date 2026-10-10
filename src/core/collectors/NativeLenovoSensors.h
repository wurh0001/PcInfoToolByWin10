/*
 * 模块说明：联想 OEM 传感器接口
 * 封装联想 WMI 查询的状态、缓存和重试时间；实例由采集线程创建并使用。
 * 风扇和温度取决于具体 OEM 接口支持情况，不能据此推断所有笔记本均支持。
 */
#pragma once

#include <windows.h>
#include <wbemidl.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

/**
 * @brief 联想 (Lenovo) 笔记本/台式机原生 OEM 硬件传感器适配器 (NativeLenovoSensors)
 * 
 * 核心设计原则（新手必读）：
 * 1. 纯只读 (Read-Only) 查询：
 *    通过 Windows WMI (Windows Management Instrumentation) 访问主板 ACPI 固件接口。
 *    绝对不写入、不修改任何风扇转速策略或温控策略，确保硬件绝对安全！
 * 2. 线程隔离：
 *    仅在后台 Worker 采集线程中调用，防止 WMI 同步 RPC 查询阻塞前端 UI 渲染。
 * 3. 容错与非侵入：
 *    非联想机型或主板不支持时静默返回，不影响其他常规指标展示。
 */
class NativeLenovoSensors {
public:
    /// @brief 联想硬件传感器采样读数结构
    struct Reading {
        float cpuTemperature = 0;   ///< CPU 核心/封装温度 (摄氏度 °C)
        int fanCount = 0;           ///< 探测到的有效风扇数量 (通常为 1 或 2 个)
        int fan1 = -1;              ///< 1 号风扇转速 (RPM，-1 代表不可用)
        int fan2 = -1;              ///< 2 号风扇转速 (RPM，-1 代表不可用)
        HRESULT status = E_PENDING; ///< WMI 查询状态码
        uint64_t sampleTime = 0;    ///< 采样完成时间戳 (毫秒)
    };

    NativeLenovoSensors();
    ~NativeLenovoSensors();

    // 禁止拷贝构造和赋值（包含底层 COM 句柄和专有连接上下文）
    NativeLenovoSensors(const NativeLenovoSensors&) = delete;
    NativeLenovoSensors& operator=(const NativeLenovoSensors&) = delete;

    /**
     * @brief 执行一次联想 OEM 传感器采样
     * @return Reading 最新的温度与风扇转速读数
     */
    Reading Sample();

private:
    /**
     * @brief 初始化并连接到 root\\wmi 命名空间下的 ACPI 驱动节点
     */
    HRESULT Connect();

    /**
     * @brief 调用指定 ACPI WMI 方法读取硬件无符号整型数值
     */
    HRESULT Read(const wchar_t* method, uint32_t& value);

    bool comOwned = false;          ///< 当前线程的 COM 套间是否由本类成功初始化
    HRESULT comStatus = E_FAIL;     ///< COM 运行状态
    uint64_t nextAttempt = 0;       ///< 若连接失败，记录下一次允许重试的时间戳（退避重试，防止死循环轰炸 WMI）
    Reading cached;                 ///< 上一次缓存的读数

    Microsoft::WRL::ComPtr<IWbemServices> services; ///< WMI 服务接口智能指针
    std::wstring devicePath;                        ///< 目标 ACPI 设备实例对象路径
};
