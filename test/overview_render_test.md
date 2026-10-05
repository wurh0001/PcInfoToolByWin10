# 纯白完整长条：概览与六项展开页视觉检查

这项测试调用生产环境的 `DrawWhiteOverview`、`DrawWhiteCpuDetails`、`DrawWhiteNetworkDetails` 和 `DrawWhiteBar`，绘制到独立内存位图并导出 PNG；不采集桌面、不操作其他窗口。示例数值只存在于测试文件，生产程序继续使用采集器。

```powershell
cmake -S . -B build -DPCINFO_BUILD_VISUAL_TESTS=ON
cmake --build build --config Release --target PcInfoTool-Pro PcInfoTool-OverviewRenderTest
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/overview-render-100.png
build/Release/PcInfoTool-OverviewRenderTest.exe 1.5 test/cpu-render-150.png cpu
build/Release/PcInfoTool-OverviewRenderTest.exe 2 test/cpu-render-200.png cpu
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/cpu-render-24cores.png cpu 24
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/network-render-100.png network
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/network-render-left.png network-left
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/network-render-right.png network-right
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/network-render-empty.png network 0
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/gpu-render-100.png gpu
build/Release/PcInfoTool-OverviewRenderTest.exe 2 test/gpu-render-200.png gpu
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/gpu-render-missing.png gpu-missing
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/memory-render-100.png memory
build/Release/PcInfoTool-OverviewRenderTest.exe 2 test/memory-render-200.png memory
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/memory-render-missing.png memory-missing
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/disk-render-100.png disk
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/disk-render-missing.png disk-missing
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/sensor-render-100.png sensor
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/sensor-render-missing.png sensor-missing
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/sensor-render-fahrenheit.png sensor-fahrenheit
build/Release/PcInfoTool-OverviewRenderTest.exe 1 test/sensor-render-gaps.png sensor-gaps
```

- 检查六行、按钮和悬浮条命中矩形；原始图标资源能成功解码；标题与内存读数没有超出布局槽位。
- 精确字体为 Noto Sans SC。检查器在字体缺失时失败；生产程序按 MiSans → Microsoft YaHei UI → Segoe UI 降级，降级后不能保证文字完全一致。
- 380×32 悬浮条、360×284 概览面板、16 DIP 间距是可见布局尺寸。CPU 展开页在 16 个逻辑处理器时为 360×637，详情卡为 338×361；每超过两行的核心行增加 34 DIP，后续模块和页脚同步下移。额外的透明阴影留白不计入定位和命中区域。
- 图标来自 Figma 原资源，PNG 是原 SVG 的 4×栅格化版本，嵌入 exe，无需联网加载。曲线来自实时历史数据，因此不要求与示例曲线逐点一致。
- 范围是纯白主题的完整长条概览和六项展开页；六项展开的紧凑胶囊和固定状态复用校准内容，CPU／内存／网络／温度的单项直达也已校准。未展开的紧凑／固定概览仍使用既有路径。
- 内存面板为 360×651 DIP，详情卡 338×375，标题为收起命中区域。容量统一为 GiB，缺失容量、提交限制、历史和进程显示状态文案，不补示例值。组成有效性及其他容器见下方“内存详情检查”；采集器原有估算字段不再被纯白内存详情当作有效组成展示。
- GPU 面板为 360×623 DIP，详情卡 338×347，标题为收起命中区域，不重复显示 GPU 长条。型号和进程名称过长时省略；显存条按真实比例显示，二进制容量明确标为 GiB。缺失型号、温度、频率、功耗和显存不补示例值，空进程与空历史提供状态文案。采集器原有的部分 GPU 估算值不属于本次视觉校准的修改范围。
- 网络面板为 360×565 DIP，详情卡 338×289，图表宽 314。下载与上传使用同一时间索引、同一纵轴比例；悬停浮层在左右边缘自动避让，空历史显示收集中。其他容器、数据口径及接口同步限制见下方“网络详情检查”。
- CPU 水桶为 24×28 DIP、横向间距 12、行距 6；按真实逻辑处理器数每行最多 8 个、各行居中。填充高度为 26×利用率，透明度为 0.18+0.40×利用率，0% 完全空，100% 填满内部且编号仍居中。
- CPU 完整长条为 360×637、紧凑胶囊为 360×559（采用 Static / CPU 视觉基准，原型 549 仅作状态参考），单项直达为 360×373 DIP。16 核时内容分别为含标题的 338×361 和无重复标题的 338×335；多于两行时每行额外增加 34 DIP，缺少核心列表时显示不可用，不填示例核心。
- CPU 检查模式：`cpu-compact`、`cpu-direct`、`cpu-direct-focus`、`cpu-direct-hover`、`cpu-missing`、`cpu-direct-missing`、`cpu-wide`、`cpu-direct-wide`、`cpu-gaps`、`cpu-viewport`、`cpu-direct-viewport`。可用最后一个参数指定 0／1／8／9／24／64／128 核。焦点／速览示例选择核心 3，显示真实 fixture 中的 34%；主程序没有示例值兜底。
- 核心编号使用 Noto Sans SC 固定数字宽度；检查器验证 `111` 与 `888` 的宽度相等。24×28 桶的中心命中、水平间距、Tab／Shift+Tab／Home／上下箭头也有自动检查。悬停／聚焦浮层 154×48，展示当前利用率与核心索引；无效核心值显示破折号。
- CPU 可见窗口按锚点所在显示器工作区约束高度；沿用已有展开方向，正文裁切与滚轮滚动，标题栏及页脚不随正文移动，键盘聚焦会滚动到对应核心。`*-viewport` 模式模拟 400 DIP 高度与滚动位置。真实跨屏、缩放变化和点击未在离屏图中验证。
- 本次只统一 CPU 的静态终点与核心交互，没有新增 300 ms 模块形变动画；动画规范中的完整过渡仍需单独实现和验收。采集器的采样有效性标记、时间戳及部分硬件温度估算问题不在本次 UI 校准范围内。
- 离屏通过不等于桌面交互通过。还需在真实程序中手动检查悬停、展开/收起、固定、关闭、设置入口和多显示器 DPI 行为。
- 悬停速览模式：`hover-cpu`、`hover-temp`、`hover-memory`、`hover-network`；对应尺寸为 154/136/185/144×48 DIP，相对条左侧偏移 0/44/90/216 DIP，底部间距 4 DIP（温度 4.5 DIP）。字体 Noto Sans SC，标签 10/14 Regular，数值 16/22 Medium，内容栈居中从 y=4 开始，分隔线高 28。每侧 24 DIP 透明阴影留白不计入可见尺寸。
- 所有速览支持 `-missing` 测试；CPU／内存／网络支持 `-wide`，温度支持 `-fahrenheit`。例如 `build/Release/PcInfoTool-OverviewRenderTest.exe 1 build/visual-tests/hover/hover-temp-1.png hover-temp`。检查合法零值、满内存可用量为零、缺失温度与空历史不伪造峰值、MB/s 十进制换算、华氏转换，以及负坐标显示器工作区边缘定位。
- 原指标停留 250 ms 后显示速览，跨指标重新计时，离开／拖动／点击立即隐藏；详情可见时不弹出速览。速览为不激活、鼠标穿透的只读窗口，点击原指标才进入详情。已接入悬浮条 `WM_DPICHANGED` 重新缩放；实际跨屏拖动和快速悬停仍需桌面验收。
- 磁盘面板为 360×616 DIP，详情卡 338×340；读取蓝色、写入金色，共用时间轴与比例。历史单位 MB/s，当前读数由 B/s 转为十进制 MB/s，卷容量按 GiB。物理磁盘切换与统计有效性说明见下方“磁盘详情检查”。
- 传感器面板为 360×664 DIP，详情卡 338×388；温度范围从有效历史样本计算，零值与非有限温度不画入曲线，缺口分段绘制。摄氏／华氏读数与范围同步切换，风扇负数显示未提供、0 RPM 保留为零转速。页面不再补示例值，但采集器原有温度、功耗和转速估算／默认数据没有在此视觉任务中改为硬件读取。

### 图片输出与 GPU 检查

历史 Release 图片已移动至 `build/visual-tests/cpu`、`hover`、`regression`。程序与测试 exe 仍放在 `build/Release`。检查器会自动把直接输出到 exe 同目录的图片重定向至 `build/visual-tests/<分类>`，并创建输出目录、打印实际路径；不要把测试图片作为运行资源分发。

```powershell
build/Release/PcInfoTool-OverviewRenderTest.exe 1 build/visual-tests/gpu/full-100.png gpu
build/Release/PcInfoTool-OverviewRenderTest.exe 1.5 build/visual-tests/gpu/compact-150.png gpu-compact
build/Release/PcInfoTool-OverviewRenderTest.exe 2 build/visual-tests/gpu/wide-200.png gpu-compact-wide
```

GPU 完整长条和紧凑胶囊（含固定状态）共用校准绘制，分别为 360×623、360×545 DIP，详情卡 338×347。紧凑采用 `223:2179` 静态视觉基准；原型 `238:2979` 的 535 高度仅作交互状态参考，不混用尺寸。检查模式：`gpu`、`gpu-compact`、`gpu-pinned`、`gpu-compact-pinned`、`gpu-missing`、`gpu-compact-missing`、`gpu-wide`、`gpu-compact-wide`、`gpu-gaps`。历史按 60 个槽位右对齐，非有限或越界样本断线，合法 0% 保留。未知值不补示例数据；采集器未提供有效性标记的零值仍无法区分空闲和未采到。未新增 GPU 动画或独立直达页面。

测试 PNG 已由 `.gitignore` 排除；不增加详细 Figma 导出资料。

### 磁盘详情检查

完整长条／固定页 360×616、紧凑胶囊／固定页 360×538 DIP（静态 `223:2367`，原型 528 只作状态参考）。详情卡 338×340，完整 y=158、紧凑 y=118；标题为收起命中区域，无重复磁盘长条。紧凑上方 CPU/GPU 双列、内存整行，下方网络/传感器双列。沿用嵌入的原始 Disclosure、按钮和设置图标；没有新增文档外的独立磁盘直达页或形变动画。

模式：`disk`、`disk-compact`、`disk-pinned`、`disk-compact-pinned`、`disk-missing`、`disk-compact-missing`、`disk-wide`、`disk-compact-wide`、`disk-gaps`、`disk-one`、`disk-many`、`disk-full`、`disk-invalid`、`disk-unsupported`。图片输出至 `build/visual-tests/disk`。

两条历史使用同一纵轴、尾对齐至60槽位；无效样本断线，合法零速率保留，空历史显示收集中。大速率使用十进制 GB/s，长卷标签为容量读数让位；满卷／空卷不保留错误间隙，容量未知或 free>total 显示破折号及中性条。最多展示两个卷，多卷显示总数。新增 `diskStatisticsValid`：现有采集器活动时间／响应时间占位值标为无效，纯白详情显示破折号，QA fixture 显式提供有效统计。

选择区域现已接入真实物理磁盘，菜单、身份及统计口径见下方“磁盘选择器检查”。无物理设备快照时显示汇总与设备未提供，而不是假型号／列表。实际桌面点击、跨屏及动画尚需独立验收。

### 磁盘选择器检查

复用完整／紧凑磁盘详情。选择器菜单宽256、三项时高128 DIP，圆角10、内边距4、描边1，阴影0/4/14/0、alpha12%；选项248×38，行间距2，标题12/18 Medium、描述10/14 Regular，选中底色EDF3FB。菜单位于名称下方，完整坐标(24,211)、紧凑(24,171)；选中标记使用Figma `201:905` 原始16×16导出图片，嵌入资源212。关闭时不改变详情卡与面板高度。

模式：`disk-selector`、`disk-selector-open`、`disk-selector-compact-open`、`disk-selector-second`、`disk-selector-single-open`、`disk-selector-many-open`、`disk-selector-long-open`、`disk-selector-unavailable-open`、`disk-selector-reorder-open`、`disk-selector-removed-open`、`disk-selector-empty`。四档缩放共44项，图片输出 `build/visual-tests/disk-selector`。检查尺寸、点击选择、键盘End/Enter、Esc优先关闭菜单、外部点击不穿透、重排保持设备ID与移除后回退。超过5项限制菜单高度，方向键／滚轮滚动，不增加主面板高度。

采集器通过 [SetupDiGetDeviceInstanceIdW](https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceinstanceidw) 绑定PnP设备实例ID，磁盘编号仅用于显示。逐盘计数使用 [DISK_PERFORMANCE](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ns-winioctl-disk_performance)；读写速率由计数差与单调时钟间隔计算，活动时间由空闲时间差计算，平均响应按读写完成次数加权，0次I/O显示0 ms。首次样本／计数回退／查询失败不补零，历史用NaN断线。库存每10秒刷新；设备移除后清除基线，当前选择失效时回退到第一个可用设备，无设备关闭菜单。ID在本次Windows设备实例身份范围内稳定，不承诺更换端口或系统重装后保持同一ID。

卷通过磁盘extents映射；跨盘卷标注“跨盘卷”，容量是整个卷容量，不当成该物理盘独占容量。当前仅映射有盘符的本地固定卷；无盘符分区／仅目录挂载卷尚未展示。详情当前最多显示前2个卷，更多时标注总数。设备型号、总容量和接口来自查询，查询失败显示未知，不补三星型号。所有设备统计有效时才显示完整汇总曲线；部分设备统计不可用时不把部分合计伪装为完整汇总。采集器原有模拟磁盘波形已移除。

`disk-live` 模式执行4秒只读采集并输出设备数、有效统计数和卷映射数，不导出真实磁盘截图或身份信息。本机检查结果为2个物理设备、2组有效吞吐统计、3个卷映射。此结果不代表所有设备驱动都支持统计；性能数据可用性受驱动／权限影响。未写设备、未启用额外性能计数器或要求管理员运行；实际拔插、多显示器、菜单动画与桌面交互仍需验收。

### 网络详情检查

完整长条／固定页 360×565、紧凑胶囊／固定页 360×487（采用静态 `223:2558`，原型 477 仅作状态参考）、单项直达 360×301 DIP。共享正文，直达不重复内部标题／灰色卡片／概览页脚；紧凑上方 CPU/GPU、内存/磁盘两行，下方传感器整行。图表命中框完整 y=285、紧凑 y=209、直达 y=103，宽314。下载／上传使用同一纵轴和尾对齐时间索引；不足60样本时靠右，异常值分段绘制，无样本不显示假0和假峰值。探针缺失读数为破折号，浮层限制在图表内，离开窗口立即隐藏。

模式：`network`、`network-compact`、`network-pinned`、`network-compact-pinned`、`network-direct`、`network-plain`、`network-left`、`network-right`、`network-missing`、`network-compact-missing`、`network-direct-missing`、`network-wide`、`network-compact-wide`、`network-direct-wide`、`network-gaps`。最后参数可指定0/1/5/60样本。测试图片放 `build/visual-tests/network`。

速率采用十进制 MB/s，大读数切换 GB/s；接口及IPv6过长省略，累计流量采用十进制容量。现有采集器仍汇总所有活动硬件接口，而接口名/IP来自首次发现的连接，尚未统一为逐接口快照；中断、重新连接、逐样本时间戳与有效性尚未提供，当前探针时间按末次接收时间及1秒间隔推算，不能声称精确设备同步或完全实现数据状态规范。本轮未新增模块形变动画；跨屏与真实鼠标交互需桌面验收。极端速率在悬浮条上的截断是既有问题，不在网络详情校准范围内。

### 内存详情检查

完整长条／固定页 360×651，紧凑胶囊／固定页 360×573（静态 `184:1787`，原型 563 仅作状态参考），单项直达 360×387 DIP。共享内容 314 DIP 宽，直达不重复显示内部标题／灰色详情卡／概览页脚。所有容量使用 GiB；利用率由已用与总量计算，大容量主值适当缩小字号，进程名称让位于右侧读数并省略。

`memory`、`memory-compact`、`memory-pinned`、`memory-compact-pinned`、`memory-direct` 支持静态检查；还可使用 `memory-missing`、`memory-compact-missing`、`memory-direct-missing`、`memory-wide`、`memory-compact-wide`、`memory-direct-wide`、`memory-full`、`memory-gaps`、`memory-unsupported`。图片放 `build/visual-tests/memory`。

历史按 60 个槽位右对齐，NaN／越界样本断线，不把缺失补零。新增 `memoryCompositionValid` 契约：只有真实测量且互不重叠、合计等于总量的正在使用／待机／空闲字段才能绘制三段组成。现有采集器的 SystemCache 或按比例估算不满足契约，生产页缓存／空闲显示破折号，组成条仅区分已用与剩余，并注明“组成未提供”。测试 fixture 明确提供有效组成，仅用于离屏 QA。尚未接入真正的待机列表读取，也未增加模块形变动画。

### 传感器详情检查

完整长条／固定页 360×664，紧凑胶囊／固定页 360×586 DIP（静态 `223:2738`，原型 576 仅作交互参考）。详情卡 338×388，完整 y=230、紧凑 y=152；标题可收起，不重复传感器长条。紧凑上方 CPU/GPU、内存/磁盘双列，网络整行，无下方剩余指标。

模式：`sensor`、`sensor-compact`、`sensor-pinned`、`sensor-compact-pinned`、`sensor-missing`、`sensor-compact-missing`、`sensor-fahrenheit`、`sensor-compact-fahrenheit`、`sensor-gaps`、`sensor-compact-gaps`、`sensor-wide`、`sensor-compact-wide`、`sensor-invalid`、`sensor-zero`。四档缩放 1／1.25／1.5／2，共 56 项检查；图片输出到 `build/visual-tests/sensor`。

温度有效范围为有限且大于 0、不超过 200°C；零值沿用当前快照的未提供约定。历史按 60 槽位靠右显示，无效样本断线，范围仅计算有效历史。摄氏／华氏读数及范围同步转换，较长范围缩小字号。负转速标注未提供读数，合法 0 RPM 保留。采集器已有温度、功耗、频率及转速估算／默认值尚未替换为硬件读取；本轮未新增独立传感器直达页或形变动画，实际桌面交互仍需验收。

### 温度直达检查

纯白 `236:2468` 单项面板 360×289 DIP，圆角20；40 DIP透明阴影留白不计入定位及命中。标题 y=5，当前温度 y=33，更新说明 y=79，338 DIP宽历史 y=100，时间轴 y=157，四行关联指标 y=178/199/220/241，底部分隔线 y=262，操作行 y=267。复用 Noto Sans SC 与公共绘制资源，不重复概览标题、内部卡片或传感器选择入口。

模式：`temperature`、`temperature-probe`、`temperature-left`、`temperature-right`、`temperature-missing`、`temperature-fahrenheit`、`temperature-wide`、`temperature-gaps`、`temperature-invalid`、`temperature-zero`、`temperature-hot`、`temperature-off`、`temperature-interval`，四档缩放共52项。图片输出 `build/visual-tests/temperature`。检查面板、图表和底部命中尺寸；关闭提醒后的点击将阈值设为75°C，并触发设置保存回调。

有效温度沿用传感器约定，统计只计算有效历史；缺失显示破折号，不填58°C、假型号或假曲线。历史尾对齐60槽位并断开无效值，探针仅在悬停时显示，左右边缘限制在图表内，离开窗口隐藏；探针落在未提供样本上时显示状态，不取附近值。华氏转换同时作用于当前值、探针、统计及提醒阈值。长型号显式省略，三位数主值给状态标签让位；高温及未提供使用不同文字，不只改颜色。0 RPM保留，负转速注明未提供。

“查看全部传感器”沿用主面板传感器入口。提醒沿用设置页的85→90→95→关闭→75→80→85循环并持久化，尚未新增下拉菜单。时间标签按当前采样设置估算；历史没有逐样本时间戳，更改采样间隔后的旧样本时间不能保证精确。采集器硬件温度及其他估算数据未在本轮替换，数据状态完整契约、真实多显示器交互和动画仍需后续验收。

### 设置外观检查

`248:3779`／`248:3900` 共用360×201 DIP纯白面板，圆角20、描边DDE3EB1、阴影0/8/24、alpha7%。标题(11,5)、完成(324,8.5)、标签栏y=33高30，三个标签宽107.333、间距8；外观标签y=67，主题按钮y=89高29、宽80/77/80/80、间距6；布局标题y=122，选项y=146高29、宽166、间距6；说明y=181高15。采用Noto Sans SC，主题与布局选中底色EDF3FB、蓝色1 DIP内描边，无额外常驻指标或显示提醒区域。

模式：`appearance`、`appearance-compact`、`appearance-glass`、`appearance-glass-minimum`、`appearance-glass-maximum`、`appearance-dark`、`appearance-system`、`appearance-save-failed`，四档缩放共32项；图片输出 `build/visual-tests/appearance`。检查主题／布局通知、纯白不改变保存的透明度、隐藏控件不可点击、10%／100%端点、切标签后重新计算高度，以及配置往返和保留其他INI区段。配置测试只创建并删除自己的临时文件，不修改用户配置。

玻璃主题的透明度区域是实现扩展：说明y=203、滑块命中y=227..245，面板高259；不把此高度混入201 DIP纯白设计基准。只在明确选择“亮色玻璃”时显示，深色、纯白、跟随系统均隐藏。滑块有效范围10%..100%，继续沿用既有分层材质alpha映射，不表示所有材质表面的实际alpha都等于滑块百分比。

主题、布局、透明度保存在exe同目录 `config.ini` 的独立 `ProAppearance` 区段，并在Pro启动时读取；不覆盖Lite的Appearance区段。写入失败时显示未保存提示。新打开设置默认外观标签，完成关闭面板；监控与常规页仍沿用原来的渲染，后续功能文档继续校准。外观页深色配色已适配，但玻璃／深色的其他详情页仍沿用原实现，不代表已完成全主题视觉复刻。

跟随系统读取Windows应用亮暗偏好，不再固定解析为纯白；收到设置／主题变更消息时刷新。系统深色情况下仍进入既有非纯白详情绘制路径，尚未单独校准；真实系统主题切换、跨屏、材质效果、键盘可访问性和动画需桌面验收。
