# Code Review 2026-09-26 — max 轮全树评审报告

> 评审模式：/code-review max --fix（10 角度代理并行全树扫描 + 逐条验证 + 分批落地）
> 范围：src/ 全树 + cmake/ 约定合规；基线构建干净、selftest 44 项 verify/contract PASS。
> 批次提交：0a8be4aa（正确性快赢）→ 1258fb9c（取消/原子时限）→ 6b141f54（死代码/复制收敛）

## 一、评审角度与发现汇总（~60 条，重叠合并后 ~40 独立问题）

| 角度 | 发现数 | 核心结论 |
|------|-------:|----------|
| 复用/禁造轮子 | 8 | 手写 HTTP 客户端×3、手写 DNS 栈×2、手写 URL 解析×2、手写协议解析器（libcurl 已链接却未用） |
| 不变式/取消检查 | 8 | 分层记账（watchdog/套件 deadline/两段取消）无一层真正停止工作；G5 21 探针仅 1 处取消检查 |
| 高度/根因 | 8 | 取消最终以主线程 QThreadPool 析构阻塞兑现（至 ~120s）；档案算术两处已错 |
| 正确性/主线程竞态 | 8 | 取消不递增代际→迟到结果混入；凭据持久化阻塞输入栈；连通性刷新无代际 |
| 跨文件线程追踪 | 8 | detectCountry 缓存跨运行不失效；IconProvider 锁外裸指针；DNS TTL quint32→int 回绕撞哨兵 |
| 深度正确性 | 8 | 死共享 DoH 栈含 Windows SIGSEGV 模式；文件级静态 SIOF 违规；detach 线程无上界 |
| 简化 | 8 | makeResult 五份漂移；派生链三份手抄；21 探针前导 20 份；死预览 QML 栈 ~600 行 |
| 效率 | 8 | 报告渲染/导出主线程同步；接口枚举重复（每轮至 15 次）；图标两阶段管线重复扫描 |
| 包装/缓存正确性 | 8 | 报告目标未转义（导出 HTML 注入）；exportPdf 非原子写；负缓存覆盖正缓存 |
| 约定合规 | 3 弱候选 | 全树近乎全合规（12 类规则零命中）；joinable() 守卫 2 处、enum Status 低置信 |

## 二、已落地修复（本轮 3 批）

### 批 1 — 正确性快赢（0a8be4aa）
- 报告目标 toHtmlEscaped（导出 HTML 注入）；exportPdf 改 QSaveFile 原子写
- DNS TTL 钳制 INT_MAX（污染信号丢失）；DPAPI 改每用户密钥
- cancel() 递增代际；负缓存不覆盖正条目；SIOF 两处；IconProvider 按值返回
- DeviceCapability 缓存加锁；AdapterRegistry release 硬检查；死信号删除；缓存键分隔符

### 批 2 — 取消/原子时限核心（1258fb9c）
- tcpProbe/httpOnce 接入 RunContext（20 调用点）；G5 16 处取消→Cancelled 终态
- redirect 55s 跳链预算、compression 收敛 54s、Internet 档案 180→300s（算术修正）
- 取消指针贯穿 waitForCompletion/getFeedback——120s 等待即时解堵；AppState::cancel 调 GeoProbe::clear()
- DnsResolver 在飞查询上界 8；G4/G3 循环逐相位取消

### 批 3 — 死代码清除 + 复制收敛（6b141f54，净 -929 行）
- 删死共享 DoH/HTTPS 栈（含 Windows SIGSEGV 模式）——活 G3 dohQueryFull 并行化+2000ms
- 删只写不读结果持久化（顺杀主线程 fsync）；删 DiagnosticMeta 重复显示名列
- makeResult 五份→单一工厂（顺修 G1/G2/G3 错误区块空白）；派生链三份→单一助手
- 21 探针前导→tryNormalizeTarget；extractHostname 双份→共享；删死预览 QML 栈

### 批 4 — 复用铁律零新依赖替换
- parseHttpUrl → QUrl（IPv6 截断/userinfo 误解析修复）
- runDiagnostics URL 组装 → QUrl（IPv6 自动括号、端口拆分、路径编码；auth 字节保真插入）

## 三、跟进计划（未落地项）

| 项 | 原因 | 计划 |
|----|------|------|
| libcurl 桌面分支（httpOnce/httpDownload） | 双路径需 NO_CURL 分支+全平台回归 | 下一轮，NO_CURL 宏已就绪 |
| c-ares（DnsWire/DnsResolver） | 新依赖，iOS/Android 交叉编译需评估 | 立评估项 |
| 协议族库（libldap/QMqttClient/mongo-c-driver BSON） | 新依赖；G5 协议族 Desktop-only | 立评估项 |
| iOS/Android 平台路径取消接入（HttpDiagnostics.mm/NetworkDiagnostics.cpp） | 平台文件本地不可编译验证 | 随 CI 轮 |
| 报告预览/导出移出主线程（QtConcurrent） | QML 调用语义变更需联调 | 效率轮 |
| 接口枚举单趟化+刷新代际（H7/H8/C8） | 中等重构 | 效率轮 |
| 图标 placeholder 阶段按名缓存（H4） | 中等重构 | 效率轮 |
| C++ PDF 族（NativePdfDocument/PlatformPdfRenderer）删除 | 平台文件，需 CI 验证 | 随预览栈第二轮 |

## 四、评审方法论记录

- 10 角度代理并行：5 正确性（逐行核心扫描/探针不变式/跨文件线程追踪/C++ 陷阱/包装缓存）+ 5 清理（复用/简化/效率/高度根因/约定合规）
- 逐条验证：读引用位置确认后方落地；豁免项显式记录（如 DnsResolver 缓存互斥完整、ProbeDatabase 代际防护健全）
- 每批：构建（ninja）+ selftest（44 项 verify/contract）+ pre-commit（30 项）全过才提交推送
- 弃项记录：J3（Status 不在权威保留字清单）、D8（日志行序，最低严重度）、B4b（iOS 0ms 计时编造，待平台轮）
