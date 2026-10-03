# PC Info Pro · Figma → C++ / Direct2D 开发交付

## 轻度紧凑化更新 · 2026-10-03

亮色、暗色、交互原型、状态示例与共享指标行已同步收紧纵向空白。指标行由 44 降为 36 DIP，总览面板由 415 降为 355 DIP；悬浮条仍为 380×32，面板宽仍为 420。保留字号、详细图表高度和分组层次，不用整体缩放来制造紧凑感。

当前面板内边距为左右 16、上下 12 DIP；展开详情卡片为左右 12、上下 8。普通页脚高 26，设置按钮 24×24、图标 14；总览微型曲线宽 132 DIP。温度与设置页收紧分组间距。长传感器原型保留滚动，100 个导航目标检查无失效。交付 JSON 的 geometry 已同步，但尚未接入生产绘制代码。

## 设计整理更新 · 2026-10-02

Figma 现分为当前设计、交互原型、开发规范、组件库和 Pro V1 暗色设计五页。暗色页已按当前三张亮色评审板同步紧凑悬浮条、温度、固定与设置布局；早期暗色画板和无引用的旧 MiniBar 主组件已移除。共享 MetricRow 主组件保留。

全工程旧 V0.x 文案已替换为 V1，逻辑尺寸统一为 DIP，容量标签统一为 GiB / MiB，速度继续采用 MB/s。普通面板刷新信息统一移至页脚。原型保持亮色演示；暗色页是同步后的静态设计，不宣称具有独立暗色原型。

[最新暗色设计](https://www.figma.com/design/otKVFJ24sjAu4gdtpvCMM7?node-id=60-1456)

版本：V1 交付基线，2026-10-02。本文是设计与开发建议，不表示 Pro 界面已实现。

## 1. 设计入口与本次补充

- [状态与恢复画板](https://www.figma.com/design/otKVFJ24sjAu4gdtpvCMM7?node-id=45-629)：启动采样、温度不可用、网络断开、高温高负载、采样过期、减少透明度六种状态。
- [窗口、交互与开发契约](https://www.figma.com/design/otKVFJ24sjAu4gdtpvCMM7?node-id=45-792)：尺寸、DPI、展开方向、固定与置顶、控件状态、设备选择、验收顺序。
- [亮色交互评审](https://www.figma.com/design/otKVFJ24sjAu4gdtpvCMM7?node-id=33-1190)。
- [亮色原型起点](https://www.figma.com/design/otKVFJ24sjAu4gdtpvCMM7?node-id=37-1533)。
- [悬浮条主组件](https://www.figma.com/design/otKVFJ24sjAu4gdtpvCMM7?node-id=24-915)。

深浅色共 27 个普通标题栏已统一为“固定”胶囊与独立关闭按钮；固定状态使用“已固定”。原有导航链接保留。
本次新增内容是可编辑图层，不是整页截图。新规范中的异常状态和设备菜单是静态验收示例，不宣称已经具有真实采样、阈值判断或键盘行为。

现有原型的固定跳转进入固定总览，未模拟每个模块的独立固定状态。实现时必须保留当前模块、设备和滚动位置。长面板原型滚动与真实软件固定标题/页脚的要求也有差别，以开发契约为准。

## 2. 推荐的协作方式

采用“版本化设计基线 + 令牌快照 + 行为契约 + 实机验收”。Figma 负责视觉与组件定义，C++ 负责窗口、采集、绘制与性能。不能把 Figma 导出的 CSS 当作 Win32 的实现。

1. 确认一个发布基线，给相关画板命名 V1；之后的探索放在新版本，避免开发中途跟着试验稿漂移。
2. 每次交付从 Figma 导出同一份颜色、间距、圆角、字体与组件 ID；附带设计变更摘要。
3. 当前快照为同目录的 pro-v1-design-tokens.json。颜色、间距、圆角与文字样式来自真实 Figma 变量；geometry 是本次规范值。此文件尚未接入编译。
4. 实施阶段再添加 JSON → C++ 头文件生成器。生成文件不手改；Pro 使用独立令牌映射，暂不覆盖 Lite 的现有 Theme.h。
5. 对每个组件保存 Figma node ID、代码入口、状态枚举与数据字段的映射。代码接口稳定后再发布 Code Connect。
6. 用相同假数据、相同 DPI 截取 Figma 与软件的局部图进行复核；布局/行为错误优先于细微材质差异。

## 3. 组件与代码的对接范围

| 设计内容 | 当前项目入口 | 开发工作 |
|---|---|---|
| 悬浮条与命中区域 | src/ui/MiniBarWindow.cpp | Pro 自己的紧凑布局；CPU 占用与温度分别点击 |
| 展开面板、失焦与滚动 | src/ui/FlyoutWindow.cpp | 可参考现有窗口机制，Pro 采用独立状态模型 |
| 颜色与材质 | src/ui/Theme.h、AcrylicHelper.h | 建立 Pro 令牌适配，验证系统背景兼容性 |
| 文字与绘制 | LayeredD2DRenderer.cpp、D2DContext.cpp | 统一字体、字重、行高、DPI 与绘制坐标 |
| 快照与历史 | src/core/MetricsSnapshot.h | 时间戳、有效性、设备 ID 与可选传感器字段 |
| Pro 主入口 | src/pro/ProApp.h | 目前仍为 MessageBox 骨架，尚无实际 Pro 组件接口 |

以上是现有入口与推荐职责，不是已完成的代码映射。不要为不存在的组件接口生成看似可用的调用示例。

建议实现组件：MiniBar、MetricRow、HeaderActions、HistoryChart、DeviceSelector、StatusBadge、PreferencesPanel。绘制边界与命中边界分开：可保持 32 DIP 条高，同时用清晰的可点击分区和 tooltip；不能扩大重叠区域抢占相邻指标。

## 4. 尺寸、字体与窗口行为

- Figma 坐标作为 DIP：悬浮条 380×32，面板宽 420，内边距左右 16 / 上下 12，条与面板间隔 8；指标行高 36。
- HWND 物理边界使用 round(DIP × dpi / 96)；Direct2D 的逻辑绘制坐标避免重复缩放。具体取决于实际渲染目标 DPI 设置。
- 125% 条为 475×40 px，150% 为 570×48，200% 为 760×64。
- 面板高度不超过当前显示器工作区高度减 32 DIP；标题/页脚固定，仅中部内容滚动。常规高度由实际内容决定。
- 靠近下边缘向上展开，靠近右边缘右对齐。计算每个显示器的工作区，不假定主屏或全屏范围。处理跨屏 DPI、任务栏与工作区变化。
- 未固定失焦关闭，固定后保持；Esc 关闭面板，不退出程序；固定不等于置顶。固定与取消固定均保留选中模块、设备和滚动位置。
- 标题关闭只收起面板；程序退出应在独立菜单/设置入口，不让“×”产生歧义。

设计字体当前为 Noto Sans SC；现有 D2DContext 使用 Segoe UI，LayeredD2DRenderer 使用 MiSans。字体度量不同，必须在开发试做阶段统一。推荐先检验生产字体可用性及分发许可，再把 Figma 同步到生产字体；若选择回退字体，应重新验收省略、对齐与中文行高，不承诺自动像素一致。

文字样式已在 JSON 导出；悬浮条的 9/11/12 DIP 是局部微型字号，不由 13 DIP Body 样式强行替换。数字使用稳定宽度布局，预留 100%、三位温度与网速较大数值的空间；通过换单位/小数位控制长度，而不是挤高悬浮条。

## 5. 材质：把视觉意图转换为 Windows 能力

Figma GLASS 的折射、色散、深度等不是 Direct2D / DWM 可直接导入的参数。设计保留原生 GLASS，生产端以可读性、兼容性和低占用为验收目标，先做一条悬浮条和一个面板的材质验证。

- 基础组合：系统背景模糊 + 半透明白色 tint + 细白描边 + 轻阴影；文字和曲线保持不透明。
- 亮色面板参考 tint alpha 0.40，描边 alpha 0.85，圆角 20 DIP；控件采用更局部的透明层次。参数是视觉起点，需要实机调校，不是跨渲染器的等价值。
- Figma 的 panel/surface/hover 变量是白色，透明度在 paint / effect 上；不能直接把白色令牌当成最终不透明玻璃底色。
- 当前 AcrylicHelper 动态调用 SetWindowCompositionAttribute 的 accent 路径。需要实机验证它与当前 layered 窗口/绘制路径的兼容性；不在本阶段替换现有后端。
- 可研究 Windows 11 官方 DWM 系统背景；DWM_SYSTEMBACKDROP_TYPE 的最低支持为 build 22621。不能据此推断当前窗口结构无需改动就支持。
- 系统透明效果关闭、高对比度/辅助功能要求、后台兼容失败或用户选择减少透明度时，采用不透明底色、描边和轻阴影；不以采样失败冒充材质失败。
- 先验证深色/浅色/高纹理壁纸和文字对比，再决定是否值得添加自定义折射。不要预先把 Lite 的约 9 MB 工作集目标当作 Pro 已达到的事实。

## 6. 数据契约与目前缺口

MetricsSnapshot 现有 CPU 总体/逻辑核心、内存、GPU 使用率、网络速率、磁盘容量字段可作为首轮输入。当前没有温度、频率、功耗、风扇、VRAM、Top 进程、磁盘吞吐的对应快照字段；设计稿中的示例数字不是采集能力证明。

MetricHistory 目前是固定 60 个 float 的环形队列，无时间戳和有效性。若修改采样间隔，60 点不等于 60 秒。建议增加时间戳及质量状态，用时间筛选最近 60 秒；选择最大采样率后明确缓冲容量。保留低分配实现，但不能牺牲真实时间含义。

每个指标区分：Loading、Valid、Unavailable、Error、Stale；告警等级另存为 Normal/Warning/Critical，不与质量状态混为一谈。可选数值为空与有效零值分开。设备使用稳定 ID 和显示名称；设备失联不自动把历史解释成另一设备的数据。

探针显示真实时间、数值与单位；缺失段留空，不连接跨越采集失败的曲线。超过三个采样周期未更新标记过期，显示最后有效值与“几秒前”。联网且无流量可以显示 0；传感器不可用显示 —；确认网络断开且有效计数归零才显示断开状态下的 0。

高温示例阈值 85°C、连续三次触发，低于 80°C 连续五次恢复，是可配置的 UI 演示规则，不是通用硬件安全结论；应由传感器能力和用户配置决定。高负载阈值与温度阈值独立。

统一单位：设计的 MB/s 表示十进制 1,000,000 B/s；GiB 表示 2^30 bytes，MiB 表示 2^20 bytes。Figma 容量标签已统一为 GiB / MiB。现有 FormatSpeed 使用 1024 分级但标签 K/s、M/s，应在实施时同步数据换算和标签，不能只改算法不改 UI。

## 7. Code Connect / Dev Mode 的使用时机

官方当前提供 framework-agnostic 的 Code Connect template files，可为任何语言/框架配置代码片段，因此 C++ 并非完全不能接入。它适合把真实组件 API 与 Figma 组件对应，并让 Dev Mode / MCP 返回项目自己的用法；它不是 Win32 窗口、采集、玻璃材质和输入消息的自动实现器。

本阶段不安装 CLI、不发布模板。原因是 Pro 目前仍为骨架，组件接口尚未稳定；账号的发布权限与计划资格也尚未验证。推荐先完成 MiniBar / MetricRow / HistoryChart 实际接口，再为三者做一个小范围映射。仅做设计读取时，MCP + 节点链接 + 本地令牌快照已足够起步。

## 8. 首轮开发验收

1. 用确定性的假数据实现悬浮条 → 总览 → CPU 详情。温度可用与不可用均可演示，不依赖本机硬件。
2. 六种异常/恢复状态、hover/pressed/focus/disabled、探针和设备选择覆盖。
3. 检查 100/125/150/200% DPI，跨屏与靠边展开，不裁字、不越过工作区。
4. 检查固定/取消固定、失焦、Esc、关闭、滚动位置；设置入口与选项能持久化。
5. 检查深浅壁纸和实色回退。以约定阈值测量 CPU、工作集、采样耗时、绘制耗时；目标另行制定，不能只凭截图验收。
6. 再逐项接入采集后端，完善 GPU/磁盘/传感器与进程模块。保持 Lite 的用户现有修改和行为不受影响。

## 参考依据

- [Figma Code Connect template files](https://developers.figma.com/docs/code-connect/template-files/)
- [Microsoft Direct2D and High-DPI](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-high-dpi)
- [Microsoft DWM_SYSTEMBACKDROP_TYPE](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type)

本次只新增设计交付文件与修改 Figma，没有改动已有软件源码。后续实施可以将本说明作为逐项验收清单。
