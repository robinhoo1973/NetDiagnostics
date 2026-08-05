# NetDiagnostics

<p align="center">
  <b>专业跨平台网络诊断工具</b><br/>
  基于 Qt 6 / QML 和 libcurl 构建<br/>
  <sub>iOS &middot; Android &middot; Windows &middot; macOS &middot; Linux</sub>
</p>

[English](README.md) | [简体中文](README_zh_CN.md) | [繁體中文](README_zh_TW.md)

## 屏幕截图

桌面截图为真实系统级截图。iOS 截图从真机手动截取，含浅色/深色双主题。Android 截图从模拟器或真机截取。

### 桌面端

| 阶段 | Linux | Windows | macOS |
|---|---|---|---|
| **仪表盘** | <img src="resources/doc/screenshot/linux/5-dashboard.png" width="200"/> | <img src="resources/doc/screenshot/windows/5-dashboard.png" width="200"/> | <img src="resources/doc/screenshot/macos/5-dashboard.png" width="200"/> |
| **运行中** | <img src="resources/doc/screenshot/linux/2-running.png" width="200"/> | <img src="resources/doc/screenshot/windows/2-running.png" width="200"/> | <img src="resources/doc/screenshot/macos/2-running.png" width="200"/> |
| **结果** | <img src="resources/doc/screenshot/linux/3-complete.png" width="200"/> | <img src="resources/doc/screenshot/windows/3-complete.png" width="200"/> | <img src="resources/doc/screenshot/macos/3-complete.png" width="200"/> |
| **详情** | <img src="resources/doc/screenshot/linux/4-detail.png" width="200"/> | <img src="resources/doc/screenshot/windows/4-detail.png" width="200"/> | <img src="resources/doc/screenshot/macos/4-detail.png" width="200"/> |
| **报告** | <img src="resources/doc/screenshot/linux/6-report.png" width="200"/> | <img src="resources/doc/screenshot/windows/6-report.png" width="200"/> | <img src="resources/doc/screenshot/macos/6-report.png" width="200"/> |
| **配置** | <img src="resources/doc/screenshot/linux/7-config.png" width="200"/> | <img src="resources/doc/screenshot/windows/7-config.png" width="200"/> | <img src="resources/doc/screenshot/macos/7-config.png" width="200"/> |
| **设置** | <img src="resources/doc/screenshot/linux/8-settings.png" width="200"/> | <img src="resources/doc/screenshot/windows/8-settings.png" width="200"/> | <img src="resources/doc/screenshot/macos/8-settings.png" width="200"/> |

### iOS — 真机截图

<p align="center">
  <sub>iPhone 真机截取 &nbsp;·&nbsp; 浅色/深色双主题 &nbsp;·&nbsp; 通过 <code>&lt;picture&gt;</code> 媒体查询自动匹配</sub>
</p>

#### 诊断流程

<table>
<tr>
  <td align="center" width="33%"><b>空闲</b></td>
  <td align="center" width="33%"><b>运行中</b></td>
  <td align="center" width="33%"><b>结果</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-idle-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-idle-light.jpg" width="100%" alt="诊断 — 空闲状态">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-running-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-running-light.jpg" width="100%" alt="诊断 — 运行中">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-complete-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-complete-light.jpg" width="100%" alt="诊断 — 结果">
  </picture></td>
</tr>
</table>

#### 仪表盘与报告

<table>
<tr>
  <td align="center" width="33%"><b>仪表盘</b></td>
  <td align="center" width="33%"><b>摘要</b></td>
  <td align="center" width="33%"><b>报告</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/dashboard-complete-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/dashboard-complete-light.jpg" width="100%" alt="仪表盘含结果">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/dashboard-summary-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/dashboard-summary-light.jpg" width="100%" alt="仪表盘摘要卡片">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/report-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/report-light.jpg" width="100%" alt="诊断报告预览">
  </picture></td>
</tr>
</table>

#### 配置与设置

<table>
<tr>
  <td align="center" width="50%"><b>配置</b></td>
  <td align="center" width="50%"><b>设置</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/config-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/config-light.jpg" width="100%" alt="诊断组配置">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/settings-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="100%" alt="应用设置与关于">
  </picture></td>
</tr>
</table>

### Android

> Android 截图从模拟器或真机截取。

| 阶段 | 手机 | 平板 |
|---|---|---|
| **仪表盘** | <img src="resources/doc/screenshot/android/phone/5-dashboard.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/5-dashboard.png" width="220"/> |
| **运行中** | <img src="resources/doc/screenshot/android/phone/2-running.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/2-running.png" width="220"/> |
| **结果** | <img src="resources/doc/screenshot/android/phone/3-complete.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/3-complete.png" width="220"/> |
| **报告** | <img src="resources/doc/screenshot/android/phone/6-report.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/6-report.png" width="220"/> |
| **配置** | <img src="resources/doc/screenshot/android/phone/7-config.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/7-config.png" width="220"/> |

## 功能特性

### 诊断引擎 — 46 项测试，5 个分组

| 组 | 测试数 | 说明 |
|-------|-------|-------------|
| **G1** — 系统与适配器 | 8 | 网络适配器、NIC 高级信息、WiFi 信息、有线网信息、DHCP 状态、IP 配置、活动连接、蜂窝网络信息 |
| **G2** — 连接与安全 | 6 | 网络配置文件、TCP 设置、默认网关、路由表、ARP 表、代理设置 |
| **G3** — 互联网与 DNS | 6 | Netskope 状态、DNS 服务器、DNS 缓存、DNS 可信度、IP 地理定位、互联网连接与速度 |
| **G4** — 远程主机 | 6 | DNS 解析、Ping、IPv6 连接、路由追踪、路径 Ping、MTU 发现 |
| **G5** — 协议 | 20 | URL 解析、TCP 连接、服务横幅、HTTP 请求、HTTP 头、安全头、SSL 证书、HTTP 重定向、HTTP 压缩、HTTP 计时、FTP、SSH、Email、Telnet、MySQL、PostgreSQL、Redis、MongoDB、LDAP、MQTT |

### 核心能力

- **跨平台** — 单一 C++/QML 代码库覆盖 iOS、Android、Windows、macOS、Linux
- **实时诊断** — 每项测试完成后实时推送结果，含进度指示器和状态徽章
- **深色/浅色双主题** — 自定义主题引擎，青色强调色调，可在设置中切换
- **9 语言界面** — English、Français、Deutsch、Русский、Italiano、简体中文、繁體中文、Español、Português
- **报告导出** — PDF 和 HTML 报告，含摘要卡片、分组统计和详细诊断输出
- **分组配置** — 每次诊断运行可按组或逐项启用/禁用测试
- **G5 协议套件** — 原始 TCP 套接字诊断：MySQL、PostgreSQL、Redis、MongoDB、LDAP、MQTT，以及 FTP、SSH、SMTP/IMAP/POP3、Telnet 横幅检测
- **DNS 诊断** — dig 风格输出，含解析器缓存检查、服务器响应性和可信度校验
- **目标分析** — URL 组件拆解、IP 分类（RFC 1918 / CGNAT / APIPA / 环回 / 公网）、已知端口参考表
- **高级内购** — 非消耗型解锁，可通过系统分享菜单和邮件分享报告

## 支持平台

| 平台 | 架构 |
|----------|------|
| iOS | arm64 |
| Android | arm64, x86_64 |
| Windows | x86_64（静态和动态） |
| macOS | arm64 |
| Linux | x86_64, arm64 |

## 技术栈

| 层级 | 技术 |
|-------|-----------|
| 框架 | Qt 6 (C++17) — Core、Concurrent、Quick、QuickControls2、Network、Widgets |
| UI | QML + 自定义 ThemeEngine，9 语言国际化（`Tr.*` 单例） |
| HTTP/HTTPS | libcurl（桌面端）、NSURLSession（iOS）、HttpURLConnection（Android） |
| TCP / SSL | QTcpSocket、QSslSocket，含 X.509 证书链检查 |
| 平台 API | WLAN API + IP Helper（Windows）、NetworkExtension + CoreTelephony（iOS）、ConnectivityManager + WifiManager via JNI（Android）、SystemConfiguration + CoreWLAN（macOS） |
| 构建 | CMake 3.22+、Ninja、GitHub Actions CI |
| 字体 | JetBrains Mono（UI）、DejaVu Sans Mono（终端输出的制表符） |

## 许可证

MIT
