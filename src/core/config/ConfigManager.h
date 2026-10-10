/*
 * 模块说明：本地配置与启动项
 * INI 保存外观、监控和位置；注册表 Run 键保存启动命令；Lite/Pro 使用各自的启动项。
 * 读取后校验范围和顺序，写入返回成功标志；注册表存在启动项不等于系统最终一定执行它。
 */
// ConfigManager.h
// 配置管理模块：统一负责读写本地 config.ini，以及“开机自启动”注册表项。
//
// 为什么全部写成 inline：
//   这个头文件会被多个目标同时包含(Lite 版、Pro 版、性能测试等)。
//   如果函数是普通定义，多个 .cpp 包含它就会产生重复符号，导致链接错误；
//   加上 inline 后，链接器会自动把这些实现合并成一份。
//
// 配置文件位置：始终放在“程序 exe 所在目录\config.ini”。
// 这样程序被移动到别的目录时，配置会跟着程序一起走，属于绿色软件的做法。
#pragma once
// Windows API：GetModuleFileNameW、GetPrivateProfileIntW、RegOpenKeyExW、RegSetValueExW 等。
#include <windows.h>
// std::wstring、std::to_wstring，用于宽字符路径拼接和数字转字符串。
#include <string>
#include <array>
#include <algorithm>
// 主题预设枚举 Theme::ThemePreset，AppConfig 中会用到。
#include "ui/theme/Theme.h"
#include "core/config/ProGeneralSettings.h"

// 用命名空间集中管理配置相关函数，调用形式为 ConfigManager::LoadConfig()。
namespace ConfigManager {

    // 主配置结构：对应 config.ini 中的 [Window]、[Appearance]、[Behavior] 三个节。
    struct AppConfig {
        // 窗口左上角的屏幕 X 坐标；-1 表示“从未保存过”，由程序按默认规则计算位置。
        int posX = -1;
        // 窗口左上角的屏幕 Y 坐标；-1 表示“从未保存过”。
        int posY = -1;
        // 贴边状态：0=未贴边，1=上，2=下，3=左，4=右。
        int dockEdge = 0;
        // 主题预设：0=雾透冰晶，1=冰川凝白，2=深邃暗夜，3=极光晨曦，4=赛博赛道。
        int themePreset = 0;
        // 展开样式：0=毛玻璃圆环(RingGlass)，1=暗夜条形(BarDark)。
        int themeStyle = 0;
        // 鼠标移入时是否自动隐藏主体，只保留右侧箭头按钮作为操作入口。
        bool autoHideOnHover = false;
        // 是否随 Windows 启动；这一项不写进 ini，而是实时读注册表得到。
        bool autoStartWithWindows = false;
    };

    // 计算 config.ini 的完整路径。
    // 规则：取当前 exe 所在目录，再拼接固定文件名 config.ini。
    // 例如 exe 在 D:\Tools\PcInfoTool.exe，结果就是 D:\Tools\config.ini。
    inline std::wstring GetConfigFilePath() {
        // 用来接收可执行文件完整路径的缓冲区；MAX_PATH 是 Windows 传统的 260 字符上限。
        wchar_t exePath[MAX_PATH];
        // 第一个参数传 NULL 表示查询“当前进程自己”的可执行文件路径。
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        // 转成 std::wstring，方便后面做查找和截取。
        std::wstring path(exePath);
        // 从后往前找最后一个路径分隔符；同时兼容 "\" 和 "/" 两种写法。
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            // substr(0, lastSlash + 1) 保留分隔符本身，即只去掉文件名，留下目录部分。
            path = path.substr(0, lastSlash + 1);
        }
        // 在目录后面拼接配置文件名，得到完整路径。
        path += L"config.ini";
        return path;
    }

    // 从 config.ini 读取主配置。
    // 特点：文件不存在、某个节或某个键不存在时都不会报错，
    // 而是返回调用时传入的默认值，因此首次运行也能得到一套合理配置。
    inline AppConfig LoadConfig() {
        // 先使用结构体里的默认值，后面再用文件中的实际值逐项覆盖。
        AppConfig cfg;
        // 先算出 config.ini 的完整路径。
        std::wstring ini = GetConfigFilePath();

        // GetPrivateProfileIntW 参数顺序：节名、键名、找不到时的默认值、ini 文件路径。
        // 读取窗口位置；默认 -1 表示“没有保存过位置”。
        cfg.posX = GetPrivateProfileIntW(L"Window", L"PosX", -1, ini.c_str());
        cfg.posY = GetPrivateProfileIntW(L"Window", L"PosY", -1, ini.c_str());
        // 读取贴边状态，默认 0(未贴边)。
        cfg.dockEdge = GetPrivateProfileIntW(L"Window", L"DockEdge", 0, ini.c_str());
        // 读取主题预设，默认 0(雾透冰晶)。
        cfg.themePreset = GetPrivateProfileIntW(L"Appearance", L"ThemePreset", 0, ini.c_str());
        // 读取展开样式，默认 0(毛玻璃圆环)。
        cfg.themeStyle = GetPrivateProfileIntW(L"Appearance", L"ThemeStyle", 0, ini.c_str());
        // ini 里用 0/1 表示开关，这里转成 bool：不等于 0 即视为开启。
        cfg.autoHideOnHover = (GetPrivateProfileIntW(L"Behavior", L"AutoHideOnHover", 0, ini.c_str()) != 0);

        // 开机自启动不读 ini，而是直接查注册表。
        // 这里只反映 Run 注册表值是否存在；任务管理器的启动批准状态没有在此另行读取。
        HKEY hKey;
        // 打开当前用户的开机启动项：
        // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run。
        // KEY_READ 表示本次只需要读取权限。
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            // 用于接收注册表值内容的缓冲区；这里只关心“是否存在”，不解析具体内容。
            wchar_t val[MAX_PATH] = {};
            // 传入缓冲区字节大小，Windows 会把实际读取到的字节数回填到这里。
            DWORD len = sizeof(val);
            // 查询名为 PcInfoTool 的值；查询成功即说明已经设置过开机启动。
            if (RegQueryValueExW(hKey, L"PcInfoTool", NULL, NULL, (LPBYTE)val, &len) == ERROR_SUCCESS) {
                cfg.autoStartWithWindows = true;
            }
            // 注册表句柄必须成对关闭，否则会持续占用系统句柄资源。
            RegCloseKey(hKey);
        }

        // 返回组装好的配置，调用方按字段读取即可。
        return cfg;
    }

    // Pro 版专用外观设置，对应 ini 中的 [ProAppearance] 节。
    struct ProAppearance {
        // 主题模式，取值 0-3，默认 1。
        int themeMode = 1;
        // 布局模式，取值 0-1，默认 0。
        int layoutMode = 0;
        // 毛玻璃不透明度，取值 0.1-1.0，默认 0.4。
        float glassOpacity = .4f;
    };

    // 读取 Pro 版外观设置。
    // path 带默认参数：不传就用默认的 config.ini 路径，传入自定义路径主要是为了方便测试。
    inline ProAppearance LoadProAppearance(const std::wstring& path=GetConfigFilePath()) {
        ProAppearance value;
        // 读取主题模式：先取整数，再用 std::clamp 限制到 0-3。
        // 这样做是为了防止用户手动改坏 ini 后传入非法值，导致绘制或枚举转换出错。
        value.themeMode=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"ProAppearance",L"ThemeMode",1,path.c_str())),0,3);
        // 读取布局模式并限制到 0-1。
        value.layoutMode=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"ProAppearance",L"LayoutMode",0,path.c_str())),0,1);
        // 透明度在 ini 中以“百分比整数”保存(默认 40)；
        // 先限制到 10-100，再除以 100 转成 0.1-1.0 的浮点数。
        value.glassOpacity=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"ProAppearance",L"GlassOpacityPercent",40,path.c_str())),10,100)/100.f;
        return value;
    }

    // 保存 Pro 版外观设置；三项全部写入成功才返回 true。
    inline bool SaveProAppearance(const ProAppearance& value,const std::wstring& path=GetConfigFilePath()) {
        // WritePrivateProfileStringW 返回非 0 表示写入成功，因此用 != 0 转成 bool。
        const bool theme=WritePrivateProfileStringW(L"ProAppearance",L"ThemeMode",std::to_wstring(value.themeMode).c_str(),path.c_str())!=0;
        const bool layout=WritePrivateProfileStringW(L"ProAppearance",L"LayoutMode",std::to_wstring(value.layoutMode).c_str(),path.c_str())!=0;
        // 写入前先把 0.1-1.0 的浮点透明度还原成 10-100 的整数百分比；
        // 加 0.5f 是为了四舍五入，避免 0.4 被直接截断成 39。
        const bool opacity=WritePrivateProfileStringW(L"ProAppearance",L"GlassOpacityPercent",std::to_wstring(static_cast<int>(std::clamp(value.glassOpacity,.1f,1.f)*100+.5f)).c_str(),path.c_str())!=0;
        // 三个结果用 && 连接：只有都成功才认为整体保存成功。
        return theme&&layout&&opacity;
    }

    // Pro 版监控与指示器设置，对应 ini 中的 [ProMonitoring] 节
    struct ProMonitoring {
        // 4 个指标的显示开关 (依次对应: 0:CPU占用, 1:CPU温度, 2:内存占用, 3:网络速率)
        std::array<bool,4> visible{true, true, true, true};
        // 4 个指标在悬浮条上的排列顺序 (默认 0,1,2,3，可通过拖拽重排)
        std::array<int,4> order{0, 1, 2, 3};
        // UI 刷新选项：0 对应 0.5 秒，1 对应 1 秒，2 对应 2 秒；不改变采集线程周期。
        int interval = 1;
        // 温度单位 (0: 摄氏度 °C, 1: 华氏度 °F)，默认 0
        int unit = 0;
        // CPU 高温警报阈值 (默认 85°C；0 代表关闭报警；可选 75~95 且为 5 的倍数)
        int alert = 85;
        // 当前选中的物理磁盘硬件设备 ID (例如 NVMe 硬盘的设备实例路径)
        std::wstring diskId;
    };

    /**
     * @brief 从 ini 文件读取 Pro 版监控指示器与警报设置
     * @param path ini 文件路径（默认取当前 exe 同目录下的 config.ini）
     */
    inline ProMonitoring LoadProMonitoring(const std::wstring& path = GetConfigFilePath()) {
        ProMonitoring v;
        std::array<bool,4> seen{};
        bool valid = true;

        // 循环读取 4 个指示器的显隐状态与排列次序
        for (int i = 0; i < 4; ++i) {
            auto key = L"Visible" + std::to_wstring(i);
            v.visible[i] = GetPrivateProfileIntW(L"ProMonitoring", key.c_str(), 1, path.c_str()) != 0;

            key = L"Order" + std::to_wstring(i);
            int id = static_cast<int>(GetPrivateProfileIntW(L"ProMonitoring", key.c_str(), i, path.c_str()));
            // 校验顺序索引是否在 0~3 范围内，且没有重复出现
            if (id < 0 || id > 3 || seen[id]) valid = false;
            else seen[id] = true;
            v.order[i] = id;
        }
        // 如果 ini 中配置的顺序无效或重复，回退为默认顺序 [0, 1, 2, 3]
        if (!valid) v.order = {0, 1, 2, 3};

        // 读取采样间隔并限制在 0~2 之间
        v.interval = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"ProMonitoring", L"Interval", 1, path.c_str())), 0, 2);
        // 读取温度单位 (0: °C, 1: °F)
        v.unit = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"ProMonitoring", L"TemperatureUnit", 0, path.c_str())), 0, 1);
        // 读取高温警报值，仅允许 0(关闭) 或 75~95 之间的 5 的倍数
        int alert = static_cast<int>(GetPrivateProfileIntW(L"ProMonitoring", L"AlertCelsius", 85, path.c_str()));
        v.alert = (alert == 0 || (alert >= 75 && alert <= 95 && alert % 5 == 0)) ? alert : 85;

        // 读取选中的磁盘设备 ID
        wchar_t disk[2048] = {};
        GetPrivateProfileStringW(L"ProMonitoring", L"DiskId", L"", disk, 2048, path.c_str());
        v.diskId = disk;
        return v;
    }

    /**
     * @brief 将 Pro 版监控指示器设置持久化到 ini 文件
     * @return true 保存成功；false 写入遇到错误
     */
    inline bool SaveProMonitoring(const ProMonitoring& v, const std::wstring& path = GetConfigFilePath()) {
        bool ok = true;
        auto write = [&](const wchar_t* key, const std::wstring& value) {
            if (!WritePrivateProfileStringW(L"ProMonitoring", key, value.c_str(), path.c_str())) ok = false;
        };

        for (int i = 0; i < 4; ++i) {
            write((L"Visible" + std::to_wstring(i)).c_str(), v.visible[i] ? L"1" : L"0");
            write((L"Order" + std::to_wstring(i)).c_str(), std::to_wstring(v.order[i]));
        }
        write(L"Interval", std::to_wstring(v.interval));
        write(L"TemperatureUnit", std::to_wstring(v.unit));
        write(L"AlertCelsius", std::to_wstring(v.alert));
        write(L"DiskId", v.diskId);
        return ok;
    }

    /**
     * @brief 检查 Pro 专业版是否已在注册表设置开机自启
     * 
     * 注意：Pro 版使用独立的键名 "PcInfoTool-Pro"，与 Lite 版的 "PcInfoTool" 隔离，互不干扰
     */
    inline bool IsProAutoStartEnabled() {
        DWORD bytes = 0;
        return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            L"PcInfoTool-Pro", RRF_RT_REG_SZ, nullptr, nullptr, &bytes) == ERROR_SUCCESS;
    }

    /**
     * @brief 开启或关闭 Pro 专业版的开机自启
     * @param enabled true: 写入注册表；false: 从注册表删除对应键值
     */
    inline bool SetProAutoStart(bool enabled) {
        HKEY key = nullptr;
        const auto opened = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
        if (opened != ERROR_SUCCESS) return false;

        LSTATUS result;
        if (enabled) {
            wchar_t exe[32768];
            DWORD size = GetModuleFileNameW(nullptr, exe, 32768);
            if (!size || size >= 32768) {
                RegCloseKey(key);
                return false;
            }
            // 给可执行文件路径加上双引号，避免路径含空格导致系统解析失败
            const auto command = L"\"" + std::wstring(exe) + L"\"";
            result = RegSetValueExW(key, L"PcInfoTool-Pro", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        } else {
            result = RegDeleteValueW(key, L"PcInfoTool-Pro");
            if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
        }
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    /**
     * @brief 读取 Pro 专业版的常规系统行为设置
     */
    inline ProGeneralSettings LoadProGeneral(const std::wstring& path = GetConfigFilePath()) {
        ProGeneralSettings v;
        auto read = [&](const wchar_t* key, int def) {
            return static_cast<int>(GetPrivateProfileIntW(L"ProGeneral", key, def, path.c_str()));
        };

        v.autoStart = IsProAutoStartEnabled();
        v.alwaysOnTop = read(L"AlwaysOnTop", 1) != 0;
        v.lockPosition = read(L"LockPosition", 0) != 0;
        v.hideInFullscreen = read(L"HideInFullscreen", 1) != 0;
        v.rememberPosition = read(L"RememberPosition", 1) != 0;
        v.reduceMotion = read(L"ReduceMotion", 0) != 0;

        // 读取快捷键设置并做合法性校验
        const UINT mods = read(L"HotkeyModifiers", MOD_CONTROL | MOD_ALT);
        const UINT key = read(L"HotkeyKey", 'I');
        if (IsValidProHotkey(mods, key)) {
            v.hotkeyModifiers = mods;
            v.hotkeyKey = key;
        }

        // 读取上次记忆的屏幕坐标
        v.positionValid = v.rememberPosition && read(L"PositionValid", 0) != 0;
        v.positionX = std::clamp(read(L"PositionX", 0), -1000000, 1000000);
        v.positionY = std::clamp(read(L"PositionY", 0), -1000000, 1000000);
        v.savedDpi = std::clamp(read(L"PositionDpi", 96), 48, 768);
        return v;
    }

    /**
     * @brief 保存 Pro 专业版的常规系统行为设置到 ini 文件
     */
    inline bool SaveProGeneral(const ProGeneralSettings& v, const std::wstring& path = GetConfigFilePath()) {
        bool ok = true;
        auto write = [&](const wchar_t* key, int value) {
            if (!WritePrivateProfileStringW(L"ProGeneral", key, std::to_wstring(value).c_str(), path.c_str())) ok = false;
        };

        write(L"AlwaysOnTop", v.alwaysOnTop);
        write(L"LockPosition", v.lockPosition);
        write(L"HideInFullscreen", v.hideInFullscreen);
        write(L"RememberPosition", v.rememberPosition);
        write(L"ReduceMotion", v.reduceMotion);
        write(L"HotkeyModifiers", v.hotkeyModifiers);
        write(L"HotkeyKey", v.hotkeyKey);
        write(L"PositionValid", v.rememberPosition && v.positionValid);
        write(L"PositionX", v.positionX);
        write(L"PositionY", v.positionY);
        write(L"PositionDpi", v.savedDpi);
        return ok;
    }

    // 把主配置写回 config.ini。
    // 注意：WritePrivateProfileStringW 只能写字符串，
    // 所以整数要先经过 std::to_wstring 转换，再用 c_str() 取宽字符指针。
    // 这里写入的节名/键名与 LoadConfig 中读取的完全一致，保证存取配套。
    inline void SaveConfig(const AppConfig& cfg) {
        // 写入前先确定目标 ini 文件路径。
        std::wstring ini = GetConfigFilePath();

        // 参数顺序：节名、键名、要写入的字符串值、ini 文件路径。
        // 保存窗口 X 坐标；文件不存在时 Windows 会自动创建。
        WritePrivateProfileStringW(L"Window", L"PosX", std::to_wstring(cfg.posX).c_str(), ini.c_str());
        // 保存窗口 Y 坐标。
        WritePrivateProfileStringW(L"Window", L"PosY", std::to_wstring(cfg.posY).c_str(), ini.c_str());
        // 保存贴边状态，下次启动可直接恢复贴边行为。
        WritePrivateProfileStringW(L"Window", L"DockEdge", std::to_wstring(cfg.dockEdge).c_str(), ini.c_str());
        // 保存主题预设。
        WritePrivateProfileStringW(L"Appearance", L"ThemePreset", std::to_wstring(cfg.themePreset).c_str(), ini.c_str());
        // 保存展开样式。
        WritePrivateProfileStringW(L"Appearance", L"ThemeStyle", std::to_wstring(cfg.themeStyle).c_str(), ini.c_str());
        // 布尔值按 "0" / "1" 保存，读取时再用 != 0 还原成 bool。
        WritePrivateProfileStringW(L"Behavior", L"AutoHideOnHover", cfg.autoHideOnHover ? L"1" : L"0", ini.c_str());
    }

    // 开启或关闭“随 Windows 启动”。
    // 实现方式：在当前用户的 Run 注册表项下，写入或删除名为 PcInfoTool 的字符串值。
    // 该注册表项属于当前用户，因此不需要管理员权限。
    // 返回 false 表示无法打开注册表(通常是被安全软件或组策略限制)。
    inline bool SetAutoStart(bool enable) {
        HKEY hKey;
        // KEY_WRITE 表示需要写入权限：开启时要写入值，关闭时要删除值。
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
            return false;
        }

        if (enable) {
            // 取得当前 exe 的完整路径，注册表需要知道开机时该启动哪个程序。
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            // 用英文双引号包住整个路径，避免安装目录含空格时启动命令被拆成多个参数。
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
            // RegSetValueExW 参数依次为：句柄、值名、保留参数 0、数据类型 REG_SZ(字符串)、数据指针、数据字节长度。
            // 长度 = (字符数 + 结尾的 \0) × 每个宽字符占用的 2 字节。
            RegSetValueExW(hKey, L"PcInfoTool", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        } else {
            // 关闭自启动：直接删除该注册表值即可，下次开机不会再运行本程序。
            RegDeleteValueW(hKey, L"PcInfoTool");
        }
        // 使用完注册表必须关闭句柄。
        RegCloseKey(hKey);
        return true;
    }

    // 查询当前是否已开启开机自启动。
    // 判断依据只有一个：注册表 Run 项下是否存在名为 PcInfoTool 的值。
    inline bool IsAutoStartEnabled() {
        HKEY hKey;
        // 默认认为未开启；只有确实查到了才改成 true。
        bool enabled = false;
        // KEY_READ 表示本次只需要读取权限。
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            // 只用于接收数据，内容本身不解析，因此缓冲区初始化为 0 即可。
            wchar_t val[MAX_PATH] = {};
            // 传入缓冲区大小，Windows 会回填实际读取到的长度。
            DWORD len = sizeof(val);
            // 查询成功即表示该自启动项存在。
            if (RegQueryValueExW(hKey, L"PcInfoTool", NULL, NULL, (LPBYTE)val, &len) == ERROR_SUCCESS) {
                enabled = true;
            }
            // 查询结束后关闭句柄。
            RegCloseKey(hKey);
        }
        return enabled;
    }
}
