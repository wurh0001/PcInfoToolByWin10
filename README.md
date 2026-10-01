# 🚀 PcInfoTool (极轻量 Windows 硬件悬浮状态监控套件)

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-blue?style=flat-square&logo=windows)
![Language](https://img.shields.io/badge/Language-Modern%20C%2B%2B20-00599C?style=flat-square&logo=c%2B%2B)
![Renderer](https://img.shields.io/badge/Renderer-Direct2D%20%2F%20DirectWrite-success?style=flat-square)
![Memory](https://img.shields.io/badge/Memory-<10%20MB%20RAM-brightgreen?style=flat-square)
![CPU](https://img.shields.io/badge/CPU-<0.1%25-brightgreen?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-orange?style=flat-square)

**专为极客、游戏玩家与极简主义者打造的 Windows 边缘吸附式微型硬件监控器。**  
告别 Electron 与臃肿运行时，回归原生 C++ 与 Direct2D 的丝滑流畅与极致能效。

[核心亮点](#-核心亮点) • [效果展示](#-界面美学) • [性能实测](#-极客级性能标杆) • [快速开始](#-快速开始) • [快捷操作与交互](#-交互手势与快捷操作)

</div>

---

## 🌟 核心亮点

- ⚡ **真正的微开销（Runtime-Free）**：纯原生 C++20 编译，无 Electron、无 .NET CLR、无 Qt 等重型依赖；常驻工作集仅约 **8 ~ 10 MB**，闲置谷值低至 **1.5 MB**，CPU 平均占用稳定在 **0.03% ~ 0.05%**。
- 🎨 **液态玻璃美学（Fluid Frosted Glass）**：基于 Windows 原生 Direct2D 硬件加速图元渲染，具备逐像素 Alpha 半透明通道、镜面镜缘高光（Specular Rim Highlight）与亚克力冰雾质感。
- 📈 **60 秒平滑折线波形图（Smooth Waveform Charts）**：展开详情面板（Flyout）内嵌 CPU、物理内存、双通道网络（下载/上传独立自适应量程）以及 GPU 核心负载的实时动态波形，配带呼吸光标点与三阶参考网格。
- 🧲 **智能边缘吸附与微型条收折（Smart Docking）**：支持拖拽靠近屏幕四边自动磁吸，鼠标离开后收缩为极简胶囊三色状态条（CPU/RAM/GPU），不遮挡正常工作视野。
- 🛡️ **鼠标自动避让（Ghost Pass-Through）**：开启避让模式后，鼠标滑过主体自动透明穿透，完全不阻碍底部窗口与桌面图标操作。
- 🧠 **零动态堆分配引擎（Zero Heap Allocation）**：历史指标全量采用固定容量内联环形队列（RingBuffer）管理，零内存移动碎片，彻底杜绝垃圾回收卡顿（GC Pause）。

---

## 🖼️ 界面美学

### 1. 悬浮吸附条（MiniBar）
- **流体圆环玻璃态（Liquid Ring Glass）**：双核/多核 CPU、RAM 独立微型环形计量仪，搭配网络流速动态指示。
- **微型黑曜石色条态（Obsidian Bar）**：硬核线性进度色条与数值直读。
- **贴边收折态（Collapsed Tri-Bar）**：屏幕边缘仅保留一条纤细三色微光胶囊，直观呈现负载高低。

### 2. 展开式全局仪表板（Flyout Dashboard）
点击右侧圆形操作键即刻呼出高集成度卡片：
- **CPU 总体负载卡片**：薄荷绿平滑波形 + 0%/50%/100% 虚线基准网格 + 实时动态呼吸光标。
- **物理内存 (RAM) 监控**：柔和紫罗兰曲线，动态呈现 `已用 / 总量 GB` 及实时百分比。
- **双通道网络流速**：左右分栏并列卡片，分为 **↓ 下载流速** 与 **↑ 上传流速**，自动根据当前流速自适应动态量程。
- **GPU 核心负载卡片**：青蓝色波形曲线，直接对接 Windows PDH 统一性能接口，无需第三方显卡 SDK。
- **本地存储空间看板**：直观展示本地各固定磁盘（C:\、D:\ 等）容量分配与健康胶囊条。

---

## 📊 极客级性能标杆

内置测试套件 `test/test_performance.exe`（基于 Windows 物理内核 API `GetProcessTimes`、`GetProcessMemoryInfo` 高精度连续采样）：

```text
========================== 综合性能分析评估报告 ==========================
  [物理内存工作集 (WorkingSet)]    : 平均 9.82 MB  (区间: 1.56 ~ 14.36 MB)
  [私有独占内存 (PrivateMemory)]   : 平均 18.00 MB
  [CPU 资源消耗 (CPU Usage)]       : 平均 0.05 %  (峰值仅: 0.19 %)
--------------------------------------------------------------------------
【综合评级: 极优 (Ultra Light)】
  深度评估结论:
   1. 内存极低: 常驻工作集跌破 10 MB 大关，即使运行 3A 大作或 4K 视频渲染也完全零感知。
   2. 算力零压: CPU 平均占用仅 0.05 %，远优于大部分系统原生小部件。
   3. 零驱动负担: 纯 Direct2D 软件表面合成 + PDH 统一接口，未加载重型显卡闭源 SDK。
==========================================================================
```

---

## 🎨 五套精心调校的主题预设

右键菜单可随时无缝热切换：
1. 🫧 **雾透冰晶 (Translucent Glass)**：超高透冷雾磨砂玻璃，微发光白边，最受用户欢迎的原版通透质感。
2. ❄️ **冰川凝白 (Glacier White)**：明亮模式首选，超透白月光玻璃配柔光青蓝与霓紫。
3. 🌙 **深邃暗夜 (Obsidian Dark)**：黑曜石高级暗色调，纯净静谧不刺眼。
4. 🔮 **极光晨曦 (Aurora Glass)**：幻彩极光流光色彩与微光边框。
5. ⚡ **赛博赛道 (Cyber Neon)**：硬核科技高饱和霓虹与极速反差。

---

## 🛠️ 交互手势与快捷操作

| 操作动作 | 触发效果 |
| :--- | :--- |
| **鼠标左键按住拖动** | 自由移动监控条至屏幕任意位置 |
| **拖动靠近屏幕边缘** | 自动吸附至左、右、上、下边缘 |
| **鼠标移开吸附条** | 贴边时自动缩起为极简三色状态胶囊 |
| **鼠标靠近收起胶囊** | 自动平滑展开完整监控条 |
| **点击右侧圆形箭头** | 呼出 / 收起 60 秒平滑波形图展开仪表板（Flyout） |
| **鼠标右键单击** | 呼出功能菜单：切换主题、切换条形/圆环样式、开关避让模式、退出 |

---

## 🚀 快速开始

### 运行环境
- **操作系统**：Windows 10 / 11 (x64)
- **依赖需求**：纯原生 Win32 + Direct2D，**无需安装任何运行时环境**。

### 编译构建

#### 推荐方式（一键脚本）
本项目自带极速编译脚本，使用 Visual Studio 2022 开发人员环境：

```bat
# 克隆仓库
git clone https://github.com/your-username/pc-information-tool.git
cd pc-information-tool

# 执行一键构建
build.bat
```
构建产物将输出在：`build\Release\PcInfoTool.exe`。

#### 手动 CMake 构建
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 运行性能检测
在 `test` 目录或项目根目录下执行：
```powershell
.\test\test_performance.exe 10   # 运行 10 秒高精度采样评估
```

---

## 💡 为什么 PcInfoTool 如此轻快？

1. **没有 Chromium 和 Node.js 的包袱**：拒绝动辄上百兆的跨端框架，回归纯正的原生平台生态。
2. **GPU 硬件按需绘制（Demand-Driven Rendering）**：
   - 面板折叠时，波形图管线完全静默，GPU 负载降至绝对零；
   - 绝不使用高频空转循环，仅随系统时钟心跳与局部交互脏矩形刷新。
3. **内核级 API 瞬时调用**：
   - CPU 与多核心负载通过内核级 `NtQuerySystemInformation` 与 `GetSystemTimes` 采集；
   - 内存与网络通过 `GlobalMemoryStatusEx` 与 `GetIfTable2` 纳秒级直接读取；
   - 拒绝启子进程解析文本，零 IPC 损耗。

---

## 📄 开源许可证

本项目遵循 [MIT License](LICENSE) 开源协议。欢迎提交 PR、提出 Issue 与建议！
