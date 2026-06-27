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
    MSYS2 installation path (default: $env:LOCALAPPDATA\msys64)
.PARAMETER QtVersion
    Qt version for source build (default: 6.11.1)

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
    [string]$MsysPath = "$env:LOCALAPPDATA\msys64",
    [string]$QtVersion = "6.11.1"
)

$ErrorActionPreference = "Stop"
$script:ORIGINAL_EAP = $ErrorActionPreference
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

# ============================================================================
# 2b. Auto-Install MSYS2 (if not found)
# ============================================================================
function Install-Msys2 {
    param([string]$TargetPath)
    
    Write-Step "Auto-Installing MSYS2"
    Write-Info "Target: $TargetPath"
    
    # Try winget first (Windows 11 / modern Windows 10)
    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if ($winget) {
        Write-Info "Using winget to install MSYS2..."
        winget install MSYS2.MSYS2 --location "$TargetPath" --accept-source-agreements --accept-package-agreements 2>&1 | Out-Null
        if (Test-Path (Join-Path $TargetPath "usr/bin/bash.exe")) {
            Write-OK "MSYS2 installed via winget"
            return $true
        }
        Write-Warn "winget install failed, trying direct download..."
    }
    
    # Fallback: download installer
    $installerUrl = "https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-x86_64-latest.exe"
    $installer = Join-Path $env:TEMP "msys2-installer.exe"
    
    Write-Info "Downloading MSYS2 installer..."
    try {
        Invoke-WebRequest -Uri $installerUrl -OutFile $installer -UseBasicParsing
    } catch {
        Write-Err "Failed to download MSYS2 installer: $_"
        return $false
    }
    
    Write-Info "Running MSYS2 installer (silent)..."
    $parent = Split-Path $TargetPath -Parent
    Start-Process -FilePath $installer -ArgumentList "in","--confirm-command","--accept-messages","--root","$TargetPath" -Wait -NoNewWindow
    Remove-Item $installer -Force -ErrorAction SilentlyContinue
    
    if (Test-Path (Join-Path $TargetPath "usr/bin/bash.exe")) {
        Write-OK "MSYS2 installed successfully"
        return $true
    }
    
    Write-Err "MSYS2 installation failed"
    return $false
}

# ============================================================================
# 2c. Install MSYS2 Build Packages (via pacman)
# ============================================================================
function Install-Msys2Packages {
    param([string]$MsysRoot, [string]$EnvName)
    
    Write-Step "Installing MSYS2 Build Dependencies"
    
    $bash = Join-Path $MsysRoot "usr/bin/bash.exe"
    if (-not (Test-Path $bash)) {
        Write-Err "bash.exe not found at $bash"
        return $false
    }
    
    $packages = @(
        "mingw-w64-ucrt-x86_64-cmake",
        "mingw-w64-ucrt-x86_64-ninja",
        "mingw-w64-ucrt-x86_64-gcc",
        "mingw-w64-ucrt-x86_64-perl",
        "mingw-w64-ucrt-x86_64-python",
        "mingw-w64-ucrt-x86_64-pkg-config",
        "mingw-w64-ucrt-x86_64-openssl",
        "mingw-w64-ucrt-x86_64-zlib",
        "mingw-w64-ucrt-x86_64-brotli",
        "mingw-w64-ucrt-x86_64-pcre2",
        "mingw-w64-ucrt-x86_64-libb2"
    )
    # Note: Qt6 is always built from source (bundled 3rdparty → zero DLL).
    # Qt6 source is cached at $env:LOCALAPPDATA\netdiag-qt6-static after first build.
    
    $pacman_cmd = "pacman -S --noconfirm --needed " + ($packages -join " ")
    
    Write-Info "Running: pacman -S --noconfirm --needed <packages>"
    Write-Info "This may take several minutes on first run..."
    
    # Temporarily allow non-terminating errors (pacman warnings are normal)
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    
    # Step 1: Initialize pacman keyring and sync databases
    Write-Info "Initializing pacman (keyring + database sync)..."
    $init_cmd = "export MSYSTEM=$EnvName; export PATH=/$EnvName/bin:/usr/bin:`$PATH; sed -i '/^XferCommand/d' /etc/pacman.conf 2>/dev/null; echo 'XferCommand = /usr/bin/curl -kL -C - -f -o %o %u' >> /etc/pacman.conf; pacman-key --init 2>&1; pacman-key --populate 2>&1; echo 'Server = http://mirrors.ustc.edu.cn/msys2/msys/`$arch' > /etc/pacman.d/mirrorlist.msys; echo 'Server = http://mirrors.ustc.edu.cn/msys2/mingw/ucrt64' > /etc/pacman.d/mirrorlist.ucrt64; echo 'Server = http://mirrors.ustc.edu.cn/msys2/mingw/mingw64' > /etc/pacman.d/mirrorlist.mingw64; pacman -Sy --noconfirm 2>&1"
    $tmpLog = Join-Path $SCRIPT_DIR "netdiag-pacman-init.log"
    & $bash -lc $init_cmd 2>&1 | Tee-Object -FilePath $tmpLog
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "pacman init may have warnings (see: $tmpLog)"
    }
    
    # Step 2: Install packages
    Write-Info "Installing packages..."
    $env_cmd = "export MSYSTEM=$EnvName; export PATH=/$EnvName/bin:/usr/bin:`$PATH; $pacman_cmd 2>&1"
    $tmpLog = Join-Path $SCRIPT_DIR "netdiag-pacman-install.log"
    & $bash -lc $env_cmd 2>&1 | Tee-Object -FilePath $tmpLog
    if ($LASTEXITCODE -ne 0) {
        Write-Warn "Some packages may have failed to install (see: $tmpLog)"
        Write-Warn "Try running manually: pacman -Syu && pacman -S <packages>"
        Write-Info "--- Last 10 lines of pacman output ---"
        Get-Content $tmpLog -Tail 10 | ForEach-Object { Write-Host "  $_" }
    } else {
        Write-OK "Build dependencies installed"
    }
    $ErrorActionPreference = $prevEAP
}

# ============================================================================
# 2d. Build Qt6 from source (bundled 3rdparty → zero DLL)
# ============================================================================
function Invoke-Qt6SourceBuild {
    Write-Step "Qt6 Source Build (Zero-DLL)"

    $Qt6CacheRoot = "$env:LOCALAPPDATA\netdiag-qt6-static"
    $script:QT6_INSTALL_DIR = Join-Path $Qt6CacheRoot "install"
    $qtConfigCm = Join-Path $script:QT6_INSTALL_DIR "lib\cmake\Qt6\Qt6Config.cmake"

    if (Test-Path $qtConfigCm) {
        Write-OK "Qt6 source build cache hit: $($script:QT6_INSTALL_DIR)"
        return
    }

    Write-Info "Qt6 not cached — building from source (one-time, ~15-20 min)..."
    Write-Info "Source cache: $Qt6CacheRoot"

    # Clean previous partial builds
    if (Test-Path $Qt6CacheRoot) {
        Remove-Item -Recurse -Force $Qt6CacheRoot -ErrorAction SilentlyContinue
    }
    $srcDir = Join-Path $Qt6CacheRoot "src"
    $buildDir = Join-Path $Qt6CacheRoot "build"
    New-Item -ItemType Directory -Path $srcDir -Force | Out-Null
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    New-Item -ItemType Directory -Path $script:QT6_INSTALL_DIR -Force | Out-Null

    # Generate bash build script for Qt6 source build
    $qtBuildScript = Join-Path $TEMP_DIR "build-qt6.sh"
    $qtSrcMsys = ConvertTo-MsysPath $srcDir
    $qtBuildMsys = ConvertTo-MsysPath $buildDir
    $qtInstallMsys = ConvertTo-MsysPath $script:QT6_INSTALL_DIR
    $cmakeCacheMsys = ConvertTo-MsysPath (Join-Path $TEMP_DIR "cmake-qt6.log")
    $ninjaLogQtMsys = ConvertTo-MsysPath (Join-Path $TEMP_DIR "ninja-qt6.log")

    $qt_bash_content = @"
#!/usr/bin/env bash
set -euxo pipefail

export MSYSTEM=$($script:MSYS2_ENV.ToString().ToUpper())
export PATH=/$($script:MSYS2_ENV)/bin:/usr/bin:`$PATH

QT_VER="$($QtVersion)"
QT_MAJMIN="`${QT_VER%.*}"

echo "==========================================="
echo "  Building Qt `$QT_VER from source (Zero-DLL)"
echo "  Submodules: qtbase qtshadertools qtdeclarative qttools"
echo "  Source dir: $($qtSrcMsys)"
echo "  Build dir:  $($qtBuildMsys)"
echo "  Install to: $($qtInstallMsys)"
echo "==========================================="

# --- Download source (if not cached) ---
if [ ! -f "$($qtSrcMsys)/CMakeLists.txt" ]; then
    echo ">>> Downloading Qt `$QT_VER source..."
    curl -fsSL "https://download.qt.io/official_releases/qt/`${QT_MAJMIN}/`${QT_VER}/single/qt-everywhere-src-`${QT_VER}.tar.xz" \
        -o /tmp/qt-src.tar.xz
    echo ">>> Extracting..."
    tar -xJf /tmp/qt-src.tar.xz -C "$($qtSrcMsys)" --strip-components=1
    rm /tmp/qt-src.tar.xz
else
    echo ">>> Qt source already downloaded"
fi

cd "$($qtSrcMsys)"

echo ">>> Configuring (submodules: qtbase,qtshadertools,qtdeclarative,qttools)..."
cmake -G Ninja -B "$($qtBuildMsys)" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$($qtInstallMsys)" \
    -DCMAKE_CXX_FLAGS="-static -static-libgcc -static-libstdc++ -O2" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static" \
    -DCMAKE_C_FLAGS="-static -static-libgcc -O2" \
    -DFEATURE_static=ON -DFEATURE_shared=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DFEATURE_pch=ON -DFEATURE_icu=OFF -DFEATURE_dbus=OFF \
    -DFEATURE_system_zlib=OFF \
    -DFEATURE_system_brotli=OFF \
    -DFEATURE_system_pcre2=OFF \
    -DQT_BUILD_SUBMODULES="qtbase;qtshadertools;qtdeclarative;qttools" \
    -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
    2>&1 | tee "$($cmakeCacheMsys)"

echo ">>> Building (4 submodules)..."
cmake --build "$($qtBuildMsys)" --parallel 2>&1 | tee "$($ninjaLogQtMsys)"

echo ">>> Installing..."
cmake --install "$($qtBuildMsys)"

echo ">>> Qt `$QT_VER source build complete."
exit 0
"@

    Write-Info "Writing Qt6 build script..."
    $utf8NoBom = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($qtBuildScript, $qt_bash_content, $utf8NoBom)

    # Run the Qt6 build
    $bash_exe = Join-Path $MsysPath "usr\bin\bash.exe"
    $msys_script = ConvertTo-MsysPath $qtBuildScript

    Write-Info "Starting Qt6 source build (this will take 15-20 minutes on first run)..."
    Write-Host ""

    $proc = Start-Process -FilePath $bash_exe `
        -ArgumentList "-l", "$msys_script" `
        -NoNewWindow -Wait -PassThru

    Write-Host ""
    if ($proc.ExitCode -ne 0) {
        Write-Err "Qt6 source build failed (exit code: $($proc.ExitCode))"
        Write-Err "Check logs at: $TEMP_DIR (cmake-qt6.log, ninja-qt6.log)"
        exit $proc.ExitCode
    }

    if (Test-Path $qtConfigCm) {
        Write-OK "Qt6 source build cached at: $($script:QT6_INSTALL_DIR)"
    } else {
        Write-Err "Qt6 install succeeded but Qt6Config.cmake not found at: $qtConfigCm"
        exit 1
    }
}

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
        Write-Warn "MSYS2 not found at $MsysPath"
        Write-Info "Attempting auto-install..."
        $installed = Install-Msys2 -TargetPath $MsysPath
        if (-not $installed) {
            Write-Err "MSYS2 auto-install failed."
            Write-Err "Manual install: https://www.msys2.org/"
            Write-Err "Or specify path: -MsysPath D:\msys64"
            exit 1
        }
        Write-Info "Installing build dependencies (first time may take ~5 min)..."
        Install-Msys2Packages -MsysRoot $MsysPath -EnvName "UCRT64"
        $script:MSYS2_OK = $true
        $script:MSYS2_ENV = "ucrt64"
    }

    # -- Static Qt6 detection —
    # Qt6 is always built from source with bundled 3rdparty libraries.
    # This is the only way to achieve true zero-DLL output: the Qt6 third-party
    # dependencies (zlib, pcre2, brotli, libb2, png, freetype, etc.) must be
    # compiled into the Qt6 static libs, not referenced as external DLLs.
    # Cached at $env:LOCALAPPDATA\netdiag-qt6-static — first build ~15-20 min,
    # subsequent builds < 1 min (cache hit).
    Invoke-Qt6SourceBuild
    $script:QT6_STATIC_CMAKE = Join-Path $script:QT6_INSTALL_DIR "lib\cmake\Qt6"
    Write-OK "Static Qt6 (source-built, zero-DLL): $($script:QT6_STATIC_CMAKE)"

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
export CMAKE_PREFIX_PATH="`$QT6_CMAKE/../.."

# Common static cmake flags — fully static, zero non-OS DLLs
# Bash array keeps flags safely quoted (paths with spaces survive)
CMAKE_COMMON=(
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++ -static -Wl,-Bstatic -O2"
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++ -static -Wl,-Bstatic -Wl,--gc-sections"
    -DCMAKE_FIND_LIBRARY_SUFFIXES=".a"
    -DCMAKE_LINK_SEARCH_START_STATIC=ON
    -DCMAKE_C_FLAGS="-static-libgcc -static -Wl,-Bstatic -O2"
    -DNO_CURL=ON
    -DBUILD_TESTS=OFF
)

# Capture compiler info first (avoids PS subexpression confusion)
COMPILER_VER=`$(gcc --version 2>/dev/null | head -1 || echo unknown)`
QT6_CHECK=`$(cmake --find-package -DNAME=Qt6 -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST 2>/dev/null || echo `$QT6_CMAKE)`

echo "==========================================="
echo "  Compiler: `$COMPILER_VER"
echo "  Qt6:      `$QT6_CHECK"
echo "==========================================="

# Build one target — used for both production and simulator
# Arguments: `$1=label(prod|sim) `$2=ninja-target `$3=BUILD_SIMULATOR(OFF|ON)
build_target() {
    set -e
    local label="`$1" target="`$2" sim_flag="`$3"
    local build_dir="`${BUILD}/`${label}"

    echo ""
    echo "=== Building `$label: `$target ==="
    rm -rf "`${build_dir}" && mkdir -p "`${build_dir}"
    cd "`${build_dir}"

    echo "  -> CMake configure (`$label)..."
    cmake "`${CMAKE_COMMON[@]}" \
        -DQt6_DIR="`${QT6_CMAKE}" \
        -DBUILD_SIMULATOR="`${sim_flag}" \
        "`${PROJ}" 2>&1 | tee "`${DIST}/cmake-`${label}.log"

    echo "  -> Ninja build (`$label)..."
    ninja "`${target}" 2>&1 | tee "`${DIST}/ninja-`${label}.log"

    # Strip debug symbols to reduce size
    strip "`${target}$($ext)" 2>/dev/null || true

    echo "  [DONE] `$label: `$target"
}

# --- Main ---
EXIT_PROD=0
EXIT_SIM=0

if [ "$($script:PROD_FLAG)" = "ON" ] && [ "$($script:SIM_FLAG)" = "ON" ]; then
    # Parallel build — independent build trees, no conflicts
    ( build_target "prod" "net_diagnostic" "OFF" ) &
    PID_PROD=`$!
    ( build_target "sim" "net_diagnostic_sim" "ON" ) &
    PID_SIM=`$!
    wait `$PID_PROD || EXIT_PROD=`$?
    wait `$PID_SIM || EXIT_SIM=`$?
elif [ "$($script:PROD_FLAG)" = "ON" ]; then
    build_target "prod" "net_diagnostic" "OFF"
elif [ "$($script:SIM_FLAG)" = "ON" ]; then
    build_target "sim" "net_diagnostic_sim" "ON"
fi

if [ `$EXIT_PROD -ne 0 ] || [ `$EXIT_SIM -ne 0 ]; then
    echo "ERROR: Build failed (prod=`$EXIT_PROD sim=`$EXIT_SIM)" >&2
    exit 1
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

    # Pre-build static analysis — non-fatal, skip on syntax error
    $prebuildScript = Join-Path $SCRIPT_DIR "prebuild-check.ps1"
    if (Test-Path $prebuildScript) {
        Write-Step "Pre-Build Static Analysis"
        try {
            $prebuildResult = & $prebuildScript -MsysPath $MsysPath 2>&1
            $prebuildResult | ForEach-Object { Write-Host $_ }
            if ($LASTEXITCODE -ge 2) {
                Write-Err "Pre-build check found critical errors — aborting build."
                exit 2
            }
            Write-OK "Pre-build checks passed"
        } catch {
            Write-Warn "Pre-build check skipped (syntax error in prebuild-check.ps1 — will fix later)"
        }
    }

    Detect-Platform
    Initialize-BuildEnv
    Test-Dependencies
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