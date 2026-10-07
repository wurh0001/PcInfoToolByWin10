# Explicit lists keep unreviewed/experimental files out of production targets.
set(PCINFO_CORE_SOURCES
    src/core/config/ConfigManager.h
    src/core/config/ProGeneralSettings.h
    src/core/metrics/MetricsSnapshot.h
    src/core/metrics/MetricDataState.h
    src/core/collectors/MetricsCollector.cpp
    src/core/collectors/MetricsCollector.h
    src/core/collectors/PhysicalDiskSampler.cpp
    src/core/collectors/PhysicalDiskSampler.h
    src/core/collectors/NativeGpuSensors.h
    src/core/collectors/NativeAmdTemperature.h
    src/core/collectors/NativeAmdTemperature.cpp
    src/core/collectors/NativeLenovoSensors.h
    src/core/collectors/NativeLenovoSensors.cpp
)

set(PCINFO_SHARED_UI_SOURCES
    src/ui/rendering/LayeredD2DRenderer.cpp
    src/ui/rendering/LayeredD2DRenderer.h
    src/ui/platform/AcrylicHelper.h
    src/ui/platform/DpiHelper.h
    src/ui/theme/Theme.h
    src/ui/controls/CustomContextMenu.cpp
    src/ui/controls/CustomContextMenu.h
)

set(PCINFO_LITE_SOURCES
    src/lite/windows/MiniBarWindow.cpp
    src/lite/windows/MiniBarWindow.h
    src/lite/windows/FlyoutWindow.cpp
    src/lite/windows/FlyoutWindow.h
)

set(PCINFO_PRO_SOURCES
    src/pro/windows/ProContextMenu.cpp
    src/pro/windows/ProContextMenu.h
    src/pro/windows/ProMiniBarWindow.cpp
    src/pro/windows/ProMiniBarWindow.h
    src/pro/windows/ProMiniBarTooltipWindow.cpp
    src/pro/windows/ProMiniBarTooltipWindow.h
    src/pro/windows/ProDashboardWindow.cpp
    src/pro/windows/ProDashboardWindow.h
    src/pro/rendering/ProOverviewVisuals.cpp
    src/pro/rendering/ProOverviewVisuals.h
    src/pro/interaction/ProPanelMotion.h
    src/pro/interaction/ProWindowBehavior.h
    src/pro/assets/ProOverviewAssets.rc
)

# D2DContext is a retained, currently unused renderer; it is not added to targets.
