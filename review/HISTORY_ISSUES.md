# NetDiagnostics 历史问题与解决方案完整文档

> 生成/更新日期: 2026-07-17
> 覆盖提交: `6012382` → `ebd3cf3` (25+ commits)
> 包含: CI 脚本 + 主程序代码 (C++/QML) + 构建系统

---

## 目录

- [一、UX/UI 问题](#一uxui-问题)
- [二、Speed Test 核心引擎](#二speed-test-核心引擎)
- [三、CI 脚本问题](#三ci-脚本问题)
- [四、iOS/macOS 构建与部署](#四iosmacos-构建与部署)
- [五、C++ 代码质量与重构](#五c-代码质量与重构)
- [六、模式识别：高频错误类型](#六模式识别高频错误类型)
- [七、预防措施检查清单](#七预防措施检查清单)

---

## 一、UX/UI 问题

### 1.1 分享按钮图标颜色不随 Theme 变化

**5WHY 根因**: `ShareButtons.qml` 中 `compactBtn`/`labeledBtn` 的 `accent` 属性通过 `Loader.onLoaded` + `Qt.binding()` 间接设置。`onLoaded` 在 QML 声明式绑定传播之**前**执行，捕获到默认值 `ThemeEngine.failRed` 而非调用方传入的 `ThemeEngine.cyan`。多个 `Binding on accent` 元素在 Loader 创建实例时存在冲突。

**解决方案**:
- 替换为单个显式 `Binding { target; property; value: ternary }` 模式（Qt 5.0+ 兼容，Loader 安全）
- 移除 `Loader.onLoaded` 中的 `item.accent = Qt.binding(...)` 代码
- 更新头部注释反映实际实现

**文件**: `src/Common/View/widgets/ShareButtons.qml`

---

### 1.2 分享按钮图标尺寸不匹配按钮

**5WHY 根因**: compact 模式 `_iconSize = 24`，`_btnHeight = 36`。图标仅 67% 填充。

**解决方案**:
- compact: `_iconSize = _btnHeight`（36dp，100% 填充）
- labeled: `_iconSize = Math.round(_btnHeight * 0.9)`（36dp，90% 填充）
- wide: `_iconSize = 40`（83% 填充，48dp 按钮）

**文件**: `src/Common/View/widgets/ShareButtons.qml`

---

### 1.3 诊断结果 header badge 布局错位

**5WHY 根因**: Badge 行使用 `Item { Layout.fillWidth: true }` spacer 右对齐，但在窄屏上 RowLayout 被压缩导致 spacer 失效。`implicitHeight=56` 不足以容纳双行内容。

**解决方案**:
- `implicitHeight` 提高到 72px
- 内边距从 4→6，行间距从 2→4
- 桌面端 badge 内联显示，手机端左对齐单独行

**文件**: `src/Diagnostics/View/DiagnosticScreen.qml`

---

### 1.4 SummaryCards 拼接错误

**5WHY 根因**: `Tr.totalDiagsLabel + (pass+warn+fail+skip+info)` 没有分隔符，显示为 "Total Diagnostics5"。

**解决方案**: 添加 `": "` 分隔符。

**文件**: `src/Common/View/widgets/SummaryCards.qml`

---

### 1.5 ThemeEngine.colors 不响应 Theme 切换

**5WHY 根因**: `readonly property var colors: ({...})` 在初始化时创建 JS 快照对象，`applyTheme()` 无法更新快照内的原始值。

**解决方案**: 移除 `readonly`，在 `applyTheme()` 中重建 `colors` 对象。

**文件**: `src/Common/View/theme/ThemeEngine.qml`

---

### 1.6 iOS 启动闪退 — compact 属性不存在

**5WHY 根因**: 前一轮修复删除了 `SummaryCards.qml` 中 `property bool compact`（死代码），但 `DashboardScreen.qml:131` 仍引用 `compact: true`。QML 引擎在加载时验证所有属性，失败 → `abort()`。

**解决方案**: 移除 `DashboardScreen.qml` 中的 `compact: true` 赋值。

**文件**: `src/Dashboard/View/DashboardScreen.qml`

**教训**: 删除公共 API（QML 属性、C++ 方法/字段）时，必须全文搜索引用。

---

## 二、Speed Test 核心引擎

### 2.1 InternetConnectivity Country Unknown 超时 180s

**5WHY 根因**: GeoIP HTTP 服务（ipip.net/ip-api.com 等）在 GFW 内全部被阻断 → 返回 "XX" → curated fallback 8 server 仅 2 CN → shuffle 后未命中 → allServers 回退 → download/upload 阶段 `dlTotalMs` guard 不计失败超时 → 480s 累积。

**解决方案**:
- 替换全部 HTTP GeoIP 为 DNS(UDP+DoH) → HTTPS GeoIP(x4) → 静态 IP 前缀链
- download/upload 阶段增加 `totalTimer.elapsed()` 墙钟守卫
- 每 tier 最多 fallback 3 server
- upload connect 超时从 30s 降到 10s

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.2 TCP Ping 全 1ms，无法区分 server

**5WHY 根因**: `tcpPingMs` 同区域内 CN server TCP RTT 全部 ~1ms（timer 精度限制）。TCP 连接成功 ≠ HTTP 下载能力。

**解决方案**:
- 新增 `tcpPingAvg()`：≤1ms 的 server 跑 50 次取平均值（~0.02ms 有效精度）
- TCP 筛选作为 Stage 1，喂 top-8 到 Stage 2 微下载

**文件**: `src/Diagnostics/Model/GCommon.cpp`, `GHelpers.h`

---

### 2.3 Micro-download 筛选全部失败

**5WHY 根因**:
1. `httpDownload` 要求严格 HTTP 200 (`contains(" 200 ")`)，CN server 返回非标准 HTTP 或二进制协议
2. `headersDone` 未找到 `\r\n\r\n` 时数据滞留在 `headerBuf`，`body` 为空

**解决方案**:
- HTTP 状态检查放宽：接受 3 位数字提取 + 非 HTTP 200 但 `bytes>1000` 的响应
- 二进制回退：`!headersDone && headerBuf.size()>1000` → 直接作为原始 body
- 微下载阶段增加 per-server 失败原因日志

**文件**: `src/Diagnostics/Model/GCommon.cpp`, `G3InternetSpeedTest.cpp`

---

### 2.4 Upload POST Host Header 缺少端口

**5WHY 根因**: `Host: best->host` 未包含端口 8080，反向代理可能错误路由（RFC 7230 §5.4）。

**解决方案**: 非 80 端口时包含 `host:port`。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.5 httpGet 硬编码 30s guard 覆盖调用方 timeout

**5WHY 根因**: `recvTimer.elapsed() > 30000` 覆写了 `timeoutMs` 参数（如 5000）。

**解决方案**: 使用 `timeoutMs` 替代硬编码值。

**文件**: `src/Diagnostics/Model/GCommon.cpp`

---

### 2.6 httpLatencyMs 200 状态码检查过严

**5WHY 根因**: `contains(" 200 ")` 要求空格包围，失败于 `"HTTP/1.0 200\r\n"`（无尾空格）。

**解决方案**: 提取第一个空格后的 3 位数字码，直接与 "200" 比较。

**文件**: `src/Diagnostics/Model/GCommon.cpp`

---

### 2.7 detectCountry timeoutMs 未传播

**5WHY 根因**: 接收参数 `timeoutMs`，但向子函数传递硬编码 2000/3000。

**解决方案**: 使用 `qMin(timeoutMs, 2000)` 正确传播。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.8 tcpPingAvg 过滤条件过严

**5WHY 根因**: `if (lat > 0)` 过滤了 sub-ms 连接（`tcpPingMs` 截断返回 0）。50x 平均后的亚毫秒延迟在 `>0` 判断下被误判为"不可达"。

**解决方案**: 改为 `if (lat >= 0.0)`。失败时显式返回 -1.0，可达时返回 `>= 0`。

**文件**: `src/Diagnostics/Model/GCommon.cpp`

---

### 2.9 Candidate pool 缺少全球多样性备份

**5WHY 根因**: country=CN 时候选池仅 CN server → VPN 用户（GeoIP CN，物理境外）全部不可达。

**解决方案**: 始终添加 4 个全球备份 server（Stockholm/Singapore/Stuttgart/Seoul）。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.10 QString arg() 链式调用 Bug

**5WHY 根因**: 多层 `QString::arg()` 链式调用——内部 `.arg()` 的输出被外部 `.arg()` 再次解析。`.arg(a,b,c).arg(d)` 多参数 arg 消费 `%1-%3`，后续 `.arg(d)` 从 `%1` 搜索 → 找不到 → 返回含 `%4` 的字面量。

**解决方案**: 使用 `QStringLiteral` + 多参数 `arg(a, b, c)` 替代链式 `.arg().arg()`。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

**教训**: 禁止链式 `QString::arg()` 调用。始终使用多参数版本。

---

### 2.11 CandidateResult 缺少 latencyMs 字段

**5WHY 根因**: TCP screening 重构后服务器筛选被拆为两阶段：
- Phase 2 (TCP): `TcpResult {srv, latencyMs}` → `tcpRanked[]`
- Phase 3 (DL): `CandidateResult {srv, mbps}` → `results[]`

`CandidateResult` 从 `{srv, mbps, latencyMs}` 被改为 `{srv, mbps}`（丢失了 latencyMs），但 Phase 5（下载回退循环）仍然引用 `results[idx].latencyMs`。此问题被**两次**引入并修复：
- 第一次 (`0eb0967`)：修复 `ranked`→`results` 未完成的变量重命名
- 第二次 (`e7f4a0c` / `8d5610a`)：TCP 预筛选重构后字段再次丢失

**解决方案**: 恢复 `double latencyMs = 0;` 字段，从 `tcpRanked[i].latencyMs` 初始化（O(1)，正确数据源），将 `bestLatency` 类型从 `int` 改为 `double`。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

**教训**: 同一结构体被多次"修复"说明架构变更需要完整审查所有下游消费者。

---

### 2.12 Pre-validation sort 顺序错误

**5WHY 根因**: `sort()` 在 pre-validation 之后 → 未排序时 `ranked[0]` 不是真正的最优 server。

**解决方案**: `sort()` 移到 pre-validation 之前。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.13 bestLatency 未初始化

**5WHY 根因**: `bestLatency` 局部变量声明时未初始化（原始代码 `int bestLatency;`）。在某些控制流下（未触发服务器切换），变量从未被赋值，输出随机垃圾值（UB）。

**解决方案**: `int bestLatency = 0;` → `double bestLatency = 0;`（同时修正类型为 double 以保留亚毫秒精度）。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.14 TCP 输出未排序

**5WHY 根因**: 候选列表被 `shuffle` 以实现地理多样性负载均衡，shuffle 后的顺序直接用于输出显示。每次运行输出顺序不同，不可复现。

**解决方案**: 在输出前对 TCP 结果按延迟排序。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 2.15 iOS 上传响应读取缺少局部变量声明

**5WHY 根因**: `fdset` 和 `tv` 在 macOS/Linux 的 `select()` 调用中声明，但 iOS 使用不同的网络 API（NSURLSession），平台条件编译中没有为 iOS 路径提供这些变量的声明。

**解决方案**: 在 iOS 编译路径中添加 `fd_set fdset;` 和 `struct timeval tv;` 声明。

**文件**: iOS 上传响应读取代码

---

## 三、CI 脚本问题

### 3.1 pipefail 缺失导致构建失败被静默忽略

**发现位置**: `ios.yml` Verify Xcode build、Build and archive iOS; `macOS.yml` Bundle Qt frameworks、Ad-hoc code sign

**提交**: `841842b`

**现象**: xcodebuild / macdeployqt 失败后 CI 步骤仍然打印 `[PASS]` 并继续执行。

**5WHY 分析**:

| Why | 分析 |
|-----|------|
| 1 | 构建命令失败被静默忽略——步骤总是打印 `[PASS]` |
| 2 | `cmd ... 2>&1 \| tail -N` 的管道退出码来自 `tail`，而非 `cmd` |
| 3 | GitHub Actions 默认 shell 是 `bash -e {0}`——`-e` 已设置但 `pipefail` **未**设置 |
| 4 | `tail -N` 总是退出 0，即使接收到来自失败命令的数据 |
| 5 | **根因**: 开发者假设 `set -e` 会通过管道传播——实际不会。POSIX 默认行为让管道中**最后**一个命令决定退出码 |

**解决方案（temp file + exit code 模式）**:

```bash
# ❌ 旧写法（失败被静默忽略）
xcodebuild ... build 2>&1 | tail -30
echo "[PASS] Xcode build verified"

# ✅ 新写法（捕获真实退出码）
XCODEBUILD_LOG=$(mktemp)
set +e
xcodebuild ... build >"$XCODEBUILD_LOG" 2>&1
XCODEBUILD_EXIT=$?
set -e
tail -30 "$XCODEBUILD_LOG"
if [ $XCODEBUILD_EXIT -ne 0 ]; then
  echo "[FAIL] Xcode build failed (exit $XCODEBUILD_EXIT)"
  rm -f "$XCODEBUILD_LOG"
  exit 1
fi
rm -f "$XCODEBUILD_LOG"
echo "[PASS] Xcode build verified"
```

**受影响的步骤（已全部修复）**:
- `ios.yml`: Verify Xcode build、Build and archive iOS、Package IPA
- `macOS.yml`: Bundle Qt frameworks、Ad-hoc code sign、Sign .app and package as .pkg

---

### 3.2 Verify Info.plist Keys 永远不失败

**发现位置**: `ios.yml` Verify Info.plist Keys

**提交**: `841842b`

**现象**: 缺失的 plist key 被报告为 `[MISS]` 但步骤不会退出非零。App Store 提交必需这些 key——缺失会导致审核被拒，但 CI 给出虚假通过。

**5WHY 分析**:

| Why | 分析 |
|-----|------|
| 1 | 缺失 plist key 被报告但步骤不失败 |
| 2 | 循环结构没有跟踪失败状态——没有累加器变量 |
| 3 | 从 `Verify iOS Diagnostic Symbols`（**有** `exit 1`）复制了检查模式，但漏掉了失败退出逻辑 |
| 4 | `[MISS]` 输出仅是信息性的 |
| 5 | **根因**: 错误处理模式不完整——循环打印缺失 key 但不将失败聚合为最终退出码 |

**解决方案**: 添加 `FAILED=0` 累加器 + `exit 1` 最终检查 + plist 文件不存在时独立 `exit 1`。

---

### 3.3 xcarchive 中 .app 路径不匹配

**发现位置**: `ios.yml` deploy 作业 Package iOS IPA

**提交**: `841842b`

**现象**: 归档成功后 `[X] No .app found`。

**5WHY 分析**:

| Why | 分析 |
|-----|------|
| 1 | 路径 `Products/Applications/net_diagnostics.app` 不存在 |
| 2 | CMake 设定 `XCODE_ATTRIBUTE_PRODUCT_NAME "NetDiagnostics"` (`netdiag-target.cmake:278`) |
| 3 | xcodebuild 使用 PRODUCT_NAME，而非 scheme/target 名称来命名输出 .app |
| 4 | 开发者假设 scheme name == product name——只在 Ninja 构建下成立 |
| 5 | **根因**: 产品名称被 CMake 显式覆盖，但 IPA 打包硬编码了 Ninja 风格名称 |

**Ninja vs Xcode 产物对照**:

| 构建系统 | .app 名称 | 原因 |
|---------|-----------|------|
| Ninja | `net_diagnostics.app` | CMake target 名称 |
| Xcode | `NetDiagnostics.app` | `XCODE_ATTRIBUTE_PRODUCT_NAME` |

**解决方案**: 先尝试 PRODUCT_NAME 路径 (`NetDiagnostics.app`) → 回退 scheme name → 最后 `find` 搜索。

---

### 3.4 codesign 失败在 find-while-read 循环中被吞掉

**发现位置**: `macOS.yml` Ad-hoc code sign、Sign .app and package as .pkg

**提交**: `841842b`

**现象**: codesign 在 `find ... | while read` 管道中失败但步骤继续执行。

**5WHY 分析**:

| Why | 分析 |
|-----|------|
| 1 | codesign 失败被静默忽略 |
| 2 | `find \| while read; do codesign; done`——while 循环在子 shell 中运行（管道的缘故） |
| 3 | 子 shell 中的命令失败不影响父 shell；子 shell 总是返回 0 |
| 4 | 没有 `\|\| exit 1` 将失败从子 shell 传播出来 |
| 5 | **根因**: 未意识到 `while read` 在管道右侧时运行在子 shell 中，需显式错误传播 |

**注意**: 即使加了 `set -o pipefail`，没有 `|| exit 1` 的 codesign 失败仍不会使管道失败——`while read` 总是在读完所有输入后退出 0。

**解决方案**: 每个 codesign 命令加 `|| { echo "[FAIL] codesign ..."; exit 1; }`。

**工作原理**: `exit 1` 退出子 shell → 子 shell 退出码 1 → `find` 收到 SIGPIPE → `pipefail` 使管道退出码非零 → `set -e` 终止步骤。

---

### 3.5 pipefail + grep -q 导致 SIGPIPE 误报

**发现位置**: `ios.yml` Verify iOS Diagnostic Symbols

**提交**: `72c01e7`

**现象**: 步骤失败报 `LLVM ERROR: IO failure on output stream: Broken pipe`，但符号实际存在。

**5WHY 分析**:

| Why | 分析 |
|-----|------|
| 1 | 添加 `pipefail` 后符号验证步骤失败 |
| 2 | `nm \| c++filt \| grep -q "$sym"`——`grep -q` 找到匹配后**立即退出** |
| 3 | 早期退出关闭管道 → `nm` 收到 SIGPIPE → `llvm-nm` 以非零退出（"LLVM ERROR"） |
| 4 | SIGPIPE 是**良性的**——grep 找到了符号（成功！），但 `pipefail` 将上游退出码当作失败 |
| 5 | **根因**: `pipefail` 与 `grep -q` 不兼容——grep 的预期行为（找到即退）恰好在管道中触发 SIGPIPE |

**解决方案**:

```bash
# ❌ 管道方式——grep -q 早期退出导致上游 SIGPIPE
if nm -g "$BIN" 2>/dev/null | c++filt | grep -q "$sym"; then

# ✅ 进程替换——无管道 = 无 SIGPIPE
if grep -q "$sym" < <(nm -g "$BIN" 2>/dev/null | c++filt 2>/dev/null); then
```

**关键原则**: 任何包含 `grep -q` 的管道都不要使用 `pipefail`。改用进程替换 `< <(...)` 消除管道。

---

### 3.6 Windows Static 构建超时

**发现位置**: `build.yml` windows-x86_64-static

**提交**: `8c38da2`

**5WHY 根因**: Qt 6.11.1 源码编译需要 > 90 分钟。Qt 版本从 6.8.3 升级到 6.11.1 增加了编译时间，但 timeout 设置未同步更新。

**解决方案**: `timeout-minutes: 90` → `timeout-minutes: 150`。

**教训**: 升级工具链版本时，必须检查并更新对应的 timeout 设置。

---

### 3.7 ASC_KEYFILE 未导出导致密钥文件泄漏 (macOS)

**发现位置**: `macOS.yml` deploy-testflight → Write ASC key

**提交**: `841842b`

**5WHY 根因**: Write ASC key 步骤创建了密钥文件 (`AuthKey_*.p8`) 但未将路径导出到 `GITHUB_ENV`。Clean up keys 步骤执行 `rm -f "$ASC_KEYFILE"` 时变量为空——密钥文件从未被删除。`ios.yml` 同一步骤**有**导出语句，`macOS.yml` 在代码复制时**遗漏**。

**解决方案**: 在 macOS deploy Write ASC key 步骤末尾添加 `echo "ASC_KEYFILE=${K}" >> "$GITHUB_ENV"`。

---

## 四、iOS/macOS 构建与部署

### 4.1 iOS 编译错误 — ranked/bestLatency 未声明 (8 errors)

**提交**: `0eb0967` (修复)、`ecca761` (引入)

**5WHY 根因**: 提交 `ecca761` 用 micro-download pipeline 替换了 TCP-ping 排名——Phase 3-4 使用新变量名 (`results`/`CandidateResult`)，Phase 5（下载回退循环）仍引用旧变量名 (`ranked`/`bestLatency`)。不完整的重构。

**修复**: `ranked` → `results` (全局替换 Phase 5)、添加 `latencyMs` 字段、声明 `bestLatency`。

---

### 4.2 iOS 符号验证 pipefail SIGPIPE

**提交**: `72c01e7` (修复)、`841842b` (引入)

详见 [3.5 pipefail + grep -q 导致 SIGPIPE 误报](#35-pipefail--grep--q-导致-sigpipe-误报)。

---

### 4.3 iOS 上传响应读取缺少局部变量声明

**提交**: `6012382`

详见 [2.15 iOS 上传响应读取缺少局部变量声明](#215-ios-上传响应读取缺少局部变量声明)。

---

### 4.4 iOS 启动闪退 — compact 属性不存在

**提交**: `ebd3cf3`、`d723a58`

详见 [1.6 iOS 启动闪退](#16-ios-启动闪退--compact-属性不存在)。

---

## 五、C++ 代码质量与重构

### 5.1 DRY: latencyProbe 重复 3 次

**5WHY 根因**: `httpLatencyMs` → `tcpPingMs` fallback 模式在 server selection 中重复 3 处。

**解决方案**: 提取为 `latencyProbe` lambda，3 处调用点统一。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 5.2 DRY: upload raw socket → tcpConnect()

**5WHY 根因**: Upload 连接代码 15 行手动 socket/nonblock/select/getSockopt，但 `NetUtil.h` 已有 `tcpConnect()`。

**解决方案**: 替换为 `tcpConnect(best->host, best->port, 10000)`。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 5.3 upload 随机数据重复生成

**5WHY 根因**: 每个 upload tier 重新分配 `QByteArray(dataSize)` 并填充伪随机数（~4MB × 10 次 = 40MB）。

**解决方案**: 预生成最大尺寸 buffer 一次，`left(dataSize)` 切片复用。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 5.4 CN-prepend 逻辑重复

**5WHY 根因**: `!chinaFirst` 和 `chinaFirst` 两个分支做同样的 CN-prepend，仅在日志消息不同。

**解决方案**: 合并为单个条件块，三元表达式选日志。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 5.5 多角色审查修复 (7 项汇总)

提交 `520cd98` 是一次大规模多角色审查（正确性 + 安全性 + 性能 + 可复现性），修复了以下问题：

| # | 问题 | 类别 | 修复 |
|---|------|------|------|
| 1 | upload Host 端口未从 URL 解析 | 正确性 | 从 URL 中提取端口号 |
| 2 | httpGet 超时未传递给底层调用 | 正确性 | 将 timeout 参数转发到 `httpGet` 内部 |
| 3 | httpLatencyMs 未检查 HTTP 200 | 正确性 | 添加状态码验证 |
| 4 | ThemeEngine.colors 响应式失效 | 正确性 | 确保 QML 属性绑定正确触发 |
| 5 | SummaryCards 分隔线未显示 | UI | 修复分隔线组件的可见性逻辑 |
| 6 | detectCountry timeoutMs 未转发 | 正确性 | 将 timeout 参数传播到内部函数 |
| 7 | tcpPingAvg >=0 可达性检查 | 正确性 | 与 2.8 节相同的浮点比较问题 |

---

## 六、模式识别：高频错误类型

基于 25+ 个提交的分析：

| 模式 | 出现次数 | 涉及章节 | 预防策略 |
|------|---------|---------|---------|
| **不完整重构** | 5x | 2.11, 2.12, 2.14, 4.1, 1.6 | 变量重命名后全局搜索；删除 API 前全文查找引用 |
| **边界条件错误** | 4x | 2.8, 2.3, 2.13, 2.9 | `>0` vs `>=0`；HTTP 非 200；未初始化；单一国家候选池 |
| **管道错误处理** | 3x | 3.1, 3.4, 3.5 | temp file > pipefail + pipeline；进程替换 > 管道 + grep -q |
| **平台差异遗漏** | 3x | 3.3, 2.15, 3.7 | Ninja vs Xcode 产物名；iOS vs macOS API；跨文件复制遗漏 |
| **参数/超时未传播** | 3x | 2.5, 2.7, 3.6 | 函数签名变更检查所有调用点；工具链升级同步 timeout |
| **验证不完整** | 2x | 3.2, 2.10 | 累加器 + exit 1；禁止链式 arg() |

---

## 七、预防措施检查清单

### Shell 脚本（修改 CI 时）

- [ ] 所有 `cmd | tail -N` 模式改为 temp file + exit code 方式
- [ ] 所有验证步骤有失败累加器 + `exit 1`
- [ ] 包含 `grep -q` 的管道改用进程替换 `< <(...)`
- [ ] `find ... | while read` 循环中每个命令都有 `|| { echo FAIL; exit 1; }`
- [ ] 密钥/敏感文件路径通过 `GITHUB_ENV` 导出，cleanup 用 `if: always()` 守卫
- [ ] `.app` 路径区分 Ninja（target 名）和 Xcode（PRODUCT_NAME）产物
- [ ] 升级工具链时同步更新 `timeout-minutes`

### C++ / QML（修改速度测试或 UI 时）

- [ ] `CandidateResult` 结构体有完整字段：`srv`, `mbps`, `latencyMs`
- [ ] `results.append()` 参数数量与结构体字段匹配
- [ ] Phase 5（下载回退循环）使用正确的变量名（`results`，非 `ranked`）
- [ ] `bestLatency` 类型为 `double` 且已初始化
- [ ] `tcpPingAvg` 可达性检查使用 `>= 0`（非 `> 0`）
- [ ] 无链式 `QString::arg().arg()` 调用
- [ ] 排序在预验证和显示之前执行
- [ ] HTTP 状态码检查容忍非标准响应
- [ ] 候选池始终包含全球多样性备份
- [ ] 超时参数正确传播到所有子函数
- [ ] 删除公共 API 前全文搜索引用（`grep -rn "propertyName" src/`）
- [ ] 修改 struct 字段后搜索所有 `.fieldName` 引用
- [ ] 所有平台（iOS/Android/Desktop）均通过编译

### 提交前

- [ ] 至少在 iOS CI 上触发并等待通过
- [ ] 检查 `[PASS]`/`[OK]` 输出确认所有验证步骤通过
- [ ] 确认无新引入的编译警告
- [ ] 检查是否有新增的未初始化变量

---

> **最后更新**: 2026-07-17
> **覆盖提交**: `6012382` → `ebd3cf3` (25 commits)
> **相关文件**: `.github/workflows/iOS.yml`, `.github/workflows/macOS.yml`, `.github/workflows/build.yml`, `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`, `src/Diagnostics/Model/GCommon.cpp`, `cmake/netdiag-target.cmake`, `scripts/ios/asc-app.sh`, `src/Common/View/widgets/ShareButtons.qml`, `src/Dashboard/View/DashboardScreen.qml`
