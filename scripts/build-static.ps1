<#
.SYNOPSIS
    NetDiagnostic Static Build - Fully static linked compilation script
.DESCRIPTION
    Automated static compilation of production and simulator executables.
    No non-OS DLL dependencies (fully static linked).
    All temp files created under %%TEMP%% and auto-deleted on completion.
    Final output in dist/ directory.
    File naming: netdiag-{os}-{arch}.exe / netdiag-sim-{os}-{arch}.exe

.PARAMETER ProdOnly
    Build only production version (default: both)
.PARAMETER SimOnly
    Build only simulator version (default: both)
.PARAMETER Clean
    Clean previous build artifacts before building
.PARAMETER NoCleanTemp
    Keep temp files under %%TEMP%% (for debugging)
.PARAMETER MsysPath
    MSYS2 installation path (default: C:\msys64)
.PARAMETER Qt6StaticPath
    Static Qt6 cmake path (default: auto-detect MSYS2 ucrt64/qt6-static)

.EXAMPLE
    .\scripts\build-static.ps1
    Build both production + simulator

.EXAMPLE
    .\scripts\build-static.ps1 -ProdOnly -Clean
    Clean then build production only

.EXAMPLE
    .\scripts\build-static.ps1 -MsysPath D:\msys64
    Specify MSYS2 path
#>

param(
    [switch]$ProdOnly,
    [switch]$SimOnly,
    [switch]$Clean,
    [switch]$NoCleanTemp,
    [string]$MsysPath = "C:\msys64",
    [string]$Qt6StaticPath = ""
)

$ErrorActionPreference = "Stop"
$Host.UI.RawUI.WindowTitle = "NetDiagnostic Static Build"

# ============================================================================
# 0. Constants & Paths
# ============================================================================
$SCRIPT_DIR   = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$PROJECT_DIR  = (Resolve-Path (Split-Path -Parent $SCRIPT_DIR)).Path
$DIST_DIR     = Join-Path $PROJECT_DIR "dist"
$TEMP_DIR     = Join-Path $env:TEMP "netdiag-static-build"
$BUILD_DIR    = Join-Path $TEMP_DIR "build"
$CMAKE_LOG    = Join-Path $TEMP_DIR "cmake.log"
$NINJA_LOG    = Join-Path $TEMP_DIR "ninja.log"
$BASH_SCRIPT  = Join-Path $TEMP_DIR "build.sh"

# ============================================================================
# 1. Helper Functions
# ============================================================================
function Write-Info  { Write-Host "  [INFO]  $args" -ForegroundColor Cyan }
function Write-OK    { Write-Host "  [OK]    $args" -ForegroundColor Green }
function Write-Warn  { Write-Host "  [WARN]  $args" -ForegroundColor Yellow }
function Write-Err   { Write-Host "  [ERROR] $args" -ForegroundColor Red }
function Write-Step  { Write-Host "`n-- $args --" -ForegroundColor White }

function ConvertTo-MsysPath {
    param([string]$WinPath)
    ($WinPath -replace '\\','/' -replace '^([A-Za-z]):','/$1').ToLower()
}

function Show-Banner {
    Write-Host @"

====================================================
     NetDiagnostic Static Build
     Fully Static Linked - Zero non-OS DLL deps
====================================================

"@ -ForegroundColor Cyan
}

# ============================================================================
# 2. Platform Detection
# ============================================================================
function Detect-Platform {
    Write-Step "Platform Detection"

    $script:BUILD_OS = ""
    $script:BUILD_ARCH = ""

    # Cross-version OS detection (PS5/PS7 compatible)
    $plat = [System.Environment]::OSVersion.Platform
    $osEnv = [System.Environment]::GetEnvironmentVariable("OS")
    if ([string]::IsNullOrEmpty($osEnv)) { $osEnv = "" }
    if ($plat -eq "Win32NT" -or $osEnv -match "Windows") {
        $script:BUILD_OS = "win"
    }
    elseif ($plat -eq "Unix") {
        # PS7: $IsLinux/$IsMacOS; PS5: check further
        $script:BUILD_OS = "linux"
    }
    else {
        Write-Err "Cannot detect OS. Platform: $plat"
        Write-Err "Supported: Windows, Linux, macOS"
        exit 1
    }

    $arch = [System.Environment]::GetEnvironmentVariable("PROCESSOR_ARCHITECTURE")
    if ($arch -eq "AMD64" -or $arch -eq "x86_64" -or $arch -eq "EM64T") {
        $script:BUILD_ARCH = "x86_64"
    }
    elseif ($arch -eq "ARM64" -or $arch -eq "AArch64" -or $arch -eq "aarch64") {
        $script:BUILD_ARCH = "arm64"
    }
    else {
        Write-Err "Cannot detect CPU arch: $arch"
        exit 1
    }

    Write-OK "OS:   $script:BUILD_OS"
    Write-OK "CPU:  $script:BUILD_ARCH"
    Write-OK "Output: netdiag-$($script:BUILD_OS)-$($script:BUILD_ARCH).exe"
}

# ============================================================================
# 3. Dependency Check
# ============================================================================
function Test-Dependencies {
    Write-Step "Dependency Check"

    $script:MSYS2_OK = $false
    $script:QT6_STATIC_CMAKE = ""
    $script:MSYS2_ENV = ""

    # -- MSYS2 detection --
    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    if (Test-Path $bash_exe) {
        Write-OK "MSYS2 installed: $MsysPath"
        $script:MSYS2_OK = $true

        $ucrt64 = Join-Path $MsysPath "ucrt64"
        $mingw64 = Join-Path $MsysPath "mingw64"
        $clang64 = Join-Path $MsysPath "clang64"
        $clangarm64 = Join-Path $MsysPath "clangarm64"

        if ($script:BUILD_ARCH -eq "arm64" -and (Test-Path $clangarm64)) {
            $script:MSYS2_ENV = "clangarm64"
            Write-OK "MSYS2 env: CLANGARM64"
        }
        elseif ($script:BUILD_ARCH -eq "x86_64" -and (Test-Path $ucrt64)) {
            $script:MSYS2_ENV = "ucrt64"
            Write-OK "MSYS2 env: UCRT64"
        }
        elseif ($script:BUILD_ARCH -eq "x86_64" -and (Test-Path $mingw64)) {
            $script:MSYS2_ENV = "mingw64"
            Write-OK "MSYS2 env: MINGW64"
        }
        elseif ($script:BUILD_ARCH -eq "x86_64" -and (Test-Path $clang64)) {
            $script:MSYS2_ENV = "clang64"
            Write-OK "MSYS2 env: CLANG64"
        }
        else {
            Write-Err "MSYS2 installed but no matching compiler environment"
            Write-Err "  Need: ucrt64/mingw64 (x86_64) or clangarm64 (ARM64)"
            exit 1
        }
    }
    else {
        Write-Err "MSYS2 not found (expected: $MsysPath)"
        Write-Err ""
        Write-Err "Install MSYS2 (required):"
        Write-Err "  1. Download: https://www.msys2.org/"
        Write-Err "  2. After install, run MSYS2 UCRT64 terminal:"
        Write-Err "     pacman -Syu"
        Write-Err "     pacman -S mingw-w64-ucrt-x86_64-qt6-static"
        Write-Err "     pacman -S mingw-w64-ucrt-x86_64-curl"
        Write-Err "     pacman -S mingw-w64-ucrt-x86_64-cmake"
        Write-Err "     pacman -S mingw-w64-ucrt-x86_64-ninja"
        Write-Err "     pacman -S mingw-w64-ucrt-x86_64-gcc"
        Write-Err "  3. Or specify path: -MsysPath D:\\msys64"
        exit 1
    }

    # -- Static Qt6 detection --
    if ($Qt6StaticPath -ne "") {
        if (Test-Path (Join-Path $Qt6StaticPath "Qt6Config.cmake")) {
            $script:QT6_STATIC_CMAKE = $Qt6StaticPath
            Write-OK "Static Qt6 (specified): $Qt6StaticPath"
        }
        else {
            Write-Err "Invalid static Qt6 path: $Qt6StaticPath"
            exit 1
        }
    }
    else {
        $msys_env_dir = Join-Path $MsysPath $script:MSYS2_ENV
        $candidates = @(
            (Join-Path $msys_env_dir "qt6-static\lib\cmake\Qt6"),
            (Join-Path $msys_env_dir "lib\cmake\Qt6")
        )
        $found = $false
        foreach ($c in $candidates) {
            if (Test-Path (Join-Path $c "Qt6Config.cmake")) {
                $script:QT6_STATIC_CMAKE = $c
                $found = $true
                Write-OK "Static Qt6: $c"
                break
            }
        }
        if (-not $found) {
            Write-Err "Static Qt6 not found. In MSYS2 $($script:MSYS2_ENV) terminal:"
            Write-Err "  pacman -S mingw-w64-ucrt-x86_64-qt6-static"
            Write-Err "  pacman -S mingw-w64-ucrt-x86_64-qt6-imageformats"
            Write-Err "  pacman -S mingw-w64-ucrt-x86_64-qt6-svg"
            Write-Err ""
            Write-Err "Or use -Qt6StaticPath to specify static Qt6 cmake directory"
            exit 1
        }
    }

    # -- Static libcurl check --
    $msys_env_dir = Join-Path $MsysPath $script:MSYS2_ENV
    $curl_a = Join-Path $msys_env_dir "lib\libcurl.a"
    $curl_dll_a = Join-Path $msys_env_dir "lib\libcurl.dll.a"
    if ((Test-Path $curl_a) -and -not (Test-Path $curl_dll_a)) {
        Write-OK "Static libcurl: libcurl.a"
    }
    elseif (Test-Path $curl_a) {
        Write-Warn "libcurl.a exists but libcurl.dll.a also present (may not be static)"
    }
    else {
        Write-Warn "libcurl.a not found - install: pacman -S mingw-w64-ucrt-x86_64-curl"
    }
}

# ============================================================================
# 4. Build Environment Setup
# ============================================================================
function Initialize-BuildEnv {
    Write-Step "Build Environment Setup"

    if ($Clean) {
        if (Test-Path $TEMP_DIR) {
            Write-Info "Cleaning old build temp files..."
            Remove-Item -Recurse -Force $TEMP_DIR -ErrorAction SilentlyContinue
        }
        if (Test-Path $DIST_DIR) {
            Write-Info "Cleaning old artifacts..."
            Remove-Item (Join-Path $DIST_DIR "netdiag-*") -Force -ErrorAction SilentlyContinue
            Remove-Item (Join-Path $DIST_DIR "netdiag-sim-*") -Force -ErrorAction SilentlyContinue
        }
    }

    $dirs = @($TEMP_DIR, $BUILD_DIR, $DIST_DIR)
    foreach ($d in $dirs) {
        if (-not (Test-Path $d)) {
            New-Item -ItemType Directory -Path $d -Force | Out-Null
        }
    }
    Write-OK "Temp dir: $TEMP_DIR"
    Write-OK "Dist dir: $DIST_DIR"
}

# ============================================================================
# 5. Static Build (core)
# ============================================================================
function Invoke-StaticBuild {
    Write-Step "Static Compilation"

    $script:PROD_FLAG = "OFF"
    $script:SIM_FLAG  = "OFF"

    if ($ProdOnly) {
        $script:PROD_FLAG = "ON"; $script:SIM_FLAG = "OFF"
    }
    elseif ($SimOnly) {
        $script:PROD_FLAG = "OFF"; $script:SIM_FLAG = "ON"
    }
    else {
        $script:PROD_FLAG = "ON"; $script:SIM_FLAG = "ON"
    }

    $ext = ""
    if ($script:BUILD_OS -eq "win") { $ext = ".exe" }
    $script:PROD_NAME = "netdiag-$($script:BUILD_OS)-$($script:BUILD_ARCH)$ext"
    $script:SIM_NAME  = "netdiag-sim-$($script:BUILD_OS)-$($script:BUILD_ARCH)$ext"

    # -- Generate bash build script --
    # Conventions:
    #   `$VAR   -> Bash variable $VAR (backtick prevents PowerShell interpolation)
    #   $PS_VAR -> PowerShell value, embedded directly into bash script
    #
    # We pass Windows paths and let the bash script convert via cygpath
    # (MSYS2's native path converter), which handles junctions and OneDrive
    # redirects correctly.
    $bash_content = @"
#!/usr/bin/env bash
set -euo pipefail

# MSYS2 environment setup
export MSYSTEM=$($script:MSYS2_ENV.ToString().ToUpper())
export PATH=/$($script:MSYS2_ENV)/bin:/usr/bin:`$PATH
export PKG_CONFIG_PATH=/$($script:MSYS2_ENV)/lib/pkgconfig
export CMAKE_CXX_STANDARD=17
export CMAKE_CXX_STANDARD_REQUIRED=ON
export CURL_STATICLIB=1

# Convert Windows paths to MSYS2 Unix paths (handles OneDrive junctions etc.)
PROJ=`$(cygpath -u "$($PROJECT_DIR)")
BUILD=`$(cygpath -u "$($BUILD_DIR)")
DIST=`$(cygpath -u "$($DIST_DIR)")
TEMP_MSYS=`$(cygpath -u "$($TEMP_DIR)")
QT6_CMAKE=`$(cygpath -u "$($script:QT6_STATIC_CMAKE)")
export CMAKE_PREFIX_PATH="`$QT6_CMAKE/.."

# Static link flags - core: fully static, zero non-OS DLLs
# -static:              disable dynamic linking, use .a static libs only
# -static-libgcc:       statically link libgcc
# -static-libstdc++:    statically link libstdc++
# --start-group/--end-group already handled by CMakeLists.txt
STATIC_FLAGS="-static-libgcc -static-libstdc++ -static"
LINK_FLAGS="-static-libgcc -static-libstdc++ -static -Wl,--gc-sections"

# Capture compiler info first (avoids PS subexpression confusion)
COMPILER_VER=`$(gcc --version 2>/dev/null | head -1 || echo unknown)`
QT6_CHECK=`$(cmake --find-package -DNAME=Qt6 -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST 2>/dev/null || echo `$QT6_CMAKE)`

echo "==========================================="
echo "  Compiler: `$COMPILER_VER"
echo "  Qt6:      `$QT6_CHECK"
echo "==========================================="
echo ""

# -- Production version --
if [ "$($script:PROD_FLAG)" = "ON" ]; then
    echo "=== Building production: $($script:PROD_NAME) ==="
    BUILD_DIR="`${BUILD}/prod"
    rm -rf "`${BUILD_DIR}" && mkdir -p "`${BUILD_DIR}"
    cd "`${BUILD_DIR}"

    echo "  -> CMake configure..."
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="`${STATIC_FLAGS} -O2" \
        -DCMAKE_EXE_LINKER_FLAGS="`${LINK_FLAGS}" \
        -DCMAKE_C_FLAGS="`${STATIC_FLAGS} -O2" \
        -DCMAKE_PREFIX_PATH="`${QT6_CMAKE}/.." \
        -DBUILD_SIMULATOR=OFF \
        -DBUILD_TESTS=OFF \
        "`${PROJ}" > "`$DIST/cmake-prod.log" 2>&1

    echo "  -> Ninja build..."
    ninja net_diagnostic > "`$DIST/ninja-prod.log" 2>&1

    # Strip debug symbols to reduce size
    strip net_diagnostic$ext 2>/dev/null || true

    echo "  [DONE] Production: $($script:PROD_NAME)"
fi

# -- Simulator version --
if [ "$($script:SIM_FLAG)" = "ON" ]; then
    echo ""
    echo "=== Building simulator: $($script:SIM_NAME) ==="
    BUILD_DIR="`${BUILD}/sim"
    rm -rf "`${BUILD_DIR}" && mkdir -p "`${BUILD_DIR}"
    cd "`${BUILD_DIR}"

    echo "  -> CMake configure..."
    cmake -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="`${STATIC_FLAGS} -O2" \
        -DCMAKE_EXE_LINKER_FLAGS="`${LINK_FLAGS}" \
        -DCMAKE_C_FLAGS="`${STATIC_FLAGS} -O2" \
        -DCMAKE_PREFIX_PATH="`${QT6_CMAKE}/.." \
        -DBUILD_SIMULATOR=ON \
        -DBUILD_TESTS=OFF \
        "`${PROJ}" > "`$DIST/cmake-sim.log" 2>&1

    echo "  -> Ninja build..."
    ninja net_diagnostic_sim > "`$DIST/ninja-sim.log" 2>&1

    # Strip debug symbols to reduce size
    strip net_diagnostic_sim$ext 2>/dev/null || true

    echo "  [DONE] Simulator: $($script:SIM_NAME)"
fi

echo ""
echo "==========================================="
echo "  Static build complete"
echo "==========================================="

exit 0
"@

    # Write bash script to %TEMP% (Unix LF line endings)
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($BASH_SCRIPT, $bash_content, $utf8NoBom)
    Write-Info "Build script generated: $BASH_SCRIPT"

    # Execute build
    Write-Info "Starting MSYS2 static compilation..."
    Write-Info "(Large Qt6 projects may take 5-15 minutes for first build)"
    Write-Host ""

    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    $msys_script = ConvertTo-MsysPath $BASH_SCRIPT

    $proc = Start-Process -FilePath $bash_exe `
        -ArgumentList "-l", "$msys_script" `
        -NoNewWindow -Wait -PassThru

    Write-Host ""

    if ($proc.ExitCode -ne 0) {
        Write-Err "Build failed (exit code: $($proc.ExitCode))"
        Write-Err "Check logs at: $TEMP_DIR"
        exit $proc.ExitCode
    }

    # Copy artifacts from build dir to dist (PowerShell handles OneDrive redirects)
    $prod_build = Join-Path $BUILD_DIR "prod\net_diagnostic$ext"
    $sim_build  = Join-Path $BUILD_DIR "sim\net_diagnostic_sim$ext"
    if ($script:PROD_FLAG -eq "ON" -and (Test-Path $prod_build)) {
        # Retry up to 5 times for file-in-use
        $copied = $false
        for ($i = 1; $i -le 5; $i++) {
            try {
                Copy-Item $prod_build (Join-Path $DIST_DIR $script:PROD_NAME) -Force -ErrorAction Stop
                $copied = $true; break
            } catch {
                if ($i -lt 5) { Start-Sleep -Seconds 1 }
            }
        }
        if ($copied) { Write-OK "Copied: $($script:PROD_NAME)" }
        else { Write-Warn "Could not copy production (file in use)" }
    }
    if ($script:SIM_FLAG -eq "ON" -and (Test-Path $sim_build)) {
        # Retry up to 5 times for file-in-use (MSYS2 may hold handle briefly)
        $copied = $false
        for ($i = 1; $i -le 5; $i++) {
            try {
                Copy-Item $sim_build (Join-Path $DIST_DIR $script:SIM_NAME) -Force -ErrorAction Stop
                $copied = $true
                break
            } catch {
                if ($i -lt 5) { Start-Sleep -Seconds 1 }
            }
        }
        if ($copied) { Write-OK "Copied: $($script:SIM_NAME)" }
        else { Write-Warn "Could not copy simulator (file in use)" }
    }

    # Collect non-OS DLLs needed by the executables (MSYS2 Qt6-static limitation)
    # Filter: only non-system DLLs; copy from MSYS2 /ucrt64/bin to dist/
    $dll_collected = @{}
    $msys_bin_dir = Join-Path $MsysPath "$($script:MSYS2_ENV)\bin"
    $exe_paths = @()
    if ($script:PROD_FLAG -eq "ON") { $exe_paths += Join-Path $DIST_DIR $script:PROD_NAME }
    if ($script:SIM_FLAG -eq "ON") { $exe_paths += Join-Path $DIST_DIR $script:SIM_NAME }
    
    $system_dll_pattern = "api-ms|KERNEL|ADVAPI|GDI32|USER32|SHELL|ole32|WS2|ntdll|bcrypt|d3d|dxgi|DNSAPI|IPHLPAPI|SETUPAPI|WINHTTP|SHCORE|SHLWAPI|VERSION|WINMM|wlanapi|IMM32|dwmapi|DWrite|ncrypt|NETAPI|AUTHZ|UxTheme|comdlg32|CRYPT32|Secur32|USERENV|OLEAUT32|WTSAPI32|RPCRT4|USP10|WLDAP32"
    
    function Collect-Dlls {
        param([string]$exe)
        $dlls = & "$($MsysPath)\usr\bin\bash.exe" -l -c "export PATH=/ucrt64/bin:/usr/bin:`$PATH; objdump -p '$(ConvertTo-MsysPath $exe)' 2>/dev/null | grep 'DLL Name' | awk '{print `$3}' | grep -viE '$system_dll_pattern'" 2>$null
        foreach ($dll in $dlls) {
            $dll = $dll.Trim()
            if ($dll -and -not $dll_collected[$dll]) {
                $src = Join-Path $msys_bin_dir $dll
                $dst = Join-Path $DIST_DIR $dll
                if (Test-Path $src) {
                    Copy-Item $src $dst -Force -ErrorAction SilentlyContinue
                    $dll_collected[$dll] = $true
                    Write-Info "  DLL: $dll"
                    # Recursively check
                    Collect-Dlls $dst
                }
            }
        }
    }
    
    foreach ($exe in $exe_paths) {
        if (Test-Path $exe) {
            Write-Info "Collecting DLLs for: $(Split-Path $exe -Leaf)"
            Collect-Dlls $exe
        }
    }
    if ($dll_collected.Count -gt 0) {
        Write-Warn "Collected $($dll_collected.Count) non-OS DLL(s) — MSYS2 Qt6-static limitation"
        Write-Warn "For truly zero-DLL builds, Qt6 must be rebuilt from source with -static"
    }
}

# ============================================================================
# 6. Artifact Verification + DLL Dependency Check
# ============================================================================
function Test-StaticLinkage {
    Write-Step "Artifact Verification + Static Link Check"

    $ext = ""
    if ($script:BUILD_OS -eq "win") { $ext = ".exe" }

    $prod_path = Join-Path $DIST_DIR $script:PROD_NAME
    $sim_path  = Join-Path $DIST_DIR $script:SIM_NAME

    $script:ProdBuilt = $false
    $script:SimBuilt  = $false

    # Check artifacts exist
    if ($script:PROD_FLAG -eq "ON") {
        if (Test-Path $prod_path) {
            $prod_size = [math]::Round((Get-Item $prod_path).Length / 1MB, 1)
            Write-OK "Production: $($script:PROD_NAME) ($($prod_size) MB)"
            $script:ProdBuilt = $true
        } else {
            Write-Err "Production not generated: $prod_path"
        }
    }
    if ($script:SIM_FLAG -eq "ON") {
        if (Test-Path $sim_path) {
            $sim_size = [math]::Round((Get-Item $sim_path).Length / 1MB, 1)
            Write-OK "Simulator: $($script:SIM_NAME) ($($sim_size) MB)"
            $script:SimBuilt = $true
        } else {
            Write-Err "Simulator not generated: $sim_path"
        }
    }

    # DLL dependency check via MSYS2 objdump
    if (-not $script:MSYS2_OK) { return }

    $msys_dll_check = @'
#!/usr/bin/env bash
set -e

SYSTEM_DLLS="KERNEL32\.DLL|USER32\.DLL|GDI32\.DLL|ADVAPI32\.DLL|SHELL32\.DLL|ole32\.DLL|OLEAUT32\.DLL|WS2_32\.DLL|IPHLPAPI\.DLL|DNSAPI\.DLL|WINHTTP\.DLL|WLANAPI\.DLL|SETUPAPI\.DLL|SHCORE\.DLL|VERSION\.DLL|IMM32\.DLL|CRYPT32\.DLL|RPCRT4\.DLL|USP10\.DLL|WLDAP32\.DLL|NTDLL\.DLL|bcrypt\.dll|ncrypt\.dll|NETAPI32\.DLL|AUTHZ\.DLL|UxTheme\.DLL|comdlg32\.DLL|dwmapi\.DLL|SHLWAPI\.DLL|Secur32\.DLL|USERENV\.DLL|WINMM\.DLL|WSOCK32\.DLL|api-ms-win|ext-ms-win"

check_exe() {
    local exe="$1" label="$2"
    echo ""
    echo "-- $label --"
    if [ ! -f "$exe" ]; then
        echo "  SKIP: file not found"
        return
    fi
    local size_kb=$(du -k "$exe" | cut -f1)
    echo "  Size: ${size_kb} KB"

    local non_sys=0
    while IFS= read -r dll; do
        if ! echo "$dll" | grep -qiE "$SYSTEM_DLLS"; then
            echo "  [WARN] Non-system DLL: $dll"
            non_sys=1
        fi
    done < <(objdump -p "$exe" 2>/dev/null | grep "DLL Name" | awk '{print $3}')

    if [ $non_sys -eq 0 ]; then
        echo "  [OK] No external DLL deps (OS system DLLs only)"
    else
        echo "  [ERR] Non-system DLL deps found - static link incomplete"
    fi
}
'@

    $msys_dll_check += @"

check_exe "`$DIST/$($script:PROD_NAME)" "Production"
check_exe "`$DIST/$($script:SIM_NAME)" "Simulator"

exit 0
"@

    $check_path = Join-Path $TEMP_DIR "check-deps.sh"
    [System.IO.File]::WriteAllText($check_path, $msys_dll_check, (New-Object System.Text.UTF8Encoding $false))
    $msys_check = ConvertTo-MsysPath $check_path

    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    $null = Start-Process -FilePath $bash_exe `
        -ArgumentList "-l", "$msys_check" `
        -NoNewWindow -Wait
}

# ============================================================================
# 7. Cleanup
# ============================================================================
function Invoke-Cleanup {
    if ($NoCleanTemp) {
        Write-Warn "Temp files retained: $TEMP_DIR"
        return
    }
    Write-Step "Cleanup Temp Files"
    # Copy build logs to dist before deleting temp
    $logs = @("cmake-prod.log","ninja-prod.log","cmake-sim.log","ninja-sim.log")
    foreach ($log in $logs) {
        $src = Join-Path $TEMP_DIR $log
        if (Test-Path $src) { Copy-Item $src $DIST_DIR -Force -ErrorAction SilentlyContinue }
    }
    if (Test-Path $TEMP_DIR) {
        Remove-Item -Recurse -Force $TEMP_DIR -ErrorAction SilentlyContinue
        Write-OK "Deleted: $TEMP_DIR"
    }
    else {
        Write-Info "No temp files to clean"
    }
}

# ============================================================================
# 8. Final Report
# ============================================================================
function Show-Report {
    Write-Host ""
    Write-Host "====================================================" -ForegroundColor Green
    Write-Host "              Build Results                         " -ForegroundColor Green
    Write-Host "====================================================" -ForegroundColor Green
    Write-Host ""

    Get-ChildItem $DIST_DIR -Filter "netdiag-*" | ForEach-Object {
        $size = if ($_.Length -gt 1MB) {
            "$([math]::Round($_.Length/1MB, 1)) MB"
        } else {
            "$([math]::Round($_.Length/1KB, 1)) KB"
        }
        $color = if ($_.Name -match "sim") { "Yellow" } else { "Cyan" }
        Write-Host "  $($_.Name)" -ForegroundColor $color -NoNewline
        Write-Host "  $size" -ForegroundColor Gray
    }

    Write-Host ""
    Write-Host "Output directory: $DIST_DIR" -ForegroundColor White
    Write-Host ""

    $ext = if ($script:BUILD_OS -eq "win") { ".exe" } else { "" }
    Write-Host "Run production:" -ForegroundColor Gray
    Write-Host "  .\dist\netdiag-$($script:BUILD_OS)-$($script:BUILD_ARCH)${ext}" -ForegroundColor White
    Write-Host ""
    Write-Host "Run simulator:" -ForegroundColor Gray
    Write-Host "  .\dist\netdiag-sim-$($script:BUILD_OS)-$($script:BUILD_ARCH)${ext}" -ForegroundColor White
    Write-Host ""
}

# ============================================================================
# Main
# ============================================================================
try {
    Show-Banner
    Detect-Platform
    Test-Dependencies
    Initialize-BuildEnv
    Invoke-StaticBuild
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
