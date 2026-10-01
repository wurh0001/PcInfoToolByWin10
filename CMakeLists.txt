# 要求使用 CMake 3.20 或更高版本。
cmake_minimum_required(VERSION 3.20)

# 创建名为 PcInfoTool 的 C++ 项目。
project(PcInfoTool LANGUAGES CXX)

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

# 递归查找 src 目录下的所有 C++ 源文件和头文件。
# 查找到的文件列表保存在 SRC_FILES 变量中。
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.h"
)

# 生成名为 PcInfoTool.exe 的 Windows GUI 程序。
# WIN32 表示使用 Windows GUI 子系统，对应代码中的 wWinMain 入口函数。
# ${SRC_FILES} 表示将前面找到的所有源文件加入目标。
add_executable(PcInfoTool WIN32 ${SRC_FILES})

# 为 PcInfoTool 链接 Windows 系统库。
# PRIVATE 表示这些库只供 PcInfoTool 这个目标使用。
target_link_libraries(PcInfoTool PRIVATE
    d2d1       # Direct2D：绘制窗口背景、图形和曲线。
    dwrite     # DirectWrite：绘制高质量文字。
    dwmapi     # Desktop Window Manager：桌面窗口管理相关功能。
    iphlpapi   # IP Helper API：读取网络适配器和网络流量信息。
    user32     # Windows 窗口、消息、鼠标、菜单等基础 API。
    gdi32      # GDI：设备上下文、位图等传统图形功能。
    ole32      # COM：支持 COM 初始化和相关 Windows 组件。
    shell32    # Windows Shell：系统 Shell 相关功能。
    pdh        # Performance Data Helper：读取 CPU 等性能计数器。
)

# 编译性能测试工具 test_performance.exe 到 test 目录
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/test_perf.cpp")
    add_executable(test_performance "test/test_perf.cpp")
    target_link_libraries(test_performance PRIVATE psapi)
    set_target_properties(test_performance PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/test"
    )
endif()

