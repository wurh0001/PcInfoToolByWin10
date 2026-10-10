# ⚡ PcInfoTool (极轻量 Windows 原生硬件悬浮监控套件)

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x64)-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Language](https://img.shields.io/badge/Language-Modern%20C%2B%2B20-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Renderer](https://img.shields.io/badge/Renderer-Direct2D%20%2F%20DirectWrite-76B900?style=for-the-badge)
![Memory](https://img.shields.io/badge/Memory-<10%20MB%20RAM-brightgreen?style=for-the-badge)
![CPU](https://img.shields.io/badge/CPU-<0.05%25-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-orange?style=for-the-badge)

<br/>

**专为极客、发烧友与极简主义者倾力打造的 Windows 边缘吸附式微型硬件监控套件。**  
告别 Electron、Python、.NET 等臃肿运行时与内存吞噬，回归纯粹 C++20 与 Direct2D 硬件加速的极致丝滑与零感开销。

<br/>

[双版本特性对比](#-双版本矩阵-pro-vs-lite) • [Pro 专业版亮点](#-pcinfotool-pro-专业版) • [Lite 经典版亮点](#-pcinfotool-lite-经典版) • [性能实测](#-硬核性能标杆) • [快速开始与编译](#-快速上手与编译构建) • [快捷操作说明](#-交互手势与快捷操作)

</div>

---

## 💎 双版本矩阵 (Pro vs Lite)

PcInfoTool 现在提供两个量身定制的独立版本，满足不同使用场景下的极致追求：

| 维度 / 特性 | 🚀 PcInfoTool Pro (专业版) | 🪶 PcInfoTool Lite (轻量版) |
| :--- | :--- | :--- |
| **设计定位** | **次世代 macOS 级高集成硬件仪表板** | **经典纯粹、极低占用的极简吸附条** |
| **可执行文件** | `PcInfoTool-Pro.exe` | `PcInfoTool-Lite.exe` (或 `PcInfoTool.exe`) |
| **悬浮栏视觉** | 380 × 32 DIP 纯白/晶透微型状态栏，支持分段点按 | 支持流体圆环玻璃态、黑曜石条态、三色微光折叠胶囊 |
| **展开面板 (Flyout)** | **六大模块全功能可折叠风琴看板** (CPU/GPU/内存/磁盘/网络/传感器) | 经典并列波形卡片式面板 |
| **悬停速览 (Hover Peek)** | 鼠标悬停分段即时弹出极简微型气泡（轻量无侵入） | — |
| **波形图历史** | 60 秒高精度连续波形，贝塞尔曲线与渐变区域填充 | 60 秒平滑曲线与三阶参考线 |
| **硬件传感器** | 原生零驱动读取：NVML GPU 温度/功耗/时钟 + 物理磁盘温度 | 纯原生 API 纳秒级采集系统基础性能 |
| **偏好设置面板** | 独立图形化偏好设置（外观/监控/常规、主题与透明度切换） | 原生右键上下文菜单即时切换 |
| **常驻内存 (RAM)** | 约 **10 ~ 14 MB** | **< 10 MB** (闲置低至 **1.5 MB**) |
| **CPU 占用** | **< 0.05%** | **< 0.03%** |
| **适合人群** | 追求高质感 UI、详尽硬件参数、多模块一键下钻的极客玩家 | 追求极限轻盈、极简桌面美学、游戏免打扰的用户 |

---

## 🚀 PcInfoTool Pro (专业版)

> **像素级对齐 Modern Handoff 设计规范，将桌面硬件监控美学推向极致。**

### 1. 🎛️ 六大可折叠仪表板（Accordion Dashboard）
面板采用 360 DIP 黄金宽度，支持全展开概览与单模块快速下钻：
- **CPU 核心分析**：大字号瞬时占用百分比、封装温度、多核心拓扑（P/E 核）阶梯条、运行频率与占用 Top 进程直读。
- **GPU 深度看板**：实时利用率、NVML 核心温度、显存占用进度双段条、功耗 (W)、核心频率 (GHz) 及 3D/编解码引擎细分负载。
- **物理内存看板**：已用 / 可用 GiB 直读、提交限制、页面缓冲池与高内存进程排行。
- **双通道网络看板**：下载 / 上传独立自适应动态量程图表、实时速率、网络适配器型号与连接状态。
- **磁盘阵列看板**：物理磁盘读写吞吐速率双曲线（读青 / 写橙）、平均响应时间 (ms)、分区卷容量健康条。
- **传感器与状态**：风扇转速、硬件温度矩阵与运行时间计时器。

### 2. 🔍 悬停速览微气泡（Hover Peek Tooltip）
无需点击展开庞大面板，鼠标掠过悬浮条对应指标段，瞬间弹出半透明极简微气泡，快速查看当前温度、读写速率或显存详情，移开即隐，毫无视觉负担。

### 3. ⚙️ 全功能偏好设置（Preferences）
- **主题模式无缝切换**：纯白（Pure White）、亮色玻璃（Glass Light）、深色（Dark）、跟随系统。
- **自定义指示器**：自由开关悬浮条上展示的指标（CPU / 温度 / 内存 / 网络）并支持拖拽重排次序。
- **采样与提醒**：自定义刷新频率（1s/2s/5s）、摄氏/华氏温度单位切换、CPU 高温智能提醒阈值。

---

## 🪶 PcInfoTool Lite (轻量版)

> **极度克制、纤薄小巧。只保留最核心的感知，让监控存在于无形之中。**

### 1. 🧲 屏幕边缘智能磁吸与收折
- 支持拖拽靠近屏幕上下左右四边自动磁吸对齐。
- **贴边收折态（Collapsed Tri-Bar）**：鼠标离开后自动收缩为极简三色状态微光胶囊（CPU / RAM / GPU），仅占几个像素，绝不遮挡日常网页与文档边缘。
- **鼠标靠近唤醒**：鼠标滑过收起胶囊时瞬间平滑展开完整监控条。

### 2. 🎨 两种经典风格与五套调色预设
- **流体圆环玻璃态（Liquid Ring Glass）**：独立微型环形仪表，搭配网络上下行流速动态波浪。
- **黑曜石色条态（Obsidian Bar）**：硬核线性进度色条与数值直读。
- **预设调色板**：雾透冰晶、冰川凝白、深邃暗夜、极光晨曦、赛博赛道，右键菜单一键热切换。

### 3. 🛡️ 鼠标穿透与避让模式（Ghost Pass-Through）
开启避让模式后，监控条遇到鼠标自动变为半透明且鼠标事件直接穿透至底层窗口，彻底不阻碍底层桌面图标点击与窗口操作。

---

## 📊 硬核性能标杆

内置测试套件 `test_performance.exe`（基于 Windows 内核 API `GetProcessTimes` 与 `GetProcessMemoryInfo` 连续毫秒级采样）：

```text
========================== 综合性能分析评估报告 ==========================
  [物理内存工作集 (WorkingSet)]    : 平均 9.82 MB  (区间: 1.56 ~ 14.36 MB)
  [私有独占内存 (PrivateMemory)]   : 平均 18.00 MB
  [CPU 资源消耗 (CPU Usage)]       : 平均 0.05 %  (峰值仅: 0.19 %)
--------------------------------------------------------------------------
【综合评级: 极优 (Ultra Light)】
  深度评估结论:
   1. 极低内存: 常驻工作集稳定在 10 MB 左右，无论多任务还是大型游戏均完全零感知。
   2. 极低算力: Direct2D 脏矩形按需合成，折叠时 GPU 管线完全静默，CPU 占用低于 0.05%。
   3. 零驱动依赖: 原生 Windows PDH + NVML 纯动态延迟绑定，绝不安装底层驱动守护服务。
==========================================================================
```

---

## 🛠️ 交互手势与快捷操作

### 悬浮条通用操作
| 操作动作 | 触发效果 |
| :--- | :--- |
| **鼠标左键按住拖动** | 自由移动监控条至屏幕任意位置 |
| **拖动靠近屏幕边缘** | 自动吸附至左、右、上、下边缘 |
| **鼠标左键单击展开箭头** | 呼出 / 收起 详细展开仪表板（Pro / Lite 各自对应的 Flyout） |
| **鼠标右键单击** | 呼出功能菜单：快速切换主题、样式、开关避让模式、打开设置或退出 |

### Pro 版专属操作
| 操作动作 | 触发效果 |
| :--- | :--- |
| **鼠标悬停各指标段** | 触发微型悬停速览气泡（Hover Peek Tooltip） |
| **单击悬浮条对应指标** | 直接下钻展开并定位到该模块的专属详情卡片 |
| **单击页脚设置齿轮 ⚙️** | 呼出独立图形化偏好设置面板（外观 / 监控 / 常规） |

---

## 🚀 快速上手与编译构建

### 运行环境
- **操作系统**：Windows 10 / 11 (x64)
- **运行依赖**：纯原生 Win32 + Direct2D，**免安装任何额外运行时（Runtime-Free）**。

### 编译构建

#### 推荐方式：一键脚本
使用 Visual Studio 2022 开发人员命令行环境（x64 Native Tools Command Prompt）：

```bat
# 1. 克隆代码仓库
git clone https://github.com/wurh0001/PcInfoToolByWin10.git
cd PcInfoToolByWin10

# 2. 执行一键编译构建
build.bat
```

构建成功后产物将生成在 `build/Release/`：
- `PcInfoTool-Pro.exe`：全功能专业版
- `PcInfoTool-Lite.exe`：极简轻量版

测试工具单独生成在 `build/test-tools/`，其中 `test_performance.exe` 是性能采样工具。构建缓存放在 `build/cache/`，性能日志放在 `build/performance-results/`，截图及传感器报告放在 `build/test-results/`。运行依赖的源文件放在 `build/dependencies/`。

#### 手动 CMake 构建
```powershell
cmake -S . -B build/cache/main -G "Visual Studio 17 2022" -A x64 -DPCINFO_BUILD_VISUAL_TESTS=ON
cmake --build build/cache/main --config Release
```

#### 运行性能测试与离屏回归
```powershell
# 运行 10 秒连续性能采样评估
.\build\test-tools\test_performance.exe 10

# 运行自动化页面离屏渲染回归测试
ctest --test-dir build/cache/main -C Release --output-on-failure
```

---

## 📂 项目工程架构

```text
src/
├─ app/
│  ├─ pro/              # Pro 专业版启动入口与应用上下文
│  └─ lite/             # Lite 轻量版启动入口
├─ core/
│  ├─ collectors/       # 硬件采集器 (CPU/RAM/网络/物理磁盘/NVML原生GPU)
│  ├─ metrics/          # 零堆分配环形历史队列 (RingBuffer) 与快照模型
│  └─ config/           # INI 配置读写持久化引擎
├─ ui/                  # 共享 UI 底层基础设施
│  ├─ rendering/        # Direct2D / DirectWrite 分层无闪烁渲染管线
│  ├─ platform/         # Per-Monitor V2 高 DPI 缩放与亚克力合成
│  ├─ theme/            # 调色盘与全局色彩管理
│  └─ controls/         # 高性能暗黑/亮色自绘上下文菜单
├─ pro/                 # Pro 版专属实现
│  ├─ windows/          # 状态栏、可折叠仪表板、悬停速览窗口
│  ├─ rendering/        # 专业版图标矢量绘制与排版资源
│  └─ interaction/      # 弹簧动画、防遮挡边缘对齐定位
└─ lite/windows/        # Lite 版专属实现 (MiniBarWindow, FlyoutWindow)
```

详细架构约定与维护规则请参考：[代码结构说明文档 (docs/code-structure.md)](docs/code-structure.md)。

---

## 📄 开源许可证

本项目基于 [MIT License](LICENSE) 协议开源。欢迎 Star 🌟、提交 Issue 交流反馈与发起 Pull Request！
