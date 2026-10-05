# 要求使用 CMake 3.20 或更高版本。
cmake_minimum_required(VERSION 3.20)

# 创建名为 PcInfoTool 的 C++ 项目。
project(PcInfoTool LANGUAGES CXX RC)

# 使用 C++20 标准，并要求编译器必须支持 C++20。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 是 Visual Studio 使用的 C++ 编译器。
# /utf-8 让 MSVC 按 UTF-8 读取源代码，避免中文字符串乱码。
if (MSVC)
    add_compile_options(/utf-8)
endif()

# Windows 相关预处理宏：
# UNICODE、_UNICODE：让 Windows API 使用宽字符版本，例如 CreateWindowW。
# NOMINMAX：避免 Windows 头文件定义 min/max 宏，与 std::min/std::max 冲突。
# WIN32_LEAN_AND_MEAN：减少 Windows 头文件包含的内容，降低编译时间。
add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

# ==========================================
# 共享核心源文件 (Core + UI 基础渲染器)
# ==========================================
set(CORE_SOURCES
    "src/core/MetricsCollector.cpp"
    "src/core/PhysicalDiskSampler.cpp"
    "src/core/MetricsCollector.h"
    "src/core/MetricsSnapshot.h"
    "src/core/ConfigManager.h"
    "src/ui/LayeredD2DRenderer.cpp"
    "src/ui/LayeredD2DRenderer.h"
    "src/ui/AcrylicHelper.h"
    "src/ui/DpiHelper.h"
    "src/ui/Theme.h"
)

# 共享系统链接库
set(SYSTEM_LIBS
    d2d1
    dwrite
    dwmapi
    iphlpapi
    ws2_32
    user32
    gdi32
    ole32
    shell32
    pdh
)

# ==========================================
# 目标 1: PcInfoTool-Lite (原版成熟形态：紧凑吸附条 + 345px 概览波形卡片)
# ==========================================
set(LITE_SOURCES
    ${CORE_SOURCES}
    "src/ui/MiniBarWindow.cpp"
    "src/ui/MiniBarWindow.h"
    "src/ui/FlyoutWindow.cpp"
    "src/ui/FlyoutWindow.h"
    "src/ui/CustomContextMenu.cpp"
    "src/ui/CustomContextMenu.h"
    "src/main.cpp"
)

add_executable(PcInfoTool-Lite WIN32 ${LITE_SOURCES})
target_link_libraries(PcInfoTool-Lite PRIVATE ${SYSTEM_LIBS})

# 保持默认别名 PcInfoTool 也生成，方便兼容旧脚本
add_executable(PcInfoTool WIN32 ${LITE_SOURCES})
target_link_libraries(PcInfoTool PRIVATE ${SYSTEM_LIBS})

# ==========================================
# 目标 2: PcInfoTool-Pro (Mac Stats 风格专业版长条面板)
# ==========================================
set(PRO_SOURCES
    ${CORE_SOURCES}
    "src/pro/ProOverviewVisuals.cpp"
    "src/pro/ProOverviewAssets.rc"
    "src/ui/CustomContextMenu.cpp"
    "src/ui/CustomContextMenu.h"
    "src/pro/ProMiniBarWindow.cpp"
    "src/pro/ProMiniBarWindow.h"
    "src/pro/ProMiniBarTooltipWindow.cpp"
    "src/pro/ProMiniBarTooltipWindow.h"
    "src/pro/ProDashboardWindow.cpp"
    "src/pro/ProDashboardWindow.h"
    "src/pro/ProApp.h"
    "src/pro/main_pro.cpp"
)

add_executable(PcInfoTool-Pro WIN32 ${PRO_SOURCES})
target_link_libraries(PcInfoTool-Pro PRIVATE ${SYSTEM_LIBS} windowscodecs)

option(PCINFO_BUILD_VISUAL_TESTS "Build offscreen overview/CPU renderer tests" OFF)
if (PCINFO_BUILD_VISUAL_TESTS)
    set(PRO_TEST_SOURCES ${PRO_SOURCES})
    list(REMOVE_ITEM PRO_TEST_SOURCES "src/pro/main_pro.cpp")
    add_executable(PcInfoTool-OverviewRenderTest ${PRO_TEST_SOURCES} "test/overview_render_test.cpp")
    target_link_libraries(PcInfoTool-OverviewRenderTest PRIVATE ${SYSTEM_LIBS} windowscodecs)
endif()


# 编译性能测试工具 test_performance.exe 到 test 目录
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_perf.cpp")
    add_executable(test_performance "test/test_perf.cpp")
    target_link_libraries(test_performance PRIVATE psapi)
    set_target_properties(test_performance PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/test"
    )
endif()
