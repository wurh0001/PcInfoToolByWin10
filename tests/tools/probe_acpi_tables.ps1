# Read-only firmware dump for diagnostics. Never evaluates ACPI control methods.
param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/sensor-probe/acpi'))
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AcpiTableProbe {
 [DllImport("kernel32.dll", SetLastError=true)] public static extern uint EnumSystemFirmwareTables(uint provider, byte[] buffer, uint size);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern uint GetSystemFirmwareTable(uint provider, uint id, byte[] buffer, uint size);
}
'@
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$taskProvider = [uint32]0x41435049
$taskSize = [AcpiTableProbe]::EnumSystemFirmwareTables($taskProvider, $null, 0)
if (!$taskSize) { throw "Enum failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
$taskIds = New-Object byte[] $taskSize
[AcpiTableProbe]::EnumSystemFirmwareTables($taskProvider, $taskIds, $taskSize) | Out-Null
$taskSeen = @{}
for ($taskOffset=0; $taskOffset -lt $taskSize; $taskOffset+=4) {
 $taskId = [BitConverter]::ToUInt32($taskIds, $taskOffset)
 $taskName = [Text.Encoding]::ASCII.GetString($taskIds, $taskOffset, 4)
 if ($taskSeen.ContainsKey($taskName)) { Write-Output "$taskName duplicate: API exposes first only"; continue }
 $taskSeen[$taskName]=$true
 # Avoid unrelated firmware tables (such as Windows product keys).
 if ($taskName -notin @('DSDT','SSDT','FACP')) { continue }
 $taskLength = [AcpiTableProbe]::GetSystemFirmwareTable($taskProvider, $taskId, $null, 0)
 if (!$taskLength) { Write-Output "$taskName unavailable"; continue }
 $taskBuffer = New-Object byte[] $taskLength
 $taskActual = [AcpiTableProbe]::GetSystemFirmwareTable($taskProvider, $taskId, $taskBuffer, $taskLength)
 if ($taskActual -ne $taskLength) { throw "$taskName length changed" }
 [IO.File]::WriteAllBytes((Join-Path $taskOutput "$taskName.dat"), $taskBuffer)
 Write-Output "$taskName $taskLength bytes"
 $taskText=[Text.Encoding]::ASCII.GetString($taskBuffer)
 [regex]::Matches($taskText, '[A-Z_][A-Z0-9_]{3,}') | ForEach-Object Value | Where-Object { $_ -match 'WM|TEMP|GMZN|CPUT|GPUT' } | Sort-Object -Unique
}
# Registry copies include DSDT and individual SSDTs missing from the public API.
Get-ChildItem 'Registry::HKEY_LOCAL_MACHINE\HARDWARE\ACPI' -Recurse | ForEach-Object {
 $taskKey = $_
 foreach ($taskValueName in $taskKey.GetValueNames()) {
  $taskValue = $taskKey.GetValue($taskValueName)
  if ($taskValue -isnot [byte[]] -or $taskValue.Length -lt 36) { continue }
  $taskSignature=[Text.Encoding]::ASCII.GetString($taskValue,0,4)
  if ($taskSignature -notin @('DSDT','SSDT')) { continue }
  $taskParts=$taskKey.Name.Split('\')
  $taskLabel=$taskParts[3]
  [IO.File]::WriteAllBytes((Join-Path $taskOutput "registry-$taskLabel.dat"),$taskValue)
  Write-Output "Registry $taskLabel $($taskValue.Length) bytes"
 }
}
