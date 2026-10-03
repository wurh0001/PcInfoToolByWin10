Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public class WinEnum {
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
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

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
}
"@

$procs = Get-Process PcInfoTool-Pro -ErrorAction SilentlyContinue
if (-not $procs) {
    Write-Host "No PcInfoTool-Pro process found!"
    exit 1
}

$pidNum = [uint32]$procs[0].Id
Write-Host "Found process PID: $pidNum"

$hwnds = [WinEnum]::GetProcessWindows($pidNum)
Write-Host "Found $($hwnds.Count) windows."

$miniBarHwnd = [IntPtr]::Zero
$dashHwnd = [IntPtr]::Zero

foreach ($h in $hwnds) {
    $sbClass = New-Object System.Text.StringBuilder 256
    [WinEnum]::GetClassName($h, $sbClass, 256) | Out-Null
    $className = $sbClass.ToString()

    $sbText = New-Object System.Text.StringBuilder 256
    [WinEnum]::GetWindowText($h, $sbText, 256) | Out-Null
    $text = $sbText.ToString()

    $vis = [WinEnum]::IsWindowVisible($h)

    $rect = New-Object WinEnum+RECT
    [WinEnum]::GetWindowRect($h, [ref]$rect)
    $w = $rect.Right - $rect.Left
    $hH = $rect.Bottom - $rect.Top

    Write-Host "HWND: $h | Class: '$className' | Title: '$text' | Visible: $vis | Rect: $($rect.Left),$($rect.Top),$w,$hH"

    if ($className -like "*MiniBar*") {
        $miniBarHwnd = $h
    }
    if ($className -like "*Dashboard*") {
        $dashHwnd = $h
    }
}

# If dashboard is not visible or not open, toggle it
if ($dashHwnd -ne [IntPtr]::Zero) {
    if (-not [WinEnum]::IsWindowVisible($dashHwnd)) {
        Write-Host "Dashboard not visible, toggling via MiniBar..."
        [WinEnum]::PostMessage($miniBarHwnd, 0x0111, [IntPtr]101, [IntPtr]::Zero)
        Start-Sleep -Seconds 2
    }
}

# Capture Dashboard
if ($dashHwnd -ne [IntPtr]::Zero) {
    $rect = New-Object WinEnum+RECT
    [WinEnum]::GetWindowRect($dashHwnd, [ref]$rect)
    $w = $rect.Right - $rect.Left
    $h = $rect.Bottom - $rect.Top

    if ($w -gt 0 -and $h -gt 0) {
        $bitmap = New-Object System.Drawing.Bitmap $w, $h
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size $w, $h))
        $outPath = [System.IO.Path]::Combine($PSScriptRoot, "dashboard_capture.png")
        $bitmap.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $graphics.Dispose()
        $bitmap.Dispose()
        Write-Host "SUCCESS: Saved dashboard screenshot to: $outPath"
    }
}
