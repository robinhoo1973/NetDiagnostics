<#
.SYNOPSIS
    CI Test + Code Review Enforcement Script
.DESCRIPTION
    Runs in GitHub Actions. Validates:
    1. Build with ND_TESTING=ON compiles
    2. Headless tests pass (--test mode)
    3. Code review compliance (5WHY in recent commits, preprocessor balance)
    4. Exit 0 if all pass, 1 if any check fails.
.NOTES
    Invoked by .github/workflows/test.yml
#>
param([switch]$SkipBuild)

$ErrorActionPreference = "Stop"
$FAILURES = 0

function Pass { Write-Host "  [PASS] $args" -ForegroundColor Green }
function Fail { Write-Host "  [FAIL] $args" -ForegroundColor Red; $script:FAILURES++ }
function Step { Write-Host "`n=== $args ===" -ForegroundColor Cyan }

Push-Location $PSScriptRoot\..

# ═════════════════════════════════════════════════════════════════════════════
# 1. Build with ND_TESTING=ON
# ═════════════════════════════════════════════════════════════════════════════
Step "1. Build (ND_TESTING=ON)"
if (-not $SkipBuild) {
    & .\scripts\build-static.ps1 -ProdOnly -Testing -NoCleanTemp
    if ($LASTEXITCODE -ne 0) { Fail "Build failed"; Pop-Location; exit 1 }
    Pass "Build succeeded"
}

# ═════════════════════════════════════════════════════════════════════════════
# 2. Run headless tests
# ═════════════════════════════════════════════════════════════════════════════
Step "2. Headless tests (--test mode)"
$testLog = "$env:TEMP\netdiag-ci-test-$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
$proc = Start-Process -FilePath ".\dist\netdiag-win-x86_64.exe" `
    -ArgumentList "--test" -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $testLog
if ($proc.ExitCode -eq 0) {
    Pass "All headless tests passed"
} else {
    Fail "Headless tests failed (exit $($proc.ExitCode)). Log: $testLog"
}

# ═════════════════════════════════════════════════════════════════════════════
# 3. Code review compliance checks
# ═════════════════════════════════════════════════════════════════════════════
Step "3. Code review compliance"

# 3a. Recent commits have 5WHY analysis
$recentCommits = git log --oneline -10 --format="%s"
$whyCount = ($recentCommits | Select-String "Why #1" | Measure-Object).Count
if ($whyCount -gt 0) {
    Pass "5WHY analysis found in $whyCount recent commit(s)"
} else {
    Fail "No 5WHY analysis in last 10 commits (searching for 'Why #1')"
}

# 3b. Preprocessor balance check
$imbalance = @()
Get-ChildItem -Path src -Recurse -Include *.cpp,*.h | ForEach-Object {
    $ifCount = (Select-String -Path $_.FullName -Pattern '^#if|^#ifdef|^#ifndef' | Measure-Object).Count
    $endCount = (Select-String -Path $_.FullName -Pattern '^#endif' | Measure-Object).Count
    if ($ifCount -ne $endCount) {
        $imbalance += "$($_.Name): #if=$ifCount #endif=$endCount"
    }
}
if ($imbalance.Count -eq 0) {
    Pass "Preprocessor balance: all files OK"
} else {
    Fail "Preprocessor imbalance in $($imbalance.Count) file(s):`n    $($imbalance -join "`n    ")"
}

# 3c. CODE_REVIEW_CHECKLIST.md exists
if (Test-Path "docs\CODE_REVIEW_CHECKLIST.md") {
    Pass "CODE_REVIEW_CHECKLIST.md exists"
} else {
    Fail "docs/CODE_REVIEW_CHECKLIST.md not found"
}

# 3d. 5WHY-TEMPLATE.md exists
if (Test-Path "docs\5WHY-TEMPLATE.md") {
    Pass "5WHY-TEMPLATE.md exists"
} else {
    Fail "docs/5WHY-TEMPLATE.md not found"
}

# ═════════════════════════════════════════════════════════════════════════════
# Summary
# ═════════════════════════════════════════════════════════════════════════════
Step "CI Summary"
if ($FAILURES -eq 0) {
    Write-Host "`n  All CI checks passed. Ready to merge." -ForegroundColor Green
    Pop-Location; exit 0
} else {
    Write-Host "`n  $FAILURES CI check(s) FAILED. Fix before merging." -ForegroundColor Red
    Pop-Location; exit 1
}
