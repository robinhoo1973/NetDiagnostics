# NetDiagnostics

<p align="center">
  <b>Professional cross-platform network diagnostic toolkit</b><br/>
  Built with Qt 6 / QML and libcurl<br/>
  <sub>iOS &middot; Android &middot; Windows &middot; macOS &middot; Linux</sub>
</p>

[English](README.md) | [简体中文](README_zh_CN.md) | [繁體中文](README_zh_TW.md)

## Screenshots

Desktop screenshots are real OS-level captures. iOS screenshots are captured
manually from a real device with light &amp; dark theme variants. Android
screenshots are captured from an emulator or physical device.

### Desktop

| Stage | Linux | Windows | macOS |
|---|---|---|---|
| **Dashboard** | <img src="resources/doc/screenshot/linux/5-dashboard.png" width="200"/> | <img src="resources/doc/screenshot/windows/5-dashboard.png" width="200"/> | <img src="resources/doc/screenshot/macos/5-dashboard.png" width="200"/> |
| **Running** | <img src="resources/doc/screenshot/linux/2-running.png" width="200"/> | <img src="resources/doc/screenshot/windows/2-running.png" width="200"/> | <img src="resources/doc/screenshot/macos/2-running.png" width="200"/> |
| **Results** | <img src="resources/doc/screenshot/linux/3-complete.png" width="200"/> | <img src="resources/doc/screenshot/windows/3-complete.png" width="200"/> | <img src="resources/doc/screenshot/macos/3-complete.png" width="200"/> |
| **Detail** | <img src="resources/doc/screenshot/linux/4-detail.png" width="200"/> | <img src="resources/doc/screenshot/windows/4-detail.png" width="200"/> | <img src="resources/doc/screenshot/macos/4-detail.png" width="200"/> |
| **Report** | <img src="resources/doc/screenshot/linux/6-report.png" width="200"/> | <img src="resources/doc/screenshot/windows/6-report.png" width="200"/> | <img src="resources/doc/screenshot/macos/6-report.png" width="200"/> |
| **Config** | <img src="resources/doc/screenshot/linux/7-config.png" width="200"/> | <img src="resources/doc/screenshot/windows/7-config.png" width="200"/> | <img src="resources/doc/screenshot/macos/7-config.png" width="200"/> |
| **Settings** | <img src="resources/doc/screenshot/linux/8-settings.png" width="200"/> | <img src="resources/doc/screenshot/windows/8-settings.png" width="200"/> | <img src="resources/doc/screenshot/macos/8-settings.png" width="200"/> |

### iOS — Real Device Screenshots

<p align="center">
  <sub>Captured on iPhone &nbsp;·&nbsp; Light &amp; Dark theme &nbsp;·&nbsp; Auto-matching via <code>&lt;picture&gt;</code> media queries</sub>
</p>

#### Diagnostic Flow

<table>
<tr>
  <td align="center" width="33%"><b>Idle</b></td>
  <td align="center" width="33%"><b>Running</b></td>
  <td align="center" width="33%"><b>Results</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-idle-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-idle-light.jpg" width="100%" alt="Diagnostics — idle state">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-running-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-running-light.jpg" width="100%" alt="Diagnostics — running">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-complete-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-complete-light.jpg" width="100%" alt="Diagnostics — results">
  </picture></td>
</tr>
</table>

#### Dashboard &amp; Report

<table>
<tr>
  <td align="center" width="33%"><b>Dashboard</b></td>
  <td align="center" width="33%"><b>Summary</b></td>
  <td align="center" width="33%"><b>Report</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/dashboard-complete-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/dashboard-complete-light.jpg" width="100%" alt="Dashboard with results">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/dashboard-summary-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/dashboard-summary-light.jpg" width="100%" alt="Dashboard summary cards">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/report-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/report-light.jpg" width="100%" alt="Diagnostic report preview">
  </picture></td>
</tr>
</table>

#### Configuration &amp; Settings

<table>
<tr>
  <td align="center" width="50%"><b>Config</b></td>
  <td align="center" width="50%"><b>Settings</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/config-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/config-light.jpg" width="100%" alt="Diagnostic group configuration">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/settings-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="100%" alt="App settings and about">
  </picture></td>
</tr>
</table>

### Android

> ⚠️ Android screenshots require a CI emulator (no KVM on GitHub-hosted
> runners — software emulation only). Generated on `workflow_dispatch`.

| Stage | Phone | Tablet |
|---|---|---|
| **Dashboard** | <img src="resources/doc/screenshot/android/phone/5-dashboard.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/5-dashboard.png" width="220"/> |
| **Running** | <img src="resources/doc/screenshot/android/phone/2-running.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/2-running.png" width="220"/> |
| **Results** | <img src="resources/doc/screenshot/android/phone/3-complete.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/3-complete.png" width="220"/> |
| **Report** | <img src="resources/doc/screenshot/android/phone/6-report.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/6-report.png" width="220"/> |
| **Config** | <img src="resources/doc/screenshot/android/phone/7-config.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/7-config.png" width="220"/> |

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

## Supported Platforms

| Platform | Arch | Notes |
|----------|------|-------|
| iOS | arm64 | StoreKit IAP, share sheet, WiFi SSID, native HTTP/DNS |
| Android | arm64 / x86_64 | Share sheet via FileProvider, JNI native diagnostics |
| Windows | x86_64 | Static (zero-DLL) and dynamic builds via MSYS2 UCRT64 |
| macOS | arm64 | Universal binary, native Homebrew Qt 6 |
| Linux | x86_64 / arm64 | AppImage + deb + rpm packages |

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

# Cross-compile specific target
./scripts/build-all.sh --target windows-x86_64

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
# Qt generates the release APK. CI aligns and signs it with apksigner; local
# release builds require signing with your own keystore before installation.
cmake --build build/android --target apk
# Do not install any *-unsigned.apk artifact.
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
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
  -DND_STATIC_QT=ON -DND_STRICT_STATIC_WINDOWS=ON -B build -S .
ninja -C build net_diagnostics
# The GitHub Actions static artifact is one EXE. Its PE imports are limited
# to Windows system APIs; GCC, Qt, curl, and other third-party DLLs are static.
```

## CI/CD

Automated builds on every push via GitHub Actions.

| Workflow | Platforms |
|----------|-----------|
| **build.yml** | Linux (x86_64 + arm64), Windows (x86_64 static + dynamic), macOS (arm64), Android (arm64 + x86_64) |
| **apple.yml** | macOS (arm64 app bundle), iOS (arm64 → TestFlight) |

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
