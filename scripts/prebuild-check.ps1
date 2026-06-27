<#
.SYNOPSIS
    Pre-build static analysis — catch common issues BEFORE building
.DESCRIPTION
    Checks for known failure patterns without running cmake/ninja:
    1. Path space-safety — all paths passed to external tools must handle spaces
    2. cmake variable semantics — Package_DIR vs CMAKE_PREFIX_PATH
    3. Fallback path integrity — fallback paths must match target semantics
    4. MSYS2 static library completeness — Qt6 .a files must exist

    Exit code 0 = all checks passed (safe to build)
    Exit code 1 = warnings found (may still build)
    Exit code 2 = errors found (build will fail)
#>

param(
    [string]$MsysPath = "$env:LOCALAPPDATA\msys64"
)

$ErrorActionPreference = "Stop"
$ExitCode = 0
$Warnings = 0
$Errors = 0

function Write-Check { Write-Host "  [CHECK] $args" -ForegroundColor Cyan }
function Write-Pass  { Write-Host "  [PASS]  $args" -ForegroundColor Green; $script:Passes++ }
function Write-Warn  { Write-Host "  [WARN]  $args" -ForegroundColor Yellow; $script:Warnings++; $script:ExitCode = [Math]::Max($script:ExitCode, 1) }
function Write-Fail  { Write-Host "  [FAIL]  $args" -ForegroundColor Red; $script:Errors++; $script:ExitCode = 2 }

Write-Host @"

====================================================
     Pre-Build Static Analysis
     Detecting known failure patterns...
====================================================

"@ -ForegroundColor Cyan

$PROJECT_DIR = if ($PSScriptRoot) { (Resolve-Path (Split-Path -Parent $PSScriptRoot)).Path } else { (Get-Location).Path }

# ============================================================================
# 1. PATH SPACE-SAFETY SCAN
# ============================================================================
Write-Host "-- 1. Path Space-Safety Scan --" -ForegroundColor White

# 1a. Check project source path for spaces (affects CMAKE_SOURCE_DIR everywhere)
if ($PROJECT_DIR -match '\s') {
    Write-Warn "Project path contains spaces: $PROJECT_DIR"
    Write-Warn "  → All CMakeLists.txt references to `$" + "{CMAKE_SOURCE_DIR} must be quoted"

    # Scan CMakeLists.txt for unquoted CMAKE_SOURCE_DIR usage in flag variables
    $cmakeFile = Join-Path $PROJECT_DIR "CMakeLists.txt"
    if (Test-Path $cmakeFile) {
        $lines = Get-Content $cmakeFile
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $line = $lines[$i]
            # Check: ${CMAKE_SOURCE_DIR} in a set() for FLAGS but without \" quoting
            if ($line -match 'CMAKE_SOURCE_DIR' -and $line -match 'FLAGS' -and $line -notmatch '\\\\"') {
                Write-Fail "  CMakeLists.txt:$($i+1): CMAKE_SOURCE_DIR in FLAGS without \" quoting"
                Write-Fail "    $($line.Trim())"
                Write-Fail "    Fix: use -I\\\"`${CMAKE_SOURCE_DIR}/...\\\" (cmake escaped quotes)"
            }
        }
    }
} else {
    Write-Pass "Project path has no spaces: $PROJECT_DIR"
}

# 1b. Check OneDrive path specifically
if ($PROJECT_DIR -match 'OneDrive') {
    Write-Check "OneDrive path detected — extra vigilance needed for path quoting"
}

# ============================================================================
# 2. CMAKE VARIABLE SEMANTICS CHECK
# ============================================================================
Write-Host "-- 2. cmake Variable Semantics --" -ForegroundColor White

$ps1File = Join-Path $PROJECT_DIR "scripts\build-static.ps1"
if (Test-Path $ps1File) {
    $content = Get-Content $ps1File -Raw

    # 2a. Check: if QT6_CMAKE is known (verified path), should use -DQt6_DIR not -DCMAKE_PREFIX_PATH
    if ($content -match 'QT6_CMAKE' -and $content -match 'CMAKE_PREFIX_PATH.*QT6_CMAKE') {
        Write-Warn "Using CMAKE_PREFIX_PATH with known QT6_CMAKE path"
        Write-Warn "  → Prefer -DQt6_DIR=`$" + "{QT6_CMAKE} for exact package location"
        Write-Warn "  → CMAKE_PREFIX_PATH is a search hint (cmake can silently fall back to system paths)"
        Write-Warn "  → Qt6_DIR is an exact directive (cmake uses ONLY this path, or fails clearly)"
    } else {
        Write-Pass "Qt6 path handling uses exact Qt6_DIR directive"
    }
}

# ============================================================================
# 3. MSYS2 STATIC LIBRARY INTEGRITY CHECK
# ============================================================================
Write-Host "-- 3. MSYS2 Static Library Integrity --" -ForegroundColor White

$msysUcrt = Join-Path $MsysPath "ucrt64"
$staticQt6Dir = Join-Path $msysUcrt "qt6-static\lib\cmake\Qt6"
$systemQt6Dir = Join-Path $msysUcrt "lib\cmake\Qt6"

# 3a. Check if static Qt6 exists
if (Test-Path (Join-Path $staticQt6Dir "Qt6Config.cmake")) {
    Write-Pass "Static Qt6 found: $staticQt6Dir"

    # 3b. Verify it's actually static (contains .a files, not just .dll.a)
    $staticLibDir = Join-Path $msysUcrt "qt6-static\lib"
    if (Test-Path $staticLibDir) {
        $aFiles = Get-ChildItem -Path $staticLibDir -Filter "libQt6Core.a" -ErrorAction SilentlyContinue
        $dllFiles = Get-ChildItem -Path $staticLibDir -Filter "libQt6Core.dll.a" -ErrorAction SilentlyContinue
        if ($aFiles) {
            Write-Pass "  Verified: libQt6Core.a exists (true static library)"
        } elseif ($dllFiles -and -not $aFiles) {
            Write-Warn "  libQt6Core.dll.a found but no libQt6Core.a — may be import library, not static"
        }
    }

    # 3c. Check Qt6Quick (required by project)
    if (Test-Path (Join-Path $staticQt6Dir "Qt6QuickConfig.cmake")) {
        Write-Pass "  Qt6QuickConfig.cmake found"
    } else {
        Write-Fail "  Qt6QuickConfig.cmake NOT found — install: pacman -S mingw-w64-ucrt-x86_64-qt6-declarative-static"
    }
} else {
    Write-Fail "Static Qt6 NOT found at: $staticQt6Dir"
    Write-Fail "  Install: pacman -S mingw-w64-ucrt-x86_64-qt6-static"
}

# 3d. Check: system Qt6 must NOT be used as fallback
if (Test-Path (Join-Path $systemQt6Dir "Qt6Config.cmake")) {
    Write-Check "System Qt6 exists at: $systemQt6Dir"
    Write-Check "  This is the DYNAMIC Qt6 — must NOT be used as static build fallback"
    Write-Check "  Verify that build script never falls back to this path"
}

# ============================================================================
# 4. BASH SCRIPT ROBUSTNESS CHECK
# ============================================================================
Write-Host "-- 4. Bash Script Robustness --" -ForegroundColor White

if ($content) {
    # 4a. Check pipefail is set (prevents pipe masking command failures)
    if ($content -match 'set\s+-.*pipefail' -or $content -match 'set\s+-euo\s+pipefail') {
        Write-Pass "bash: set -o pipefail is configured"
    } else {
        Write-Warn "bash: set -o pipefail NOT found — cmake | tee failures may be masked"
    }

    # 4b. Check for cmake in pipeline (| tee) followed by critical commands
    if ($content -match 'cmake.*\|\s*tee') {
        if (-not ($content -match 'pipefail')) {
            Write-Warn "cmake output piped to tee without pipefail — cmake errors may be silently ignored"
        }
    }
}

# ============================================================================
# 5. WINDOWS RESOURCE COMPILER CHECK
# ============================================================================
Write-Host "-- 5. Windows RC Compiler Path Safety --" -ForegroundColor White

if (Test-Path (Join-Path $PROJECT_DIR "resources\netanalysis.rc")) {
    Write-Check "netanalysis.rc exists — will be compiled by windres"
    if ($PROJECT_DIR -match '\s') {
        $rcLine = (Get-Content (Join-Path $PROJECT_DIR "CMakeLists.txt") | Select-String 'CMAKE_RC_FLAGS').Line
        if ($rcLine -match '\\\\"') {
            Write-Pass "CMAKE_RC_FLAGS path is quoted (safe for spaces)"
        } else {
            Write-Fail "CMAKE_RC_FLAGS path NOT quoted — windres will fail with spaces in path"
            Write-Fail "  Fix: set(CMAKE_RC_FLAGS ... -I\\\"`${CMAKE_SOURCE_DIR}/resources\\\")"
        }
    } else {
        Write-Pass "Project path has no spaces — RC compilation safe"
    }
}

# ============================================================================
# SUMMARY
# ============================================================================
Write-Host ""
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "  Pre-Build Check Summary" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "  Errors  : $Errors" -ForegroundColor $(if ($Errors -gt 0) { "Red" } else { "Green" })
Write-Host "  Warnings: $Warnings" -ForegroundColor $(if ($Warnings -gt 0) { "Yellow" } else { "Green" })
Write-Host "  Exit code: $ExitCode" -ForegroundColor $(if ($ExitCode -ne 0) { "Red" } else { "Green" })

if ($ExitCode -eq 0) {
    Write-Host "`n  ✅ Ready to build." -ForegroundColor Green
} elseif ($ExitCode -eq 1) {
    Write-Host "`n  ⚠️  Warnings found — build may still succeed." -ForegroundColor Yellow
} else {
    Write-Host "`n  ❌ Errors found — fix them before building." -ForegroundColor Red
}

exit $ExitCode
