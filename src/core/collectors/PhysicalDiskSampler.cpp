/*
 * 模块说明：磁盘枚举、容量和性能查询
 * SetupAPI 枚举设备；DeviceIoControl 查询型号、真实物理容量、性能计数与温度。
 * 容量优先用 GENERIC_READ 句柄查询精确长度，再尝试几何信息，不能累加卷容量代替。
 * 累计读写字节/操作次数除以采样时间差得到吞吐/IOPS；首轮只能保存计算基准。
 */
#include "core/collectors/PhysicalDiskSampler.h"
#include <setupapi.h>
#include <algorithm>
#include <limits>
#include <set>
#pragma comment(lib,"setupapi.lib")

namespace {
/**
 * @brief Windows 磁盘设备接口 GUID (GUID_DEVINTERFACE_DISK)
 * 用于通过 Windows SetupAPI 枚举系统当前插入的所有物理磁盘设备接口
 */
const GUID DiskInterface = { 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } };

/**
 * @brief RAII 风格的安全句柄包装器
 * 确保在函数返回或发生异常离开作用域时，自动调用 CloseHandle 关闭 Windows 内核句柄，防止资源泄露
 */
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
    }
};

/**
 * @brief 打开设备对象获取内核句柄
 * 
 * 关键技巧（新手必读）：
 * 默认 access = 0（只查询设备元数据，不需要读写扇区数据）。
 * 这样即使用户在普通非管理员权限下运行，也能成功向驱动发送 IOCTL 查询设备型号与性能计数器！
 */
Handle Open(const std::wstring& path, DWORD access = 0) {
    return { CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr) };
}

/**
 * @brief 从 STORAGE_DEVICE_DESCRIPTOR 原始字节缓冲区中提取 ASCII 描述字符串并转为宽字符 (wstring)
 */
std::wstring DescriptorString(const std::vector<unsigned char>& bytes, DWORD offset) {
    if (!offset || offset >= bytes.size()) return {};
    const auto* s = reinterpret_cast<const char*>(bytes.data() + offset);
    size_t length = 0;
    while (offset + length < bytes.size() && s[length]) ++length;
    if (offset + length == bytes.size()) return {};
    const int count = MultiByteToWideChar(CP_ACP, 0, s, static_cast<int>(length), nullptr, 0);
    std::wstring result(count, L' ');
    MultiByteToWideChar(CP_ACP, 0, s, static_cast<int>(length), result.data(), count);
    while (!result.empty() && result.back() == L' ') result.pop_back(); // 去除末尾填充的空格
    const auto first = result.find_first_not_of(L' ');
    return first == std::wstring::npos ? L"" : result.substr(first);
}
}

/**
 * @brief 刷新物理磁盘清单库存 (RefreshInventory)
 * 
 * 核心原理（新手必读）：
 * 1. 为什么不用盘符访问？
 *    Windows 上的盘符（如 C:、D:）是“卷 (Volume)”，而一块真实的物理硬盘可以被分为多个分区，
 *    甚至多个物理硬盘可以组成一个存储池卷。为了准确监控硬件本身的吞吐、接口类型和温度，
 *    必须使用 SetupAPI 直接探测物理驱动器。
 * 2. SetupDiGetClassDevsW：
 *    向 Windows 即插即用管理器请求当前系统中所有挂载的磁盘设备接口句柄列表。
 * 3. IOCTL_STORAGE_GET_DEVICE_NUMBER：
 *    获取设备的物理驱动器编号（DeviceNumber，例如 0 对应 PhysicalDrive0）。
 * 4. IOCTL_STORAGE_QUERY_PROPERTY：
 *    查询驱动器硬件描述符，获取厂商型号（Model）以及总线类型（NVMe、SATA、USB 等）。
 * 5. 容量查询的双重保底：
 *    - 优先尝试 IOCTL_DISK_GET_LENGTH_INFO（需要 GENERIC_READ 权限）。
 *    - 若普通用户权限不足，则回退使用 IOCTL_DISK_GET_DRIVE_GEOMETRY_EX（无需数据读取权限即可拿到扇区几何尺寸）。
 */
void PhysicalDiskSampler::RefreshInventory() {
    std::vector<Device> found;
    const HDEVINFO set = SetupDiGetClassDevsW(&DiskInterface, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return;

    for (DWORD i = 0;; ++i) {
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(set, nullptr, &DiskInterface, i, &iface)) break;

        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &size, nullptr);
        if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 65536) continue;

        std::vector<unsigned char> buffer(size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA dev{};
        dev.cbSize = sizeof(dev);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, size, nullptr, &dev)) continue;

        wchar_t id[4096];
        if (!SetupDiGetDeviceInstanceIdW(set, &dev, id, 4096, nullptr)) continue;

        Device device;
        device.path = detail->DevicePath;
        device.info.id = id;

        auto handle = Open(device.path);
        DWORD returned = 0;
        STORAGE_DEVICE_NUMBER number{};
        if (handle.value == INVALID_HANDLE_VALUE || !DeviceIoControl(handle.value, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &number, sizeof(number), &returned, nullptr)) {
            // 设备在清单中但暂时不可读时，保留上一轮的历史记录，避免被误判为拔出断开
            const auto previous = std::find_if(devices.begin(), devices.end(), [&](const auto& d) {
                return d.info.id == device.info.id;
            });
            if (previous != devices.end()) {
                device.info = previous->info;
                found.push_back(std::move(device));
            }
            continue;
        }

        device.info.number = number.DeviceNumber;
        STORAGE_PROPERTY_QUERY query{};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        STORAGE_DESCRIPTOR_HEADER header{};

        // 读取磁盘硬件描述信息（型号、总线接口类型）
        if (DeviceIoControl(handle.value, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &header, sizeof(header), &returned, nullptr) &&
            header.Size >= sizeof(STORAGE_DEVICE_DESCRIPTOR) && header.Size <= 65536) {
            std::vector<unsigned char> bytes(header.Size);
            if (DeviceIoControl(handle.value, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), bytes.data(), static_cast<DWORD>(bytes.size()), &returned, nullptr) &&
                returned >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
                bytes.resize(returned);
                const auto* descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(bytes.data());
                device.info.model = DescriptorString(bytes, descriptor->ProductIdOffset);
                switch (descriptor->BusType) {
                    case BusTypeNvme: device.info.busType = L"NVMe"; break;
                    case BusTypeSata: device.info.busType = L"SATA"; break;
                    case BusTypeUsb:  device.info.busType = L"USB"; break;
                    case BusTypeScsi: device.info.busType = L"SCSI"; break;
                    case BusTypeRAID: device.info.busType = L"RAID"; break;
                    default:          device.info.busType = L"接口未知"; break;
                }
            }
        }

        if (device.info.model.empty()) device.info.model = L"型号未提供";

        // 查询物理磁盘容量（优先通过带 GENERIC_READ 句柄查精确长度）
        GET_LENGTH_INFORMATION length{};
        auto capacityHandle = Open(device.path, GENERIC_READ);
        if (capacityHandle.value != INVALID_HANDLE_VALUE &&
            DeviceIoControl(capacityHandle.value, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length), &returned, nullptr) &&
            returned >= sizeof(length) && length.Length.QuadPart > 0) {
            device.info.capacityBytes = length.Length.QuadPart;
        }

        // 降级方案：若权限受限无法以 GENERIC_READ 打开，使用只读元数据句柄查询磁盘几何结构
        if (!device.info.capacityBytes) {
            std::vector<unsigned char> geometryBytes(4096);
            if (DeviceIoControl(handle.value, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, geometryBytes.data(), static_cast<DWORD>(geometryBytes.size()), &returned, nullptr) &&
                returned >= FIELD_OFFSET(DISK_GEOMETRY_EX, Data)) {
                const auto* geometry = reinterpret_cast<const DISK_GEOMETRY_EX*>(geometryBytes.data());
                if (geometry->DiskSize.QuadPart > 0) device.info.capacityBytes = geometry->DiskSize.QuadPart;
            }
        }
        found.push_back(std::move(device));
    }

    SetupDiDestroyDeviceInfoList(set);
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        return a.info.number < b.info.number;
    });
    devices = std::move(found);

    // 清理已拔出移除设备的基准历史缓存
    for (auto it = baselines.begin(); it != baselines.end();) {
        if (std::none_of(devices.begin(), devices.end(), [&](const auto& d) { return d.info.id == it->first; })) {
            it = baselines.erase(it);
        } else {
            ++it;
        }
    }
}
/**
 * @brief 采样所有物理磁盘的实时性能指标（读写速率、活动时间、响应时间、温度）并关联卷分区
 * 
 * 核心原理（新手必读）：
 * 1. 缓存刷新节流：每 10 秒刷新一次物理硬件库存 (RefreshInventory)，避免每秒频繁扫描 PnP 接口造成性能浪费。
 * 2. 卷与物理磁盘映射（IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS）：
 *    - 用户看到的盘符是 "C:"、"D:"。打开 "\\\\.\\C:" 并发送该 IOCTL，驱动会精确返回这个逻辑盘符
 *      位于哪一个物理磁盘编号上（DiskNumber = 0 即 PhysicalDrive0）。
 *    - 如果一个卷跨越了多个物理磁盘（跨盘卷/RAID），代码会自动追加 "（跨盘卷）" 提示。
 * 3. NVMe / SATA 固态硬盘温度读取（StorageDeviceTemperatureProperty）：
 *    - 现代 Windows 10/11 的标准存储驱动 (stornvme.sys) 提供了标准的温度查询属性。
 *    - 只要向驱动发送 IOCTL_STORAGE_QUERY_PROPERTY，就能直接读出芯片当前的摄氏度数，完全不需要第三方闭源驱动！
 * 4. 读写速率与响应时间（IOCTL_DISK_PERFORMANCE）：
 *    - 驱动维护了从开机以来的累计读取字节数 (BytesRead)、写入字节数 (BytesWritten)、读写耗时与空闲时间 (IdleTime)。
 *    - 速率 (Bps) = (本次字节 - 上次字节) / 时间差 (seconds)。
 *    - 磁盘活动时间百分比 (Active Time %) = 100% - (空闲时间差 / 统计流逝时间)。
 *    - 平均响应时间 (ms) = (总耗时差 / 10000) / 读写总次数。
 */
// volumes 是逻辑卷清单；先维护设备清单和卷归属，再从累计计数差计算实时读写指标。
// 返回列表副本供快照持有，内部 Device 保存下一轮计算所需基准。
std::vector<PhysicalDiskInfo> PhysicalDiskSampler::Sample(const std::vector<DiskInfo>& volumes) {
    const auto now = GetTickCount64();
    if (!inventoryTicks || now - inventoryTicks >= 10000) {
        RefreshInventory();
        inventoryTicks = now;
    }

    std::vector<PhysicalDiskInfo> result;
    for (const auto& device : devices) {
        result.push_back(device.info);
    }

    // 将逻辑卷（盘符）精确匹配归属到各自所在的物理磁盘
    for (const auto& volume : volumes) {
        if (volume.drive.size() < 2) continue;
        auto handle = Open(L"\\\\.\\" + volume.drive.substr(0, 2));
        if (handle.value == INVALID_HANDLE_VALUE) continue;

        std::vector<unsigned char> bytes(65536);
        DWORD returned = 0;
        if (!DeviceIoControl(handle.value, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, bytes.data(), static_cast<DWORD>(bytes.size()), &returned, nullptr) ||
            returned < sizeof(VOLUME_DISK_EXTENTS)) continue;

        const auto* extents = reinterpret_cast<const VOLUME_DISK_EXTENTS*>(bytes.data());
        const size_t max = (returned - FIELD_OFFSET(VOLUME_DISK_EXTENTS, Extents)) / sizeof(DISK_EXTENT);
        if (extents->NumberOfDiskExtents > max) continue;

        std::set<DWORD> mapped;
        for (DWORD i = 0; i < extents->NumberOfDiskExtents; ++i) {
            mapped.insert(extents->Extents[i].DiskNumber);
        }

        for (auto& disk : result) {
            if (mapped.count(disk.number)) {
                auto v = volume;
                if (mapped.size() > 1) v.label += L"（跨盘卷）";
                disk.volumes.push_back(std::move(v));
            }
        }
    }

    const float missing = std::numeric_limits<float>::quiet_NaN();

    // 遍历每一个物理硬盘，查询实时温度与 IO 读写性能计数器
    for (size_t i = 0; i < devices.size(); ++i) {
        auto& disk = result[i];
        auto& baseline = baselines[disk.id];
        auto handle = Open(devices[i].path);
        DISK_PERFORMANCE counters{};
        DWORD returned = 0;

        // 1. 读取固态硬盘芯片温度
        if (handle.value != INVALID_HANDLE_VALUE) {
            STORAGE_PROPERTY_QUERY query{};
            query.PropertyId = StorageDeviceTemperatureProperty;
            query.QueryType = PropertyStandardQuery;
            std::vector<unsigned char> bytes(4096);
            if (DeviceIoControl(handle.value, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), bytes.data(), static_cast<DWORD>(bytes.size()), &returned, nullptr) &&
                returned >= FIELD_OFFSET(STORAGE_TEMPERATURE_DATA_DESCRIPTOR, TemperatureInfo) + sizeof(STORAGE_TEMPERATURE_INFO)) {
                const auto* temperatures = reinterpret_cast<const STORAGE_TEMPERATURE_DATA_DESCRIPTOR*>(bytes.data());
                if (temperatures->InfoCount > 0 &&
                    temperatures->TemperatureInfo[0].Temperature >= -20 &&
                    temperatures->TemperatureInfo[0].Temperature <= 150) {
                    disk.temperatureC = static_cast<float>(temperatures->TemperatureInfo[0].Temperature);
                    disk.temperatureValid = true;
                }
            }
        }

        // 2. 读取读写吞吐与活动性能
        const bool measured = (handle.value != INVALID_HANDLE_VALUE) &&
            DeviceIoControl(handle.value, IOCTL_DISK_PERFORMANCE, nullptr, 0, &counters, sizeof(counters), &returned, nullptr) &&
            returned >= sizeof(counters);

        const double seconds = (now - baseline.ticks) / 1000.0;
        if (measured && baseline.valid && seconds > 0 &&
            counters.BytesRead.QuadPart >= baseline.counters.BytesRead.QuadPart &&
            counters.BytesWritten.QuadPart >= baseline.counters.BytesWritten.QuadPart &&
            counters.ReadTime.QuadPart >= baseline.counters.ReadTime.QuadPart &&
            counters.WriteTime.QuadPart >= baseline.counters.WriteTime.QuadPart &&
            counters.IdleTime.QuadPart >= baseline.counters.IdleTime.QuadPart) {

            // 计算每秒读写字节数 (Bps)
            disk.readBps = static_cast<uint64_t>((counters.BytesRead.QuadPart - baseline.counters.BytesRead.QuadPart) / seconds);
            disk.writeBps = static_cast<uint64_t>((counters.BytesWritten.QuadPart - baseline.counters.BytesWritten.QuadPart) / seconds);
            disk.throughputValid = true;

            // 计算活动时间百分比 (Active Time %)
            disk.activePercent = static_cast<float>(std::clamp(100.0 * (1.0 - (counters.IdleTime.QuadPart - baseline.counters.IdleTime.QuadPart) / (seconds * 10000000.0)), 0.0, 100.0));

            // 计算平均响应时间 (ms)
            const auto reads = static_cast<DWORD>(counters.ReadCount - baseline.counters.ReadCount);
            const auto writes = static_cast<DWORD>(counters.WriteCount - baseline.counters.WriteCount);
            const uint64_t operations = static_cast<uint64_t>(reads) + writes;
            disk.responseTimeMs = operations ? static_cast<float>((counters.ReadTime.QuadPart - baseline.counters.ReadTime.QuadPart + counters.WriteTime.QuadPart - baseline.counters.WriteTime.QuadPart) / 10000.0 / operations) : 0;
            disk.statisticsValid = true;
        } else {
            disk.status = measured ? L"正在收集首批统计" : L"统计未提供（权限或设备不支持）";
        }

        // 维护历史曲线折线图
        baseline.reads.Push(disk.throughputValid ? static_cast<float>(disk.readBps / 1000000.0) : missing);
        baseline.writes.Push(disk.throughputValid ? static_cast<float>(disk.writeBps / 1000000.0) : missing);
        disk.readHistory = baseline.reads;
        disk.writeHistory = baseline.writes;

        // 保存本轮测量作为下一轮差量计算的基准
        baseline.counters = counters;
        baseline.ticks = now;
        baseline.valid = measured;
    }
    return result;
}
