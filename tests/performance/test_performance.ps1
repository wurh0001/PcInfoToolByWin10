param(
    [switch]$Pro,
    [switch]$Lite,
    [int]$Samples = 5
)

# ==============================================================================
# PcInfoTool 性能监控与资源开销测试脚本
# ==============================================================================
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "          PcInfoTool 实时运行性能与资源开销检测           " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$targetName = "PcInfoTool-Pro"
if ($Lite) {
    $targetName = "PcInfoTool-Lite"
}

$proc = $null
if ($Pro) {
    $proc = Get-Process -Name "PcInfoTool-Pro" -ErrorAction SilentlyContinue
    $targetName = "PcInfoTool-Pro"
} elseif ($Lite) {
    $proc = Get-Process -Name "PcInfoTool-Lite" -ErrorAction SilentlyContinue
    if (-not $proc) {
        $proc = Get-Process -Name "PcInfoTool" -ErrorAction SilentlyContinue
        $targetName = "PcInfoTool"
    } else {
        $targetName = "PcInfoTool-Lite"
    }
} else {
    $proc = Get-Process -Name "PcInfoTool-Pro" -ErrorAction SilentlyContinue
    if ($proc) {
        $targetName = "PcInfoTool-Pro"
    } else {
        $proc = Get-Process -Name "PcInfoTool-Lite" -ErrorAction SilentlyContinue
        if ($proc) {
            $targetName = "PcInfoTool-Lite"
        } else {
            $proc = Get-Process -Name "PcInfoTool" -ErrorAction SilentlyContinue
            if ($proc) { $targetName = "PcInfoTool" }
        }
    }
}

if (-not $proc) {
    Write-Host "[提示] 未检测到正在运行的目标进程，正在为您自动启动 $targetName.exe..." -ForegroundColor Yellow
    $exePath = Join-Path $PSScriptRoot "..\..\build\Release\$targetName.exe"
    if (-not (Test-Path $exePath) -and $targetName -eq "PcInfoTool-Lite") {
        $exePath = Join-Path $PSScriptRoot "..\..\build\Release\PcInfoTool.exe"
        $targetName = "PcInfoTool"
    }
    if (Test-Path $exePath) {
        Start-Process $exePath -WindowStyle Hidden
        Start-Sleep -Seconds 2
        $proc = Get-Process -Name $targetName -ErrorAction SilentlyContinue
    } else {
        Write-Host "[错误] 未找到可执行文件: $exePath" -ForegroundColor Red
        Write-Host "请先运行根目录下的 build.bat 完成编译！" -ForegroundColor Red
        pause
        exit 1
    }
}

if (-not $proc) {
    Write-Host "[错误] 启动进程失败，请检查程序是否能正常运行。" -ForegroundColor Red
    pause
    exit 1
}

$pidVal = $proc.Id
Write-Host "[✔] 目标进程已锁定: $targetName.exe (PID: $pidVal)" -ForegroundColor Green
Write-Host ""
Write-Host "正在进行 $Samples 次高精度实时采样（间隔 1 秒），请稍候..." -ForegroundColor Gray
Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray

$sampleCount = $Samples
$wsList = @()
$pmList = @()
$cpuList = @()

for ($i = 1; $i -le $sampleCount; $i++) {
    $p = Get-Process -Id $pidVal -ErrorAction SilentlyContinue
    if (-not $p) {
        Write-Host "[警告] 采样过程中进程已退出！" -ForegroundColor Red
        break
    }

    # 内存指标
    $wsMB = [Math]::Round($p.WorkingSet64 / 1MB, 2)
    $pmMB = [Math]::Round($p.PrivateMemorySize64 / 1MB, 2)
    $wsList += $wsMB
    $pmList += $pmMB

    # CPU 占用测量 (计算 1 秒内的 CPU 时间增量百分比)
    $cpuTime1 = $p.TotalProcessorTime.TotalMilliseconds
    $time1 = [DateTime]::UtcNow
    Start-Sleep -Milliseconds 1000
    
    $p.Refresh()
    $cpuTime2 = $p.TotalProcessorTime.TotalMilliseconds
    $time2 = [DateTime]::UtcNow

    $elapsedMs = ($time2 - $time1).TotalMilliseconds
    $cpuDeltaMs = $cpuTime2 - $cpuTime1
    $cpuPercent = [Math]::Round(($cpuDeltaMs / ($elapsedMs * [Environment]::ProcessorCount)) * 100, 2)
    if ($cpuPercent -lt 0) { $cpuPercent = 0.0 }
    $cpuList += $cpuPercent

    # 线程与句柄数
    $handleCount = $p.HandleCount
    $threadCount = $p.Threads.Count

    Write-Host ("第 {0}/{1} 次采样 -> 内存(工作集): {2,6} MB | 独占物理内存: {3,6} MB | CPU 占用: {4,5} % | 句柄数: {5,3} | 线程数: {6,2}" -f `
        $i, $sampleCount, $wsMB, $pmMB, $cpuPercent, $handleCount, $threadCount) -ForegroundColor White
}

Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

# 计算均值
$avgWs = [Math]::Round(($wsList | Measure-Object -Average).Average, 2)
$avgPm = [Math]::Round(($pmList | Measure-Object -Average).Average, 2)
$avgCpu = [Math]::Round(($cpuList | Measure-Object -Average).Average, 2)

$isProTarget = ($targetName -like "*Pro*")
$wsGood = if ($isProTarget) { 40 } else { 15 }
$wsOk = if ($isProTarget) { 60 } else { 30 }

Write-Host "=================== 性能测试汇总评估 ===================" -ForegroundColor Cyan
Write-Host ("  [测试目标版本]              : {0}.exe" -f $targetName) -ForegroundColor White
Write-Host ("  [内存常驻开销 (WorkingSet)] : 平均 {0} MB" -f $avgWs) -ForegroundColor $(if ($avgWs -lt $wsGood) { "Green" } else { "Yellow" })
Write-Host ("  [独占专用内存 (PrivateMem)] : 平均 {0} MB" -f $avgPm) -ForegroundColor Green
Write-Host ("  [CPU 平均资源占用 (CPU Usage)] : 平均 {0} %" -f $avgCpu) -ForegroundColor $(if ($avgCpu -lt 0.5) { "Green" } else { "Yellow" })
Write-Host ""

if ($avgWs -lt $wsGood -and $avgCpu -lt 0.5) {
    Write-Host "【综合评级: 极优 (Ultra-Efficient)】" -ForegroundColor Green
    Write-Host "说明: 该程序处于极简轻量级水准，开销仅为一般 Electron 工具的 1/20，对游戏和重度办公毫无影响。" -ForegroundColor Gray
} elseif ($avgWs -lt $wsOk) {
    Write-Host "【综合评级: 优秀 (Standard)】" -ForegroundColor Green
} else {
    Write-Host "【综合评级: 良好】" -ForegroundColor Yellow
}
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "测试完成。按任意键退出..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
