# =============================================================================
# drive-windows.ps1 - External UI driver for NetDiagnostics (Windows desktop)
# =============================================================================
# Launches the app, drives it like a real user (mouse + SendKeys), and
# captures OS-level screenshots per runtime stage.  ZERO app source
# modifications - pure OS-level automation of the production binary.
#
# Stage sequence (shared): 1-idle -> 2-running -> 3-complete -> 4-detail ->
#   5-dashboard -> 6-report -> 7-config -> 8-settings
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/screenshot/drive-windows.ps1 `
#       -AppPath build/net_diagnostics.exe -OutDir resources/doc/screenshot/windows
#
# Env overrides (all optional):
#   $env:ND_CAPTURE_TARGET    target URL       (default http://localhost:8888)
#   $env:ND_CAPTURE_MAX_TESTS tests per group  (default 4)
#   $env:ND_RESULT_ROW_Y      first result row (default 225)
#
# Notes:
#   - The app is frameless + C++ showMaximized(); we restore + MoveWindow to
#     (0,0,WIN_W,WIN_H) so the internal fixed layout maps to known coordinates.
#   - Coordinates are derived from the window rect (same layout math as the
#     Linux driver), so a different window size still works.
# =============================================================================
param(
    [Parameter(Mandatory = $true)][string]$AppPath,
    [string]$OutDir = "resources\doc\screenshot\windows"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class NW {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hgt, bool repaint);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint data, UIntPtr extra);
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
[NW]::SetProcessDPIAware() | Out-Null

# -- Config ----------------------------------------------------------------
$TargetUrl = if ($env:ND_CAPTURE_TARGET) { $env:ND_CAPTURE_TARGET } else { "http://localhost:8899" }
$MaxTests = if ($env:ND_CAPTURE_MAX_TESTS) { [int]$env:ND_CAPTURE_MAX_TESTS } else { 4 }
$ResultRowY = if ($env:ND_RESULT_ROW_Y) { [int]$env:ND_RESULT_ROW_Y } else { 0 }
$WinW = 1440; $WinH = 1100
$OutDirAbs = Join-Path (Get-Location) $OutDir
New-Item -ItemType Directory -Force -Path $OutDirAbs | Out-Null

function Log([string]$m) { Write-Host "[DRIVE] $m" -ForegroundColor Cyan }
function Warn([string]$m) { Write-Host "[WARN]  $m" -ForegroundColor Yellow }
function Die([string]$m) { Write-Error $m; exit 1 }

if (-not (Test-Path $AppPath)) { Die "app not found: $AppPath" }

# -- Slow deterministic HTTP server (keeps the run alive for the Running shot) -
$ServerProc = $null
$serverPy = Join-Path (Get-Location) "scripts\screenshot\slow-http-server.py"
if (Test-Path $serverPy) {
    try {
        $ServerProc = Start-Process -FilePath "python" -ArgumentList $serverPy, "8899", "2.0" -PassThru -WindowStyle Hidden
        Log "slow-http-server started (pid $($ServerProc.Id), localhost:8899)"
    } catch { Warn "failed to start slow-http-server: $_" }
}

$proc = $null
$hwnd = [IntPtr]::Zero

function Launch-App {
    param([string]$AppPath)
    # Fresh QSettings -> deterministic default config (all tests enabled).
    Remove-Item -Path "HKCU:\Software\robinhoo1973\NetDiagnostics" -Recurse -Force -ErrorAction SilentlyContinue
    $env:ND_MAX_TESTS = "$MaxTests"   # existing runtime cap (no source change)

    Log "starting app: $AppPath"
    $script:proc = Start-Process -FilePath $AppPath -PassThru

    # 5WHY: FindWindow(null, "NetDiagnostics") failed (window never found),
    # so coordinates defaulted to the full screen and clicks hit nothing.
    # Use process MainWindowHandle instead, and wait up to 60 s for the
    # window to materialize on the slow CI runner.
    $script:hwnd = [IntPtr]::Zero
    for ($i = 0; $i -lt 120 -and $script:hwnd -eq [IntPtr]::Zero; $i++) {
        Start-Sleep -Milliseconds 500
        try { $script:proc.Refresh() } catch {}
        $script:hwnd = $script:proc.MainWindowHandle
    }
    if ($script:hwnd -eq [IntPtr]::Zero) {
        Warn "window not found after 60s — continuing (captures may be blank)"
        # Last-resort fallback: scan top-level windows by class name
        $script:hwnd = [NW]::FindWindow("Qt6QML", $null)
        if ($script:hwnd -eq [IntPtr]::Zero) { $script:hwnd = [NW]::FindWindow($null, "NetDiagnostics") }
    } else {
        Log "window found (hwnd=$($script:hwnd))"
    }

    if ($script:hwnd -ne [IntPtr]::Zero) {
        [NW]::ShowWindow($script:hwnd, 9) | Out-Null          # SW_RESTORE
        Start-Sleep -Milliseconds 300
        [NW]::MoveWindow($script:hwnd, 0, 0, $WinW, $WinH, $true) | Out-Null
        [NW]::SetForegroundWindow($script:hwnd) | Out-Null
        Start-Sleep -Seconds 3
    }
}

function Stop-App {
    if ($script:proc -and -not $script:proc.HasExited) { Stop-Process -Id $script:proc.Id -Force -ErrorAction SilentlyContinue }
    $script:proc = $null; $script:hwnd = [IntPtr]::Zero
    Start-Sleep -Seconds 1
}

Launch-App $AppPath

# -- Window rect -> coordinates (fixed internal layout) --------------------
$rect = New-Object NW+RECT
[NW]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$W = $rect.Right - $rect.Left; $H = $rect.Bottom - $rect.Top
if ($W -le 0) { $W = $WinW; $H = $WinH }
$WX = $rect.Left; $WY = $rect.Top
$ToolbarCy = $WY + 74
$HostCx = $WX + [int](($W + 94) / 2)
$RunCx = $WX + $W - 26
$NavY = $WY + $H - 28
$NavX0 = $WX + [int](($W - 539) / 2)
$NavDashCx = $NavX0 + 60
$NavDiagCx = $NavX0 + 192
$NavCfgCx = $NavX0 + 342
$NavSetCx = $NavX0 + 482
$DetailX0 = $WX + [int](($W - 700) / 2)
$DetailY0 = $WY + [int](($H - 620) / 2)
$DetailCloseCx = $DetailX0 + 670
$DetailCloseCy = $DetailY0 + 30
$ReportBtnX = $WX + [int]($W / 2)
$ReportBtnY = $WY + 1035
if ($ResultRowY -eq 0) { $ResultRowY = $WY + 240 }
$ResultRowX = $WX + 400

Log "layout ${W}x${H} toolbar_y=$ToolbarCy host_x=$HostCx run_x=$RunCx nav_y=$NavY target=$TargetUrl"

# -- Helpers ---------------------------------------------------------------
function Click([int]$x, [int]$y, [string]$label) {
    [NW]::SetCursorPos($x, $y) | Out-Null
    [NW]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)   # left down
    [NW]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)   # left up
    Log "clicked $label ($x,$y)"
    Start-Sleep -Milliseconds 900
}
function TypeUrl {
    $wshell = New-Object -ComObject WScript.Shell
    foreach ($ch in $TargetUrl.ToCharArray()) { $wshell.SendKeys($ch); Start-Sleep -Milliseconds 25 }
    Log "typed target: $TargetUrl"
    Start-Sleep -Milliseconds 900
}
function NavTo([int]$x, [string]$label) { Click $x $NavY "nav-$label"; Start-Sleep -Milliseconds 700 }
function ScrollDown {
    for ($i = 0; $i -lt 30; $i++) {
        [NW]::mouse_event(0x0800, 0, 0, [uint32](-120), [UIntPtr]::Zero)  # wheel down
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Seconds 2
}
function Capture([string]$stage) {
    $f = Join-Path $OutDirAbs "$stage.png"
    $bmp = New-Object System.Drawing.Bitmap($WinW, $WinH)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try { $g.CopyFromScreen($WX, $WY, 0, 0, $bmp.Size) } catch { Warn "CopyFromScreen failed: $_" }
    $bmp.Save($f, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Log "captured $stage -> $f"
}
function ScreenHash {
    $bmp = New-Object System.Drawing.Bitmap($WinW, $WinH)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try { $g.CopyFromScreen($WX, $WY, 0, 0, $bmp.Size) } catch { }
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    $sha = [System.Security.Cryptography.SHA256]::Create()
    return ([BitConverter]::ToString($sha.ComputeHash($ms.ToArray()))).Replace("-", "")
}
function WaitStable([int]$maxSec = 60) {
    $t = 0; $a = ""; $b = ""
    while ($t -lt $maxSec) {
        $a = ScreenHash; Start-Sleep -Seconds 4; $b = ScreenHash
        if ($a -eq $b) { Log "screen stable after ~$($t + 4)s"; Start-Sleep -Seconds 2; return }
        $t += 4
    }
    Warn "screen not stable within ${maxSec}s - proceeding"
}

# ==========================================================================
# PASS A - 1..6 (report preview is the last stage; its overlay cannot be
# dismissed reliably (the window's own close button stacks above it), so we
# relaunch for the remaining screens).
# ==========================================================================
Start-Sleep -Seconds 2
Capture "1-idle"

# -- Stage 2: Running ------------------------------------------------------
Click $HostCx $ToolbarCy "host-field"
TypeUrl
Click $RunCx $ToolbarCy "run-button"
Start-Sleep -Seconds 2
Capture "2-running"

# -- Stage 3: Complete -----------------------------------------------------
WaitStable 60
Capture "3-complete"

# -- Stage 4: Detail (keyboard: Tab Tab Enter - focus is on the run button
# after clicking Run; result rows are activeFocusOnTab and Enter opens detail)
$wshell4 = New-Object -ComObject WScript.Shell
$wshell4.SendKeys("{TAB}"); Start-Sleep -Milliseconds 300
$wshell4.SendKeys("{TAB}"); Start-Sleep -Milliseconds 300
$wshell4.SendKeys("{ENTER}"); Start-Sleep -Seconds 2
Capture "4-detail"
Click $DetailCloseCx $DetailCloseCy "detail-close"

# -- Stage 5: Dashboard ----------------------------------------------------
NavTo $NavDashCx "dashboard"
Capture "5-dashboard"

# -- Stage 6: Report preview -----------------------------------------------
Click $ReportBtnX $ReportBtnY "review-report"
Start-Sleep -Seconds 3
Capture "6-report"
Stop-App

# ==========================================================================
# PASS B - 7-config -> 8-settings (fresh launch)
# ==========================================================================
Launch-App $AppPath
NavTo $NavCfgCx "config"
Capture "7-config"

NavTo $NavSetCx "settings"
Capture "8-settings"

Stop-App
Log "done - screenshots in $OutDirAbs"
if ($ServerProc -and -not $ServerProc.HasExited) { Stop-Process -Id $ServerProc.Id -Force -ErrorAction SilentlyContinue }
exit 0
