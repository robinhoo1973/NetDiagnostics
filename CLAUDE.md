# NetDiagnostics — Project Defaults & Global Code Review Checks

## Global Default Code Check Items

> **Every commit MUST pass the checks below. These are enforced by `scripts/pre-commit`.**

### Primary Reference Documents

| Document | Purpose |
|----------|---------|
| [docs/review/ios-startup-crash-5why-analysis.md](docs/review/ios-startup-crash-5why-analysis.md) | **iOS startup crash 5WHY root cause analysis + prevention checklist** — THE authoritative reference for all crash patterns |
| [docs/review/ios-ci-known-issues.md](docs/review/ios-ci-known-issues.md) | iOS CI / build / link / entitlement / QColor / CMake known issue patterns |
| [review/app_startup_crash_analysis.md](review/app_startup_crash_analysis.md) | Detailed startup flow analysis for all platforms |

### Pre-commit Hook

```bash
# Install:
bash scripts/install-hooks

# Run manually:
bash scripts/pre-commit
```

The hook runs **17 checks** covering:
1. CMake comment syntax (`#` not `//`)
2. CMake regex `{n}` quantifier
3. Preprocessor `#if(defined(X))` style (no `#ifdef`/`#ifndef`)
4. No `#elif` (use `#else` + `#if`)
5. No UTF-8 BOM
6. Palette sync (AppColors.h ↔ Palette.js)
7. Apple entitlements derived attributes
8. CI YAML `pipefail`
9. `QColor(QRgb)` 8-digit hex alpha
10. Apple SDK reserved words in enums
11. `std::thread::join()` safety
12. **QML: non-existent properties** (`Rectangle.shadow`, `border.visible`, `ScaleTransform`, `compact:true`, `font.family` comma-list)
13. **QML: `readonly property` placement** (must be at root level, not inside object literals)
14. **C++: SIOF** (`static const` non-trivial object in headers → use Meyer's Singleton)
15. **QML: `Qt.styleHints` static binding** (static Qt init order risk)
16. **QML: inline Component with `sourceComponent`** (eager compilation risk for platform-specific types)
17. **C++: `.join()` without `joinable()` guard in destructor context**

### Commit Message Format

```
<type>(<scope>): <description>

Where <type> is: fix | feat | refactor | docs | ci | chore
      <scope> is: ios | android | qml | cmake | ci | capture | report | ...
```

If the commit closes a 5WHY root-cause fix, include `(5WHY)` in the description.

### Build Commands

```bash
# Windows static (pre-commit smoke test)
powershell -File scripts/pre-commit-check.ps1
powershell -File scripts/pre-commit-check.ps1 -Debug  # with startup logging

# Full platform build
bash scripts/build-release.sh all
```

---

## Critical Anti-Patterns (from 30+ iOS crash fixes)

### 1. The Universal Crash Chain
```
Any single QML/C++ error → engine.load() fails → rootObjects empty → return -1 → instant quit
```
**iOS static Qt is unforgiving.** What works on desktop dynamic Qt WILL crash on iOS.
See: [docs/review/ios-startup-crash-5why-analysis.md#1-core-finding](docs/review/ios-startup-crash-5why-analysis.md#1-core-finding)

### 2. QML Eager Compilation
```
Inline Component { Type {...} } → eager compilation (imports resolved at parent-doc compile time)
Loader { source: "qrc:/..." } → deferred compilation (imports resolved at activation time)
```
**Always use Loader+source URL for platform-specific types.**

### 3. Static Init Order (SIOF)
```
Header file: static const QMap<...> → construction before main() → dyld order undefined → SIGSEGV
Fix: Meyer's Singleton → function-local static → lazy init on first use
```

### 4. Signal Handler Object Destruction
```
onClicked → runDiagnostics() → emit runStatusChanged() → QML binding → object destroyed on signal handler stack → qFatal()
Fix: Qt.callLater(function() { ... }) → deferred to after signal handler unwinds
```

### 5. Platform QRC Isolation
```
Any .qml referencing platform-specific C++ types → MUST be in conditional QRC (IOS OR ANDROID guard)
```

---

## Key Project Files

| File | Role |
|------|------|
| [src/main.cpp](src/main.cpp) | App entry point, QML engine loading, crash exit |
| [src/app/AppState.cpp](src/app/AppState.cpp) | Core app state, controller creation |
| [src/Common/View/main.qml](src/Common/View/main.qml) | Root QML (ApplicationWindow) |
| [src/Common/View/AppContent.qml](src/Common/View/AppContent.qml) | Main content (StackView → DiagnosticScreen) |
| [CMakeLists.txt](CMakeLists.txt) | Build config, conditional QRC |
| [cmake/netdiag-target.cmake](cmake/netdiag-target.cmake) | Target config, qt_import_qml_plugins |
| [.github/workflows/apple.yml](.github/workflows/apple.yml) | macOS/iOS CI |
