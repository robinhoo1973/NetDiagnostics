# NetDiagnostics

<p align="center">
  <b>專業跨平台網路診斷工具</b><br/>
  基於 Qt 6 / QML 和 libcurl 構建<br/>
  <sub>iOS &middot; Android &middot; Windows &middot; macOS &middot; Linux</sub>
</p>

[English](README.md) | [简体中文](README_zh_CN.md) | [繁體中文](README_zh_TW.md)

## 螢幕截圖

桌面截圖為真實系統級截圖。iOS 截圖從真機手動截取，含淺色/深色雙主題。Android 截圖從模擬器或真機截取。

### 桌面端

| 階段 | Linux | Windows | macOS |
|---|---|---|---|
| **儀表板** | <img src="resources/doc/screenshot/linux/5-dashboard.png" width="200"/> | <img src="resources/doc/screenshot/windows/5-dashboard.png" width="200"/> | <img src="resources/doc/screenshot/macos/5-dashboard.png" width="200"/> |
| **執行中** | <img src="resources/doc/screenshot/linux/2-running.png" width="200"/> | <img src="resources/doc/screenshot/windows/2-running.png" width="200"/> | <img src="resources/doc/screenshot/macos/2-running.png" width="200"/> |
| **結果** | <img src="resources/doc/screenshot/linux/3-complete.png" width="200"/> | <img src="resources/doc/screenshot/windows/3-complete.png" width="200"/> | <img src="resources/doc/screenshot/macos/3-complete.png" width="200"/> |
| **詳情** | <img src="resources/doc/screenshot/linux/4-detail.png" width="200"/> | <img src="resources/doc/screenshot/windows/4-detail.png" width="200"/> | <img src="resources/doc/screenshot/macos/4-detail.png" width="200"/> |
| **報告** | <img src="resources/doc/screenshot/linux/6-report.png" width="200"/> | <img src="resources/doc/screenshot/windows/6-report.png" width="200"/> | <img src="resources/doc/screenshot/macos/6-report.png" width="200"/> |
| **配置** | <img src="resources/doc/screenshot/linux/7-config.png" width="200"/> | <img src="resources/doc/screenshot/windows/7-config.png" width="200"/> | <img src="resources/doc/screenshot/macos/7-config.png" width="200"/> |
| **設定** | <img src="resources/doc/screenshot/linux/8-settings.png" width="200"/> | <img src="resources/doc/screenshot/windows/8-settings.png" width="200"/> | <img src="resources/doc/screenshot/macos/8-settings.png" width="200"/> |

### iOS — 真機截圖

<p align="center">
  <sub>iPhone 真機截取 &nbsp;·&nbsp; 淺色/深色雙主題 &nbsp;·&nbsp; 透過 <code>&lt;picture&gt;</code> 媒體查詢自動匹配</sub>
</p>

#### 診斷流程

<table>
<tr>
  <td align="center" width="33%"><b>空閒</b></td>
  <td align="center" width="33%"><b>執行中</b></td>
  <td align="center" width="33%"><b>結果</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-idle-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-idle-light.jpg" width="100%" alt="診斷 — 空閒狀態">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-running-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-running-light.jpg" width="100%" alt="診斷 — 執行中">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/diagnostics-complete-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/diagnostics-complete-light.jpg" width="100%" alt="診斷 — 結果">
  </picture></td>
</tr>
</table>

#### 儀表板與報告

<table>
<tr>
  <td align="center" width="33%"><b>儀表板</b></td>
  <td align="center" width="33%"><b>摘要</b></td>
  <td align="center" width="33%"><b>報告</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/dashboard-complete-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/dashboard-complete-light.jpg" width="100%" alt="儀表板含結果">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/dashboard-summary-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/dashboard-summary-light.jpg" width="100%" alt="儀表板摘要卡片">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/report-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/report-light.jpg" width="100%" alt="診斷報告預覽">
  </picture></td>
</tr>
</table>

#### 配置與設定

<table>
<tr>
  <td align="center" width="50%"><b>配置</b></td>
  <td align="center" width="50%"><b>設定</b></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/config-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/config-light.jpg" width="100%" alt="診斷組配置">
  </picture></td>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/settings-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="100%" alt="應用設定與關於">
  </picture></td>
</tr>
</table>

### Android

> Android 截圖從模擬器或真機截取。

| 階段 | 手機 | 平板 |
|---|---|---|
| **儀表板** | <img src="resources/doc/screenshot/android/phone/5-dashboard.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/5-dashboard.png" width="220"/> |
| **執行中** | <img src="resources/doc/screenshot/android/phone/2-running.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/2-running.png" width="220"/> |
| **結果** | <img src="resources/doc/screenshot/android/phone/3-complete.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/3-complete.png" width="220"/> |
| **報告** | <img src="resources/doc/screenshot/android/phone/6-report.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/6-report.png" width="220"/> |
| **配置** | <img src="resources/doc/screenshot/android/phone/7-config.png" width="140"/> | <img src="resources/doc/screenshot/android/tablet/7-config.png" width="220"/> |

## 主題

NetDiagnostics 內建自訂主題引擎，支援**深色**和**淺色**兩種主題，可在設定中切換。截圖使用 `<picture>` 媒體查詢自動匹配你的 GitHub 主題。

<table>
<tr>
  <td align="center" width="50%"><b>深色</b></td>
  <td align="center" width="50%"><b>淺色</b></td>
</tr>
<tr>
  <td><img src="resources/doc/screenshot/ios/phone/settings-dark.jpg" width="100%" alt="設定 — 深色主題"></td>
  <td><img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="100%" alt="設定 — 淺色主題"></td>
</tr>
</table>

> **強調色：** 青色 (`#22D3EE`) &nbsp;|&nbsp; **背景：** `#0F172A`（深色）/ `#FFFFFF`（淺色） &nbsp;|&nbsp; **卡片：** 雙主題均採用細邊框 + 圓角設計。

## 多語言

NetDiagnostics 內建完整的 **9 語言國際化系統**，由自訂 `Tr.*` QML 單例驅動。所有 UI 字串——導航標籤、診斷狀態訊息、報告章節標題、群組名稱和設定描述——均透過統一的 `t()` 函式在執行階段翻譯，該函式同時接受 9 種語言變體。

語言選擇器位於 **設定 → 語言**，以 ComboBox 下拉框形式呈現，按字母順序列出所有語言。切換後顯示 3 秒 toast 確認，防止誤觸後迷失在不熟悉的語言介面中。

### 支援的語言

| # | 語言 | 內部索引 | UI 標籤 |
|---|----------|:---:|----------|
| 1 | English | `0` | English |
| 2 | Français | `1` | Français |
| 3 | Deutsch | `2` | Deutsch |
| 4 | Русский | `3` | Русский |
| 5 | Italiano | `4` | Italiano |
| 6 | 简体中文 | `5` | 简体中文 |
| 7 | 繁體中文 | `6` | 繁體中文 |
| 8 | Español | `7` | Español |
| 9 | Português | `8` | Português |

### 語言選擇器

<table>
<tr>
  <td align="center"><b>設定 — 語言下拉框</b><br/><sub>ComboBox 按字母順序列出全部 9 種語言；切換時顯示 toast 提示。</sub></td>
</tr>
<tr>
  <td><picture>
    <source media="(prefers-color-scheme: dark)" srcset="resources/doc/screenshot/ios/phone/settings-dark.jpg">
    <img src="resources/doc/screenshot/ios/phone/settings-light.jpg" width="320" alt="設定頁面顯示語言選擇下拉框">
  </picture></td>
</tr>
</table>

### 國際化架構

```
QML: Tr.dashboard  →  Translations.qml (pragma Singleton)
                    →  t("Dashboard", "Tableau de bord", "Dashboard", ...)
                    →  lang index 來自 appState.languageIndex (C++)
                    →  即時、零延遲的 UI 重新渲染
```

> **關鍵設計決策：**
> - **單一來源 `t()` 函式** — 一條字串的所有 9 種翻譯集中在一行；新增語言只需在程式碼庫中增加一個參數位置。
> - **按字母順序排序** — ComboBox 模型按顯示名稱排序，使用者可預測地找到自己的語言，與內部索引無關。
> - **持久化儲存** — `appState.setLanguage(idx)` 寫入 QSettings；選擇在應用程式重啟後依然生效。
> - **Toast 回饋** — 防止「誤觸後迷失」的 UX 缺陷：誤觸後 toast 顯示所選語言名稱，使用者可立即切換回來。

## 功能特性

### 診斷引擎 — 46 項測試，5 個群組

| 組 | 測試數 | 說明 |
|-------|-------|-------------|
| **G1** — 系統與適配器 | 8 | 網路適配器、NIC 進階資訊、WiFi 資訊、有線網資訊、DHCP 狀態、IP 配置、活動連線、蜂巢網路資訊 |
| **G2** — 連線與安全 | 6 | 網路設定檔、TCP 設定、預設閘道、路由表、ARP 表、代理設定 |
| **G3** — 網際網路與 DNS | 6 | Netskope 狀態、DNS 伺服器、DNS 快取、DNS 可信度、IP 地理定位、網際網路連線與速度 |
| **G4** — 遠端主機 | 6 | DNS 解析、Ping、IPv6 連線、路由追蹤、路徑 Ping、MTU 發現 |
| **G5** — 協定 | 20 | URL 解析、TCP 連線、服務橫幅、HTTP 請求、HTTP 標頭、安全標頭、SSL 憑證、HTTP 重新導向、HTTP 壓縮、HTTP 計時、FTP、SSH、Email、Telnet、MySQL、PostgreSQL、Redis、MongoDB、LDAP、MQTT |

### 核心能力

- **跨平台** — 單一 C++/QML 程式碼庫涵蓋 iOS、Android、Windows、macOS、Linux
- **即時診斷** — 每項測試完成後即時推送結果，含進度指示器和狀態徽章
- **深色/淺色雙主題** — 自訂主題引擎，青色強調色調，可在設定中切換
- **9 語言介面** — English、Français、Deutsch、Русский、Italiano、简体中文、繁體中文、Español、Português
- **報告匯出** — PDF 和 HTML 報告，含摘要卡片、分組統計和詳細診斷輸出
- **分組配置** — 每次診斷執行可按群組或逐項啟用/停用測試
- **G5 協定套件** — 原始 TCP 通訊端診斷：MySQL、PostgreSQL、Redis、MongoDB、LDAP、MQTT，以及 FTP、SSH、SMTP/IMAP/POP3、Telnet 橫幅偵測
- **DNS 診斷** — dig 風格輸出，含解析器快取檢查、伺服器回應性和可信度校驗
- **目標分析** — URL 元件拆解、IP 分類（RFC 1918 / CGNAT / APIPA / 迴路 / 公網）、已知埠參考表
- **進階內購** — 非消耗型解鎖，可透過系統分享選單和郵件分享報告

## 支援平台

| 平台 | 架構 |
|----------|------|
| iOS | arm64 |
| Android | arm64, x86_64 |
| Windows | x86_64（靜態和動態） |
| macOS | arm64 |
| Linux | x86_64, arm64 |

## 技術堆疊

| 層級 | 技術 |
|-------|-----------|
| 框架 | Qt 6 (C++17) — Core、Concurrent、Quick、QuickControls2、Network、Widgets |
| UI | QML + 自訂 ThemeEngine，9 語言國際化（`Tr.*` 單例） |
| HTTP/HTTPS | libcurl（桌面端）、NSURLSession（iOS）、HttpURLConnection（Android） |
| TCP / SSL | QTcpSocket、QSslSocket，含 X.509 憑證鏈檢查 |
| 平台 API | WLAN API + IP Helper（Windows）、NetworkExtension + CoreTelephony（iOS）、ConnectivityManager + WifiManager via JNI（Android）、SystemConfiguration + CoreWLAN（macOS） |
| 建置 | CMake 3.22+、Ninja、GitHub Actions CI |
| 字型 | JetBrains Mono（UI）、DejaVu Sans Mono（終端輸出的製表符） |

## 授權條款

MIT
