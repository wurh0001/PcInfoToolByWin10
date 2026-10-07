#pragma once
#include <windows.h>
#include <cstdint>

// Worker-owned adapter, restricted to the validated Renoir sensor and signed module.
class NativeAmdTemperature {
public:
    struct Reading { float temperature=0; HRESULT status=E_PENDING; uint64_t sampleTime=0; };
    ~NativeAmdTemperature();
    Reading Sample();
    NativeAmdTemperature()=default;
    NativeAmdTemperature(const NativeAmdTemperature&)=delete;
    NativeAmdTemperature& operator=(const NativeAmdTemperature&)=delete;
private:
    using Execute=HRESULT(WINAPI*)(HANDLE,PCSTR,const ULONG64*,SIZE_T,PULONG64,SIZE_T,PSIZE_T);
    using Close=HRESULT(WINAPI*)(HANDLE);
    HRESULT Initialize();
    void Reset();
    HMODULE library=nullptr;
    HANDLE device=nullptr, pciMutex=nullptr;
    Execute execute=nullptr;
    Close close=nullptr;
    uint64_t nextAttempt=0;
    HRESULT lastStatus=E_PENDING;
};
