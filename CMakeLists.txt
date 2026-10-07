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
    # Optional locally verified signed PawnIO module. No downloads or driver installation.
    set(pawnio_module "${PROJECT_SOURCE_DIR}/build/sensor-probe/pawnio-modules-0.2.11/AMDFamily17.bin")
    if(EXISTS "${pawnio_module}")
        file(SHA256 "${pawnio_module}" pawnio_hash)
        if(NOT pawnio_hash STREQUAL "dae74615761b78bdf064dfb3e136252ddcc6fc727d88f14738d0e5800d427a91")
            message(FATAL_ERROR "Unexpected PawnIO module hash")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/sensors"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${pawnio_module}" "$<TARGET_FILE_DIR:${target}>/sensors/AMDFamily17.bin")
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
    add_executable(PcInfoTool-ContextMenuTest tests/rendering/context_menu_test.cpp src/pro/windows/ProContextMenu.cpp src/pro/rendering/ProOverviewVisuals.cpp src/pro/assets/ProOverviewAssets.rc src/ui/rendering/LayeredD2DRenderer.cpp)
    pcinfo_configure_target(PcInfoTool-ContextMenuTest tests/rendering/context_menu_test.cpp src/pro/windows/ProContextMenu.cpp src/pro/rendering/ProOverviewVisuals.cpp src/pro/assets/ProOverviewAssets.rc src/ui/rendering/LayeredD2DRenderer.cpp)
    target_link_libraries(PcInfoTool-ContextMenuTest PRIVATE ${PCINFO_SYSTEM_LIBS} windowscodecs)
    add_executable(PcInfoTool-OverviewRenderTest ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_PRO_SOURCES} tests/rendering/overview_render_test.cpp)
    pcinfo_configure_target(PcInfoTool-OverviewRenderTest ${PCINFO_CORE_SOURCES} ${PCINFO_SHARED_UI_SOURCES} ${PCINFO_PRO_SOURCES} tests/rendering/overview_render_test.cpp)
    target_link_libraries(PcInfoTool-OverviewRenderTest PRIVATE ${PCINFO_SYSTEM_LIBS} windowscodecs)
    enable_testing()
    add_test(NAME context-menu COMMAND PcInfoTool-ContextMenuTest "${PROJECT_SOURCE_DIR}/build/visual-tests")
    set_tests_properties(context-menu PROPERTIES TIMEOUT 20)
    foreach(mode IN ITEMS cpu gpu memory network disk sensor state-unsupported placement-native motion-native)
        add_test(NAME "render-${mode}" COMMAND PcInfoTool-OverviewRenderTest 1 "${PROJECT_SOURCE_DIR}/build/visual-tests/${mode}.png" "${mode}")
    endforeach()
endif()

if(PCINFO_BUILD_PERFORMANCE_TESTS)
    add_executable(test_performance tests/performance/test_perf.cpp)
    pcinfo_configure_target(test_performance tests/performance/test_perf.cpp)
    target_link_libraries(test_performance PRIVATE psapi)
endif()
