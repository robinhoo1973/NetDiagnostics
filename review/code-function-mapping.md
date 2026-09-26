# code-function-mapping — 模块/函数映射与原子时限契约

> 生成：2026-09-26 /loop max 评审轮（10 角度 × ~60 发现）
> 维护规则：本文件与 src/ 同步更新；任何改动探针时限、取消通道或阻塞原语签名时须同步本表。

## 一、模块地图

| 模块 | 关键文件 | 职责 |
|------|----------|------|
| 入口/引擎 | src/main.cpp | QML 引擎装载、图标 provider、语言桥；崩溃出口（根对象空即退出） |
| 应用状态 | src/app/AppState.cpp/.h | 目标解析/组装、套件编排（runDiagnostics/cancel）、结果/契约下发、连通性缓存、报告桥 |
| 配置 | src/Configuration/Controller/ConfigurationController.cpp/.h | 启用集/激活组/端口扫描配置持久化 |
| 套件编排 | src/Diagnostics/Model/DiagnosticSuite.cpp/.h | 分组调度、套件 deadline(600s)、池拥有（QThreadPool 子对象） |
| 探针基座 | src/Common/Services/DiagnosticBase.cpp/.h | watchdog(durationProfileMs)、两段式取消（标志+5s abortGrace）、终态单发 |
| 探针 G1–G5 | src/Diagnostics/Model/G1–G5/Adapters.cpp | 平台/适配器/安全/远程主机/协议五组探针 |
| 共享助手 | src/Diagnostics/Model/GHelpers.h、GCommon.cpp | 工具缓存、URL 解析、终端派生链、测速 HTTP |
| DNS | src/Common/Services/DnsResolver.cpp/.h、DnsWire.h | 有界解析缓存、wire 解析、DoH 客户端（G3 本地） |
| GeoProbe | src/Diagnostics/Model/GeoProbe.cpp、ProbeExecutor.cpp、ProbeFeedback.cpp、src/Common/Services/ProbeDatabase.cpp | 服务器库、TTFB 批次执行、统计聚合、完成等待 |
| 平台适配 | src/Common/Services/PlatformAdapter.h、AdapterRegistry.cpp、src/Common/Platform/* | RunContext/RunSnapshot、适配器注册、能力探测 |
| 报告 | src/Report/Model/ReportEngine.cpp | HTML/PDF 生成（原子写）、预览渲染 |
| 图标/主题 | src/Common/Services/IconProvider.cpp/.h | SVG 两阶段着色、渲染缓存 |
| 结果/契约 | src/Common/Model/DiagnosticResult.cpp/.h、DiagnosticMeta.cpp/.h、DiagNames.h | 结果工厂、档案（durationProfileMs）、显示名单一来源 |

## 二、原子时限契约（2026-09-26 轮落地）

### 取消通道（单一事实源：`RunContext.cancelled`）

| 阻塞原语 | 位置 | 取消行为 |
|----------|------|----------|
| tcpProbe / httpOnce | G5/Adapters.cpp | 各阻塞相位前置 `cancelledBy(ctx)`；取消即弃（连接/TLS/读循环） |
| G5 各探针 | G5/Adapters.cpp | helper 返回后检查 → `DiagnosticResult::cancelled` 终态（16 处） |
| G4 traceroute 回退 | G4/Adapters.cpp | 4 端口循环逐端口响应取消 |
| G3 DnsIntegrity Phase2 | G3/Adapters.cpp | 线程内逐相位短路（DoH→UDP→评分前） |
| waitForCompletion | ProbeDatabase.cpp | `cancelled` 指针逐秒检查（用户取消/watchdog 均置位）；clear() 代际变化同效 |
| getFeedback | GeoProbe/ProbeFeedback | 透传取消指针 |
| AppState::cancel | AppState.cpp | 置标志 → GeoProbe::clear()（代际解堵 120s）→ `++m_runGeneration`（丢弃迟到结果） |
| watchdog 超时 | DiagnosticBase.cpp | `cancelled.store(true)` → 工人下个检查点停止（不再编造 Timeout 后空转） |
| 平台路径（iOS NSURLSession / Android JNI） | HttpDiagnostics.mm / NetworkDiagnostics.cpp | 未接入（已知限制，见跟进计划） |

### 时限档案（DiagnosticMeta.cpp 单一来源）

| 探针 | 档案 | 内部最坏（2026-09-26 修正后） |
|------|------|------|
| G5HttpRedirect | 60000 | 55s 全局跳链预算（每跳 DNS3+连8+TLS8≈19s 上限内收敛，部分链 Warning） |
| G5HttpCompression | 60000 | 2×12000 ≈ 54s |
| G3InternetConnectivity | 300000 | TTFB 批 ~120s + 6 档测速；病理重复试由取消兜底，套件 600s 为最终兑底 |
| G4Traceroute | 90000 | 逐跳 3s×30 有界 + PTR 500ms；回退 4 端口×3s 可取消 |

## 三、单一事实源清单（本轮收敛）

| 事实 | 单一来源 | 曾复制处 |
|------|----------|----------|
| 结果构造 | `DiagnosticResult::makeResult` | G1–G5 五份（G1/G2/G3 曾缺 errorOutput 回填） |
| 终端派生链 | `SystemDiagnostics::derivedTerminalText` | resultFor/剪贴板/报告体三份 |
| 探针前导 | G5 `tryNormalizeTarget` | 21 探针 20 份（文案已漂移） |
| 主机名提取 | `SystemDiagnostics::extractHostname` | G4 + iOS 两份 |
| 显示名 | `diagDisplayName(id)` | DiagnosticMeta 列（已删，44 条重复） |
| DoH 查询 | G3 本地 dohQueryFull（并行+2000ms） | GCommon 死共享栈（已删） |
| 国家缓存 | G3 `sDetectCountry` + `clearDetectCountryCache()` | 曾函数级静态跨运行不失效 |

## 四、网络层复用跟进计划（铁律：禁造轮子）

> 已落地零新依赖替换：QUrl（parseHttpUrl/URL 组装）、DiagnosticResult 工厂。
> 已落地 libcurl 桌面分支（2026-09-26）：G5 httpOnce、GCommon httpDownload/
> httpUpload/httpTtfb 均走 curl easy API（NO_CURL 保留 socket 兜底，移动端不
> 链接 curl 时自动回退）。以下为剩余项，按优先级排：

| # | 现状（自造轮子） | 成熟开源方案 | 平台门控 | 状态 |
|---|------------------|--------------|----------|------|
| 1 | G5 httpOnce 手写 HTTP/1.1（无 chunked、手跟重定向） | **libcurl** easy API | `#if !defined(NO_CURL)` | ✅ 已落地（1caed0ea） |
| 2 | GCommon httpDownload/httpUpload/httpTtfb（">1KB 即成功"启发式） | **libcurl**（FOLLOWLOCATION/CONNECTTIMEOUT_MS） | `#if !defined(NO_CURL)` | ✅ 已落地（2a407654） |
| 3 | NetUtil.h 手写 POSIX/WinSock 连接层（FD_SETSIZE 缺陷） | **QTcpSocket**（connectToHost+waitForConnected） | 全平台（Qt 自带） | 待办 |
| 4 | DnsWire.h 手写 RFC1035 + DnsResolver 手写线程池解析 | **c-ares**（ares_search/parse_reply，非阻塞） | 新依赖，交叉编译需评估 | 待评估 |
| 5 | G5 MongoDB BSON/LDAP BER/MySQL 握手/MQTT CONNECT 手写 | **mongo-c-driver** BSON / **OpenLDAP libldap** / **QMqttClient**(Qt MQTT 模块) | 新依赖（G5 协议族 Desktop-only） | 待评估 |

铁律执行顺序：零新依赖替换已落地（QUrl）；libcurl 桌面分支为下一优先级（NO_CURL 宏已就绪）；c-ares 与协议库需新依赖评估后单独立项。
