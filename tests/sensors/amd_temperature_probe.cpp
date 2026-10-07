// Independent diagnostic: no service installation, tuning, arbitrary addresses or production integration.
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

// Public Core Temp shared-memory ABI, 4-byte alignment. Diagnostic reference only.
// https://alcpu.com/CoreTemp/developers.html
#pragma pack(push,4)
struct CoreTempReference {
    uint32_t load[256],tjMax[128],cores,cpus;
    float temperature[256],vid,cpuSpeed,fsbSpeed,multiplier;
    char name[100];
    uint8_t fahrenheit,deltaToTjMax,tdpSupported,powerSupported;
    uint32_t version;
};
#pragma pack(pop)
static_assert(sizeof(CoreTempReference)==2692);

struct ReferenceReader {
    HANDLE mapping=nullptr;
    const void* view=nullptr;
    ~ReferenceReader() { if(view) UnmapViewOfFile(view);if(mapping) CloseHandle(mapping); }
    bool OpenMapping() {
        mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,L"CoreTempMappingObjectEx");
        if(!mapping) mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,L"Global\\CoreTempMappingObjectEx");
        if(mapping) view=MapViewOfFile(mapping,FILE_MAP_READ,0,0,sizeof(CoreTempReference));
        return view!=nullptr;
    }
    bool Read(float& first,float& highest) const {
        if(!view) return false;
        CoreTempReference data{};std::memcpy(&data,view,sizeof(data));
        if(data.cpus!=1||data.cores<1||data.cores>256||data.fahrenheit>1||data.deltaToTjMax>1) return false;
        highest=-std::numeric_limits<float>::infinity();
        for(unsigned i=0;i<data.cores;++i) {
            float value=data.temperature[i];
            if(data.fahrenheit) value=(value-32)*5/9;
            if(data.deltaToTjMax) value=data.tjMax[0]-value;
            if(!std::isfinite(value)||value<=0||value>125) return false;
            if(i==0) first=value;
            highest=std::max(highest,value);
        }
        return true;
    }
};

using Open = HRESULT (WINAPI*)(PHANDLE);
using Load = HRESULT (WINAPI*)(HANDLE,const UCHAR*,SIZE_T);
using Execute = HRESULT (WINAPI*)(HANDLE,PCSTR,const ULONG64*,SIZE_T,PULONG64,SIZE_T,PSIZE_T);
using Close = HRESULT (WINAPI*)(HANDLE);

static std::string Timestamp(bool utc) {
    SYSTEMTIME time{};
    if(utc) GetSystemTime(&time);else GetLocalTime(&time);
    char buffer[40]{};
    std::snprintf(buffer,sizeof(buffer),"%04u-%02u-%02uT%02u:%02u:%02u.%03u%s",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,utc?"Z":"");
    return buffer;
}

static bool ParseNumber(const wchar_t* value,int minimum,int maximum,int& result) {
    wchar_t* end=nullptr; const long number=std::wcstol(value,&end,10);
    if(end==value||*end||number<minimum||number>maximum) return false;
    result=static_cast<int>(number);return true;
}

int wmain(int argc,wchar_t** argv) {
    const bool compare=argc==4 && std::wcscmp(argv[3],L"--compare")==0;
    const bool csvMode=argc==6 && std::wcscmp(argv[3],L"--csv")==0;
    int samples=compare?20:5,interval=1000;
    if((argc!=3&&!compare&&!csvMode)||(csvMode&&(!ParseNumber(argv[4],1,14400,samples)||!ParseNumber(argv[5],250,10000,interval)))) {
        std::puts("Usage: probe <module> <report> [--compare | --csv samples interval_ms]"); return 2;
    }
    std::filesystem::path reportPath(argv[2]);
    if(csvMode) reportPath+=L".status.txt";
    std::ofstream report{reportPath};
    if(!report) return 2;
    std::ofstream csv;
    if(csvMode) {
        csv.open(std::filesystem::path(argv[2]));if(!csv) return 2;
        csv<<"sample,local_time,utc_time,elapsed_ms,read_duration_ms,hresult,raw_smn,returned_entries,valid,tctl_c,session_min_c,session_max_c\n";
        std::puts("Recording AMD Tctl. No per-core temperature or tuning requests.");
    }
    int cpu[4]{}; __cpuid(cpu,0);
    char vendor[13]{};
    std::memcpy(vendor,cpu+1,4); std::memcpy(vendor+4,cpu+3,4); std::memcpy(vendor+8,cpu+2,4);
    __cpuid(cpu,1);
    const unsigned signature=cpu[0],baseFamily=(signature>>8)&15;
    const unsigned family=baseFamily==15?baseFamily+((signature>>20)&255):baseFamily;
    const unsigned model=((signature>>4)&15)|((baseFamily==6||baseFamily==15)?((signature>>12)&240):0);
    report<<"Vendor="<<vendor<<" Family="<<family<<" Model="<<model<<"\n";
    if(std::strcmp(vendor,"AuthenticAMD")||family!=0x17||model!=0x60) { report<<"Unsupported CPU: diagnostic limited to Renoir\n"; return 3; }
    ReferenceReader reference;
    if(compare && !reference.OpenMapping()) { report<<"CoreTemp shared memory unavailable error="<<GetLastError()<<"\n";return 10; }
    std::ifstream module{std::filesystem::path(argv[1]),std::ios::binary|std::ios::ate};
    if(!module||module.tellg()<=0||module.tellg()>1024*1024) { report<<"Invalid module file\n";return 4; }
    std::vector<UCHAR> bytes(static_cast<size_t>(module.tellg())); module.seekg(0);
    if(!module.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) return 4;
    HMODULE lib=LoadLibraryExW(L"C:\\Program Files\\PawnIO\\PawnIOLib.dll",nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!lib) { report<<"DLL error="<<GetLastError()<<"\n";return 5; }
    auto open=reinterpret_cast<Open>(GetProcAddress(lib,"pawnio_open"));
    auto load=reinterpret_cast<Load>(GetProcAddress(lib,"pawnio_load"));
    auto execute=reinterpret_cast<Execute>(GetProcAddress(lib,"pawnio_execute"));
    auto close=reinterpret_cast<Close>(GetProcAddress(lib,"pawnio_close"));
    if(!open||!load||!execute||!close) { FreeLibrary(lib); return 5; }
    HANDLE handle=nullptr; HRESULT status=open(&handle);
    report<<"Open HRESULT=0x"<<std::hex<<static_cast<unsigned long>(status)<<std::dec<<"\n";
    if(FAILED(status)) { FreeLibrary(lib);return 6; }
    status=load(handle,bytes.data(),bytes.size());
    report<<"Load HRESULT=0x"<<std::hex<<static_cast<unsigned long>(status)<<std::dec<<"\n";
    if(FAILED(status)) { close(handle);FreeLibrary(lib);return 7; }
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Global\\Access_PCI");
    if(!mutex) { report<<"Mutex error="<<GetLastError()<<"\n";close(handle);FreeLibrary(lib);return 8; }
    unsigned valid=0,paired=0;
    double differenceSum=0,maxDifference=0;
    float lowest=125,highest=0,referenceLowest=125,referenceHighest=0;
    const ULONGLONG started=GetTickCount64();
    for(int i=0;i<samples;++i) {
        float before=0,beforeHighest=0;
        const bool beforeValid=compare && reference.Read(before,beforeHighest);
        DWORD wait=WaitForSingleObject(mutex,1000);
        if(wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED) { report<<"PCI lock unavailable="<<wait<<"\n";break; }
        // Index selection occurs inside the official module; no thermal-policy/MSR writes.
        ULONG64 address=0x59800,raw=0; SIZE_T count=0;
        const auto readStarted=GetTickCount64();
        status=execute(handle,"ioctl_read_smn",&address,1,&raw,1,&count);
        const auto readEnded=GetTickCount64();
        const auto localTime=Timestamp(false),utcTime=Timestamp(true);
        ReleaseMutex(mutex);
        float sampleTemperature=0;bool sampleValid=false;
        report<<"Sample="<<i<<" uptime_ms="<<GetTickCount64()<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(status)<<" raw=0x"<<raw<<std::dec<<" entries="<<count;
        if(SUCCEEDED(status)&&count==1&&raw!=0&&raw<0xFFFFFFFFull) {
            float temperature=static_cast<float>(raw>>21)*0.125f;
            if((raw&(1u<<19))||((raw&(3u<<16))==(3u<<16))) temperature-=49.0f;
            if(temperature>0 && temperature<=125) {
                report<<" Tctl_C="<<temperature;++valid;
                sampleTemperature=temperature;sampleValid=true;
                lowest=std::min(lowest,temperature);highest=std::max(highest,temperature);
                float after=0,afterHighest=0;
                if(beforeValid && reference.Read(after,afterHighest)) {
                    report<<" CoreTemp_first_before_C="<<before<<" CoreTemp_first_after_C="<<after<<" CoreTemp_hottest_C="<<afterHighest;
                    const double difference=std::abs(temperature-after);
                    differenceSum+=difference;maxDifference=std::max(maxDifference,difference);++paired;
                    referenceLowest=std::min(referenceLowest,after);referenceHighest=std::max(referenceHighest,after);
                } else if(compare) report<<" CoreTemp_invalid";
            }
            else report<<" invalid_temperature="<<temperature;
        }
        report<<"\n";report.flush();
        if(csvMode) {
            csv<<i<<','<<localTime<<','<<utcTime<<','<<(readEnded-started)<<','<<(readEnded-readStarted)<<",0x"<<std::hex<<static_cast<unsigned long>(status)<<",0x"<<raw<<std::dec<<','<<count<<','<<(sampleValid?1:0)<<',';
            if(sampleValid) csv<<sampleTemperature;
            csv<<',';if(valid) csv<<lowest;
            csv<<',';if(valid) csv<<highest;
            csv<<'\n';csv.flush();
            if(i%10==0||i==samples-1) std::printf("Sample %d/%d: %s %.3f C\n",i+1,samples,sampleValid?"Tctl":"INVALID",sampleTemperature);
        }
        if(FAILED(status)) break;
        if(i!=samples-1) {
            const ULONGLONG next=started+static_cast<ULONGLONG>(i+1)*interval,now=GetTickCount64();
            if(next>now) Sleep(static_cast<DWORD>(next-now));
        }
    }
    CloseHandle(mutex);close(handle);FreeLibrary(lib);
    report<<"ValidSamples="<<valid<<"\n";
    if(valid) report<<"ObservedTctlMin_C="<<lowest<<" ObservedTctlMax_C="<<highest<<"\n";
    if(compare) {
        report<<"PairedSamples="<<paired<<"\n";
        if(paired) report<<"CoreTempObservedFirstMin_C="<<referenceLowest<<" CoreTempObservedFirstMax_C="<<referenceHighest<<" MeanAbsoluteDifference_C="<<differenceSum/paired<<" MaxAbsoluteDifference_C="<<maxDifference<<"\n";
    }
    return valid==static_cast<unsigned>(samples)&&(!compare||paired==valid)?0:9;
}
