/*
 * 模块说明：PawnIO 主温度读取流程
 * 检查 CPU 型号 → 读取 exe 旁 sensors 模块 → 动态绑定 PawnIO → 固定地址读取。
 * 只读 SMN 0x59800，使用 Global\Access_PCI 协调访问；失败清理并等待 30 秒重试。
 * 需要管理员权限；返回失败状态由上层决定回退或显示无数据，不伪造正常温度。
 */
#include "core/collectors/NativeAmdTemperature.h"
#include <intrin.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

NativeAmdTemperature::~NativeAmdTemperature() { Reset(); }
// 失败和析构共用的清理路径：先关同步句柄和设备，最后卸载提供函数指针的 DLL。
// 清空指针后可重新 Initialize；Reset 本身不把失败状态改成成功。
void NativeAmdTemperature::Reset() {
    if(pciMutex) CloseHandle(pciMutex);
    if(device && close) close(device);
    if(library) FreeLibrary(library);
    pciMutex=nullptr;device=nullptr;library=nullptr;execute=nullptr;close=nullptr;
}
/**
 * 验证 CPU 与模块，加载 PawnIO 导出函数并打开设备；HRESULT 通知失败原因。
 */
HRESULT NativeAmdTemperature::Initialize() {
    // CPUID 的结果分别放入四个寄存器槽位；vendor 按 EBX、EDX、ECX 拼接。
    // 花括号初始化让数组先归零，最后一个字节保持字符串终止符。
    int cpu[4]{};__cpuid(cpu,0);char vendor[13]{};
    std::memcpy(vendor,cpu+1,4);std::memcpy(vendor+4,cpu+3,4);std::memcpy(vendor+8,cpu+2,4);
    __cpuid(cpu,1);const unsigned signature=cpu[0],base=(signature>>8)&15;
    // 从签名提取基础/扩展家族与型号；只允许本机交叉验证过的 Renoir 型号。
    const unsigned family=base==15?base+((signature>>20)&255):base;
    const unsigned model=((signature>>4)&15)|((base==6||base==15)?((signature>>12)&240):0);
    if(std::strcmp(vendor,"AuthenticAMD")||family!=0x17||model!=0x60) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    wchar_t executable[32768]{};
    const DWORD length=GetModuleFileNameW(nullptr,executable,32768);
    if(!length||length>=32768) return E_FAIL;
    // 模块跟随 exe 分发；构建过程校验 SHA256，此处只负责读取文件与大小检查。
    const auto modulePath=std::filesystem::path(executable).parent_path()/L"sensors"/L"AMDFamily17.bin";
    std::ifstream module(modulePath,std::ios::binary|std::ios::ate);
    if(!module||module.tellg()<=0||module.tellg()>1024*1024) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    std::vector<UCHAR> bytes(static_cast<size_t>(module.tellg()));module.seekg(0);
    if(!module.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) return E_FAIL;
    library=LoadLibraryExW(L"C:\\Program Files\\PawnIO\\PawnIOLib.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library) return HRESULT_FROM_WIN32(GetLastError());
    // using 定义 DLL 导出函数的类型，包含调用约定；GetProcAddress 返回地址后转成此类型。
    // 这里绑定函数而非静态链接，所以 DLL 未安装时可以返回明确错误。
    using Open=HRESULT(WINAPI*)(PHANDLE);
    using Load=HRESULT(WINAPI*)(HANDLE,const UCHAR*,SIZE_T);
    const auto open=reinterpret_cast<Open>(GetProcAddress(library,"pawnio_open"));
    const auto load=reinterpret_cast<Load>(GetProcAddress(library,"pawnio_load"));
    execute=reinterpret_cast<Execute>(GetProcAddress(library,"pawnio_execute"));
    close=reinterpret_cast<Close>(GetProcAddress(library,"pawnio_close"));
    if(!open||!load||!execute||!close) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    // HRESULT 表示 Windows 成功/失败状态；FAILED 检查失败位，不能只比较是否等于 0。
    HRESULT hr=open(&device);if(FAILED(hr)) return hr;
    hr=load(device,bytes.data(),bytes.size());if(FAILED(hr)) return hr;
    pciMutex=CreateMutexW(nullptr,FALSE,L"Global\\Access_PCI");
    return pciMutex?S_OK:HRESULT_FROM_WIN32(GetLastError());
}
// 一次只读采样：退避期间直接返回上次错误，不重复加载驱动或查询硬件。
// Reading 包含数值、状态和采样时间；数值默认 0 不能单独视为有效温度。
NativeAmdTemperature::Reading NativeAmdTemperature::Sample() {
    Reading r;r.sampleTime=GetTickCount64();
    if(r.sampleTime<nextAttempt) { r.status=lastStatus;return r; }
    HRESULT hr=device?S_OK:Initialize();
    if(SUCCEEDED(hr)) {
        // 命名互斥锁协调同一 PCI 访问路径，最多等 100ms；成功获得所有权后必须释放。
        const DWORD wait=WaitForSingleObject(pciMutex,100);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED) {
            // 只选择固定 SMN 地址进行读取，不写温控策略，也不提供任意寄存器修改入口。
            ULONG64 address=0x59800,raw=0;SIZE_T count=0;
            hr=execute(device,"ioctl_read_smn",&address,1,&raw,1,&count);
            ReleaseMutex(pciMutex);
            if(SUCCEEDED(hr)) {
                if(count!=1||!raw||raw>=0xffffffffull) hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                else {
                    // 位移提取温度编码，步进是 0.125°C；特定范围标志需按寄存器编码减 49°C。
                    // 这是编码解码规则，与对比软件的 0.375°C 采样差异无关，不做人为校准。
                    float value=static_cast<float>(raw>>21)*0.125f;
                    if((raw&(1u<<19))||((raw&(3u<<16))==(3u<<16))) value-=49.f;
                    if(value>0&&value<=125) r.temperature=value;
                    else hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }
            }
        } else hr=HRESULT_FROM_WIN32(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError());
    }
    r.status=lastStatus=hr;r.sampleTime=GetTickCount64();
    // 权限、数据或设备失败都清理已有资源，30 秒后再尝试，避免每秒反复失败。
    if(FAILED(hr)) { Reset();nextAttempt=r.sampleTime+30000; }
    return r;
}
