param([string]$OutputPath = (Join-Path $PSScriptRoot '..\..\build\sensor-probe\lenovo.json'),[switch]$Native)
$ErrorActionPreference = 'Stop'
if($Native) {
    $executable=Join-Path $PSScriptRoot '..\..\build\Release\PcInfoTool-OverviewRenderTest.exe'
    & $executable 1 $OutputPath lenovo-live
    exit $LASTEXITCODE
}
$report = [ordered]@{ elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator); samples = @(); error = $null }
try {
    $device = Get-CimInstance -Namespace root/wmi -ClassName LENOVO_GAMEZONE_DATA -ErrorAction Stop | Select-Object -First 1
    if(-not $device){ throw 'No Lenovo provider instance' }
    for($i=0;$i -lt 3;$i++) {
        $sample=[ordered]@{ time=(Get-Date).ToString('o') }
        foreach($method in @('GetCPUTemp','GetGPUTemp','GetFanCount','GetFan1Speed','GetFan2Speed','GetFanMaxSpeed')) {
            try { $result=Invoke-CimMethod -InputObject $device -MethodName $method -ErrorAction Stop; $sample[$method]=@{ Data=$result.Data; ReturnValue=$result.ReturnValue } }
            catch { $sample[$method]=@{ error=$_.Exception.Message } }
        }
        $report.samples += $sample
        if($i -lt 2){Start-Sleep -Seconds 2}
    }
} catch { $report.error=$_.Exception.Message }
$resolved=[IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($resolved)) | Out-Null
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolved -Encoding utf8
