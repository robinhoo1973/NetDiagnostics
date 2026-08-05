# NetDiagnostics

<p align="center">
  <b>专业跨平台网络诊断工具</b><br/>
  基于 Qt 6 / QML 和 libcurl 构建<br/>
  <sub>iOS &middot; Android &middot; Windows &middot; macOS &middot; Linux</sub>
</p>

[English](README.md) | [简体中文](README_zh_CN.md) | [繁體中文](README_zh_HK.md)

## 屏幕截图

真实系统级截图 &middot; 深色主题 &middot; 非 mockup，未修改应用代码。

### 桌面

<table>
<tr>
  <td align="center" width="33%"><b>诊断 — 空闲</b></td>
  <td align="center" width="33%"><b>诊断 — 运行中</b></td>
  <td align="center" width="33%"><b>仪表盘 — 空闲</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/desktop/diagnostics-idle-dark.png" width="100%" alt="诊断空闲"></td>
  <td><img src="resources/doc/screenshot/desktop/diagnostics-running-dark.png" width="100%" alt="诊断运行中"></td>
  <td><img src="resources/doc/screenshot/desktop/dashboard-idle-dark.png" width="100%" alt="仪表盘空闲"></td>
</tr>
</table>

<table>
<tr>
  <td align="center" width="33%"><b>仪表盘 — 完成</b></td>
  <td align="center" width="33%"><b>配置</b></td>
  <td align="center" width="33%"><b>设置</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/desktop/dashboard-complete-dark.png" width="100%" alt="仪表盘完成"></td>
  <td><img src="resources/doc/screenshot/desktop/config-dark.png" width="100%" alt="配置"></td>
  <td><img src="resources/doc/screenshot/desktop/settings-dark.png" width="100%" alt="设置"></td>
</tr>
</table>

### 手机

<table>
<tr>
  <td align="center" width="16%"><b>空闲</b></td>
  <td align="center" width="16%"><b>运行中</b></td>
  <td align="center" width="16%"><b>仪表盘</b></td>
  <td align="center" width="16%"><b>配置</b></td>
  <td align="center" width="16%"><b>设置</b></td>
  <td align="center" width="16%"><b>报告</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/diagnostics-idle-dark.jpg">
    <img src="resources/doc/screenshot/phone/diagnostics-idle-light.jpg" width="100%" alt="空闲">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/diagnostics-running-dark.jpg">
    <img src="resources/doc/screenshot/phone/diagnostics-running-light.jpg" width="100%" alt="运行中">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/dashboard-complete-dark.jpg">
    <img src="resources/doc/screenshot/phone/dashboard-complete-light.jpg" width="100%" alt="仪表盘">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/config-dark.png">
    <img src="resources/doc/screenshot/phone/config-light.png" width="100%" alt="配置">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/settings-dark.jpg">
    <img src="resources/doc/screenshot/phone/settings-light.jpg" width="100%" alt="设置">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/phone/report-dark.jpg">
    <img src="resources/doc/screenshot/phone/report-light.jpg" width="100%" alt="报告">
  </picture></td>
</tr>
</table>

### 平板

<table>
<tr>
  <td align="center" width="33%"><b>诊断 — 空闲</b></td>
  <td align="center" width="33%"><b>诊断 — 运行中</b></td>
  <td align="center" width="33%"><b>诊断 — 完成</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/pad/diagnostics-idle-dark.png" width="100%" alt="诊断空闲"></td>
  <td><img src="resources/doc/screenshot/pad/diagnostics-running-dark.png" width="100%" alt="诊断运行中"></td>
  <td><img src="resources/doc/screenshot/pad/diagnostics-complete-dark.png" width="100%" alt="诊断完成"></td>
</tr>
</table>

<table>
<tr>
  <td align="center" width="33%"><b>仪表盘 — 完成</b></td>
  <td align="center" width="33%"><b>配置</b></td>
  <td align="center" width="33%"><b>设置</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/pad/dashboard-complete-dark.png" width="100%" alt="仪表盘完成"></td>
  <td><img src="resources/doc/screenshot/pad/config-dark.png" width="100%" alt="配置"></td>
  <td><img src="resources/doc/screenshot/pad/settings-dark.png" width="100%" alt="设置"></td>
</tr>
</table>

## 主题

NetDiagnostics 内置自定义主题引擎，支持**深色**和**浅色**两种主题，可在设置中切换。截图使用 `<picture>` 媒体查询自动匹配你的 GitHub 主题。

<table>
<tr>
  <td align="center" width="50%"><b>深色</b></td>
  <td align="center" width="50%"><b>浅色</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/phone/settings-dark.jpg" width="100%" alt="设置 — 深色主题"></td>
  <td><img src="resources/doc/screenshot/phone/settings-light.jpg" width="100%" alt="设置 — 浅色主题"></td>
</tr>
</table>

> **强调色：** 青色 (`#22D3EE`) &nbsp;|&nbsp; **背景：** `#0F172A`（深色）/ `#FFFFFF`（浅色） &nbsp;|&nbsp; **卡片：** 双主题均采用细边框 + 圆角设计。

## 多语言

NetDiagnostics 开箱即支持 **12 种语言**。打开设置，从下拉框中选择你的语言，整个应用即刻切换——每个按钮、每条标签、每个状态消息、每个报告标题。无需重启。无死角。无遗漏。

### 如何切换

1. 从底部导航栏打开 **设置**
2. 点击 **语言** 下拉框——全部 12 种语言按字母顺序排列
3. 选择语言——toast 确认你的选择（"Français ✓"）
4. 整个应用立即以新语言渲染

### 对比：英文 vs 简体中文

同一个页面，两种语言。每个标签、按钮、分组名称和描述都完整翻译——无硬编码字符串，无英文回退。

#### 设置

<table>
<tr>
  <td align="center" width="50%"><b>English</b><br/><sub>深色主题</sub></td>
  <td align="center" width="50%"><b>简体中文</b><br/><sub>浅色主题</sub></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/phone/settings-dark.jpg" width="100%" alt="设置 — English"></td>
  <td><img src="resources/doc/screenshot/phone/settings-light.jpg" width="100%" alt="设置 — 简体中文"></td>
</tr>
<tr>
  <td valign="top" colspan="2"><sub><b>此页面翻译内容：</b> 导航标签（仪表板、诊断、配置、报告、设置），章节标题（语言、主题、关于），主题切换标签，语言下拉框选项，版本信息</sub></td>
</tr>
</table>

#### 配置

<table>
<tr>
  <td align="center" width="50%"><b>English</b><br/><sub>深色主题</sub></td>
  <td align="center" width="50%"><b>简体中文</b><br/><sub>浅色主题</sub></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/phone/config-dark.png" width="100%" alt="配置 — English"></td>
  <td><img src="resources/doc/screenshot/phone/config-light.png" width="100%" alt="配置 — 简体中文"></td>
</tr>
<tr>
  <td valign="top" colspan="2"><sub><b>此页面翻译内容：</b> 5 个诊断组名称（System &amp; Adapters → 系统和适配器，Connectivity &amp; Security → 连接与安全，Internet &amp; DNS → 互联网与DNS，Remote Host → 远程主机，Protocol → 协议），46 个独立测试开关标签，全选/取消全选按钮</sub></td>
</tr>
</table>

### 可用语言

<p align="center">
  <img src="https://flagcdn.com/16x12/cn.png" width="16" height="12"> 简体中文 &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/hk.png" width="16" height="12"> 繁體中文 &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/jp.png" width="16" height="12"> 日本語 &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/kr.png" width="16" height="12"> 한국어
</p>
<p align="center">
  <img src="https://flagcdn.com/16x12/us.png" width="16" height="12"> English &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/fr.png" width="16" height="12"> Français &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/de.png" width="16" height="12"> Deutsch &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/ru.png" width="16" height="12"> Русский
</p>
<p align="center">
  <img src="https://flagcdn.com/16x12/it.png" width="16" height="12"> Italiano &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/es.png" width="16" height="12"> Español &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/br.png" width="16" height="12"> Português &nbsp;·&nbsp;
  <img src="https://flagcdn.com/16x12/sa.png" width="16" height="12"> العربية
</p>

> **提示：** 语言选择会自动保存。用户重启应用后无需重新选择。如果不小心点错语言，toast 会显示新语言名称，方便立即切换回来。

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
- **12 语言界面** — 简体中文、繁體中文、日本語、한국어、English、Français、Deutsch、Русский、Italiano、Español、Português、العربية
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
| UI | QML + 自定义 ThemeEngine，12 语言国际化（`Tr.*` 单例） |
| HTTP/HTTPS | libcurl（桌面端）、NSURLSession（iOS）、HttpURLConnection（Android） |
| TCP / SSL | QTcpSocket、QSslSocket，含 X.509 证书链检查 |
| 平台 API | WLAN API + IP Helper（Windows）、NetworkExtension + CoreTelephony（iOS）、ConnectivityManager + WifiManager via JNI（Android）、SystemConfiguration + CoreWLAN（macOS） |
| 构建 | CMake 3.22+、Ninja、GitHub Actions CI |
| 字体 | JetBrains Mono（UI）、DejaVu Sans Mono（终端输出的制表符） |

## 许可证

MIT
