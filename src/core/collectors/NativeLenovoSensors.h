#pragma once
#include <windows.h>
#include <wbemidl.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>

// Worker-thread-only, read-only OEM adapter. Never changes fan or thermal policy.
class NativeLenovoSensors {
public:
    struct Reading { float cpuTemperature=0; int fanCount=0,fan1=-1,fan2=-1; HRESULT status=E_PENDING; uint64_t sampleTime=0; };
    NativeLenovoSensors();
    ~NativeLenovoSensors();
    NativeLenovoSensors(const NativeLenovoSensors&)=delete;
    NativeLenovoSensors& operator=(const NativeLenovoSensors&)=delete;
    Reading Sample();
private:
    HRESULT Connect();
    HRESULT Read(const wchar_t* method,uint32_t& value);
    bool comOwned=false;
    HRESULT comStatus=E_FAIL;
    uint64_t nextAttempt=0;
    Reading cached;
    Microsoft::WRL::ComPtr<IWbemServices> services;
    std::wstring devicePath;
};
