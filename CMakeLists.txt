cmake_minimum_required(VERSION 3.20)
project(PcInfoTool LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PCINFO_BUILD_VISUAL_TESTS "Build offscreen rendering and interaction checks" OFF)
option(PCINFO_BUILD_PERFORMANCE_TESTS "Build the process performance sampler" ON)

include(cmake/SourceLists.cmake)

# Keep the historical executable names and build/Release output layout.
function(pcinfo_configure_target target)
    # Multi-config generators append Release/Debug under this stable product root.
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build")
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    source_group(TREE "${PROJECT_SOURCE_DIR}" FILES ${ARGN})
endfunction()

set(PCINFO_SYSTEM_LIBS d2d1 dwrite dwmapi iphlpapi ws2_32 user32 gdi32 ole32 shell32 pdh)

foreach(target IN ITEMS PcInfoTool-Lite PcInfoTool)
    add_executable(${target} WIN32 ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_LITE_SOURCES} src/app/lite/main.cpp)
    pcinfo_configure_target(${target} ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_LITE_SOURCES} src/app/lite/main.cpp)
    target_link_libraries(${target} PRIVATE ${PCINFO_SYSTEM_LIBS})
endforeach()

add_executable(PcInfoTool-Pro WIN32 ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_PRO_SOURCES} src/app/pro/main.cpp src/app/pro/ProApp.h)
pcinfo_configure_target(PcInfoTool-Pro ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_PRO_SOURCES} src/app/pro/main.cpp src/app/pro/ProApp.h)
target_link_libraries(PcInfoTool-Pro PRIVATE ${PCINFO_SYSTEM_LIBS} windowscodecs)

if(PCINFO_BUILD_VISUAL_TESTS)
    add_executable(PcInfoTool-OverviewRenderTest ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_PRO_SOURCES} tests/rendering/overview_render_test.cpp)
    pcinfo_configure_target(PcInfoTool-OverviewRenderTest ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_PRO_SOURCES} tests/rendering/overview_render_test.cpp)
    target_link_libraries(PcInfoTool-OverviewRenderTest PRIVATE ${PCINFO_SYSTEM_LIBS} windowscodecs)
    enable_testing()
    foreach(mode IN ITEMS cpu gpu memory network disk sensor state-unsupported placement-native motion-native)
        add_test(NAME "render-${mode}" COMMAND PcInfoTool-OverviewRenderTest 1 "${PROJECT_SOURCE_DIR}/build/visual-tests/${mode}.png" "${mode}")
    endforeach()
endif()

if(PCINFO_BUILD_PERFORMANCE_TESTS)
    add_executable(test_performance tests/performance/test_perf.cpp)
    pcinfo_configure_target(test_performance tests/performance/test_perf.cpp)
    target_link_libraries(test_performance PRIVATE psapi)
endif()
