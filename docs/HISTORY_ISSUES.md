# NetDiagnostics 历史问题与解决方案文档

> 生成日期: 2026-07-17 | 会话提交: 22 commits

---

## 一、UX/UI 问题

### 1. 分享按钮图标颜色不随 Theme 变化

**5WHY 根因**: `ShareButtons.qml` 中 `compactBtn`/`labeledBtn` 的 `accent` 属性通过 `Loader.onLoaded` + `Qt.binding()` 间接设置。`onLoaded` 在 QML 声明式绑定传播之**前**执行，捕获到默认值 `ThemeEngine.failRed` 而非调用方传入的 `ThemeEngine.cyan`。多个 `Binding on accent` 元素在 Loader 创建实例时存在冲突。

**解决方案**: 
- 替换为单个显式 `Binding { target; property; value: ternary }` 模式（Qt 5.0+ 兼容，Loader 安全）
- 移除 `Loader.onLoaded` 中的 `item.accent = Qt.binding(...)` 代码
- 更新头部注释反映实际实现

**文件**: `src/Common/View/widgets/ShareButtons.qml`

---

### 2. 分享按钮图标尺寸不匹配按钮

**5WHY 根因**: compact 模式 `_iconSize = 24`，`_btnHeight = 36`。图标仅 67% 填充。

**解决方案**:
- compact: `_iconSize = _btnHeight`（36dp，100% 填充）
- labeled: `_iconSize = Math.round(_btnHeight * 0.9)`（36dp，90% 填充）  
- wide: `_iconSize = 40`（83% 填充，48dp 按钮）

**文件**: `src/Common/View/widgets/ShareButtons.qml`

---

### 3. 诊断结果 header badge 布局错位

**5WHY 根因**: Badge 行使用 `Item { Layout.fillWidth: true }` spacer 右对齐，但在窄屏上 RowLayout 被压缩导致 spacer 失效。`implicitHeight=56` 不足以容纳双行内容。

**解决方案**:
- `implicitHeight` 提高到 72px
- 内边距从 4→6，行间距从 2→4
- 桌面端 badge 内联显示，手机端左对齐单独行

**文件**: `src/Diagnostics/View/DiagnosticScreen.qml`

---

### 4. SummaryCards 拼接错误

**5WHY 根因**: `Tr.totalDiagsLabel + (pass+warn+fail+skip+info)` 没有分隔符，显示为 "Total Diagnostics5"。

**解决方案**: 添加 `": "` 分隔符。

**文件**: `src/Common/View/widgets/SummaryCards.qml`

---

### 5. ThemeEngine.colors 不响应 Theme 切换

**5WHY 根因**: `readonly property var colors: ({...})` 在初始化时创建 JS 快照对象，``applyTheme()`` 无法更新快照内的原始值。

**解决方案**: 移除 `readonly`，在 `applyTheme()` 中重建 `colors` 对象。

**文件**: `src/Common/View/theme/ThemeEngine.qml`

---

### 6. iOS 启动闪退 — compact 属性不存在

**5WHY 根因**: 前一轮修复删除了 `SummaryCards.qml` 中 `property bool compact`（死代码），但 `DashboardScreen.qml:131` 仍引用 `compact: true`。QML 引擎在加载时验证所有属性，失败 → `abort()`。

**解决方案**: 移除 `DashboardScreen.qml` 中的 `compact: true` 赋值。

**文件**: `src/Dashboard/View/DashboardScreen.qml`

---

## 二、Speed Test 核心引擎

### 7. InternetConnectivity Country Unknown 超时 180s

**5WHY 根因**: GeoIP HTTP 服务（ipip.net/ip-api.com 等）在 GFW 内全部被阻断 → 返回 "XX" → curated fallback 8 server 仅 2 CN → shuffle 后未命中 → allServers 回退 → download/upload 阶段 `dlTotalMs` guard 不计失败超时 → 480s 累积。

**解决方案**:
- 替换全部 HTTP GeoIP 为 DNS(UDP+DoH) → HTTPS GeoIP(x4) → 静态 IP 前缀链
- download/upload 阶段增加 `totalTimer.elapsed()` 墙钟守卫
- 每 tier 最多 fallback 3 server
- upload connect 超时从 30s 降到 10s

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 8. TCP Ping 全 1ms，无法区分 server

**5WHY 根因**: `tcpPingMs` 同区域内 CN server TCP RTT 全部 ~1ms（timer 精度限制）。TCP 连接成功 ≠ HTTP 下载能力。

**解决方案**:
- 新增 `tcpPingAvg()`：≤1ms 的 server 跑 50 次取平均值（~0.02ms 有效精度）
- TCP 筛选作为 Stage 1，喂 top-8 到 Stage 2 微下载

**文件**: `src/Diagnostics/Model/GCommon.cpp`, `GHelpers.h`

---

### 9. Micro-download 筛选全部失败

**5WHY 根因**:
1. `httpDownload` 要求严格 HTTP 200 (`contains(" 200 ")`)，CN server 返回非标准 HTTP 或二进制协议
2. `headersDone` 未找到 `\r\n\r\n` 时数据滞留在 `headerBuf`，`body` 为空

**解决方案**:
- HTTP 状态检查放宽：接受 3 位数字提取 + 非 HTTP 200 但 `bytes>1000` 的响应
- 二进制回退：`!headersDone && headerBuf.size()>1000` → 直接作为原始 body
- 微下载阶段增加 per-server 失败原因日志

**文件**: `src/Diagnostics/Model/GCommon.cpp`, `G3InternetSpeedTest.cpp`

---

### 10. Upload POST Host Header 缺少端口

**5WHY 根因**: `Host: best->host` 未包含端口 8080，反向代理可能错误路由（RFC 7230 §5.4）。

**解决方案**: 非 80 端口时包含 `host:port`。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 11. httpGet 硬编码 30s guard 覆盖调用方 timeout

**5WHY 根因**: `recvTimer.elapsed() > 30000` 覆写了 `timeoutMs` 参数（如 5000）。

**解决方案**: 使用 `timeoutMs` 替代硬编码值。

**文件**: `src/Diagnostics/Model/GCommon.cpp`

---

### 12. httpLatencyMs 200 状态码检查过严

**5WHY 根因**: `contains(" 200 ")` 要求空格包围，失败于 `"HTTP/1.0 200\r\n"`（无尾空格）。

**解决方案**: 提取第一个空格后的 3 位数字码，直接与 "200" 比较。

**文件**: `src/Diagnostics/Model/GCommon.cpp`

---

### 13. detectCountry timeoutMs 未传播

**5WHY 根因**: 接收参数 `timeoutMs`，但向子函数传递硬编码 2000/3000。

**解决方案**: 使用 `qMin(timeoutMs, 2000)` 正确传播。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 14. tcpPingAvg 过滤条件过严

**5WHY 根因**: `if (lat > 0)` 过滤了 sub-ms 连接（`tcpPingMs` 截断返回 0）。

**解决方案**: 改为 `if (lat >= 0.0)`。

**文件**: `src/Diagnostics/Model/GCommon.cpp`

---

### 15. Candidate pool 缺少全球多样性备份

**5WHY 根因**: country=CN 时候选池仅 CN server → VPN 用户（GeoIP CN，物理境外）全部不可达。

**解决方案**: 始终添加 4 个全球备份 server（Stockholm/Singapore/Stuttgart/Seoul）。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 16. QString arg() 链式调用 Bug

**5WHY 根因**: `.arg(a,b,c).arg(d)` 多参数 arg 消费 `%1-%3`，后续 `.arg(d)` 从 `%1` 搜索 → 找不到 → 返回含 `%4` 的未替换字符串。

**解决方案**: 拆分为独立 `.arg()` 调用。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 17. CandidateResult 缺少 latencyMs 字段

**5WHY 根因**: TCP screening 重构后 `CandidateResult` 从 `{srv,mbps,durationMs}` 被改为 `{srv,mbps}`，但 `bestLatency` 和 download fallback 仍引用 `latencyMs` → 编译错误。

**解决方案**: 恢复 `latencyMs` 字段，从 `tcpRanked[i].latencyMs` 初始化。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 18. Pre-validation sort 顺序错误

**5WHY 根因**: `sort()` 在 pre-validation 之后 → 未排序时 `ranked[0]` 不是真正的最优 server。

**解决方案**: `sort()` 移到 pre-validation 之前。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

## 三、iOS 构建问题

### 19. iOS 编译错误 — ranked/bestLatency 未声明

**5WHY 根因**: Server selection 重构删除了 `ranked` 和 `bestLatency` 变量，但下载阶段代码未同步更新。

**解决方案**: 恢复变量声明，同步引用。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 20. iOS 符号验证 pipefail

**5WHY 根因**: CI 脚本中管道命令在前段失败时不触发错误退出。

**解决方案**: 添加 `set -o pipefail`。

**文件**: `.github/workflows/iOS.yml`

---

## 四、代码质量

### 21. DRY: latencyProbe 重复 3 次

**5WHY 根因**: `httpLatencyMs` → `tcpPingMs` fallback 模式在 server selection 中重复 3 处。

**解决方案**: 提取为 `latencyProbe` lambda，3 处调用点统一。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 22. DRY: upload raw socket → tcpConnect()

**5WHY 根因**: Upload 连接代码 15 行手动 socket/nonblock/select/getSockopt，但 `NetUtil.h` 已有 `tcpConnect()`。

**解决方案**: 替换为 `tcpConnect(best->host, best->port, 10000)`。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 23. upload 随机数据重复生成

**5WHY 根因**: 每个 upload tier 重新分配 `QByteArray(dataSize)` 并填充伪随机数（~4MB × 10 次 = 40MB）。

**解决方案**: 预生成最大尺寸 buffer 一次，`left(dataSize)` 切片复用。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

### 24. CN-prepend 逻辑重复

**5WHY 根因**: `!chinaFirst` 和 `chinaFirst` 两个分支做同样的 CN-prepend，仅在日志消息不同。

**解决方案**: 合并为单个条件块，三元表达式选日志。

**文件**: `src/Diagnostics/Model/G3/G3InternetSpeedTest.cpp`

---

## 五、避免同类问题的建议

| 场景 | 检查清单 |
|------|---------|
| 删除 QML 属性 | `grep -rn "propertyName" src/ --include="*.qml"` 全局搜索 |
| 修改 struct 字段 | 搜索所有 `.fieldName` 引用 |
| 修改函数签名 | 检查所有调用点参数是否匹配 |
| HTTP 状态检查 | 使用数字码提取而非字符串匹配 |
| 时间 guard | 始终使用 `totalTimer.elapsed()` 墙钟，不依赖成功累计值 |
| 候选池构建 | 始终包含 diversity backup |
| Qt `arg()` 链 | 每个 `.arg()` 独立调用，不混用多参形式 |
