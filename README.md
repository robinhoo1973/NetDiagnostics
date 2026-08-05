# NetDiagnostics

<p align="center">
  <b>Professional cross-platform network diagnostic toolkit</b><br/>
  Built with Qt 6 / QML and libcurl<br/>
  <sub>iOS &middot; Android &middot; Windows &middot; macOS &middot; Linux</sub>
</p>

[English](README.md) | [简体中文](README_zh_CN.md) | [繁體中文](README_zh_HK.md)

## Screenshots

Real OS-level captures &middot; Dark theme &middot; No mockups, no app modifications.

### Desktop

<table>
<tr>
  <td align="center" width="33%"><b>Diagnostics — Idle</b></td>
  <td align="center" width="33%"><b>Diagnostics — Running</b></td>
  <td align="center" width="33%"><b>Dashboard — Idle</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/desktop/diagnostics-idle-dark.png" width="100%" alt="Diagnostics idle"></td>
  <td><img src="resources/doc/screenshot/desktop/diagnostics-running-dark.png" width="100%" alt="Diagnostics running"></td>
  <td><img src="resources/doc/screenshot/desktop/dashboard-idle-dark.png" width="100%" alt="Dashboard idle"></td>
</tr>
</table>

<table>
<tr>
  <td align="center" width="33%"><b>Dashboard — Complete</b></td>
  <td align="center" width="33%"><b>Config</b></td>
  <td align="center" width="33%"><b>Settings</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/desktop/dashboard-complete-dark.png" width="100%" alt="Dashboard complete"></td>
  <td><img src="resources/doc/screenshot/desktop/config-dark.png" width="100%" alt="Config"></td>
  <td><img src="resources/doc/screenshot/desktop/settings-dark.png" width="100%" alt="Settings"></td>
</tr>
</table>

### Phone

<table>
<tr>
  <td align="center" width="16%"><b>Idle</b></td>
  <td align="center" width="16%"><b>Running</b></td>
  <td align="center" width="16%"><b>Dashboard</b></td>
  <td align="center" width="16%"><b>Config</b></td>
  <td align="center" width="16%"><b>Settings</b></td>
  <td align="center" width="16%"><b>Report</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/diagnostics-idle-dark.jpg">
    <img src="resources/doc/screenshot/phone/diagnostics-idle-light.jpg" width="100%" alt="Idle">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/diagnostics-running-dark.jpg">
    <img src="resources/doc/screenshot/phone/diagnostics-running-light.jpg" width="100%" alt="Running">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/dashboard-complete-dark.jpg">
    <img src="resources/doc/screenshot/phone/dashboard-complete-light.jpg" width="100%" alt="Dashboard">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/config-dark.png">
    <img src="resources/doc/screenshot/phone/config-light.png" width="100%" alt="Config">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/settings-dark.jpg">
    <img src="resources/doc/screenshot/phone/settings-light.jpg" width="100%" alt="Settings">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/report-dark.jpg">
    <img src="resources/doc/screenshot/phone/report-light.jpg" width="100%" alt="Report">
  </picture></td>
</tr>
</table>

### Pad

<table>
<tr>
  <td align="center" width="33%"><b>Diagnostics — Idle</b></td>
  <td align="center" width="33%"><b>Diagnostics — Running</b></td>
  <td align="center" width="33%"><b>Diagnostics — Complete</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/pad/diagnostics-idle-dark.png" width="100%" alt="Diagnostics idle"></td>
  <td><img src="resources/doc/screenshot/pad/diagnostics-running-dark.png" width="100%" alt="Diagnostics running"></td>
  <td><img src="resources/doc/screenshot/pad/diagnostics-complete-dark.png" width="100%" alt="Diagnostics complete"></td>
</tr>
</table>

<table>
<tr>
  <td align="center" width="33%"><b>Dashboard — Complete</b></td>
  <td align="center" width="33%"><b>Config</b></td>
  <td align="center" width="33%"><b>Settings</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/pad/dashboard-complete-dark.png" width="100%" alt="Dashboard complete"></td>
  <td><img src="resources/doc/screenshot/pad/config-dark.png" width="100%" alt="Config"></td>
  <td><img src="resources/doc/screenshot/pad/settings-dark.png" width="100%" alt="Settings"></td>
</tr>
</table>

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
  <td><img src="resources/doc/screenshot/phone/settings-dark.jpg" width="100%" alt="Settings — Dark theme"></td>
  <td><img src="resources/doc/screenshot/phone/settings-light.jpg" width="100%" alt="Settings — Light theme"></td>
</tr>
</table>

> **Accent colour:** Cyan (`#22D3EE`) &nbsp;|&nbsp; **Background:** `#0F172A` (dark) / `#FFFFFF` (light) &nbsp;|&nbsp; **Surface cards:** subtle border + rounded corners on both themes.

## Multi-Language

NetDiagnostics speaks **15 languages** out of the box. Open Settings, pick your
language from the dropdown, and the entire app instantly switches — every
button, label, status message, and report heading. No restart. No gaps.
No partial translations.

### How to Switch

1. Open **Settings** from the bottom navigation bar
2. Tap the **Language** dropdown — all 15 languages appear, sorted alphabetically
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
  <td><img src="resources/doc/screenshot/phone/settings-dark.jpg" width="100%" alt="Settings — English"></td>
  <td><img src="resources/doc/screenshot/phone/settings-light.jpg" width="100%" alt="Settings — 简体中文"></td>
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
  <td><img src="resources/doc/screenshot/phone/config-dark.png" width="100%" alt="Config — English"></td>
  <td><img src="resources/doc/screenshot/phone/config-light.png" width="100%" alt="Config — 简体中文"></td>
</tr>
<tr>
  <td valign="top" colspan="2"><sub><b>Translated on this screen:</b> 5 diagnostic group names (System &amp; Adapters → 系统和适配器, Connectivity &amp; Security → 连接与安全, Internet &amp; DNS → 互联网与DNS, Remote Host → 远程主机, Protocol → 协议), 46 individual test toggle labels, select-all / deselect-all buttons</sub></td>
</tr>
</table>

### Available Languages

<table>
<tr>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/cn.png" width="16" height="12" style="vertical-align:middle">&nbsp;简体中文</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/hk.png" width="16" height="12" style="vertical-align:middle">&nbsp;繁體中文</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/jp.png" width="16" height="12" style="vertical-align:middle">&nbsp;日本語</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/kr.png" width="16" height="12" style="vertical-align:middle">&nbsp;한국어</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/vn.png" width="16" height="12" style="vertical-align:middle">&nbsp;Tiếng Việt</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/in.png" width="16" height="12" style="vertical-align:middle">&nbsp;हिन्दी</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/tr.png" width="16" height="12" style="vertical-align:middle">&nbsp;Türkçe</td>
  <td align="center" width="12.5%"><img src="https://flagcdn.com/16x12/us.png" width="16" height="12" style="vertical-align:middle">&nbsp;English</td>
</tr>
<tr>
  <td align="center"><img src="https://flagcdn.com/16x12/fr.png" width="16" height="12" style="vertical-align:middle">&nbsp;Français</td>
  <td align="center"><img src="https://flagcdn.com/16x12/de.png" width="16" height="12" style="vertical-align:middle">&nbsp;Deutsch</td>
  <td align="center"><img src="https://flagcdn.com/16x12/ru.png" width="16" height="12" style="vertical-align:middle">&nbsp;Русский</td>
  <td align="center"><img src="https://flagcdn.com/16x12/it.png" width="16" height="12" style="vertical-align:middle">&nbsp;Italiano</td>
  <td align="center"><img src="https://flagcdn.com/16x12/es.png" width="16" height="12" style="vertical-align:middle">&nbsp;Español</td>
  <td align="center"><img src="https://flagcdn.com/16x12/br.png" width="16" height="12" style="vertical-align:middle">&nbsp;Português</td>
  <td align="center" colspan="2"><img src="https://flagcdn.com/16x12/sa.png" width="16" height="12" style="vertical-align:middle">&nbsp;العربية</td>
</tr>
</table>

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
- **15-language UI** — 简体中文, 繁體中文, 日本語, 한국어, Tiếng Việt, हिन्दी, Türkçe, English, Français, Deutsch, Русский, Italiano, Español, Português, العربية
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
| UI | QML with custom ThemeEngine, 15-language i18n via `Tr.*` singleton |
| HTTP/HTTPS | libcurl (desktop), NSURLSession (iOS), HttpURLConnection (Android) |
| TCP / SSL | QTcpSocket, QSslSocket with X.509 chain inspection |
| Platform APIs | WLAN API + IP Helper (Windows), NetworkExtension + CoreTelephony (iOS), ConnectivityManager + WifiManager via JNI (Android), SystemConfiguration + CoreWLAN (macOS) |
| Build | CMake 3.22+, Ninja, GitHub Actions CI |
| Fonts | JetBrains Mono (UI), DejaVu Sans Mono (box-drawing for terminal output) |

## License

MIT
