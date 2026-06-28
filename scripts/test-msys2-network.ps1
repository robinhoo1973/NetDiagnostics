<#
.SYNOPSIS
    MSYS2 Network Diagnostic - Test pacman connectivity
#>
$MsysPath = "$env:LOCALAPPDATA\msys64"
$bash = Join-Path $MsysPath "usr\bin\bash.exe"

if (-not (Test-Path $bash)) {
    Write-Host "ERROR: MSYS2 not found at $MsysPath" -ForegroundColor Red
    exit 1
}

Write-Host "=== Test 1: Bash works ===" -ForegroundColor Cyan
& $bash -lc "echo 'MSYS2 shell OK'" 2>&1

Write-Host "`n=== Test 2: HTTP connectivity ===" -ForegroundColor Cyan
& $bash -lc "curl -sI http://mirrors.ustc.edu.cn/msys2/ 2>&1 | head -5"

Write-Host "`n=== Test 3: pacman database sync (http mirrors) ===" -ForegroundColor Cyan
& $bash -lc "sed -i 's|https://|http://|g' /etc/pacman.d/mirrorlist.* 2>/dev/null; pacman -Sy --noconfirm 2>&1" | Select-Object -Last 15

Write-Host "`n=== Test 4: Package download test ===" -ForegroundColor Cyan
& $bash -lc "pacman -Sw --noconfirm mingw-w64-ucrt-x86_64-qt6-static 2>&1" | Select-Object -Last 20
