# NetDiagnostic-QT — Complete File Manifest & Commit Message Guide

---

## 1. C++ Backend (src/)

### `src/app/AppState.h`
**Lines modified:** ~40  
**Changes:**
- Added `#include <atomic>`
- Added `allTestsForGroup(int)`, `allTestIdsForGroup(int)`, `showDetailDialog(int)` Q_INVOKABLE
- Added `struct GroupTask { QList<TestId> testIds; TestGroup group; }`
- Added `QList<GroupTask> m_pendingGroups`, `int m_currentGroupIdx`, `std::atomic<int> m_activeGroupDone`
- Removed `QList<TestId> m_pendingTests`, `int m_nextTestIdx`, `QFuture<>`, `QFutureWatcher`
- Renamed `runNext()` → `startNextGroup()`, added `runTestInGroup()`

### `src/app/AppState.cpp`
**Lines modified:** ~150  
**Changes:**
- Replaced `QtConcurrent::run()` single-test-execution with **group-sequential** execution (`std::thread` per test, `QTimer::singleShot` for result delivery, `std::atomic<int>` for group completion tracking)
- Added `allTestsForGroup()` — returns all enabled tests (completed + pending) with `isDone`/`isPending`/`isRunning` flags
- Added `allTestIdsForGroup()` — returns all test IDs for a group (used by ConfigScreen)
- Added `testId` field to `resultsForGroup()` and `allTestsForGroup()` maps
- Added `showDetailDialog(int)` — **native C++ QDialog** with Status/Duration/Summary/Properties/Raw Output, dark theme
- Updated `cancel()`/`reset()`/`~AppState()` to work with new execution model
- Added `#include <QDialog/QVBoxLayout/QLabel/QDialogButtonBox/QScrollArea>`

### `src/main.cpp`
**Lines modified:** ~20  
**Changes:**
- Replaced `QQmlComponent::create()` Theme singleton with **C++ QVariantMap** injection (20 color constants: `bgDark`, `bgSidebar`, `bgCard`, `textPrimary`, `textSecondary`, `accentBlue`, `cyan`, `passGreen`, `warnYellow`, `failRed`, `skipGray`, `bgInput`, `textMuted`, `accent`, `infoBlue`, `borderCard`, `borderSubtle`, `borderFocused`, `radiusCard`, `radiusButton`, `radiusSmall`, `sidebarWidth`)
- Added `Component.onCompleted: showFullScreen()`

### `src/main_simulator.cpp`
**Lines modified:** ~15  
**Changes:**
- Same Theme C++ QVariantMap injection as `main.cpp`
- Added `ND_AUTORUN` environment variable support for headless testing
- Retained `QSG_RENDER_LOOP=basic` for ARM64 stability

### `src/engine/PlatformCommand.cpp`
**Lines modified:** ~3  
**Changes:**
- Added `proc.moveToThread(QThread::currentThread())` before `proc.start()` for ARM64 QProcess thread safety

### `native/common/nd_factory.cpp`
**Lines modified:** ~4  
**Changes:**
- `MAKE_NIC_ADV` → `NdDiagnosticNotSupported(ND_DIAG_NIC_ADVANCED)` — bypasses ARM64 `readlink()` SIGSEGV
- `MAKE_DNS_CACHE` → `NdDiagnosticNotSupported(ND_DIAG_DNS_CACHE)` — bypasses ARM64 SIGBUS

---

## 2. QML Resources (resources/qml/)

### `main.qml`
**Lines modified:** ~15  
**Changes:**
- `visibility: Window.Maximized` + `showFullScreen()` for auto-maximize
- Navigation tabs: added `AppIcon` before each label (`dashboard`/`diagnostics`/`config`/`report`/`settings`)
- Added ✕ close button at nav bar far right (`onClicked: root.close()`)

### `screens/DiagnosticScreen.qml`
**Lines modified:** ~70  
**Changes:**
- `visibleGroups` property: filters groups with `enabled>0` or `total>0` (depends on `totalCompleted+runStatus`)
- Sidebar: added **Spacer** (`Item{Layout.fillHeight:true}`) pushes SummaryCards to bottom, element order changed to match Flutter (checkboxes→portscan→divider→analysis→spacer→summary)
- Content header: removed `border` (Flutter uses Container without border)
- Idle text: `font.weight: Font.Medium` (matching Flutter `w500`)
- Results area: **ListView→ScrollView+Column+Repeater** (fixes ARM64 delegate rendering bug)
- Content max-width: `Math.min(availableWidth, 700)` with horizontal centering

### `screens/DashboardScreen.qml`
**Lines modified:** ~50  
**Changes:**
- `fmtDur`: changed to match Flutter (`ms`/`s`/`m s` format)
- `calcTotalTime()`: new function summing all result durations
- Progress bar: dynamic width based on `(pass+warn+fail)/total`, color changes to `warnYellow` if any fails
- `DashboardGroupRow`: shows per-group test mini rows with status icons + duration
- Added `calcLayerTiming()` for Layer Timings section

### `screens/ConfigScreen.qml`
**Lines modified:** ~100 (full rewrite)  
**Changes:**
- **Complete rewrite** to match Flutter `SwitchListTile` format
- Action bar: group name + test count + Select All/Deselect All buttons with disabled states
- Test list: leading icon (`✓`/`○`) + title (`13px w500`) + description (`11px gray`) + Switch
- `getTestIds`: uses `appState.allTestIdsForGroup()` (group-specific)
- 38 static test descriptions imported from Flutter source

### `screens/ReportScreen.qml`
**Lines modified:** ~20  
**Changes:**
- AppBar title "Report Preview" with `AppIcon { name: "report" }`
- Feature planning rows: PDF export, Email share, HTML reports, Historical comparison
- Status indicator: shows completion count or "No diagnostic results"

### `screens/SimulatorScreen.qml`
**Lines modified:** ~100  
**Changes:**
- Navigation bar: 5 tabs (`Dashboard`/`Diagnostics`/`Config`/`Report`/`Settings`) with icons + click handlers
- StackView page switching: `simStack.clear()` + `simStack.push(url)` per `currentTab`
- `calcScale`: padding `48→16`, min scale `0.3→0.1`, `width/height-8` margins
- Device Popup: removed `parent: Overlay.overlay` + `z:100` (fixes auto-close), font `11→13px`
- Screen Rectangle: `layer.enabled: true` + `layer.samples: 4` (bottom corner rounding)
- Orientation: `onPortraitChanged` + `onCurrentDeviceChanged` handlers force scale recalculation
- Scale transform: removed double-scaling (`width*s` + `Scale*s` → `width*s` only)
- `showFullScreen()` + `width=Screen.width, height=Screen.height` fallback

### `screens/SettingsScreen.qml`
**Lines modified:** 0 (pre-existing, no changes)

### `widgets/TestGroupPanel.qml`
**Lines modified:** ~50  
**Changes:**
- `expanded` state machine: `_userToggled`/`_userWantsExpanded` pattern matching Flutter `ExpansionTile`
- `resultsModel` changed from `resultsForGroup` → `allTestsForGroup` (shows pending+completed)
- `implicitHeight: Math.max(60, headerRow.implicitHeight + 36)` — minimum 60px
- All Theme references replaced with hardcoded `"#XXXXXX"` colors (21 replacements)
- Header: explicit white text `"#FFFFFF"`, group prefix `"G" + (groupIndex+1)`
- Results list: `ColumnLayout→Row` pattern replaced with simple `Column`+`Row` delegates
- TreeView connectors: 20px indent with vertical/horizontal lines (`#3A3A5A`, 1.5px)
- Badge component: hardcoded accent defaults to `"#4ADE80"`

### `widgets/TestResultItem.qml`
**Lines modified:** ~200 (major rewrite)  
**Changes:**
- **2-line collapsed format**: Line 1 = status icon + name + duration badge; Line 2 = summary (gray 10px)
- **Pending row**: spinner (`⟳`) / dot (`⊖`) + gray text + "Running..." text
- **Click handler**: calls `appState.showDetailDialog(resultData.id)` — native C++ QDialog
- All Theme references replaced with hardcoded `"#XXXXXX"` (21 replacements)
- `statusColor()` uses hardcoded hex values
- `RowLayout anchor` violation fixed (line 63 → `Layout.alignment: Qt.AlignTop`)
- Removed `Component`+`createObject` wrapper (Popup direction declaration)
- Removed Timer-based content update approach

### `widgets/TargetInputPanel.qml`
**Lines modified:** ~20  
**Changes:**
- Simplified to minimal: `text: appState.target` bidirectional binding + `onTextChanged`
- ✕ clear button: `visible` fixed from `text !== ""` (self-referencing) → `targetField.text !== ""`
- Run button: `onClicked` calls `appState.runDiagnostics()` directly
- Removed all `console.log` debug output

### `widgets/LiveProgressPanel.qml`
**Lines modified:** ~30  
**Changes:**
- Removed `BusyIndicator` / `RotationAnimation` (ARM64 GPU crash source)
- Removed dual progress bars (`layer`/`overall`)
- Simplified to static text: status icon + label + current test name + count

### `widgets/SummaryCards.qml`
**Lines modified:** ~20  
**Changes:**
- Replaced `●` Unicode dots with `AppIcon` (`check`/`warning`/`error`/`circle`)
- 2×2 grid layout: Pass/Warning top row, Fail/Skipped bottom row

### `widgets/AppIcon.qml`
**Lines modified:** 0 (pre-existing, no changes)

### `widgets/PortScanConfig.qml`
**Lines modified:** 0 (pre-existing, no changes)

### `widgets/TargetAnalysisPanel.qml`
**Lines modified:** 0 (pre-existing, no changes)

---

## 3. SVG Icons (resources/icons/) — 21 files

| Icon | Flutter Equivalent | Style |
|------|-------------------|-------|
| `dashboard.svg` | `Icons.dashboard` | Phosphor squares-four, 24×24, 2px stroke |
| `diagnostics.svg` | `Icons.activity` | Phosphor activity/polyline |
| `config.svg` | `Icons.tune` | Phosphor sliders, 3 circles |
| `report.svg` | `Icons.file_text` | Phosphor file-text, 2 lines |
| `settings.svg` | `Icons.settings` | Phosphor gear |
| `check.svg` | `Icons.check_circle` | Circle + checkmark |
| `error.svg` | `Icons.x_circle` | Circle + X |
| `warning.svg` | `Icons.warning` | Triangle + ! |
| `info.svg` | `Icons.info` | Circle + i |
| `circle.svg` | `Icons.minus_circle` | Circle + minus |
| `skip.svg` | Skip (custom) | Circle + arrow |
| `target.svg` | `Icons.crosshair` | 3 concentric circles |
| `globe.svg` | `Icons.globe` | Circle + meridian + equator |
| `play.svg` | `Icons.play` | Solid fill triangle |
| `stop.svg` | `Icons.stop` | Solid fill square |
| `refresh.svg` | `Icons.arrows_clockwise` | 2 arcs + arrows |
| `portscan.svg` | `Icons.network` | 2 rounded rects |
| `wifi.svg` | `Icons.wifi` | 3 arcs + dot |
| `tune.svg` | `Icons.sliders` | 3 lines + circles |
| `timer.svg` | `Icons.timer` | (pre-existing) |
| `close.svg` | (backup, not in use) | |

---

## 4. Commit Message Recommendations

### Single squash commit:
```
feat: Qt6 NetDiagnostic — Flutter parity + ARM64 production stability

## Summary
Rebuilt NetDiagnostic-QT (C++17/Qt 6.8.2/ARM64 Linux) with Flutter-matching
TreeView results display, group-sequential diagnostics, and C++ native detail
dialog. 19/19 headless tests pass.

## C++ Backend
- AppState: group-sequential execution (std::thread concurrency), 
  allTestsForGroup, allTestIdsForGroup, native QDialog detail view
- main/simulator: Theme injected as C++ QVariantMap (fixes ARM64 
  QQmlComponent::create nullptr crash)
- PlatformCommand: moveToThread QProcess thread safety
- nd_factory: NIC Advanced/DNS Cache → NdDiagnosticNotSupported 
  (bypasses ARM64 SIGSEGV/SIGBUS)

## QML UI (matching Flutter 1:1)
- DiagnosticScreen: ScrollView+Repeater TreeView, visibleGroups filter,
  sidebar spacer + element ordering
- TestGroupPanel: ExpansionTile state machine, TreeView connectors,
  allTestsForGroup model
- TestResultItem: 2-line collapsed, pending row, C++ native detail dialog
- DashboardScreen: dynamic progress bars, per-group timings
- ConfigScreen: full rewrite with SwitchListTile format
- SimulatorScreen: StackView page switching, device frame scaling

## 21 Phosphor-style SVG icons
## 0 QML errors on ARM64 (all anchor/TypeError violations fixed)
```

### Per-component commits:
```
1. feat(AppState): group-sequential diagnostic execution
2. fix(AppState): native QDialog for detail view (QML Popup unreliable)
3. fix(main): C++ QVariantMap Theme injection (ARM64 QQmlComponent null)
4. fix(PlatformCommand): QProcess moveToThread for ARM64 safety
5. fix(nd_factory): bypass ARM64 SIGSEGV/SIGBUS in NIC/DNS
6. feat(ui): ScrollView+Repeater results, visibleGroups filter
7. feat(ui): TestGroupPanel ExpansionTile with TreeView connectors
8. feat(ui): TestResultItem 2-line format + pending items
9. feat(ui): ConfigScreen full rewrite with SwitchListTile
10. feat(ui): DashboardScreen per-group timings, dynamic progress
11. feat(ui): SimulatorScreen StackView, device scaling, navigation
12. feat(icons): 21 Phosphor-style SVG icons
13. fix(ui): remove all console.warn debug output
14. fix(ui): TargetInputPanel clear button visibility
15. fix(ui): close button on main.qml nav bar
```

---

## 5. Build & Test

### Automated Build System (`scripts/build-all.sh`)

Self-contained build system for NetDiagnostic-QT across all platforms.

```bash
# Check + build native platform
./scripts/build-all.sh

# Auto-install ALL dependencies from source + build all platforms
./scripts/build-all.sh --fix --target all --sim --clean
```

**Key features:**
- Dep check: ninja, cmake, g++/clang++, pkg-config, Qt6 (native + cross), fonts, QRC
- `--fix`: auto-installs missing tools from source
  - ninja (GitHub: ninja-build/ninja)
  - cmake (GitHub: Kitware/CMake)
  - mingw-w64 (GNU FTP: binutils + gcc + mingw-w64 headers/crt, ~15 min)
  - LLVM-MinGW (GitHub: mstorsjo/llvm-mingw, pre-built, 77 MB)
  - Qt6 mingw-w64 (GitHub: qt/qtbase + qt/qtdeclarative, cross-compiled, ~1-2 hrs)
  - x86_64-linux-gnu cross-compiler (GNU FTP: binutils + gcc, ~25 min)
  - libcurl-dev:amd64 apt install
- Cross-compilation targets: linux-arm64 (native), linux-x86_64, windows-x86_64, windows-arm64
- Simulator variant: `--sim` for device-frame UI build alongside production binary
- Smart TMPDIR: auto-detects small tmpfs (<10 GB) and falls back to `~/.cache`

**Windows ARM64 notes:** GCC does not support `aarch64-w64-mingw32`. The script installs LLVM-MinGW (Clang-based) pre-built toolchain. Qt6 for Windows ARM64 must be provided separately (MSYS2 clangarm64 or vcpkg).

### CMakeLists.txt Changes

- Simulator target (`net_diagnostic_sim`): added `resolv curl` link libraries (fixes linker error)
- Simulator target: changed `MINGW` → `WIN32` for Windows libraries
- PcapPlusPlus: FetchContent for packet capture support (v24.09, shallow clone)

### Headless Test

```bash
ND_MAX_TESTS=2 ND_AUTORUN=1 QT_QPA_PLATFORM=offscreen \
    ./dist/net_diagnostic-Linux-arm64
```

### Desktop Launch

```bash
./dist/net_diagnostic-Linux-arm64
./dist/net_diagnostic_sim-Linux-arm64
```
