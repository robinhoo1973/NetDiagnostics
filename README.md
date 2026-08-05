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

> Android screenshots are captured from an emulator or physical device.

| Stage | Phone | Tablet |
|---|---|---|
| **Dashboard** | <img src="resources/doc/screenshot/android/phone/5-dashboard.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/5-dashboard.png" width="220"/> |
| **Running** | <img src="resources/doc/screenshot/android/phone/2-running.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/2-running.png" width="220"/> |
| **Results** | <img src="resources/doc/screenshot/android/phone/3-complete.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/3-complete.png" width="220"/> |
| **Report** | <img src="resources/doc/screenshot/android/phone/6-report.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/6-report.png" width="220"/> |
| **Config** | <img src="resources/doc/screenshot/android/phone/7-config.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/7-config.png" width="220"/> |

## Themes

NetDiagnostics includes a custom theme engine with **Dark** and **Light** variants,
toggleable in Settings. Screenshots use `<picture>` media queries to match your
GitHub theme automatically.

<table>
<tr>
  <td align="center" width="50%"><b>Dark</b></td>
  <td align="center" width="50%"><b>Light</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/ios/phone/settings-dark.jpg" width="100%" alt="Settings — Dark theme"></td>
  <td><img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="100%" alt="Settings — Light theme"></td>
</tr>
</table>

> **Accent colour:** Cyan (`#22D3EE`) &nbsp;|&nbsp; **Background:** `#0F172A` (dark) / `#FFFFFF` (light) &nbsp;|&nbsp; **Surface cards:** subtle border + rounded corners on both themes.

## Multi-Language

NetDiagnostics speaks **9 languages** out of the box. Open Settings, pick your
language from the dropdown, and the entire app instantly switches — every
button, label, status message, and report heading. No restart. No gaps.
No partial translations.

### How to Switch

1. Open **Settings** from the bottom navigation bar
2. Tap the **Language** dropdown — all 9 languages appear, sorted alphabetically
3. Select a language — a toast confirms your choice ("Français ✓")
4. The whole app immediately renders in the new language

### Side-by-Side: English vs 简体中文

The same screens, two languages. Every label, button, group name, and
description is fully translated — no hardcoded strings, no fallback to English.

#### Settings

<table>
<tr>
  <td align="center" width="50%"><b>English</b><br/><sub>Dark theme</sub></td>
  <td align="center" width="50%"><b>简体中文</b><br/><sub>Light theme</sub></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/ios/phone/settings-dark.jpg" width="100%" alt="Settings — English"></td>
  <td><img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="100%" alt="Settings — 简体中文"></td>
</tr>
<tr>
  <td valign="top" colspan="2"><sub><b>Translated on this screen:</b> navigation tabs (Dashboard, Diagnostics, Config, Report, Settings), section headers (Language, Theme, About), theme toggle labels, language dropdown entries, version info</sub></td>
</tr>
</table>

#### Config

<table>
<tr>
  <td align="center" width="50%"><b>English</b><br/><sub>Dark theme</sub></td>
  <td align="center" width="50%"><b>简体中文</b><br/><sub>Light theme</sub></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/ios/phone/config-dark.jpg" width="100%" alt="Config — English"></td>
  <td><img src="resources/doc/screenshot/ios/phone/config-light.jpg" width="100%" alt="Config — 简体中文"></td>
</tr>
<tr>
  <td valign="top" colspan="2"><sub><b>Translated on this screen:</b> 5 diagnostic group names (System &amp; Adapters → 系统和适配器, Connectivity &amp; Security → 连接与安全, Internet &amp; DNS → 互联网与DNS, Remote Host → 远程主机, Protocol → 协议), 46 individual test toggle labels, select-all / deselect-all buttons</sub></td>
</tr>
</table>

### Available Languages

| Language | Label in UI |
|----------|------------|
| English | English |
| Français | Français |
| Deutsch | Deutsch |
| Русский | Русский |
| Italiano | Italiano |
| 简体中文 | 简体中文 |
| 繁體中文 | 繁體中文 |
| Español | Español |
| Português | Português |

> **Tip:** The language choice is saved automatically. Your users won't need to
> re-select it after restarting the app. And if they accidentally tap the wrong
> language, the toast shows the new language name so they can switch back
> immediately.

## Features

### Diagnostic Engine — 46 tests in 5 groups

| Group | Tests | Description |
|-------|-------|-------------|
| **G1** — System &amp; Adapters | 8 | Network Adapters, NIC Advanced, WiFi Information, Wired Information, DHCP Status, IP Configuration, Active Connections, Cellular Information |
| **G2** — Connectivity &amp; Security | 6 | Network Profile, TCP Settings, Default Gateway, Routing Table, ARP Table, Proxy Settings |
| **G3** — Internet &amp; DNS | 6 | Netskope Status, DNS Servers, DNS Cache, DNS Integrity, IP Geolocation, Internet Connectivity &amp; Speed |
| **G4** — Remote Host | 6 | DNS Resolution, Ping, IPv6 Connectivity, Traceroute, PathPing, MTU Discovery |
| **G5** — Protocol | 20 | URL Parsing, TCP Connect, Service Banner, HTTP Request, HTTP Headers, Security Headers, SSL Certificate, HTTP Redirect, HTTP Compression, HTTP Timing, FTP, SSH, Email, Telnet, MySQL, PostgreSQL, Redis, MongoDB, LDAP, MQTT |

### Key Capabilities

- **Cross-platform** — single C++/QML codebase targeting iOS, Android, Windows, macOS, Linux
- **Real-time diagnostics** — results stream live as each test completes, with progress indicators and status badges
- **Dark &amp; Light themes** — custom theme engine with cyan accent palette, toggled in Settings
- **9-language UI** — English, Français, Deutsch, Русский, Italiano, 简体中文, 繁體中文, Español, Português
- **Report export** — PDF and HTML reports with summary cards, per-group breakdowns, and detailed diagnostic output
- **Group configuration** — enable/disable individual tests or entire groups per diagnostic run
- **G5 protocol suite** — raw TCP socket diagnostics for MySQL, PostgreSQL, Redis, MongoDB, LDAP, MQTT plus banner detection for FTP, SSH, SMTP/IMAP/POP3, Telnet
- **DNS diagnostics** — dig-style output with resolver cache inspection, server responsiveness, and integrity checks
- **Target Analysis** — URL component breakdown, IP classification (RFC 1918 / CGNAT / APIPA / Loopback / Public), known port reference table
- **Premium IAP** — non-consumable unlock for report sharing via OS share sheet and email

## Supported Platforms

| Platform | Architectures |
|----------|--------------|
| iOS | arm64 |
| Android | arm64, x86_64 |
| Windows | x86_64 (static and dynamic) |
| macOS | arm64 |
| Linux | x86_64, arm64 |

## Technology Stack

| Layer | Technology |
|-------|-----------|
| Framework | Qt 6 (C++17) — Core, Concurrent, Quick, QuickControls2, Network, Widgets |
| UI | QML with custom ThemeEngine, 9-language i18n via `Tr.*` singleton |
| HTTP/HTTPS | libcurl (desktop), NSURLSession (iOS), HttpURLConnection (Android) |
| TCP / SSL | QTcpSocket, QSslSocket with X.509 chain inspection |
| Platform APIs | WLAN API + IP Helper (Windows), NetworkExtension + CoreTelephony (iOS), ConnectivityManager + WifiManager via JNI (Android), SystemConfiguration + CoreWLAN (macOS) |
| Build | CMake 3.22+, Ninja, GitHub Actions CI |
| Fonts | JetBrains Mono (UI), DejaVu Sans Mono (box-drawing for terminal output) |

## License

MIT
