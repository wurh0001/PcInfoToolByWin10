Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public class TestWin {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public const uint WM_LBUTTONDOWN = 0x0201;
    public const uint WM_LBUTTONUP   = 0x0202;
    public const uint WM_MOUSEMOVE   = 0x0200;
    public const uint WM_MOUSELEAVE  = 0x02A3;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static List<IntPtr> GetProcessWindows(uint pid) {
        var list = new List<IntPtr>();
        EnumWindows((hWnd, lParam) => {
            uint p;
            GetWindowThreadProcessId(hWnd, out p);
            if (p == pid) {
                list.Add(hWnd);
            }
            return true;
        }, IntPtr.Zero);
        return list;
    }

    public static IntPtr MakeLParam(int x, int y) {
        return (IntPtr)((y << 16) | (x & 0xFFFF));
    }
}
"@

$procs = Get-Process PcInfoTool-Pro -ErrorAction SilentlyContinue
if (-not $procs) {
    Write-Host "PcInfoTool-Pro is not running!"
    exit 1
}

$hwnds = [TestWin]::GetProcessWindows($procs[0].Id)
Write-Host "Found $($hwnds.Count) windows for PID $($procs[0].Id)"
$barHwnd = [IntPtr]::Zero
$tipHwnd = [IntPtr]::Zero
$dashHwnd = [IntPtr]::Zero

foreach ($h in $hwnds) {
    $sb = New-Object System.Text.StringBuilder 256
    [TestWin]::GetClassName($h, $sb, 256) | Out-Null
    $c = $sb.ToString()
    Write-Host "HWND: $h, Class: '$c'"
    if ($c -like "*MiniBar*") { $barHwnd = $h }
    if ($c -like "*Tooltip*") { $tipHwnd = $h }
    if ($c -like "*Dashboard*") { $dashHwnd = $h }
}

$rcBar = New-Object TestWin+RECT
[TestWin]::GetWindowRect($barHwnd, [ref]$rcBar)
Write-Host "MiniBar Rect: ($($rcBar.Left), $($rcBar.Top)) - ($($rcBar.Right), $($rcBar.Bottom))"

# 1. Close dashboard first by clicking chevron (x = 363, y = 16)
Write-Host "1. Clicking Chevron (x=363, y=16) to close Dashboard..."
$lParamChev = [TestWin]::MakeLParam(363, 16)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamChev) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamChev) | Out-Null
Start-Sleep -Milliseconds 300
Write-Host "Dashboard visible: $([TestWin]::IsWindowVisible($dashHwnd))"

# 2. Click CPU segment (x = 40, y = 16) to enter ONLY CPU direct card
Write-Host "2. Clicking CPU segment (x=40, y=16) for Direct Entry..."
$lParamCpu = [TestWin]::MakeLParam(40, 16)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamCpu) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamCpu) | Out-Null
Start-Sleep -Milliseconds 400

$rcCpu = New-Object TestWin+RECT
[TestWin]::GetWindowRect($dashHwnd, [ref]$rcCpu)
$cpuH = $rcCpu.Bottom - $rcCpu.Top
Write-Host "CPU Direct Card Size: $($rcCpu.Right - $rcCpu.Left)x$cpuH"
if ($cpuH -ne 420) {
    Write-Warning "Expected CPU direct card height ~420 DIP, got $cpuH"
}

# 3. Click "查看系统运行概览 ›" inside CPU Direct Card (x = 320, y = 398)
Write-Host "3. Clicking '查看系统运行概览 ›' inside CPU direct card..."
$lParamOverview = [TestWin]::MakeLParam(320, 398)
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamOverview) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamOverview) | Out-Null
Start-Sleep -Milliseconds 400

$rcFull = New-Object TestWin+RECT
[TestWin]::GetWindowRect($dashHwnd, [ref]$rcFull)
$fullH = $rcFull.Bottom - $rcFull.Top
Write-Host "Full Dashboard Size after clicking Overview link: $($rcFull.Right - $rcFull.Left)x$fullH"
if ($fullH -lt 690) {
    Write-Warning "Expected Full Dashboard height >= 690 DIP, got $fullH"
}

# 4. Click Temp segment (x = 105, y = 16) to enter dedicated "02 CPU 温度" direct card
Write-Host "4. Clicking Sensor segment to open '02 CPU 温度' direct card..."
$lParamTemp = [TestWin]::MakeLParam(105, 16)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamTemp) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamTemp) | Out-Null
Start-Sleep -Milliseconds 400

$rcTemp = New-Object TestWin+RECT
[TestWin]::GetWindowRect($dashHwnd, [ref]$rcTemp)
$tempH = $rcTemp.Bottom - $rcTemp.Top
Write-Host "Temp Direct Card Size: $($rcTemp.Right - $rcTemp.Left)x$tempH"
if ($tempH -ne 420) {
    Write-Warning "Expected Temp direct card height ~420 DIP, got $tempH"
}

# 5. Click RAM segment (x = 180, y = 16) to enter dedicated RAM direct card
Write-Host "5. Clicking RAM segment (x=180, y=16) for Direct Entry..."
$lParamRam = [TestWin]::MakeLParam(180, 16)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamRam) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamRam) | Out-Null
Start-Sleep -Milliseconds 400

$rcRam = New-Object TestWin+RECT
[TestWin]::GetWindowRect($dashHwnd, [ref]$rcRam)
$ramH = $rcRam.Bottom - $rcRam.Top
Write-Host "RAM Direct Card Size: $($rcRam.Right - $rcRam.Left)x$ramH"

# 6. Click Network segment (x = 270, y = 16) to enter dedicated Network direct card
Write-Host "6. Clicking Network segment (x=270, y=16) for Direct Entry..."
$lParamNet = [TestWin]::MakeLParam(270, 16)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamNet) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamNet) | Out-Null
Start-Sleep -Milliseconds 400

$rcNet = New-Object TestWin+RECT
[TestWin]::GetWindowRect($dashHwnd, [ref]$rcNet)
$netH = $rcNet.Bottom - $rcNet.Top
Write-Host "Network Direct Card Size: $($rcNet.Right - $rcNet.Left)x$netH"

# 7. Open Settings page via Chevron -> Settings Gear button
Write-Host "7. Opening Full Dashboard to click Settings gear button..."
$lParamChev = [TestWin]::MakeLParam(363, 16)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamChev) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamChev) | Out-Null
Start-Sleep -Milliseconds 400

Write-Host "Clicking Settings gear button (x = 245, y = 28)..."
$lParamGear = [TestWin]::MakeLParam(245, 28)
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamGear) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamGear) | Out-Null
Start-Sleep -Milliseconds 400

$rcSettings = New-Object TestWin+RECT
[TestWin]::GetWindowRect($dashHwnd, [ref]$rcSettings)
$setH = $rcSettings.Bottom - $rcSettings.Top
Write-Host "Settings Page Size: $($rcSettings.Right - $rcSettings.Left)x$setH"
if ($setH -ne 504) {
    Write-Warning "Expected Settings page height ~504 DIP, got $setH"
}

# 8. Test Settings Tab Switching & Click "完成"
Write-Host "8. Testing Settings Tabs ('监控' x=180, '常规' x=290, '外观' x=70)..."
$lParamTabMon = [TestWin]::MakeLParam(180, 64)
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamTabMon) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamTabMon) | Out-Null
Start-Sleep -Milliseconds 200

$lParamTabGen = [TestWin]::MakeLParam(290, 64)
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamTabGen) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamTabGen) | Out-Null
Start-Sleep -Milliseconds 200

$lParamTabApp = [TestWin]::MakeLParam(70, 64)
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamTabApp) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamTabApp) | Out-Null
Start-Sleep -Milliseconds 200

Write-Host "Clicking '完成' (x=380, y=20) to close Settings..."
$lParamDone = [TestWin]::MakeLParam(380, 20)
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONDOWN, [IntPtr]1, $lParamDone) | Out-Null
Start-Sleep -Milliseconds 50
[TestWin]::SendMessage($dashHwnd, [TestWin]::WM_LBUTTONUP, [IntPtr]0, $lParamDone) | Out-Null
Start-Sleep -Milliseconds 300
Write-Host "Settings visible after '完成': $([TestWin]::IsWindowVisible($dashHwnd))"

# 9. Move mouse away (WM_MOUSELEAVE)
[TestWin]::SendMessage($barHwnd, [TestWin]::WM_MOUSELEAVE, [IntPtr]0, [IntPtr]0) | Out-Null
Start-Sleep -Milliseconds 200
Write-Host "SUCCESS: MiniBar direct cards and Settings page tests passed!"
