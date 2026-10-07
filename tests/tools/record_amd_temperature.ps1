# Uses the project's independent native probe; never installs drivers or changes policy.
param(
    [ValidateRange(1,3600)][int]$Seconds=180,
    [ValidateRange(250,10000)][int]$IntervalMs=1000,
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../../build/sensor-probe/comparison')
)
$ErrorActionPreference='Stop'
$taskAdmin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if(-not $taskAdmin) { throw 'Please run this script in an administrator PowerShell window. No automatic elevation is performed.' }
$taskProbe=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../build/Release/PcInfoTool-AmdTemperatureProbe.exe'))
$taskModule=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../build/sensor-probe/pawnio-modules-0.2.11/AMDFamily17.bin'))
foreach($taskFile in @($taskProbe,$taskModule)) { if(-not (Test-Path -LiteralPath $taskFile -PathType Leaf)) { throw "Missing diagnostic file: $taskFile" } }
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$taskCsv=Join-Path $taskOutput ('pawnio-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.csv')
if(Test-Path -LiteralPath $taskCsv) { throw "Output already exists: $taskCsv" }
$taskSamples=[int][Math]::Ceiling($Seconds*1000.0/$IntervalMs)
$taskMetadata=[ordered]@{
    startedLocal=(Get-Date).ToString('o'); startedUtc=(Get-Date).ToUniversalTime().ToString('o')
    timeZoneId=[TimeZoneInfo]::Local.Id; seconds=$Seconds; intervalMs=$IntervalMs; samples=$taskSamples
    sensor='AMD Tctl, SMN 0x59800'; statistics='Minimum/maximum of valid samples during this recording session; not per-core extrema'
    moduleVersion='PawnIO.Modules 0.2.11'; moduleSha256=(Get-FileHash -LiteralPath $taskModule -Algorithm SHA256).Hash
    probeSha256=(Get-FileHash -LiteralPath $taskProbe -Algorithm SHA256).Hash
}
$taskMetadata | ConvertTo-Json | Set-Content -LiteralPath ($taskCsv+'.session.json') -Encoding utf8
Write-Host "CSV: $taskCsv"
Write-Host "Recording approximately $Seconds seconds. Keep Core Temp logging enabled during the same period."
& $taskProbe $taskModule $taskCsv --csv $taskSamples $IntervalMs
$taskExit=$LASTEXITCODE
Write-Host "Finished. CSV: $taskCsv"
if(Test-Path -LiteralPath ($taskCsv+'.status.txt')) { Get-Content -LiteralPath ($taskCsv+'.status.txt') | Select-Object -Last 4 | Write-Host }
if($taskExit -ne 0) { throw "Probe failed or produced invalid samples (exit $taskExit). Preserve the CSV and status file for diagnosis." }
