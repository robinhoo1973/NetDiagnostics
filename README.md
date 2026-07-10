# NetDiagnostics

Professional cross-platform network diagnostic toolkit. Built with Qt 6 / QML and libcurl. Runs on **iOS, Android, Windows, macOS, and Linux**.

[简体中文](README_zh_CN.md) | [繁體中文](README_zh_TW.md)

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

## Technology Stack

**Core Framework:** Qt 6 (C++17) — Core, Concurrent, Quick, QuickControls2, Widgets, Network

**UI Layer:** QML with custom dark theme engine, 9-language i18n via Qt Linguist

**Network Libraries:**
- **libcurl** — HTTP/HTTPS diagnostics on desktop (transfer, headers, timing, compression)
- **QSslSocket** — SSL/TLS certificate inspection with full X.509 chain extraction
- **QTcpSocket** — Raw TCP connect, service banner, and 7 per-scheme protocol diagnostics
- **Native socket APIs** — `winsock2` (Windows), POSIX sockets (Linux/macOS), Network framework (iOS)

**Platform SDKs:**
- **iOS** — NetworkExtension, CoreLocation, CoreTelephony, StoreKit, CFNetwork, NSURLSession
- **Android** — JNI wrappers for ConnectivityManager, WifiManager, TelephonyManager, `HttpURLConnection`
- **Windows** — WLAN API, IP Helper API, WinHTTP, WinSock2
- **macOS** — SystemConfiguration, CoreWLAN, IOKit

**Build System:** CMake 3.22+ with Ninja generator; CI/CD via GitHub Actions (build.yml + apple.yml)

**Fonts:** JetBrains Mono (UI), DejaVu Sans Mono (box-drawing glyphs for tree-view diagnostics)

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

Automated multi-platform builds run on every push via GitHub Actions (`build.yml` and `apple.yml`). Covers Linux (x86_64/arm64), Windows (x86_64 static + dynamic), macOS (x86_64/arm64), iOS (arm64), and Android (arm64/x86_64). iOS TestFlight deployment is automated via `apple.yml`.

## In-App Purchase

A **non-consumable Premium** IAP (Product ID: `com.netdiagnostic.app.premium`) unlocks report sharing via the OS share sheet or email. Built with StoreKit on iOS and persisted through `QSettings` for offline unlock survival. Sandbox-tested through App Store Connect Sandbox Testers.

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Qt 6 | ≥ 6.2 | Core, Concurrent, Quick, QuickControls2, Network |
| libcurl | ≥ 7.80 | HTTP/HTTPS diagnostics (desktop; iOS uses NSURLSession) |
| CMake | ≥ 3.22 | Build system |

## License

MIT
