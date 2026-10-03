Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class WinFind {
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindowEx(IntPtr parentHandle, IntPtr childAfter, string className, string windowTitle);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left, Top, Right, Bottom;
    }
}
"@

$procs = Get-Process PcInfoTool-Pro -ErrorAction SilentlyContinue
if (-not $procs) {
    Write-Host "No process"
    exit 0
}
$targetPid = [uint32]$procs[0].Id
Write-Host "Searching for windows of PID: $targetPid"

$h = [IntPtr]::Zero
do {
    $h = [WinFind]::FindWindowEx([IntPtr]::Zero, $h, $null, $null)
    if ($h -ne [IntPtr]::Zero) {
        $p = 0
        [WinFind]::GetWindowThreadProcessId($h, [ref]$p)
        if ($p -eq $targetPid) {
            $sbClass = New-Object System.Text.StringBuilder 256
            [WinFind]::GetClassName($h, $sbClass, 256) | Out-Null
            $sbText = New-Object System.Text.StringBuilder 256
            [WinFind]::GetWindowText($h, $sbText, 256) | Out-Null
            $vis = [WinFind]::IsWindowVisible($h)
            $rc = New-Object WinFind+RECT
            [WinFind]::GetWindowRect($h, [ref]$rc)
            $w = $rc.Right - $rc.Left
            $hH = $rc.Bottom - $rc.Top
            Write-Host "HWND: $h | Class: '$sbClass' | Text: '$sbText' | Vis: $vis | Rect: $($rc.Left),$($rc.Top),$w,$hH"
        }
    }
} while ($h -ne [IntPtr]::Zero)
