# NetDiagnostics

Professional cross-platform network diagnostic toolkit. Built with Qt 6 / QML and libcurl. Runs on **iOS, Android, Windows, macOS, and Linux**.

## Features

### Diagnostic Engine (45 tests in 5 groups)

| Group | Tests | Description |
|-------|-------|-------------|
| **G1** — System & Adapters | 8 | Network Adapters, NIC Advanced, WiFi Information, Wired Information, DHCP Status, IP Configuration, Active Connections, Cellular Info |
| **G2** — Connectivity & Security | 6 | Network Profile, TCP Settings, Default Gateway, Routing Table, ARP Table, Proxy Settings |
| **G3** — Internet & DNS | 5 | Netskope Status, DNS Servers, DNS Cache, DNS Pollution, Internet Connectivity & Speed |
| **G4** — Remote Host | 6 | DNS Resolution, Ping, Traceroute, PathPing, MTU Discovery, Port Scan |
| **G5** — Website / URL | 20 | URL Parsing, TCP Connect, Service Banner, cURL Verbose, HTTP Headers, Security Headers, SSL Certificate, HTTP Redirect, HTTP Compression, HTTP Timing, FTP, SSH, Email, Telnet, MySQL, PostgreSQL, Redis, MongoDB, LDAP, MQTT |

### Key Features

- **Cross-platform** — single codebase for iOS, Android, Windows, macOS, Linux
- **Pure C++ diagnostics** — zero shell commands, direct OS API calls
- **Real-time engine** — results stream live as each test completes
- **Report export** — PDF (dashboard-style summary) and HTML (dark theme full detail) with progress bars, status indicators, and theme-coordinated colours
- **Settings persistence** — language, active groups, and per-test enable/disable survive app restarts via QSettings
- **Premium IAP** — non-consumable unlock for report sharing via OS share sheet / email
- **9-language UI** — English, Français, Deutsch, Русский, Italiano, 简体中文, 繁體中文, Español, Português
- **Dark theme** — custom dark UI with cyan (`#22D3EE`) accent palette; report styling matches app theme
- **G5 protocol diagnostics** — 20 per-scheme tests including MySQL, PostgreSQL, Redis, MongoDB, LDAP, MQTT via raw TCP sockets
- **DNS diagnostics** — dig-style output with HEADER/QUESTION/ANSWER sections, DNSSEC validation, pollution detection
- **Speed test** — Ookla-compatible download/upload bandwidth measurement
- **Group-sequential execution** — `std::thread` concurrency with `std::atomic` group tracking
- **Startup crash diagnostics** — `ND_DEBUG=ON` writes timestamped startup events to `%TEMP%\NetDiagnostics_startup.log`
- **Single-instance lock** — prevents duplicate application instances
- **Simulator mode** — device-frame UI for testing on desktop

## Supported Platforms

| Platform | Arch | Status |
|----------|------|--------|
| iOS | arm64 | ✅ Full support (StoreKit IAP, share sheet, WiFi SSID) |
| Android | arm64 | ✅ Full support (share sheet via FileProvider) |
| Linux | arm64 / x86_64 | ✅ Full support |
| Windows | x86_64 / ARM64 | ✅ Full support |
| macOS | x86_64 / arm64 | ✅ Full support |

## Project Structure

```
├── CMakeLists.txt              # Build configuration
├── README.md
├── doc/
│   ├── PROJECT_CHANGES.md      # File manifest & change log
│   └── app-store-localization.txt  # App Store listing (9 languages)
├── scripts/
│   ├── build-all.sh            # Multi-platform build system
│   ├── build-static.ps1        # Static build (Windows)
│   ├── generate-icons.sh       # Icon generation
│   └── toolchain/              # CMake toolchain files
├── src/
│   ├── main.cpp                # Production entry point
│   ├── main_simulator.cpp      # Simulator entry point
│   ├── app/
│   │   ├── AppState.h/.cpp       # Central state & diagnostic orchestration
│   │   ├── DiagnosticConfig.h/.cpp # Per-test enable/disable configuration
│   │   ├── ReportEngine.h/.cpp   # HTML/PDF report generation with dark theme
│   │   ├── PremiumStore.h/.cpp   # In-app purchase management
│   │   ├── NativeService.h/.cpp
│   │   └── PremiumManager.h/.cpp
│   ├── engine/
│   │   ├── PlatformShare_ios.mm / PlatformShare_android.cpp
│   │   ├── PlatformStore_ios.mm          # iOS StoreKit IAP
│   │   ├── IosWiFiHelper.mm              # iOS WiFi SSID access
│   │   ├── diagnostics/
│   │   │   ├── G1G2G3Native.h/.cpp       # G1/G2/G3 native diagnostics
│   │   │   ├── G4RemoteHost.h/.cpp       # G4 remote host tests
│   │   │   ├── NetworkProbe.h/.cpp       # Raw socket / SSL probe utilities
│   │   │   ├── G5/
│   │   │   │   ├── G5WebsiteUrl.h        # G5 function declarations
│   │   │   │   ├── G5Common.h/.cpp       # Shared G5 helpers (curl HTTP)
│   │   │   │   ├── G5UrlParsing.cpp through G5EmailDiagnostics.cpp
│   │   │   │   └── G5Telnet.cpp through G5Mqtt.cpp  # 7 protocol diagnostics
│   │   │   └── G1/ G2/ G3/ G4/          # Group-specific diagnostic files
│   │   └── task/
│   │       ├── DiagnosticTask.h/.cpp     # Task abstraction
│   │       ├── GenericTask.h/.cpp        # Generic async task wrapper
│   │       ├── TaskFactory.h/.cpp        # DiagId → task mapping
│   │       ├── IosHttpTask.mm            # iOS NSURLSession HTTP
│   │       ├── IosDnsTask.mm             # iOS DNSService DNS
│   │       ├── IosNetworkInfo.mm         # iOS network info
│   │       └── AndroidNetworkInfo.cpp    # Android JNI network info
│   ├── models/
│   │   ├── DiagnosticResult.h/.cpp  # Immutable result struct
│   │   ├── DiagId.h                 # Test IDs, groups, statuses (38 tests)
│   │   └── ResultProperty.h         # Key-value result properties
│   └── util/
│       ├── DebugSwitch.h            # Trace macros
│       ├── Logger.h/.cpp            # File-based debug logger
│       ├── PingParser.h/.cpp        # Ping output parser
│       ├── DnsResolver.h/.cpp       # DNS resolution utility
│       ├── DiagnosticFormatter.h/.cpp  # Dig-style output formatter
│       ├── PlatformShare.h          # Cross-platform share abstraction
│       └── PlatformStore.h          # Cross-platform IAP abstraction
├── resources/
│   ├── resources.qrc               # Qt resource file
│   ├── Info.plist                   # iOS bundle metadata
│   ├── NetDiagnostic.entitlements   # iOS code-signing entitlements
│   ├── netanalysis.rc              # Windows resource (icon embed)
│   ├── icons/                       # SVG/PNG/ICO app + status icons
│   ├── fonts/                       # JetBrains Mono + DejaVu Sans Mono
│   ├── qml/
│   │   ├── main.qml                 # Main window
│   │   ├── AppContent.qml           # Content layout
│   │   ├── theme/
│   │   │   ├── AppTheme.qml         # Theme constants (C++ injected)
│   │   │   ├── Translations.qml     # 9-language translations
│   │   │   └── qmldir
│   │   ├── screens/
│   │   │   ├── DashboardScreen.qml  # Overview dashboard
│   │   │   ├── DiagnosticScreen.qml # Diagnostic tree view
│   │   │   ├── ConfigScreen.qml     # Test selection
│   │   │   ├── ReportScreen.qml     # Report preview + share/Premium flow
│   │   │   ├── SettingsScreen.qml   # App settings + restore purchases
│   │   │   └── SimulatorScreen.qml  # Device frame simulator
│   │   └── widgets/
│   │       ├── AppIcon.qml          # SVG icon component
│   │       ├── DiagGroupPanel.qml   # Expandable group panel
│   │       ├── DiagResultItem.qml   # Individual test result row
│   │       ├── LiveProgressPanel.qml
│   │       ├── PortScanConfig.qml
│   │       ├── SummaryCards.qml
│   │       ├── TargetAnalysisPanel.qml
│   │       └── TargetInputPanel.qml
│   ├── android/
│   │   ├── AndroidManifest.xml
│   │   └── res/xml/file_paths.xml   # FileProvider for share sheet
│   └── Assets.xcassets/             # iOS app icon
├── tests/
│   ├── CMakeLists.txt
│   └── test_engine_quick.cpp
└── .github/workflows/
    ├── build.yml                    # CI/CD builds
    └── deploy-testflight.yml        # Automated TestFlight deployment
```

## Build

### Quick Start (automated)

```bash
# Native build (auto-detect host platform)
./scripts/build-all.sh

# Cross-compile specific target + simulator
./scripts/build-all.sh --target windows-x86_64 --sim

# Auto-fix ALL missing dependencies
./scripts/build-all.sh --fix --target all
```

### Manual Build

#### Linux / macOS

```bash
# Dependencies
sudo apt install qt6-base-dev qt6-quickcontrols2-dev libcurl4-openssl-dev cmake ninja-build  # Linux
brew install qt@6 cmake ninja curl                                                             # macOS

# Build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build net_diagnostics
```

#### iOS

```bash
cmake -G Xcode \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/qt6/ios.toolchain.cmake \
  -B build/ios
# Open build/ios/*.xcodeproj in Xcode → select device → Build
```

#### Android

```bash
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/qt6/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -B build/android
ninja -C build/android net_diagnostics
```

#### Simulator

```bash
cmake -G Ninja -DBUILD_SIMULATOR=ON -B build -S .
ninja -C build net_diagnostics_sim
```

### Debug Build (startup crash diagnostics)

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DND_DEBUG=ON -B build -S .
ninja -C build net_diagnostics
# Run app → check %TEMP%\NetDiagnostics_startup.log for timestamped startup events
```

### Headless Test

```bash
ND_MAX_TESTS=2 ND_AUTORUN=1 QT_QPA_PLATFORM=offscreen ./build/net_diagnostics
```

### Windows Static Build (MSYS2 UCRT64)

```bash
# Requires MSYS2 with mingw-w64-ucrt-x86_64-qt6-static
cmake -G Ninja -DCMAKE_PREFIX_PATH=/ucrt64/qt6-static \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -B build -S .
ninja -C build net_diagnostics
# verify zero non-OS DLLs:
objdump -p build/net_diagnostics.exe | grep "DLL Name"
```

## CI/CD

Automated builds via `.github/workflows/build.yml` for every push and PR:

| Platform | Arch | Compiler | Simulator |
|----------|------|----------|-----------|
| Linux | x86_64 / arm64 | GCC | ✅ |
| Windows | x86_64 / ARM64 | mingw-w64 / LLVM-MinGW | ✅ |
| macOS | x86_64 / arm64 | Apple Clang | ✅ |

## Deployment (iOS TestFlight)

Automated via `.github/workflows/deploy-testflight.yml`.

**Required GitHub Secrets:**

| Secret | Source |
|--------|--------|
| `APPSTORE_CONNECT_ISSUER_ID` | App Store Connect → Users and Access → Integrations → Keys |
| `APPSTORE_CONNECT_KEY_ID` | Same page |
| `APPSTORE_CONNECT_API_KEY` | Downloaded `.p8` file |
| `IOS_TEAM_ID` | developer.apple.com/account → Membership |
| `IOS_DISTRIBUTION_CERT_BASE64` | `base64 -i dist.p12` |
| `IOS_DISTRIBUTION_CERT_PASSWORD` | .p12 export password |

See the workflow file for full details.

## In-App Purchase

The app includes a **non-consumable Premium** IAP (Product ID: `com.netdiagnostic.app.premium`) that unlocks report sharing. Implementation:

- **iOS**: StoreKit via `SKProductsRequest` + `SKPaymentQueue` (`src/engine/PlatformStore_ios.mm`)
- **Restore**: `restoreCompletedTransactions` with error/no-purchase distinction
- **Persistence**: `QSettings` for offline unlock survival; manual Restore Purchases button in Settings
- **Sandbox testing**: App Store Connect → Sandbox Testers → test account

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Qt 6 | ≥ 6.2 | Core, Concurrent, Quick, QuickControls2, Network |
| libcurl | ≥ 7.80 | HTTP/HTTPS diagnostics (desktop; iOS uses NSURLSession) |
| CMake | ≥ 3.22 | Build system |

## License

MIT
