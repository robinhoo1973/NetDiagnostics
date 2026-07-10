# NetDiagnostics

专业跨平台网络诊断工具。基于 Qt 6 / QML 和 libcurl 构建，支持 **iOS、Android、Windows、macOS 和 Linux**。

## 功能特性

### 诊断引擎（5 组 45 项测试）

| 组 | 测试数 | 说明 |
|-------|-------|-------------|
| **G1** — 系统与适配器 | 8 | 网络适配器、NIC 高级信息、WiFi 信息、有线网信息、DHCP 状态、IP 配置、活动连接、蜂窝网络信息 |
| **G2** — 连接与安全 | 6 | 网络配置文件、TCP 设置、默认网关、路由表、ARP 表、代理设置 |
| **G3** — 互联网与 DNS | 5 | Netskope 状态、DNS 服务器、DNS 缓存、DNS 污染、互联网速度测试 |
| **G4** — 远程主机 | 6 | DNS 解析、Ping、路由追踪、路径 Ping、MTU 发现、端口扫描 |
| **G5** — 网站 / URL | 20 | URL 解析、TCP 连接、服务横幅、cURL 详细输出、HTTP 头、安全头、SSL 证书、HTTP 重定向、HTTP 压缩、HTTP 计时、FTP、SSH、Email、Telnet、MySQL、PostgreSQL、Redis、MongoDB、LDAP、MQTT |

### 主要特性

- **跨平台** — 单代码库覆盖 iOS、Android、Windows、macOS、Linux
- **纯 C++ 诊断** — 零 Shell 命令，直接调用操作系统 API
- **实时引擎** — 每项测试完成后实时推送结果
- **报告导出** — PDF（仪表盘风格摘要）和 HTML（暗色主题完整详情），含进度条、状态指示器和主题配色
- **设置持久化** — 语言、活动组和逐项测试启用在应用重启后自动恢复（QSettings）
- **高级内购** — 非消耗型解锁，可使用系统分享/邮件分享报告
- **9 语言界面** — English、Français、Deutsch、Русский、Italiano、简体中文、繁體中文、Español、Português
- **暗色主题** — 自定义暗色界面，青色 (`#22D3EE`) 强调色调；报告样式与应用主题一致
- **G5 协议诊断** — 20 项按协议分类的测试，包括 MySQL、PostgreSQL、Redis、MongoDB、LDAP、MQTT（基于原始 TCP 套接字）
- **DNS 诊断** — dig 风格输出，包含 HEADER/QUESTION/ANSWER 段落、DNSSEC 验证、DNS 污染检测
- **速度测试** — 兼容 Ookla 的下载/上传带宽测量
- **分组顺序执行** — `std::thread` 并发，`std::atomic` 分组跟踪
- **启动崩溃诊断** — `ND_DEBUG=ON` 将带时间戳的启动事件写入 `%TEMP%\NetDiagnostics_startup.log`
- **单实例锁** — 防止重复启动应用实例
- **模拟器模式** — 桌面端设备框 UI，便于测试

## 支持平台

| 平台 | 架构 | 状态 |
|----------|------|--------|
| iOS | arm64 | ✅ 完整支持（StoreKit 内购、分享、WiFi SSID） |
| Android | arm64 | ✅ 完整支持（通过 FileProvider 分享） |
| Linux | arm64 / x86_64 | ✅ 完整支持 |
| Windows | x86_64 / ARM64 | ✅ 完整支持 |
| macOS | x86_64 / arm64 | ✅ 完整支持 |

## 技术栈

**核心框架：** Qt 6 (C++17) — Core、Concurrent、Quick、QuickControls2、Widgets、Network

**UI 层：** QML + 自定义暗色主题引擎，通过 Qt Linguist 实现 9 语言国际化

**网络库：**
- **libcurl** — 桌面端 HTTP/HTTPS 诊断（传输、头信息、计时、压缩）
- **QSslSocket** — SSL/TLS 证书检查，完整 X.509 链提取
- **QTcpSocket** — 原始 TCP 连接、服务横幅和 7 种协议诊断
- **原生套接字 API** — `winsock2`（Windows）、POSIX 套接字（Linux/macOS）、Network framework（iOS）

**平台 SDK：**
- **iOS** — NetworkExtension、CoreLocation、CoreTelephony、StoreKit、CFNetwork、NSURLSession
- **Android** — JNI 封装（ConnectivityManager、WifiManager、TelephonyManager、`HttpURLConnection`）
- **Windows** — WLAN API、IP Helper API、WinHTTP、WinSock2
- **macOS** — SystemConfiguration、CoreWLAN、IOKit

**构建系统：** CMake 3.22+，Ninja 生成器；通过 GitHub Actions 实现 CI/CD（build.yml + apple.yml）

**字体：** JetBrains Mono（UI）、DejaVu Sans Mono（树状诊断的制表符字形）

## 构建

### 快速开始（自动化）

```bash
# 原生构建（自动检测宿主平台）
./scripts/build-all.sh

# 交叉编译指定目标 + 模拟器
./scripts/build-all.sh --target windows-x86_64 --sim

# 自动修复所有缺失依赖
./scripts/build-all.sh --fix --target all
```

### 手动构建

#### Linux / macOS

```bash
# 依赖
sudo apt install qt6-base-dev qt6-quickcontrols2-dev libcurl4-openssl-dev cmake ninja-build  # Linux
brew install qt@6 cmake ninja curl                                                             # macOS

# 构建
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build -S .
ninja -C build net_diagnostics
```

#### iOS

```bash
cmake -G Xcode \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/qt6/ios.toolchain.cmake \
  -B build/ios
# 在 Xcode 中打开 build/ios/*.xcodeproj → 选择设备 → Build
```

#### Android

```bash
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/qt6/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -B build/android
ninja -C build/android net_diagnostics
```

#### 模拟器

```bash
cmake -G Ninja -DBUILD_SIMULATOR=ON -B build -S .
ninja -C build net_diagnostics_sim
```

### 调试构建（启动崩溃诊断）

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DND_DEBUG=ON -B build -S .
ninja -C build net_diagnostics
# 运行应用 → 查看 %TEMP%\NetDiagnostics_startup.log 中的启动事件
```

### 无头测试

```bash
ND_MAX_TESTS=2 ND_AUTORUN=1 QT_QPA_PLATFORM=offscreen ./build/net_diagnostics
```

### Windows 静态构建 (MSYS2 UCRT64)

```bash
# 需要安装 MSYS2 和 mingw-w64-ucrt-x86_64-qt6-static
cmake -G Ninja -DCMAKE_PREFIX_PATH=/ucrt64/qt6-static \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -B build -S .
ninja -C build net_diagnostics
# 验证零非系统 DLL：
objdump -p build/net_diagnostics.exe | grep "DLL Name"
```

## CI/CD

每次推送通过 GitHub Actions（`build.yml` 和 `apple.yml`）自动进行多平台构建。覆盖 Linux (x86_64/arm64)、Windows (x86_64 静态 + 动态)、macOS (x86_64/arm64)、iOS (arm64) 和 Android (arm64/x86_64)。iOS TestFlight 部署通过 `apple.yml` 自动化完成。

## 应用内购买

**非消耗型高级版**内购（产品 ID：`com.netdiagnostic.app.premium`）可解锁报告分享功能（通过系统分享菜单或邮件）。基于 iOS StoreKit 构建，通过 `QSettings` 持久化存储，支持离线解锁。通过 App Store Connect 沙盒测试账号进行测试。

## 依赖

| 依赖 | 版本 | 用途 |
|------------|---------|---------|
| Qt 6 | ≥ 6.2 | Core、Concurrent、Quick、QuickControls2、Network |
| libcurl | ≥ 7.80 | HTTP/HTTPS 诊断（桌面端；iOS 使用 NSURLSession） |
| CMake | ≥ 3.22 | 构建系统 |

## 许可证

MIT
