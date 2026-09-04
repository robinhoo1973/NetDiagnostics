# iOS CI 已知问题与预防指南

> 从项目 git 历史中 `fix:` 类提交（1067 条总计，472 条最近一年）提炼的**重复性问题模式**。
> 提交前逐项自查，避免同样的问题再次进入 CI。

---

## 目录

1. [CMake 语法与正则陷阱](#1-cmake-语法与正则陷阱)
2. [预处理器 CI 检查](#2-预处理器-ci-检查)
3. [文件路径与引用完整性](#3-文件路径与引用完整性)
4. [Apple 平台签名与配置](#4-apple-平台签名与配置)
5. [Apple SDK 宏/枚举名冲突](#5-apple-sdk-宏枚举名冲突) 🔥
6. [QColor(QRgb) Alpha 字节陷阱](#6-qcolorqrgb-alpha-字节陷阱) 🔥
7. [静态 vs 动态链接配置](#7-静态-vs-动态链接配置)
8. [Qt API 版本兼容性](#8-qt-api-版本兼容性)
9. [CI 工作流配置](#9-ci-工作流配置)
10. [C++ 并发与线程安全](#10-c-并发与线程安全)
11. [缺失的头文件引用](#11-缺失的头文件引用)
12. [提交前自检清单](#12-提交前自检清单)

---

## 1. CMake 语法与正则陷阱

### 1.1 注释只能用 `#`，禁止 `//`

| 项 | 说明 |
|----|------|
| **现象** | `CMake Parse error. Expected a command name, got unquoted argument with text "//"` |
| **根因** | CMake 只认 `#` 作为注释符，`//` 被当成未加引号的命令参数 |
| **典型场景** | 从 C++/JS 文件复制注释到 CMake 时用错语法 |
| **提交溯源** | `b226896` — `fix: use CMake-style (#) comments instead of C++ (//)` |
| **检测方法** | `grep -rn '//' cmake/ CMakeLists.txt` 返回空 |

```cmake
# ✅ 正确
set(APPLE_TEAM_ID "0000000000")  # fallback for local builds

# ❌ 错误
set(APPLE_TEAM_ID "0000000000")  // fallback for local builds
```

### 1.2 CMake regex 不支持 `{n}` 量词

| 项 | 说明 |
|----|------|
| **现象** | `file(STRINGS ... REGEX)` 或 `if(MATCHES)` 静默失败，明明存在的字符串匹配不上 |
| **根因** | CMake 内置 regex 引擎**不支持** `{n}` 量词（既不是 ERE 也不是完整 BRE） |
| **提交溯源** | `9c30e98` — `fix: replace {6} quantifier with explicit hex chars in CMake regex` |
| **检测方法** | 在 CMake 文件中搜索 `\{[0-9]` 或 `{[0-9]` |

```cmake
# ❌ CMake 中永远不匹配
if(myVar MATCHES "#[0-9A-Fa-f]{6}")

# ✅ 必须显式重复
if(myVar MATCHES "#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]")
```

### 1.3 CMake regex 的其他限制

| 限制 | 说明 |
|------|------|
| `{n}` / `{n,m}` | 不支持，用重复替代 |
| `+` | 支持（一个或多个） |
| `*` | 支持（零个或多个） |
| `\|` | 有限支持 |
| `\b` | 支持词边界 |
| `[0-9A-F]` | 字符范围支持正常工作 |
| `[^ ]` | 否定字符类支持 |

---

## 2. 预处理器 CI 检查

> 历史出问题最多的领域 — **8 次修复**同一检查工具。

### 2.1 必须使用 `#if defined(X)` — 禁止 `#ifdef` / `#ifndef`

| 项 | 说明 |
|----|------|
| **现象** | CI "Code review compliance" 步骤报 `#if/#endif count mismatch` |
| **根因** | CI 正则 `^\s*#(if\|elif)\b` 只匹配 `#if`/`#elif`，不匹配 `#ifdef`/`#ifndef` |
| **提交溯源** | `b8fa217`, `62dc33f`, `8758774` — 3 次修复同一正则 |

```cpp
// ✅ 正确 — CI 正则能识别
#if defined(ND_DEBUG)
// ...
#endif

// ❌ 错误 — CI 正则无法识别
#ifdef ND_DEBUG
// ...
#endif
```

### 2.2 禁止使用 `#elif` — 用 `#else` + `#if` 替代

| 项 | 说明 |
|----|------|
| **现象** | 10 个文件 20 处 `#elif` 导致 CI 预处理器平衡检查失败 |
| **根因** | `#elif` 增加 `if` 计数但不增加 `endif` 计数（与父 `#if` 共享 `#endif`） |
| **提交溯源** | `0b57bdf` — 全项目 20 处 `#elif` → `#else`+`#if` |

```cpp
// ✅ 正确
#if defined(PLATFORM_A)
// ...
#else
#  if defined(PLATFORM_B)
// ...
#  endif  // PLATFORM_B
#endif  // PLATFORM_A

// ❌ 错误 — CI 计数失衡
#if defined(PLATFORM_A)
// ...
#elif defined(PLATFORM_B)
// ...
#endif
```

### 2.3 `#if` 后必须有非空格字符

| 项 | 说明 |
|----|------|
| **现象** | `#if defined(X)` 导致 CI 正则匹配失败 |
| **根因** | 早期 CI 正则 `^\s*#if[^ ]` — `defined` 前的空格被当成分隔符 |
| **提交溯源** | `62dc33f` — 全项目转换 `#if defined(X)` → `#if(defined(X))` |

```cpp
// ✅ 正确 — #if 后无空格
#if(defined(ND_DEBUG))

// ❌ 旧风格 — #if 后有空格
#if defined(ND_DEBUG)
```

### 2.4 禁止 UTF-8 BOM

| 项 | 说明 |
|----|------|
| **现象** | CMake/CI 的正则 `^` 行首锚点失效 |
| **根因** | UTF-8 BOM（`EF BB BF`）让 `^` 不匹配文件真实的第 0 列 |
| **提交溯源** | `143b762` — `fix(build): remove UTF-8 BOM` |

### 2.5 `#` 开头的行不是注释就是预处理器

| 项 | 说明 |
|----|------|
| **现象** | CI 把注释中的 `#` 误判为预处理器指令 |
| **根因** | 正则不够精确 |
| **提交溯源** | `ad6bf92` — `fix(ci): match only actual preprocessor directives (^\s*#)` |

---

## 3. 文件路径与引用完整性

### 3.1 移动文件必须更新所有引用

| 项 | 说明 |
|----|------|
| **现象** | CI 构建时找不到文件 → `No such file` |
| **根因** | 移动/重命名文件后只改了 CMake 的部分引用，遗漏 CI workflow 和 .gitignore |
| **提交溯源** | `54ed528` — 6 个文件移入 `resources/apple/`，需更新 4 个文件 11 处引用 |
| **检测方法** | `grep -rn "old/path" . --exclude-dir=.git` 返回空 |

**需要检查的位置：**
- `cmake/*.cmake` — CMake 模块
- `CMakeLists.txt` — 主构建文件
- `.github/workflows/*.yml` — CI 工作流
- `.gitignore` — 忽略规则
- `.github/actions/*/action.yml` — 复用 Action

### 3.2 引用的文件必须被 git 跟踪

| 项 | 说明 |
|----|------|
| **现象** | CI checkout 后 `file not found` |
| **根因** | 文件未被 `git add` 或存在于 `.gitignore` 中 |
| **检测方法** | `git ls-files --error-unmatch <file>` |

---

## 4. Apple 平台签名与配置

### 4.1 禁止在 entitlements 中硬编码 derived 属性

| 项 | 说明 |
|----|------|
| **现象** | `ITMS-90288: "key value not allowed"` |
| **根因** | `com.apple.application-identifier` 和 `com.apple.developer.team-identifier` 由 codesign 自动注入，显式声明会与 provisioning profile 冲突 |
| **提交溯源** | `a65dfe6` — `fix: remove derived entitlements from macos.entitlements.in` |

```xml
<!-- ❌ 不要显式声明以下两项 -->
<!-- <key>com.apple.application-identifier</key> -->
<!-- <key>com.apple.developer.team-identifier</key> -->
```

### 4.2 Team ID 必须从 CI secrets 传入

| 项 | 说明 |
|----|------|
| **现象** | Fork 仓库 CI 失败，Team ID 硬编码为主仓库值 |
| **根因** | Team ID 硬编码在文件或 CMake 中，不同 fork 签名标识不同 |
| **提交溯源** | `e06dd30` — `feat: pass Apple Team ID from secrets.IOS_TEAM_ID` |

```yaml
# ✅ 在 CI workflow 中
"$QT_INSTALL_DIR/$QT_VERSION/ios/bin/qt-cmake" \
  -DAPPLE_TEAM_ID="${{ secrets.IOS_TEAM_ID }}" \
  -B build -S .
```

### 4.3 Helper 进程使用独立的 entitlements

| 项 | 说明 |
|----|------|
| **现象** | `ITMS-90885: "nested executable has app identifier but no provisioning profile"` |
| **根因** | QtWebEngineProcess.app 不能声明 application-identifier，它继承父 app 的身份 |
| **提交溯源** | `318d507` — `fix: macOS signing — inside-out order + helper entitlements` |

### 4.4 Info.plist 和 entitlements 属于 `apple/` 目录

| 项 | 说明 |
|----|------|
| **规则** | `Info.plist`、`Info-macos.plist`、`*.entitlements`、`*.entitlements.in` → `resources/apple/` |
| **不在 cert/** | `cert/` 只放加密凭证（`.key`、`.p12`、`.pem`、`.mobileprovision`） |

---

## 5. Apple SDK 宏/枚举名冲突

> **7 次修复** — iOS 构建失败的第二大根因领域。

### 5.1 禁止使用 Apple SDK 保留词做枚举值

| 项 | 说明 |
|----|------|
| **现象** | iOS 编译报 "redefinition" 错误，看似无冲突的名字报错 |
| **根因** | Apple SDK 头文件 `#define` 了 `Clean`、`check`、`verify`、`TRUE`、`FALSE` 等常用词 |
| **提交溯源** | `3bf1a86`、`d775132`、`94da903`、`1b95d3e` — 5 次连续修复同一个枚举 |

```cpp
// ❌ 错误 — "Clean" 被 Apple SDK #define 为其他东西
enum Verdict { Clean, Suspect, Tampered, Hijacked };

// ✅ 正确 — 使用项目前缀隔离命名空间
enum class DiagStatus { Pass, Warning, Fail, Critical };
// 或加前缀
#define DNS_INTEGRITY_PASS    0
#define DNS_INTEGRITY_SUSPECT 1
```

### 5.2 `signals` 是 Qt 保留词

| 项 | 说明 |
|----|------|
| **现象** | struct 成员 `signals` 在 iOS Qt 构建中变成 `public` |
| **根因** | Qt 的 `signals:` 宏展开为 `public:`，任何名为 `signals` 的标识符都会被替换 |
| **提交溯源** | `e1c1f7e` — `rename signals to detectedSignals` |

```cpp
// ❌ 错误
struct Result { int signals; };

// ✅ 正确
struct Result { int detectedSignals; };
```

### 5.3 Apple SDK 高危词汇清单

永远不要用作 C++ 标识符：

| 高危词 | 原因 |
|--------|------|
| `Clean` | Apple SDK `#define` |
| `Check`、`Verify` | Apple SDK 常见宏 |
| `signals`、`slots`、`emit` | Qt 关键字展开 |
| `TRUE`、`FALSE` | 可能被 `#define` |
| `DEBUG` | macOS SDK 可能定义 |

**检测方法**：`grep -rn '#define.*\<Clean\>\|#define.*\<Check\>\|#define.*\<Verify\>'` 检查 SDK 头文件。

---

## 6. QColor(QRgb) Alpha 字节陷阱

> **1 次修复，最高影响级别 — 所有 HTML 报表图标全不可见。**

| 项 | 说明 |
|----|------|
| **现象** | 状态图标完全透明/不可见，无崩溃无报错 |
| **根因** | `QColor(QRgb)` 需要 `0xAARRGGBB`（8 位），宏定义为 `0xRRGGBB`（6 位）时 alpha 字节为 `0x00` = 全透明 |
| **提交溯源** | `a895516` — `fix: QColor(QRgb) alpha=0 bug in APPC_*_RGB macros` |

```cpp
// ❌ 错误 — alpha=0x00，图标不可见
#define APPC_PASS_GREEN_RGB  0x059669
QColor color(APPC_PASS_GREEN_RGB);  // alpha = 0x00

// ✅ 正确 — 显式设置 alpha 为 0xFF
#define APPC_PASS_GREEN_RGB  0xFF059669
```

**检测方法**：搜索 `#define.*RGB.*0x[0-9A-Fa-f]{6}$`（6 位无 alpha 的宏定义必须是 8 位）。

---

## 7. 静态 vs 动态链接配置

> **15 次修复** — 构建配置领域最频繁的问题。

### 7.1 强制静态链接的 CMake 配置

| 项 | 说明 |
|----|------|
| **现象** | Windows 构建产物运行时提示 "DLL not found" |
| **根因** | CMake `find_library` 默认优先选 `.dll.a`（导入库）而非 `.a`（静态库） |
| **提交溯源** | `82b1e65`、`d0cf18c`、`78cb0a5`、`f4d4d18` — 多次迭代才彻底解决 |

```cmake
# ✅ 必须在 find_package(Qt6) 之前设置
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
find_package(Qt6 REQUIRED ...)
```

### 7.2 静态 curl 链接必须定义 CURL_STATICLIB

| 项 | 说明 |
|----|------|
| **现象** | curl 符号找不到（`__imp_curl_*`） |
| **根因** | curl 头文件默认导出 `__declspec(dllimport)` 符号前缀 |
| **提交溯源** | `487922e` — `force static curl (CURL_STATICLIB + libcurl.a)` |

```cmake
target_compile_definitions(${TARGET} PRIVATE CURL_STATICLIB)
```

### 7.3 静态链接自检步骤

1. `objdump -p app.exe | grep "DLL Name"` — 确认无意外 DLL 依赖
2. `CMAKE_FIND_LIBRARY_SUFFIXES` 是否在 `find_package` 前设置
3. 所有第三方库是否在 `-Wl,-Bstatic` 包裹中

---

## 8. Qt API 版本兼容性

> **10 次修复** — Qt 版本升级和平台差异导致的构建失败。

### 8.1 关键规则速查

| 规则 | 错误写法 | 正确写法 |
|------|---------|---------|
| QNetworkAccessManager::get | `get(req, QByteArray())` 双参数 | `get(req)` 单参数（Qt 6.8+） |
| QtConcurrent include | `#include <QtConcurrent/QtConcurrentRun>` | `#include <QtConcurrent/QtConcurrent>` |
| QStringLiteral 必须单行 | 多行 `\` 续接 | 全部放在同一行 |
| Qt 6.8+ aqt 目录名 | `ios` / `macos` | `ios_arm64` / `clang_64` |
| iOS Qt 路径变量 | `QT_HOST_PATH` 错误路径 | 精确匹配 aqt install 目录结构 |

**提交溯源**：`4c5d220`、`472100a`、`3d90e5d`、`d0fa992`、`8b17136`、`8d69b64`

### 8.2 Qt macOS/iOS 版本兼容检查

```bash
# 确认 aqt 安装的目录结构
ls "$QT_INSTALL_DIR/$QT_VERSION/"
# 应该是 ios_arm64 (Qt 6.8+) 而非 ios

# 确认 QT_HOST_PATH 存在
ls "$QT_HOST_DIR/$QT_VERSION/clang_64/bin/qt-cmake"
```

---

## 9. CI 工作流配置

### 5.1 必须使用 `set -euo pipefail`

| 项 | 说明 |
|----|------|
| **现象** | CI 构建失败但 job 显示为成功（`exit code 0`） |
| **根因** | `xcodebuild 2>&1 \| tail -100` — tail 返回 0 掩盖了 xcodebuild 的非零退出码 |
| **提交溯源** | `72c01e7` — `fix(ci): iOS symbol verification pipefail→process substitution` |

```bash
# ✅ 正确
set -euo pipefail
xcodebuild ... 2>&1 | tee build.log
# 或
output=$(xcodebuild ... 2>&1) || { echo "$output" | tail -200; exit 1; }
```

### 5.2 GitHub Actions YAML 缩进必须一致

| 项 | 说明 |
|----|------|
| **现象** | workflow 解析失败或 heredoc 内容错位 |
| **根因** | YAML 对缩进敏感，heredoc 内的空格可能被 YAML 解释器干扰 |
| **提交溯源** | `841842b` — `fix(ci): iOS/macOS pipefail, plist verification, xcarchive path, signing error handling` |

### 5.3 变量/Secret 命名

| 变量 | 用途 | 来源 |
|------|------|------|
| `IOS_TEAM_ID` | Apple Developer Team ID | GitHub Secret |
| `ND_WIFI_ENTITLEMENT` | iOS Wi-Fi 权限开关 | CI env |
| `APPLE_TEAM_ID` | 传给 CMake 的 Team ID | `secrets.IOS_TEAM_ID` |
| `APPSTORE_CONNECT_*` | App Store Connect API 凭证 | GitHub Secret |

### 5.4 aqt 安装布局漂移 — 工具路径禁止硬编码

| 项 | 说明 |
|----|------|
| **现象** | Gate 1 AOT 全部 QML 文件 `qmlcachegen: No such file or directory`（bash exec 报 ENOENT） |
| **根因** | `aqtinstall` 未锁版本（`pip install aqtinstall`），3.x 的 mac desktop 布局由 `clang_64` 变为 `macos`；build-ios 中 `QMLCACHEGEN` env 硬编码旧路径，且 env 覆盖短路了 `qml-aot-gate.sh` 的 `find` 自愈兜底 |
| **关键教训** | 同一 kit 布局在 action 内多处硬编码（QT_HOST_PATH=macos 已更新、QMLCACHEGEN=clang_64 未更新）→ 布局知识必须**单一来源**：解析一次（磁盘实际布局为事实源），导出环境变量共用；工具路径让脚本自愈发现，不要用 env 覆盖钉死 |
| **提交溯源** | 2026-09-05 — `fix(ci): ...`（Gate 1 路径单一来源解析） |

### 5.5 Qt 平台插件必须显式部署 — macdeployqt 默认只带 cocoa

| 项 | 说明 |
|----|------|
| **现象** | startup-smoke 启动烟测 `Abort trap: 6`，日志 `Could not find the Qt platform plugin "offscreen" in ""`（可用插件仅 cocoa） |
| **根因** | macdeployqt 只部署应用实际 GUI 平台（cocoa）的插件；烟测用 `QT_QPA_PLATFORM=offscreen` 启动时，bundle 内 `Contents/PlugIns/platforms` 没有 `libqoffscreen.dylib` |
| **关键教训** | 无头烟测需要的平台插件是**可选部署项**，不是 Qt 运行时的隐含能力。构建侧显式复制进 bundle（与 cocoa 并列，运行时按 `QT_QPA_PLATFORM` 选择），fail-closed：源缺失即失败 |
| **提交溯源** | 2026-09-05 — `fix(ci): ...`（build-macos testflight 模式捆绑 offscreen 插件） |

---

## 10. C++ 并发与线程安全

> **5 次修复** — `std::terminate()` 崩溃和线程池死锁。

### 10.1 `std::thread::join()` 必须检查 joinable

| 项 | 说明 |
|----|------|
| **现象** | 程序退出时 `std::terminate()` 被调用 |
| **根因** | `noexcept` 析构函数中 `join()` 不可 join 的线程抛异常 → `terminate()` |
| **提交溯源** | `e906a9e` — `ThreadGuard destructor noexcept safety` |

```cpp
// ✅ 正确
~ThreadGuard() {
    if (t.joinable()) t.join();
}

// ❌ 错误 — joinable()==false 时抛异常 → std::terminate()
~ThreadGuard() { t.join(); }
```

### 10.2 Lambda 捕获 + 循环 + 并行 = Bug

| 项 | 说明 |
|----|------|
| **现象** | 并行代码中所有线程看到相同的循环变量值 |
| **根因** | `[&i]` 按引用捕获循环变量，线程启动时 `i` 已递增 |
| **提交溯源** | `f72c010` — `lambda capture 'i' + qMakePair in GeoIP parallel code` |

```cpp
// ✅ 正确 — 按值捕获或使用局部副本
for (int i = 0; i < n; ++i) {
    pool.enqueue([=] { process(i); });  // = 捕获值
}

// ❌ 错误 — 所有线程都看到 i 的最终值
for (int i = 0; i < n; ++i) {
    pool.enqueue([&] { process(i); });  // & 引用捕获
}
```

### 10.3 优先 std::thread 而非 QtConcurrent::run

| 项 | 说明 |
|----|------|
| **现象** | 网络/磁盘 I/O 密集时线程池死锁 |
| **根因** | `QtConcurrent::run()` 共享全局线程池，阻塞操作可耗尽 |
| **提交溯源** | `d18938b` — `std::thread in dohQueryFull instead of QtConcurrent::run` |

```cpp
// ✅ 对阻塞 I/O 使用独立线程
std::thread t([data] { blockingNetworkCall(data); });
// ... 稍后 join

// ❌ 阻塞 I/O 占用线程池
QtConcurrent::run([data] { blockingNetworkCall(data); });
```

---

## 11. 缺失的头文件引用

> **8 次修复** — 某平台编译通过但另一平台报 "undefined type"。

| 原因 | 涉及头文件 | 提交 |
|------|-----------|------|
| MinGW 不间接包含 `<windows.h>` | `HANDLE` 未定义 | `7310b91` |
| iOS SDK 不间接包含 `<resolv.h>` | `__res_state` 未定义 | `b882f7b` |
| Qt 头文件不包含 `<atomic>` `<chrono>` | 标准库类型未定义 | `491fd0c` |

**规则**：使用某类型就必须 `#include` 对应的标准头文件，不依赖间接包含链。

---

## 12. 提交前自检清单

> 每次 `git commit` 前逐项检查。打 `[x]` 表示确认通过。

### CMake
- [ ] `grep -rn '//' cmake/ CMakeLists.txt` 返回空（不含 `http://`）
- [ ] CMake regex 中无 `{n}` 量词（搜索 `{[0-9]` 和 `\{[0-9]`）
- [ ] 无 UTF-8 BOM：`file cmake/*.cmake CMakeLists.txt | grep BOM` 返回空

### 预处理器（C/C++/ObjC 文件）
- [ ] 使用 `#if(defined(X))` 格式，不使用 `#ifdef`/`#ifndef`
- [ ] 使用 `#else` + `#if` 替代 `#elif`
- [ ] 嵌套深时 `#endif` 后加注释标注条件：`#endif  // PLATFORM_IOS`

### 文件路径
- [ ] 移动/重命名文件后：`grep -rn "old/path" . --exclude-dir=.git --exclude-dir=review` 返回空
- [ ] 检查引用点：`cmake/`、`CMakeLists.txt`、`.github/workflows/`、`.gitignore`
- [ ] 新增文件已 `git add`

### Apple 平台
- [ ] entitlements 中无 `application-identifier` 和 `team-identifier`（derived，自动注入）
- [ ] `Info.plist` / `*.entitlements` 在 `resources/apple/` 下
- [ ] CI workflow 中 `APPLE_TEAM_ID` 从 `secrets.IOS_TEAM_ID` 传入 CMake

### CI 脚本
- [ ] 每个 `run:` 块以 `set -euo pipefail` 开头
- [ ] 管道后不直接用 `tail`/`head` 吞掉退出码

### Apple SDK / Qt 关键词
- [ ] 枚举值和全局名不用 `Clean`、`Check`、`Verify`、`signals`、`TRUE`
- [ ] 使用 `enum class` 而非裸 `enum`
- [ ] `QColor(QRgb)` 宏定义必须是 8 位 hex（含 `0xFF` alpha 前缀）

### 静态链接
- [ ] `CMAKE_FIND_LIBRARY_SUFFIXES=".a"` 在 `find_package` 之前
- [ ] 链接 curl 时定义了 `CURL_STATICLIB` 编译宏
- [ ] `objdump -p` 或 `ntldd` 确认无意外 DLL 依赖

### C++ 安全
- [ ] `std::thread::join()` 在析构函数中受 `if(joinable())` 保护
- [ ] Lambda 在循环中创建线程/异步任务时使用值捕获
- [ ] 阻塞 I/O 使用 `std::thread` 而非 `QtConcurrent::run()`
- [ ] 使用某类型必须显式 `#include` 对应标准头文件

### 常规
- [ ] 本地 `cmake -P cmake/VerifyPaletteSync.cmake` 通过
- [ ] 提交信息格式：`fix(<scope>): <描述>` 或 `feat(<scope>): <描述>`

---

## 7. 预提交钩子

项目提供了自动化检查脚本，覆盖上述大部分规则：

```bash
# 首次安装
bash scripts/install-hooks

# 之后每次 git commit 自动运行
# 也可以手动执行检查：
bash scripts/pre-commit
```

钩子检查项目（11 项）：CMake 注释语法、CMake regex 量词、预处理器风格（`#ifdef`/`#elif`）、UTF-8 BOM、Palette 同步、Entitlements derived 属性、CI pipefail、QColor(QRgb) alpha 字节、Apple SDK 保留词、`std::thread::join()` 安全。

> 检查失败会中止提交。详细规则见本文档各章节。

---

## 附录：问题模式统计

| 类别 | 修复次数 | 严重度 | 最严重的一次 |
|------|:------:|:----:|------|
| 静态 vs 动态链接 | **15** | 🔴 高 | Windows 构建产物缺 DLL 无法运行 |
| 预处理器 CI 检查 | **8** | 🟡 中 | 全项目 20 处 `#elif` → `#else`+`#if` |
| Apple SDK 宏/枚举冲突 | **7** | 🔴 高 | iOS 构建 redefinition 错误 |
| Qt API 版本兼容 | **10** | 🟡 中 | macOS 26 / Qt 6.8 升级导致构建失败 |
| Apple 签名/entitlement | **8** | 🔴 致命 | ITMS-90288 blocked App Store submission |
| 缺失 `#include` 头文件 | **8** | 🟡 中 | MinGW/iOS 平台编译报 undefined type |
| CI YAML/bash 语法 | **8** | 🟡 中 | pipefail 缺失掩盖构建失败 |
| C++ 并发/线程安全 | **5** | 🔴 高 | `std::terminate()` 退出时崩溃 |
| CMake 正则兼容 | **3** | 🟡 中 | `{6}` 量词导致 CI 误报 |
| CI 路径/文件引用 | **3** | 🟡 中 | 移动 6 个文件遗漏 11 处引用 |
| Team ID 硬编码 | **2** | 🟡 中 | Fork 仓库 CI 全崩 |
| CMake 注释语法 | **1** | 🔴 高 | `//` 导致解析错误 |
| QColor(QRgb) alpha | **1** | 🔴 致命 | HTML 报表所有图标不可见 |

> 最后更新：2026-07-30 | 基于 1067 条 fix 提交（472 条最近一年）全面分析
