#include "core/collectors/PhysicalDiskSampler.h"
#include <setupapi.h>
#include <algorithm>
#include <limits>
#include <set>
#pragma comment(lib,"setupapi.lib")

namespace {
const GUID DiskInterface={0x53f56307,0xb6bf,0x11d0,{0x94,0xf2,0x00,0xa0,0xc9,0x1e,0xfb,0x8b}};
struct Handle { HANDLE value=INVALID_HANDLE_VALUE; ~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);} };
Handle Open(const std::wstring& path,DWORD access=0) {return {CreateFileW(path.c_str(),access,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr)};}
std::wstring DescriptorString(const std::vector<unsigned char>& bytes,DWORD offset) {
    if(!offset || offset>=bytes.size())return {};
    const auto* s=reinterpret_cast<const char*>(bytes.data()+offset);
    size_t length=0;while(offset+length<bytes.size()&&s[length])++length;
    if(offset+length==bytes.size())return {};
    const int count=MultiByteToWideChar(CP_ACP,0,s,static_cast<int>(length),nullptr,0);
    std::wstring result(count,L' ');MultiByteToWideChar(CP_ACP,0,s,static_cast<int>(length),result.data(),count);
    while(!result.empty()&&result.back()==L' ')result.pop_back();
    const auto first=result.find_first_not_of(L' ');return first==std::wstring::npos ? L"" : result.substr(first);
}
}
void PhysicalDiskSampler::RefreshInventory() {
    std::vector<Device> found;
    const HDEVINFO set=SetupDiGetClassDevsW(&DiskInterface,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(set==INVALID_HANDLE_VALUE)return;
    for(DWORD i=0;;++i) {
        SP_DEVICE_INTERFACE_DATA iface{};iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(set,nullptr,&DiskInterface,i,&iface))break;
        DWORD size=0;SetupDiGetDeviceInterfaceDetailW(set,&iface,nullptr,0,&size,nullptr);
        if(size<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)||size>65536)continue;
        std::vector<unsigned char> buffer(size);
        auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());detail->cbSize=sizeof(*detail);
        SP_DEVINFO_DATA dev{};dev.cbSize=sizeof(dev);
        if(!SetupDiGetDeviceInterfaceDetailW(set,&iface,detail,size,nullptr,&dev))continue;
        wchar_t id[4096];if(!SetupDiGetDeviceInstanceIdW(set,&dev,id,4096,nullptr))continue;
        Device device;device.path=detail->DevicePath;device.info.id=id;
        auto handle=Open(device.path);DWORD returned=0;STORAGE_DEVICE_NUMBER number{};
        if(handle.value==INVALID_HANDLE_VALUE || !DeviceIoControl(handle.value,IOCTL_STORAGE_GET_DEVICE_NUMBER,nullptr,0,&number,sizeof(number),&returned,nullptr)) {
            // Present in SetupDi inventory but temporarily unreadable is not a disconnect.
            const auto previous=std::find_if(devices.begin(),devices.end(),[&](const auto& d){return d.info.id==device.info.id;});
            if(previous!=devices.end()){device.info=previous->info;found.push_back(std::move(device));}
            continue;
        }
        device.info.number=number.DeviceNumber;
        STORAGE_PROPERTY_QUERY query{};query.PropertyId=StorageDeviceProperty;query.QueryType=PropertyStandardQuery;
        STORAGE_DESCRIPTOR_HEADER header{};
        if(DeviceIoControl(handle.value,IOCTL_STORAGE_QUERY_PROPERTY,&query,sizeof(query),&header,sizeof(header),&returned,nullptr)&&header.Size>=sizeof(STORAGE_DEVICE_DESCRIPTOR)&&header.Size<=65536) {
            std::vector<unsigned char> bytes(header.Size);
            if(DeviceIoControl(handle.value,IOCTL_STORAGE_QUERY_PROPERTY,&query,sizeof(query),bytes.data(),static_cast<DWORD>(bytes.size()),&returned,nullptr)&&returned>=sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
                bytes.resize(returned);const auto* descriptor=reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(bytes.data());
                device.info.model=DescriptorString(bytes,descriptor->ProductIdOffset);
                switch(descriptor->BusType) {case BusTypeNvme:device.info.busType=L"NVMe";break;case BusTypeSata:device.info.busType=L"SATA";break;case BusTypeUsb:device.info.busType=L"USB";break;case BusTypeScsi:device.info.busType=L"SCSI";break;case BusTypeRAID:device.info.busType=L"RAID";break;default:device.info.busType=L"接口未知";}
            }
        }
        if(device.info.model.empty())device.info.model=L"型号未提供";
        GET_LENGTH_INFORMATION length{};
        // GET_LENGTH_INFO requires FILE_READ_ACCESS. The metadata handle above
        // intentionally has access=0 and cannot satisfy this IOCTL, even elevated.
        auto capacityHandle=Open(device.path,GENERIC_READ);
        if(capacityHandle.value!=INVALID_HANDLE_VALUE&&DeviceIoControl(capacityHandle.value,IOCTL_DISK_GET_LENGTH_INFO,nullptr,0,&length,sizeof(length),&returned,nullptr)&&returned>=sizeof(length)&&length.Length.QuadPart>0)
            device.info.capacityBytes=length.Length.QuadPart;
        if(!device.info.capacityBytes) {
            // Geometry can expose the physical size without a read-access handle.
            std::vector<unsigned char> geometryBytes(4096);
            if(DeviceIoControl(handle.value,IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,nullptr,0,geometryBytes.data(),static_cast<DWORD>(geometryBytes.size()),&returned,nullptr)&&returned>=FIELD_OFFSET(DISK_GEOMETRY_EX,Data)) {
                const auto* geometry=reinterpret_cast<const DISK_GEOMETRY_EX*>(geometryBytes.data());
                if(geometry->DiskSize.QuadPart>0) device.info.capacityBytes=geometry->DiskSize.QuadPart;
            }
        }
        found.push_back(std::move(device));
    }
    SetupDiDestroyDeviceInfoList(set);
    std::sort(found.begin(),found.end(),[](const auto& a,const auto& b){return a.info.number<b.info.number;});
    devices=std::move(found);
    for(auto it=baselines.begin();it!=baselines.end();) {
        if(std::none_of(devices.begin(),devices.end(),[&](const auto& d){return d.info.id==it->first;}))it=baselines.erase(it);else ++it;
    }
}
std::vector<PhysicalDiskInfo> PhysicalDiskSampler::Sample(const std::vector<DiskInfo>& volumes) {
    const auto now=GetTickCount64();
    if(!inventoryTicks || now-inventoryTicks>=10000) {RefreshInventory();inventoryTicks=now;}
    std::vector<PhysicalDiskInfo> result;for(const auto& device:devices)result.push_back(device.info);
    // A volume can span devices; map every extent without guessing from drive letters.
    for(const auto& volume:volumes) {
        if(volume.drive.size()<2)continue;
        auto handle=Open(L"\\\\.\\"+volume.drive.substr(0,2));if(handle.value==INVALID_HANDLE_VALUE)continue;
        std::vector<unsigned char> bytes(65536);DWORD returned=0;
        if(!DeviceIoControl(handle.value,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,nullptr,0,bytes.data(),static_cast<DWORD>(bytes.size()),&returned,nullptr)||returned<sizeof(VOLUME_DISK_EXTENTS))continue;
        const auto* extents=reinterpret_cast<const VOLUME_DISK_EXTENTS*>(bytes.data());
        const size_t max=(returned-FIELD_OFFSET(VOLUME_DISK_EXTENTS,Extents))/sizeof(DISK_EXTENT);
        if(extents->NumberOfDiskExtents>max)continue;
        std::set<DWORD> mapped;for(DWORD i=0;i<extents->NumberOfDiskExtents;++i)mapped.insert(extents->Extents[i].DiskNumber);
        for(auto& disk:result)if(mapped.count(disk.number)) {auto v=volume;if(mapped.size()>1)v.label+=L"（跨盘卷）";disk.volumes.push_back(std::move(v));}
    }
    const float missing=std::numeric_limits<float>::quiet_NaN();
    for(size_t i=0;i<devices.size();++i) {
        auto& disk=result[i];auto& baseline=baselines[disk.id];auto handle=Open(devices[i].path);
        DISK_PERFORMANCE counters{};DWORD returned=0;
        if(handle.value!=INVALID_HANDLE_VALUE) {
            STORAGE_PROPERTY_QUERY query{};query.PropertyId=StorageDeviceTemperatureProperty;query.QueryType=PropertyStandardQuery;
            std::vector<unsigned char> bytes(4096);
            if(DeviceIoControl(handle.value,IOCTL_STORAGE_QUERY_PROPERTY,&query,sizeof(query),bytes.data(),static_cast<DWORD>(bytes.size()),&returned,nullptr)
                && returned>=FIELD_OFFSET(STORAGE_TEMPERATURE_DATA_DESCRIPTOR,TemperatureInfo)+sizeof(STORAGE_TEMPERATURE_INFO)) {
                const auto* temperatures=reinterpret_cast<const STORAGE_TEMPERATURE_DATA_DESCRIPTOR*>(bytes.data());
                if(temperatures->InfoCount>0&&temperatures->TemperatureInfo[0].Temperature>=-20&&temperatures->TemperatureInfo[0].Temperature<=150) {
                    disk.temperatureC=static_cast<float>(temperatures->TemperatureInfo[0].Temperature);disk.temperatureValid=true;
                }
            }
        }
        const bool measured=handle.value!=INVALID_HANDLE_VALUE && DeviceIoControl(handle.value,IOCTL_DISK_PERFORMANCE,nullptr,0,&counters,sizeof(counters),&returned,nullptr) && returned>=sizeof(counters);
        const double seconds=(now-baseline.ticks)/1000.0;
        if(measured&&baseline.valid&&seconds>0&&counters.BytesRead.QuadPart>=baseline.counters.BytesRead.QuadPart&&counters.BytesWritten.QuadPart>=baseline.counters.BytesWritten.QuadPart&&counters.ReadTime.QuadPart>=baseline.counters.ReadTime.QuadPart&&counters.WriteTime.QuadPart>=baseline.counters.WriteTime.QuadPart&&counters.IdleTime.QuadPart>=baseline.counters.IdleTime.QuadPart) {
            disk.readBps=static_cast<uint64_t>((counters.BytesRead.QuadPart-baseline.counters.BytesRead.QuadPart)/seconds);
            disk.writeBps=static_cast<uint64_t>((counters.BytesWritten.QuadPart-baseline.counters.BytesWritten.QuadPart)/seconds);
            disk.throughputValid=true;
            disk.activePercent=static_cast<float>(std::clamp(100*(1-(counters.IdleTime.QuadPart-baseline.counters.IdleTime.QuadPart)/(seconds*10000000)),0.0,100.0));
            const auto reads=static_cast<DWORD>(counters.ReadCount-baseline.counters.ReadCount),writes=static_cast<DWORD>(counters.WriteCount-baseline.counters.WriteCount);
            const uint64_t operations=static_cast<uint64_t>(reads)+writes;
            disk.responseTimeMs=operations ? static_cast<float>((counters.ReadTime.QuadPart-baseline.counters.ReadTime.QuadPart+counters.WriteTime.QuadPart-baseline.counters.WriteTime.QuadPart)/10000.0/operations) : 0;
            disk.statisticsValid=true;
        } else disk.status=measured ? L"正在收集首批统计" : L"统计未提供（权限或设备不支持）";
        baseline.reads.Push(disk.throughputValid ? static_cast<float>(disk.readBps/1000000.0) : missing);
        baseline.writes.Push(disk.throughputValid ? static_cast<float>(disk.writeBps/1000000.0) : missing);
        disk.readHistory=baseline.reads;disk.writeHistory=baseline.writes;
        baseline.counters=counters;baseline.ticks=now;baseline.valid=measured;
    }
    return result;
}
