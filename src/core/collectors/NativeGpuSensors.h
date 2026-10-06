#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

// Uses only the already-installed NVIDIA driver. Never installs or loads a helper driver.
class NativeGpuSensors {
    HMODULE library = nullptr;
    bool initialized = false;
    using Device = void*;
    template<class T> T Function(const char* name) const { return reinterpret_cast<T>(GetProcAddress(library,name)); }
public:
    struct Reading { float temperature=0, watts=0, ghz=0; uint64_t used=0,total=0; bool memoryValid=false; };
    NativeGpuSensors() {
        wchar_t system[MAX_PATH]{};
        if(!GetSystemDirectoryW(system,MAX_PATH))return;
        library=LoadLibraryExW((std::wstring(system)+L"\\nvml.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(library) {auto init=Function<int(__cdecl*)()>("nvmlInit_v2");initialized=init&&init()==0;}
    }
    ~NativeGpuSensors() {
        if(initialized){auto shutdown=Function<int(__cdecl*)()>("nvmlShutdown");if(shutdown)shutdown();}
        if(library)FreeLibrary(library);
    }
    Reading Sample(const std::wstring& adapterName) const {
        Reading r;if(!initialized)return r;
        auto count=Function<int(__cdecl*)(unsigned*)>("nvmlDeviceGetCount_v2");
        auto handle=Function<int(__cdecl*)(unsigned,Device*)>("nvmlDeviceGetHandleByIndex_v2");
        auto name=Function<int(__cdecl*)(Device,char*,unsigned)>("nvmlDeviceGetName");
        unsigned n=0;if(!count||!handle||!name||count(&n)!=0)return r;
        for(unsigned i=0;i<n;++i) {
            Device d=nullptr;char text[256]{};
            if(handle(i,&d)!=0||name(d,text,sizeof(text))!=0)continue;
            wchar_t wide[256]{};MultiByteToWideChar(CP_UTF8,0,text,-1,wide,256);
            if(adapterName!=wide)continue; // Do not attribute a discrete GPU's readings to an integrated GPU.
            unsigned value=0;
            auto temp=Function<int(__cdecl*)(Device,unsigned,unsigned*)>("nvmlDeviceGetTemperature");
            if(temp&&temp(d,0,&value)==0&&value<=200)r.temperature=static_cast<float>(value);
            auto power=Function<int(__cdecl*)(Device,unsigned*)>("nvmlDeviceGetPowerUsage");
            if(power&&power(d,&value)==0)r.watts=value/1000.f;
            auto clock=Function<int(__cdecl*)(Device,unsigned,unsigned*)>("nvmlDeviceGetClockInfo");
            if(clock&&clock(d,0,&value)==0)r.ghz=value/1000.f;
            struct Memory {unsigned long long total,free,used;} memory{};
            auto mem=Function<int(__cdecl*)(Device,Memory*)>("nvmlDeviceGetMemoryInfo");
            if(mem&&mem(d,&memory)==0&&memory.used<=memory.total){r.used=memory.used;r.total=memory.total;r.memoryValid=true;}
            break;
        }
        return r;
    }
};
