# =============================================================================
# pre-commit-check.ps1 — Build + smoke test before committing
#
# Usage: powershell -File scripts/pre-commit-check.ps1
#        powershell -File scripts/pre-commit-check.ps1 -Debug
#
# Debug mode: enables ND_DEBUG=ON → startup log written to %%TEMP%%
# If the EXE crashes, check: notepad $env:TEMP\NetDiagnostics_startup.log
# =============================================================================

param([switch]$Debug)

$ErrorActionPreference = "Stop"
$logFile = "$env:TEMP\NetDiagnostics_startup.log"

Write-Host "=== NetDiagnostics Pre-Commit Check ===" -ForegroundColor Cyan

# Clean previous log
if (Test-Path $logFile) {
    Remove-Item $logFile -Force
    Write-Host "[LOG] Cleared previous log"
}

# ── Step 1: Configure ──────────────────────────────────────────────────
Write-Host "[1/3] Configuring static build..." -ForegroundColor Yellow
$cmakeArgs = @(
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_CXX_FLAGS=-static -static-libgcc -static-libstdc++ -O2",
    "-DCMAKE_EXE_LINKER_FLAGS=-static -static-libgcc -static-libstdc++",
    "-DCMAKE_PREFIX_PATH=C:/opt/qt6-install",
    "-DAPP_EDITION=windows-x64-static",
    "-DNO_CURL=ON",
    "-DBUILD_TESTS=OFF",
    "-B", "build-check",
    "-S", "."
)
if ($Debug) {
    $cmakeArgs += "-DND_DEBUG=ON"
    Write-Host "  [DEBUG] Startup logging ENABLED → $logFile"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Host "FAIL: CMake configure" -ForegroundColor Red; exit 1 }

# ── Step 2: Build ──────────────────────────────────────────────────────
Write-Host "[2/3] Building net_diagnostics (static)..." -ForegroundColor Yellow
ninja -C build-check net_diagnostics
if ($LASTEXITCODE -ne 0) { Write-Host "FAIL: Build" -ForegroundColor Red; exit 1 }

# ── Step 3: Smoke test ─────────────────────────────────────────────────
Write-Host "[3/3] Smoke test: launching EXE..." -ForegroundColor Yellow
$exe = "build-check\net_diagnostics.exe"
if (-not (Test-Path $exe)) { Write-Host "FAIL: EXE not found" -ForegroundColor Red; exit 1 }

$proc = Start-Process -FilePath $exe -PassThru -WindowStyle Minimized
Start-Sleep -Seconds 5

if ($proc.HasExited) {
    Write-Host "FAIL: Process exited after 5s (code: $($proc.ExitCode))" -ForegroundColor Red
    if (Test-Path $logFile) {
        Write-Host "--- Startup log (last 20 lines) ---" -ForegroundColor DarkYellow
        Get-Content $logFile -Tail 20 | ForEach-Object { Write-Host $_ -ForegroundColor Gray }
    } else {
        Write-Host "No startup log found. Re-run with -Debug to enable logging." -ForegroundColor DarkYellow
    }
    exit 1
}

Write-Host "PASS: Static EXE running (PID: $($proc.Id))" -ForegroundColor Green
$proc.Kill()
if (Test-Path $logFile) {
    Write-Host "--- Startup log ---" -ForegroundColor DarkYellow
    Get-Content $logFile | ForEach-Object { Write-Host $_ -ForegroundColor Gray }
}
Write-Host "=== Pre-commit check PASSED ===" -ForegroundColor Green
exit 0
