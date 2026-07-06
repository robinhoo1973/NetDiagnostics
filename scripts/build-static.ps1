<#
.SYNOPSIS
    NetDiagnostic Static Build — Fully static linked compilation (zero non-OS DLL)
.DESCRIPTION
    Builds production + simulator executables with truly static linking using
    the MSYS2 pre-built qt6-static package at /ucrt64/qt6-static.

    All non-Qt dependencies (zlib, brotli, pcre2, freetype, harfbuzz, libpng,
    libjpeg, libtiff, libwebp, OpenSSL, etc.) are linked statically from their
    .a counterparts in /ucrt64/lib/.  Windows system DLLs (KERNEL32, USER32, …)
    are the ONLY dynamic dependencies — the exe is fully portable with zero
    third-party DLL requirements.

    Mirrors the windows-x86_64-static job in .github/workflows/build.yml,
    adapted for local builds using the pre-built MSYS2 static Qt6 package.

    All build logs are written to dist/.

.PARAMETER ProdOnly
    Build only production version (default: both)
.PARAMETER SimOnly
    Build only simulator version (default: both)
.PARAMETER Clean
    Clean previous build artifacts before building
.PARAMETER NoCleanTemp
    Keep temp files under %TEMP% (for debugging)
.PARAMETER MsysPath
    MSYS2 installation path (default: C:\msys64)
.PARAMETER Qt6Prefix
    Static Qt6 cmake prefix path (default: /ucrt64/qt6-static within MSYS2)

.EXAMPLE
    .\scripts\build-static.ps1
    Build both production + simulator (fully static, zero non-OS DLL)

.EXAMPLE
    .\scripts\build-static.ps1 -ProdOnly -Clean
    Clean then build production only
#>

param(
    [switch]$ProdOnly,
    [switch]$SimOnly,
    [switch]$Clean,
    [switch]$NoCleanTemp,
    [string]$MsysPath = "C:\msys64",
    [string]$Qt6Prefix = "/ucrt64/qt6-static"
)

$ErrorActionPreference = "Stop"
$Host.UI.RawUI.WindowTitle = "NetDiagnostic Static Build"

# ============================================================================
# 0. Constants & Paths
# ============================================================================
$SCRIPT_DIR  = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$PROJECT_DIR = (Resolve-Path (Split-Path -Parent $SCRIPT_DIR)).Path
$DIST_DIR    = Join-Path $PROJECT_DIR "dist"
$TEMP_DIR    = Join-Path $env:TEMP "netdiag-static-build"

# Windows system libraries required by static Qt6 and its dependencies.
# Wrapped with --start-group/--end-group so the linker resolves circular refs.
# These are all OS-provided DLLs (not third-party) — the resulting exe remains
# portable across Windows 10+ installations.
$WIN_SYS_LIBS = @(
    "-lcrypt32", "-lws2_32", "-liphlpapi", "-lwinhttp",
    "-lnetapi32", "-ld3d11", "-ldxgi", "-ldwrite",
    "-luuid", "-loleaut32", "-lole32", "-lversion",
    "-lwinmm", "-lncrypt", "-luserenv", "-lsetupapi"
) -join " "

$LINKER_GROUP_FLAGS = "-Wl,--start-group $WIN_SYS_LIBS -Wl,--end-group"

# ============================================================================
# 1. Helper Functions
# ============================================================================
function Write-Info  { Write-Host "  [INFO]  $args" -ForegroundColor Cyan }
function Write-OK    { Write-Host "  [OK]    $args" -ForegroundColor Green }
function Write-Warn  { Write-Host "  [WARN]  $args" -ForegroundColor Yellow }
function Write-Err   { Write-Host "  [ERROR] $args" -ForegroundColor Red }
function Write-Step  {
    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Gray
    Write-Host "  $args" -ForegroundColor White
    Write-Host ("=" * 60) -ForegroundColor Gray
}

function ConvertTo-MsysPath {
    param([string]$WinPath)
    ($WinPath -replace '\\', '/' -replace '^([A-Za-z]):', '/$1').ToLower()
}

function Show-Banner {
    Write-Host @"

============================================================
     NetDiagnostic Static Build
     MSYS2 qt6-static + fully static linking
     Zero non-OS DLL dependencies
============================================================

"@ -ForegroundColor Cyan
}

# ============================================================================
# 2. Platform Detection
# ============================================================================
function Detect-Platform {
    Write-Step "Platform Detection"

    $script:BUILD_OS   = "win"
    $script:BUILD_ARCH = "x86_64"
    $script:EXE_EXT    = ".exe"

    $arch = [System.Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")
    if ($arch -eq "ARM64" -or $arch -eq "AArch64") {
        Write-Err "ARM64 Windows not yet supported for static builds."
        Write-Err "Use the dynamic MSYS2 build instead."
        exit 1
    }

    Write-OK "OS:    $script:BUILD_OS"
    Write-OK "CPU:   $script:BUILD_ARCH"
    Write-OK "Dist:  $DIST_DIR"
}

# ============================================================================
# 3. MSYS2 Dependency Check
# ============================================================================
function Test-Dependencies {
    Write-Step "MSYS2 Dependency Check"

    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    if (-not (Test-Path $bash_exe)) {
        Write-Err "MSYS2 not found at: $MsysPath"
        Write-Err "Install MSYS2: https://www.msys2.org/"
        Write-Err "After install, run MSYS2 UCRT64 terminal and execute:"
        Write-Err "  pacman -Syu"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-gcc"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-cmake"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-ninja"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-qt6-static"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-openssl"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-zlib"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-brotli"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-pcre2"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-libb2"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-libpng"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-libjpeg-turbo"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-libtiff"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-libwebp"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-freetype"
        Write-Err "  pacman -S --needed mingw-w64-ucrt-x86_64-harfbuzz"
        exit 1
    }

    $script:MSYS2_ENV = "ucrt64"

    # Verify critical packages
    $required_pkgs = @(
        "mingw-w64-ucrt-x86_64-gcc",
        "mingw-w64-ucrt-x86_64-cmake",
        "mingw-w64-ucrt-x86_64-ninja",
        "mingw-w64-ucrt-x86_64-qt6-static",
        "mingw-w64-ucrt-x86_64-openssl",
        "mingw-w64-ucrt-x86_64-zlib",
        "mingw-w64-ucrt-x86_64-brotli",
        "mingw-w64-ucrt-x86_64-pcre2",
        "mingw-w64-ucrt-x86_64-libpng",
        "mingw-w64-ucrt-x86_64-libjpeg-turbo",
        "mingw-w64-ucrt-x86_64-libtiff",
        "mingw-w64-ucrt-x86_64-libwebp",
        "mingw-w64-ucrt-x86_64-freetype",
        "mingw-w64-ucrt-x86_64-harfbuzz"
    )

    Write-Info "Checking required MSYS2 packages..."
    $pkgs_check = @()
    $pkgs_check += "missing=0"
    foreach ($pkg in $required_pkgs) {
        $pkgs_check += "if pacman -Q '$pkg' 2>/dev/null; then echo '  [OK]  $pkg'; else echo '  [MISS] $pkg'; missing=1; fi"
    }
    $pkgs_check += "exit `$missing"
    $check_script = "#!/usr/bin/env bash`nset -e`n" + ($pkgs_check -join "`n") + "`n"
    $check_path = Join-Path $TEMP_DIR "check-pkgs.sh"
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($check_path, $check_script, $utf8)

    $proc = Start-Process -FilePath $bash_exe `
        -ArgumentList "-l", (ConvertTo-MsysPath $check_path) `
        -NoNewWindow -Wait -PassThru

    if ($proc.ExitCode -ne 0) {
        Write-Err "Missing MSYS2 packages detected."
        Write-Err "Install in MSYS2 UCRT64 terminal:"
        Write-Err "  pacman -S --needed $($required_pkgs -join ' ')"
        exit 1
    }

    Write-OK "All MSYS2 packages present"
    Write-OK "Qt6 static prefix: $Qt6Prefix"
}

# ============================================================================
# 4. Initialize Build Environment
# ============================================================================
function Initialize-BuildEnv {
    Write-Step "Build Environment Setup"

    if ($Clean) {
        if (Test-Path $TEMP_DIR) {
            Write-Info "Cleaning build temp: $TEMP_DIR"
            Remove-Item -Recurse -Force $TEMP_DIR -ErrorAction SilentlyContinue
        }
        Get-ChildItem "$DIST_DIR\*" -Include "netdiag-*", "build-*.log", "build-*.cmake", "build-*.ninja" -ErrorAction SilentlyContinue | Remove-Item -Force
        Write-Info "Cleaned dist/ artifacts and logs"
    }

    foreach ($d in @($TEMP_DIR, $DIST_DIR)) {
        if (-not (Test-Path $d)) {
            New-Item -ItemType Directory -Path $d -Force | Out-Null
        }
    }

    Write-OK "Temp:  $TEMP_DIR"
    Write-OK "Dist:  $DIST_DIR"

    # Output file names
    $script:PROD_NAME = "netdiag-$($script:BUILD_OS)-$($script:BUILD_ARCH)$($script:EXE_EXT)"
    $script:SIM_NAME  = "netdiag-$($script:BUILD_OS)-$($script:BUILD_ARCH)-sim$($script:EXE_EXT)"
}

# ============================================================================
# 5. App Static Build (Production + Simulator in parallel)
# ============================================================================
function Invoke-AppBuild {
    Write-Step "Static Build (Production + Simulator)"

    $build_prod = -not $SimOnly
    $build_sim  = -not $ProdOnly

    $build_number = (Get-Date -Format "yyyyMMdd") + "00"

    $build_script = @"
#!/usr/bin/env bash
set -euxo pipefail

export MSYSTEM=UCRT64
export PATH=/ucrt64/bin:/usr/bin:`$PATH

PROJ=`$(cygpath -u '$PROJECT_DIR')
DIST_DIR=`$(cygpath -u '$DIST_DIR')
BUILD_BASE=`$(cygpath -u '$TEMP_DIR/build')
QT6_PREFIX='$Qt6Prefix'

CMAKE_PREFIX_PATH="`$QT6_PREFIX"

STATIC_CXX_FLAGS="-static -static-libgcc -static-libstdc++ -O2"
STATIC_LINK_FLAGS="-static -static-libgcc -static-libstdc++ -Wl,--gc-sections $LINKER_GROUP_FLAGS"
STANDARD_LIBS="$WIN_SYS_LIBS"

echo "============================================"
echo "  App Static Build"
echo "  Qt6 prefix: `$QT6_PREFIX"
echo "  Project:    `$PROJ"
echo "  Dist:       `$DIST_DIR"
echo "============================================"
echo ""

# ---- Build function ----
build_target() {
    local target="`$1"      # net_diagnostics or net_diagnostics_sim
    local sim_flag="`$2"    # OFF or ON
    local out_name="`$3"    # output filename
    local log_base="`$4"    # log file prefix under DIST_DIR

    local build_dir="`${BUILD_BASE}/`${target}"

    echo ""
    echo "=============================================="
    echo "  Configuring: `$target (SIMULATOR=`$sim_flag)"
    echo "=============================================="

    rm -rf "`${build_dir}" && mkdir -p "`${build_dir}"
    cd "`${build_dir}"

    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DPROJECT_VERSION="0.0.1" \
        -DND_BUILD_NUMBER="$build_number" \
        -DAPP_EDITION=static \
        -DCMAKE_CXX_FLAGS="`${STATIC_CXX_FLAGS}" \
        -DCMAKE_EXE_LINKER_FLAGS="`${STATIC_LINK_FLAGS}" \
        -DCMAKE_CXX_STANDARD_LIBRARIES="`${STANDARD_LIBS}" \
        -DCMAKE_PREFIX_PATH="`${CMAKE_PREFIX_PATH}" \
        -DBUILD_SIMULATOR="`${sim_flag}" \
        -DNO_CURL=ON \
        -DBUILD_TESTS=OFF \
        "`${PROJ}" 2>&1 | tee "`${DIST_DIR}/`${log_base}.cmake"

    echo ""
    echo "=== Building: `$target ==="

    if [ "`$target" = "net_diagnostics_sim" ]; then
        ninja net_diagnostics_sim 2>&1 | tee "`${DIST_DIR}/`${log_base}.ninja"
    else
        ninja net_diagnostics 2>&1 | tee "`${DIST_DIR}/`${log_base}.ninja"
    fi

    # Strip debug symbols to reduce size
    echo ""
    echo "=== Stripping + copying ==="
    strip "`${target}.exe" 2>/dev/null || true
    cp "`${target}.exe" "`${DIST_DIR}/`${out_name}"

    local sz=`$(du -h "`${DIST_DIR}/`${out_name}" | cut -f1)
    echo "  -> `$DIST_DIR/`${out_name}  (`$sz)"
}

# ---- Launch builds in parallel ----
BUILD_PROD="$($build_prod.ToString().ToLower())"
BUILD_SIM="$($build_sim.ToString().ToLower())"

pids=()

if [ "`$BUILD_PROD" = "true" ]; then
    build_target "net_diagnostics" "OFF" "$($script:PROD_NAME)" "build-prod" &
    pids+=(`$!)
fi

if [ "`$BUILD_SIM" = "true" ]; then
    build_target "net_diagnostics_sim" "ON" "$($script:SIM_NAME)" "build-sim" &
    pids+=(`$!)
fi

# ---- Wait for all ----
echo "Waiting for `${#pids[@]} build(s) to complete..."
echo ""

failed=0
for pid in "`${pids[@]}"; do
    if ! wait "`$pid"; then
        echo "ERROR: build process `$pid failed"
        failed=1
    fi
done

if [ "`$failed" -ne 0 ]; then
    echo ""
    echo "FATAL: One or more builds failed."
    exit 1
fi

echo ""
echo "=== All builds completed successfully ==="
exit 0
"@

    $build_script_path = Join-Path $TEMP_DIR "build-app.sh"
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($build_script_path, $build_script, $utf8)

    Write-Info "Starting builds in parallel..."
    if ($build_prod) { Write-Info "  Production: $($script:PROD_NAME)" }
    if ($build_sim)  { Write-Info "  Simulator:  $($script:SIM_NAME)" }
    Write-Host ""

    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    $proc = Start-Process -FilePath $bash_exe `
        -ArgumentList "-l", (ConvertTo-MsysPath $build_script_path) `
        -NoNewWindow -Wait -PassThru

    if ($proc.ExitCode -ne 0) {
        Write-Err "App build FAILED (exit: $($proc.ExitCode))"
        Write-Err "Check logs in: $DIST_DIR"
        exit $proc.ExitCode
    }

    # Verify output
    if ($build_prod) {
        $p = Join-Path $DIST_DIR $script:PROD_NAME
        if (Test-Path $p) {
            $sz = [math]::Round((Get-Item $p).Length / 1MB, 1)
            Write-OK "Production: $($script:PROD_NAME) ($sz MB)"
        } else { Write-Err "Production not found: $p" }
    }
    if ($build_sim) {
        $p = Join-Path $DIST_DIR $script:SIM_NAME
        if (Test-Path $p) {
            $sz = [math]::Round((Get-Item $p).Length / 1MB, 1)
            Write-OK "Simulator:  $($script:SIM_NAME) ($sz MB)"
        } else { Write-Err "Simulator not found: $p" }
    }
}

# ============================================================================
# 6. Verify Zero Non-OS DLL
# ============================================================================
function Test-StaticLinkage {
    Write-Step "Static Linkage Verification (Zero non-OS DLL)"

    $verify_script = @"
#!/usr/bin/env bash
set -euo pipefail

DIST_DIR=`$(cygpath -u '$DIST_DIR')

# System DLLs that are always OK (OS-provided)
SYSTEM_DLLS="KERNEL32|KERNELBASE|USER32|GDI32|ADVAPI32|SHELL32|ole32|OLEAUT32|WS2_32|IPHLPAPI|DNSAPI|WINHTTP|WLANAPI|SETUPAPI|SHCORE|VERSION|IMM32|CRYPT32|RPCRT4|USP10|WLDAP32|NTDLL|bcrypt|ncrypt|NETAPI32|AUTHZ|UxTheme|comdlg32|dwmapi|SHLWAPI|Secur32|USERENV|WINMM|WSOCK32|D3D9|D3D11|D3D12|DXGI|DWRITE|WTSAPI32|api-ms-win|ext-ms-win|uuid"

FAIL=0

check_exe() {
    local exe="`$1"
    local label="`$2"
    [ -f "`$exe" ] || { echo "  SKIP: `$label — file not found"; return; }

    echo "  Checking: `$label"
    local sz=`$(du -h "`$exe" | cut -f1)
    echo "  Size: `$sz"

    local nonsys
    nonsys=`$(objdump -p "`$exe" 2>/dev/null | grep "DLL Name" | awk '{print `$3}' | tr -d `$'\r' | grep -viE "`$SYSTEM_DLLS" || true)

    if [ -n "`$nonsys" ]; then
        echo "  [FAIL] Non-OS DLL dependencies found:"
        printf '    *** %s ***\n' `$nonsys
        FAIL=1
    else
        echo "  [OK] Zero non-OS DLL dependencies (fully static)"
    fi
}

check_exe "`$DIST_DIR/$($script:PROD_NAME)" "Production"
echo ""
check_exe "`$DIST_DIR/$($script:SIM_NAME)" "Simulator"

echo ""

if [ "`$FAIL" -ne 0 ]; then
    echo "=============================================================="
    echo "  ERROR: build is NOT zero-DLL."
    echo "  The static link did not absorb the DLLs listed above."
    echo "=============================================================="
    exit 1
fi

echo "  ==============================================================="
echo "    All executables are fully static (zero non-OS DLL)"
echo "  ==============================================================="
echo ""
echo "  NOTE: G5 (Website / URL) diagnostics are DISABLED in the"
echo "  static build. NO_CURL=ON means 13 of 38 network diagnostic"
echo "  tests are skipped (HTTP, HTTPS, speed test, service banner)."
echo "  The dynamic Windows build includes full G5 diagnostics."
echo ""

exit 0
"@

    $verify_path = Join-Path $TEMP_DIR "verify-dll.sh"
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($verify_path, $verify_script, $utf8)

    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    $proc = Start-Process -FilePath $bash_exe `
        -ArgumentList "-l", (ConvertTo-MsysPath $verify_path) `
        -NoNewWindow -Wait -PassThru

    if ($proc.ExitCode -ne 0) {
        Write-Err "DLL verification FAILED — build is NOT fully static"
        exit $proc.ExitCode
    }

    Write-OK "Zero non-OS DLL verification PASSED"
}

# ============================================================================
# 7. Cleanup
# ============================================================================
function Invoke-Cleanup {
    if ($NoCleanTemp) {
        Write-Warn "Temp files retained: $TEMP_DIR"
        return
    }
    Write-Step "Cleanup"
    if (Test-Path $TEMP_DIR) {
        Remove-Item -Recurse -Force $TEMP_DIR -ErrorAction SilentlyContinue
        Write-OK "Removed temp dir: $TEMP_DIR"
    }
}

# ============================================================================
# 8. Final Report
# ============================================================================
function Show-Report {
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host "                   Build Results                           " -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor Green
    Write-Host ""

    $artifacts = Get-ChildItem $DIST_DIR -Filter "netdiag-*" -ErrorAction SilentlyContinue
    $logs = Get-ChildItem $DIST_DIR -Filter "build-*" -ErrorAction SilentlyContinue

    if ($artifacts) {
        Write-Host "  Executables (zero non-OS DLL):" -ForegroundColor White
        foreach ($f in $artifacts) {
            $size = if ($f.Length -gt 1MB) {
                "$([math]::Round($f.Length/1MB, 1)) MB"
            } else {
                "$([math]::Round($f.Length/1KB, 1)) KB"
            }
            $color = if ($f.Name -match "sim") { "Yellow" } else { "Cyan" }
            Write-Host "    $($f.Name)" -ForegroundColor $color -NoNewline
            Write-Host "  $size" -ForegroundColor Gray
        }
    }

    if ($logs) {
        Write-Host ""
        Write-Host "  Build Logs:" -ForegroundColor White
        foreach ($f in ($logs | Sort-Object Name)) {
            $size = "$([math]::Round($f.Length/1KB, 1)) KB"
            Write-Host "    $($f.Name)" -ForegroundColor DarkGray -NoNewline
            Write-Host "  $size" -ForegroundColor Gray
        }
    }

    Write-Host ""
    Write-Host "  Output directory: $DIST_DIR" -ForegroundColor White
    Write-Host ""
    Write-Host "  Run production:" -ForegroundColor Gray
    Write-Host "    .\dist\$($script:PROD_NAME)" -ForegroundColor White
    Write-Host ""
    Write-Host "  Run simulator:" -ForegroundColor Gray
    Write-Host "    .\dist\$($script:SIM_NAME)" -ForegroundColor White
    Write-Host ""

    Write-Host "  NOTE: G5 (Website/URL) diagnostics are disabled (NO_CURL=ON)." -ForegroundColor DarkYellow
    Write-Host "  Use the MSYS2 dynamic build for full 38-test diagnostics." -ForegroundColor DarkYellow
    Write-Host ""
}

# ============================================================================
# Main
# ============================================================================
try {
    Show-Banner
    Detect-Platform
    Initialize-BuildEnv
    Test-Dependencies
    Invoke-AppBuild
    Test-StaticLinkage
    Invoke-Cleanup
    Show-Report
}
catch {
    Write-Err "Build process exception: $_"
    Write-Err "Stack: $($_.ScriptStackTrace)"
    if (-not $NoCleanTemp) {
        Write-Info "Temp files retained for debugging: $TEMP_DIR"
    }
    exit 1
}
finally {
    $Host.UI.RawUI.WindowTitle = "PowerShell"
}
