# iOS CI 已知问题与预防指南

> 从项目 git 历史中 `fix:` 类提交（2025年6月至今 472 条）提炼的**重复性问题模式**。
> 提交前逐项自查，避免同样的问题再次进入 CI。

---

## 目录

1. [CMake 语法与正则陷阱](#1-cmake-语法与正则陷阱)
2. [预处理器 CI 检查](#2-预处理器-ci-检查)
3. [文件路径与引用完整性](#3-文件路径与引用完整性)
4. [Apple 平台签名与配置](#4-apple-平台签名与配置)
5. [CI 工作流配置](#5-ci-工作流配置)
6. [提交前自检清单](#6-提交前自检清单)

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

## 5. CI 工作流配置

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

---

## 6. 提交前自检清单

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

钩子检查项目：CMake 注释语法、regex 量词、预处理器风格、BOM、Palette 同步、Entitlements derived 属性。

> 检查失败会中止提交。详细规则见本文档各章节。

---

## 附录：问题模式统计

| 类别 | 修复次数 | 最严重的一次 |
|------|:------:|------|
| 预处理器 CI 检查 | **8** | 全项目 20 处 `#elif` → `#else`+`#if` |
| CMake 正则 | **3** | `{6}` 量词导致 CI 误报 |
| Apple 签名配置 | **4** | ITMS-90288 blocked App Store submission |
| CI 路径/文件引用 | **3** | 移动 6 个文件遗漏 11 处引用更新 |
| YAML/bash 语法 | **4** | pipefail 缺失导致构建失败被掩盖 |
| CMake 注释语法 | **1** | `//` 导致解析错误 |
| Team ID 硬编码 | **2** | Fork 仓库 CI 全崩 |

> 最后更新：2026-07-30 | 基于 472 条 fix 提交分析
