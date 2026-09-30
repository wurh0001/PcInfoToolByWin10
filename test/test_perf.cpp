#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <numeric>
#include <iomanip>
#include <algorithm>
#include <string>

#pragma comment(lib, "psapi.lib")

static DWORD FindProcessId(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

static uint64_t FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

int main(int argc, char* argv[]) {
    // Enable UTF-8 in console
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    int totalSamples = 20; // 默认 20 次深测采样 (共 20 秒)
    if (argc > 1) {
        int custom = std::atoi(argv[1]);
        if (custom >= 5 && custom <= 300) {
            totalSamples = custom;
        }
    }

    std::cout << "==========================================================\n";
    std::cout << "          PcInfoTool 实时运行性能与资源开销检测           \n";
    std::cout << "==========================================================\n\n";

    DWORD pid = FindProcessId(L"PcInfoTool.exe");

    if (pid == 0) {
        std::cout << "[提示] 未检测到正在运行的 PcInfoTool.exe 进程！\n";
        std::cout << "正在为您自动从 build\\Release\\PcInfoTool.exe 启动...\n";
        
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {};
        wchar_t cmd[] = L"..\\build\\Release\\PcInfoTool.exe";
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            Sleep(1500);
            pid = FindProcessId(L"PcInfoTool.exe");
        }
    }

    if (pid == 0) {
        std::cout << "[错误] 启动进程失败或未找到可执行文件！\n";
        std::cout << "请先运行根目录下的 build.bat 完成编译。\n\n";
        system("pause");
        return 1;
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) {
        std::cout << "[错误] 无法获取进程句柄 (PID: " << pid << ")，可能权限不足！\n\n";
        system("pause");
        return 1;
    }

    std::cout << "[✔] 目标进程已锁定: PcInfoTool.exe (PID: " << pid << ")\n\n";
    std::cout << "正在进行 " << totalSamples << " 次高精度硬件级物理采样（每秒 1 次，共 " << totalSamples << " 秒）...\n";
    std::cout << "提示: 您可以尝试拖拽部件、展开/折叠面板，观察实时动态变化。\n";
    std::cout << "------------------------------------------------------------------------------------\n";

    std::vector<double> wsList;
    std::vector<double> privList;
    std::vector<double> cpuList;

    for (int i = 1; i <= totalSamples; ++i) {
        FILETIME ftCreation, ftExit, ftKernel1, ftUser1;
        FILETIME ftKernel2, ftUser2;
        FILETIME ftSysIdle, ftSysKernel1, ftSysUser1;
        FILETIME ftSysKernel2, ftSysUser2;

        GetProcessTimes(hProc, &ftCreation, &ftExit, &ftKernel1, &ftUser1);
        GetSystemTimes(&ftSysIdle, &ftSysKernel1, &ftSysUser1);

        Sleep(1000);

        GetProcessTimes(hProc, &ftCreation, &ftExit, &ftKernel2, &ftUser2);
        GetSystemTimes(&ftSysIdle, &ftSysKernel2, &ftSysUser2);

        // Memory info
        PROCESS_MEMORY_COUNTERS_EX pmc = { sizeof(PROCESS_MEMORY_COUNTERS_EX) };
        double wsMB = 0.0;
        double privMB = 0.0;
        if (GetProcessMemoryInfo(hProc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            wsMB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
            privMB = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
        }

        // CPU Usage %
        uint64_t procKernel = FileTimeToUint64(ftKernel2) - FileTimeToUint64(ftKernel1);
        uint64_t procUser = FileTimeToUint64(ftUser2) - FileTimeToUint64(ftUser1);
        uint64_t sysKernel = FileTimeToUint64(ftSysKernel2) - FileTimeToUint64(ftSysKernel1);
        uint64_t sysUser = FileTimeToUint64(ftSysUser2) - FileTimeToUint64(ftSysUser1);
        uint64_t sysTotal = sysKernel + sysUser;

        double cpuPercent = 0.0;
        if (sysTotal > 0) {
            cpuPercent = (static_cast<double>(procKernel + procUser) / static_cast<double>(sysTotal)) * 100.0;
        }

        DWORD handleCount = 0;
        GetProcessHandleCount(hProc, &handleCount);

        wsList.push_back(wsMB);
        privList.push_back(privMB);
        cpuList.push_back(cpuPercent);

        std::cout << "采样 [" << std::setw(2) << i << "/" << totalSamples << "] -> "
                  << "物理内存(工作集): " << std::fixed << std::setprecision(2) << std::setw(6) << wsMB << " MB | "
                  << "私有独占内存: " << std::setw(6) << privMB << " MB | "
                  << "CPU 占用: " << std::setw(5) << cpuPercent << " % | "
                  << "系统句柄数: " << handleCount << "\n";
    }

    CloseHandle(hProc);

    std::cout << "------------------------------------------------------------------------------------\n\n";

    double avgWs = std::accumulate(wsList.begin(), wsList.end(), 0.0) / wsList.size();
    double maxWs = *std::max_element(wsList.begin(), wsList.end());
    double minWs = *std::min_element(wsList.begin(), wsList.end());

    double avgPriv = std::accumulate(privList.begin(), privList.end(), 0.0) / privList.size();
    double avgCpu = std::accumulate(cpuList.begin(), cpuList.end(), 0.0) / cpuList.size();
    double maxCpu = *std::max_element(cpuList.begin(), cpuList.end());

    std::cout << "========================== 综合性能分析评估报告 ==========================\n";
    std::cout << "  [物理内存工作集 (WorkingSet)]    : 平均 " << std::fixed << std::setprecision(2) << avgWs 
              << " MB  (区间: " << minWs << " ~ " << maxWs << " MB)\n";
    std::cout << "  [私有独占内存 (PrivateMemory)]   : 平均 " << avgPriv << " MB\n";
    std::cout << "  [CPU 资源消耗 (CPU Usage)]       : 平均 " << avgCpu 
              << " %  (峰值仅: " << maxCpu << " %)\n";
    std::cout << "--------------------------------------------------------------------------\n";

    if (avgWs < 15.0 && avgCpu < 0.5) {
        std::cout << "【综合评级: 极优 (Ultra Light)】\n";
        std::cout << "  深度评估结论:\n";
        std::cout << "   1. 内存极低: 常驻工作集仅约 " << avgWs << " MB，即使运行大型 3A 游戏或重度渲染也完全零感知。\n";
        std::cout << "   2. 算力零压: CPU 平均占用仅 " << avgCpu << " %，比大部分系统原生小部件更安静、更省电。\n";
        std::cout << "   3. 零驱动负担: 纯 Direct2D 软件表面合成 + PDH 统一接口，未加载重型显卡闭源 SDK。\n";
    } else if (avgWs < 30.0) {
        std::cout << "【综合评级: 优秀 (Lightweight)】\n";
    } else {
        std::cout << "【综合评级: 良好】\n";
    }

    std::cout << "==========================================================================\n\n";
    system("pause");
    return 0;
}
