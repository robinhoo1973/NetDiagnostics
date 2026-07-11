# NetDiagnostic-QT — Complete File Manifest & Change Log

---

## Project Overview

**NetAnalysis** is a cross-platform network diagnostic tool built with C++17/Qt 6.8 and QML. It performs 38 diagnostic tests across 5 groups using pure OS API calls (no shell commands), libcurl for HTTP diagnostics, and a QML-based responsive UI with 7-language i18n support.

- **Language:** C++17, QML (Qt Quick 2)
- **Build:** CMake 3.22+, Ninja
- **CI/CD:** GitHub Actions (multi-platform builds)
- **Platforms:** Linux (arm64/x86_64), Windows (x86_64/arm64), macOS (x86_64/arm64)

---

## 1. Source Files (src/)

### `src/main.cpp`
Production entry point. Sets up `QGuiApplication`, injects Theme as C++ `QVariantMap` (20 color constants), loads QML main window with `showFullScreen()`.

### `src/main_simulator.cpp`
Simulator entry point. Same Theme C++ injection as `main.cpp`. Supports `ND_AUTORUN` env var for headless testing. Retains `QSG_RENDER_LOOP=basic` for ARM64 stability.

### `src/app/AppState.h` / `AppState.cpp`
Application state manager. Key features:
- **Group-sequential execution:** `std::thread` per test, `QTimer::singleShot` for result delivery, `std::atomic<int>` for group completion tracking
- `allDiagsForGroup(int)` — returns all tests (completed + pending) with `isDone`/`isPending`/`isRunning` flags
- `allDiagIdsForGroup(int)` — returns all test IDs for a group
- `groupStats(int)` — returns `{total, completed, pass, warn, fail, skip}` per group
- `showDetailDialog(int)` — native C++ QDialog with Status/Duration/Summary/Properties/Raw Output, dark theme
- `resultsVersion` property — incremented on each completed test to trigger QML model refresh

### `src/app/NativeService.h` / `NativeService.cpp`
Native OS service integration layer.

### `src/engine/PlatformCommand.h` / `PlatformCommand.cpp`
Cross-platform command execution. Includes `proc.moveToThread(QThread::currentThread())` before `proc.start()` for ARM64 QProcess thread safety.

### `src/engine/diagnostic/DiagnosticEngine.h` / `DiagnosticEngine.cpp`
Core diagnostic engine. Orchestrates test execution, manages results, handles timeouts and cancellation.

### `src/engine/diagnostic/G1G2G3Native.h` / `G1G2G3Native.cpp`
G1/G2/G3 native diagnostics (18 tests):
- **G1:** Network adapters, NIC advanced, WiFi, wired, DHCP status, IP configuration, active connections
- **G2:** Network profile, TCP settings, default gateway, routing table, ARP table, proxy settings
- **G3:** Netskope status, DNS servers, DNS cache, DNS pollution, Internet speed test

ARM64 workarounds:
- `ND_DIAG_NIC_ADVANCED` → `NdDiagnosticNotSupported` (bypasses ARM64 `readlink()` SIGSEGV)
- `ND_DIAG_DNS_CACHE` → `NdDiagnosticNotSupported` (bypasses ARM64 SIGBUS)

### `src/engine/diagnostic/G4RemoteHost.h` / `G4RemoteHost.cpp`
G4 remote host diagnostics (6 tests): DNS resolution, ping, traceroute, pathPing, MTU discovery, port scan.

### `src/engine/diagnostic/G5WebsiteUrl.h` / `G5WebsiteUrl.cpp`
G5 website/URL diagnostics (13 tests): URL parsing, TCP connect, service banner, curl verbose, HTTP headers, security headers, SSL certificate, HTTP redirect, HTTP compression, HTTP timing, FTP, SSH, Email diagnostics.

### `src/engine/runner/NetworkProbe.h` / `NetworkProbe.cpp`
Low-level network probing utilities: socket operations, ICMP/TCP/UDP probing, interface enumeration.

### `src/models/DiagId.h`
Central registry of all diagnostic test IDs, groups, and statuses:
- `DiagGroup` enum: G1, G2, G3, G4, G5
- `DiagStatus` enum: Pass, Warning, Fail, Skipped, Error, Info
- `DiagId` enum: 38 test IDs
- Utility functions: `diagGroup()`, `diagGroupLabel()`, `diagStatusIcon()`, `diagIdLabelKey()`, `allDiagIds()`, `diagIdsForGroup()`

### `src/models/DiagnosticResult.h` / `DiagnosticResult.cpp`
Immutable result struct: id, displayName, group, status, summary, details, durationMs, timestamp, properties (key-value with severity), rawOutput, errorOutput. Factory helpers: `skipped()`, `error()`, `timeout()`.

### `src/models/ResultProperty.h`
Key-value pair with `ResultPropertySeverity` (Info/Warning/Error) and nested children for tree-structured output.

### `src/util/DebugSwitch.h`
Trace macros (`TRACE`, `MAIN_LOG`) — disabled by default (no output).

### `src/util/Logger.h` / `Logger.cpp`
Logging utility.

### `src/util/PingParser.h` / `PingParser.cpp`
Ping output parser for structured ping result extraction.

---

## 2. QML Resources (resources/qml/)

### `main.qml`
Main window with `visibility: Window.Maximized` + `showFullScreen()`. Navigation tabs with AppIcon + label. ✕ close button at nav bar far right.

### `AppContent.qml`
Content layout shell.

### `theme/AppTheme.qml`
Theme constants — values injected from C++ `QVariantMap` at startup.

### `theme/Translations.qml`
7-language translation strings (English, French, German, Russian, Italian, Simplified/Traditional Chinese).

### `screens/DashboardScreen.qml`
Overview dashboard with:
- `fmtDur`: `ms`/`s`/`m s` format
- `calcTotalTime()`: sum of all result durations
- Dynamic progress bar: width based on `(pass+warn+fail)/total`, color changes to warnYellow on failure
- `DashboardGroupRow`: per-group test mini rows with status icons + duration
- `calcLayerTiming()` for Layer Timings section

### `screens/DiagnosticScreen.qml`
Main diagnostic tree view:
- `visibleGroups`: filters groups with `enabled>0` or `total>0`
- Sidebar: Spacer pushes SummaryCards to bottom
- Content: ScrollView+Column+Repeater (fixes ARM64 delegate rendering bug)
- Content max-width: `Math.min(availableWidth, 700)` with horizontal centering

### `screens/ConfigScreen.qml`
Test selection screen — SwitchListTile format:
- Action bar: group name + test count + Select All/Deselect All buttons
- Test list: leading icon + title (13px w500) + description (11px gray) + Switch
- 38 static test descriptions

### `screens/ReportScreen.qml`
Report preview (planned features): PDF export, Email share, HTML reports, Historical comparison.

### `screens/SettingsScreen.qml`
Application settings.

### `screens/SimulatorScreen.qml`
Device frame simulator:
- Navigation bar: 5 tabs (Dashboard/Diagnostics/Config/Report/Settings)
- StackView page switching: `simStack.clear()` + `simStack.push(url)`
- Device popup with device/frame selection
- Scale calculation with orientation handling
- Screen Rectangle with layer-based corner rounding

### `widgets/AppIcon.qml`
SVG icon component with `name` and `size` properties.

### `widgets/DiagGroupPanel.qml`
Expandable group panel (Flutter ExpansionTile style):
- `_userToggled` / `_userWantsExpanded` state pattern
- `allDiagsForGroup()` model binding with manual refresh trigger
- TreeView connectors: 20px indent with vertical/horizontal lines
- Auto-expand on running/completed state changes

### `widgets/DiagResultItem.qml`
Individual test result row:
- **Pending row:** spinner + gray text + "Running..." label
- **Completed row:** status icon + name + duration badge + summary
- Click handler calls `appState.showDetailDialog(resultData.id)` — native C++ QDialog

### `widgets/LiveProgressPanel.qml`
Runtime progress display — simplified static text: status icon + label + current test name + count.

### `widgets/PortScanConfig.qml`
Port scan configuration panel.

### `widgets/SummaryCards.qml`
Pass/Warning/Fail/Skip summary cards in 2×2 grid layout with AppIcon badges.

### `widgets/TargetAnalysisPanel.qml`
Target analysis section.

### `widgets/TargetInputPanel.qml`
Target host input with bidirectional binding, clear button, and Run button.

---

## 3. Icons (resources/icons/)

### Navigation Icons
| Icon | Description |
|------|-------------|
| `dashboard.svg` | Dashboard (Phosphor squares-four) |
| `diagnostics.svg` | Diagnostics (Phosphor activity/polyline) |
| `config.svg` | Config (Phosphor sliders) |
| `report.svg` | Report (Phosphor file-text) |
| `settings.svg` | Settings (Phosphor gear) |

### Status Badge Icons
| Icon | Status |
|------|--------|
| `badge-check.svg` | Pass |
| `badge-warning.svg` | Warning |
| `badge-close.svg` | Fail |
| `badge-skip.svg` | Skip |
| `badge-error.svg` | Error |
| `badge-info.svg` | Info |
| `badge-circle.svg` | Neutral |
| `badge-refresh.svg` | Refresh |

### UI Icons
| Icon | Purpose |
|------|---------|
| `play.svg`, `stop.svg`, `refresh.svg` | Run/Stop/Refresh |
| `target.svg`, `globe.svg` | Target/Globe |
| `portscan.svg`, `wifi.svg` | Port scan/WiFi |
| `tune.svg`, `timer.svg` | Tune/Timer |
| `spinner.svg` | Loading spinner |
| `mail.svg` | Email |
| `close.svg` | Close (backup) |

### Platform Icons
| Icon | Platform |
|------|----------|
| `windows.svg`, `linux.svg`, `apple.svg`, `android.svg` | Platform indicators |

### Simulator Icons
| Icon | Purpose |
|------|---------|
| `sim-icon-beaker.svg` | Lab beaker |
| `sim-icon-bug.svg` | Debug/bug |
| `sim-icon-flask.svg` | Flask |
| `sim-icon-monitor-play.svg` | Monitor with play |
| `sim-icon-network-lab.svg` | Network lab |

### App Identity
| File | Purpose |
|------|---------|
| `app-icon.svg` | Application icon (SVG) |
| `netanalysis.ico` | Windows icon |
| `netanalysis.png` | Application icon (PNG) |

---

## 4. Build System

### `scripts/build-all.sh`
Self-contained multi-platform build system:
- Dep check: ninja, cmake, g++/clang++, pkg-config, Qt6, fonts, QRC
- `--fix`: auto-installs missing tools from source (ninja, cmake, mingw-w64, LLVM-MinGW, Qt6)
- Cross-compilation targets: linux-arm64 (native), linux-x86_64, windows-x86_64, windows-arm64
- Simulator variant: `--sim` for device-frame UI build alongside production binary
- Smart TMPDIR: auto-detects small tmpfs (<10 GB) and falls back to `~/.cache`

### `scripts/build-static.ps1`
Windows PowerShell script for static builds.

### `scripts/generate-icons.sh`
Icon generation helper script.

### `scripts/toolchain/`
CMake toolchain files for cross-compilation:
- `linux-arm64.cmake` — native ARM64 build
- `linux-x86_64.cmake` — x86_64 cross-compile from ARM64
- `windows-x86_64.cmake` — mingw-w64 cross-compile
- `windows-arm64.cmake` — LLVM-MinGW cross-compile

### `.github/workflows/build.yml`
GitHub Actions CI/CD workflow:
- Triggers: push/PR to master
- 6 build jobs: Linux (x86_64/arm64), Windows (x86_64/arm64), macOS (x86_64/arm64)
- Simulator builds: Linux x86_64, Windows x86_64, macOS x86_64, macOS arm64
- Artifacts: compiled binaries + simulator binaries
- Release upload on tag push

### `CMakeLists.txt`
- Qt6: Core, Concurrent, Quick, QuickControls2, Widgets, Network
- libcurl: HTTP/HTTPS diagnostics
- `BUILD_SIMULATOR` option for simulator target
- `BUILD_TESTS` option for test suite
- Windows: links ws2_32, winhttp, iphlpapi, wlanapi, dnsapi, ole32, shell32, etc.
- Linux/macOS: links resolv

### `tests/`
- `test_engine_quick.cpp` — 19 headless tests
- Test execution: `ND_MAX_TESTS=2 ND_AUTORUN=1 QT_QPA_PLATFORM=offscreen ./build/net_diagnostic`

### `resources/resources.qrc`
Qt resource file registering all QML files, SVG icons, and fonts (61 entries).

### Other Resource Files
- `resources/netanalysis.desktop` — Linux desktop entry
- `resources/netanalysis.rc` — Windows resource file for icon embedding
- `resources/config/*.conf` — Platform-specific configuration (android, ios, linux, windows)

---

## 5. Design Assets (doc/)

| File | Description |
|------|-------------|
| `doc/design-spec.html` | Design specification with visual guidelines |
| `doc/design-tokens.json` | Design tokens (colors, spacing, typography, radio) |

---

## 6. Platform Support Matrix

| Platform | Arch | Compiler | Simulator | Status |
|----------|------|----------|-----------|--------|
| Linux | arm64 | GCC | — | ✅ Full (native) |
| Linux | x86_64 | GCC | ✅ | ✅ Full |
| Windows | x86_64 | mingw-w64 | ✅ | ✅ Cross-compile |
| Windows | ARM64 | LLVM-MinGW | — | ✅ Cross-compile |
| macOS | x86_64 | Apple Clang | ✅ | ✅ Full |
| macOS | arm64 | Apple Clang | ✅ | ✅ Full |
| Android | — | NDK | — | ⚠️ Mostly works |
| iOS | — | Xcode | — | ⚠️ Sandbox limited |

---

## 7. Commit History

| Commit | Description |
|--------|-------------|
| `fc68912` | feat: diagnostic engine refactor, network probe overhaul, UI updates, app icons, cross-platform build scripts |
| `644c215` | feat: Test→Diag rename complete, DHCP Status rewrite, code review fixes |
| `14d8101` | chore: build script updates |
| `cb08924` | feat: comprehensive code review fixes, libcurl HTTP, dig-style DNS, badge icons, cross-platform fixes |
| `4df1dbe` | fix: move CheckBox Timer inside component scope |
| `0fe44dc` | fix: iterative BFS for CheckBox traversal |
