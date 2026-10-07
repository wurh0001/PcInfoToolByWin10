#include "core/collectors/NativeAmdTemperature.h"
#include <intrin.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

NativeAmdTemperature::~NativeAmdTemperature() { Reset(); }
void NativeAmdTemperature::Reset() {
    if(pciMutex) CloseHandle(pciMutex);
    if(device && close) close(device);
    if(library) FreeLibrary(library);
    pciMutex=nullptr;device=nullptr;library=nullptr;execute=nullptr;close=nullptr;
}
HRESULT NativeAmdTemperature::Initialize() {
    int cpu[4]{};__cpuid(cpu,0);char vendor[13]{};
    std::memcpy(vendor,cpu+1,4);std::memcpy(vendor+4,cpu+3,4);std::memcpy(vendor+8,cpu+2,4);
    __cpuid(cpu,1);const unsigned signature=cpu[0],base=(signature>>8)&15;
    const unsigned family=base==15?base+((signature>>20)&255):base;
    const unsigned model=((signature>>4)&15)|((base==6||base==15)?((signature>>12)&240):0);
    if(std::strcmp(vendor,"AuthenticAMD")||family!=0x17||model!=0x60) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    wchar_t executable[32768]{};
    const DWORD length=GetModuleFileNameW(nullptr,executable,32768);
    if(!length||length>=32768) return E_FAIL;
    const auto modulePath=std::filesystem::path(executable).parent_path()/L"sensors"/L"AMDFamily17.bin";
    std::ifstream module(modulePath,std::ios::binary|std::ios::ate);
    if(!module||module.tellg()<=0||module.tellg()>1024*1024) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    std::vector<UCHAR> bytes(static_cast<size_t>(module.tellg()));module.seekg(0);
    if(!module.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) return E_FAIL;
    library=LoadLibraryExW(L"C:\\Program Files\\PawnIO\\PawnIOLib.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!library) return HRESULT_FROM_WIN32(GetLastError());
    using Open=HRESULT(WINAPI*)(PHANDLE);
    using Load=HRESULT(WINAPI*)(HANDLE,const UCHAR*,SIZE_T);
    const auto open=reinterpret_cast<Open>(GetProcAddress(library,"pawnio_open"));
    const auto load=reinterpret_cast<Load>(GetProcAddress(library,"pawnio_load"));
    execute=reinterpret_cast<Execute>(GetProcAddress(library,"pawnio_execute"));
    close=reinterpret_cast<Close>(GetProcAddress(library,"pawnio_close"));
    if(!open||!load||!execute||!close) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    HRESULT hr=open(&device);if(FAILED(hr)) return hr;
    hr=load(device,bytes.data(),bytes.size());if(FAILED(hr)) return hr;
    pciMutex=CreateMutexW(nullptr,FALSE,L"Global\\Access_PCI");
    return pciMutex?S_OK:HRESULT_FROM_WIN32(GetLastError());
}
NativeAmdTemperature::Reading NativeAmdTemperature::Sample() {
    Reading r;r.sampleTime=GetTickCount64();
    if(r.sampleTime<nextAttempt) { r.status=lastStatus;return r; }
    HRESULT hr=device?S_OK:Initialize();
    if(SUCCEEDED(hr)) {
        const DWORD wait=WaitForSingleObject(pciMutex,100);
        if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED) {
            // Only index selection, never thermal policy or arbitrary register writes.
            ULONG64 address=0x59800,raw=0;SIZE_T count=0;
            hr=execute(device,"ioctl_read_smn",&address,1,&raw,1,&count);
            ReleaseMutex(pciMutex);
            if(SUCCEEDED(hr)) {
                if(count!=1||!raw||raw>=0xffffffffull) hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                else {
                    float value=static_cast<float>(raw>>21)*0.125f;
                    if((raw&(1u<<19))||((raw&(3u<<16))==(3u<<16))) value-=49.f;
                    if(value>0&&value<=125) r.temperature=value;
                    else hr=HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }
            }
        } else hr=HRESULT_FROM_WIN32(wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError());
    }
    r.status=lastStatus=hr;r.sampleTime=GetTickCount64();
    if(FAILED(hr)) { Reset();nextAttempt=r.sampleTime+30000; }
    return r;
}
