# 活态诊断 UI 重构 — 规划文档 v3.0（融合版）

> 融合来源：本会话 v2.0 规划 + Claude Agent 方案 `ui-ux-g1-g5-spinner-iphone-ping-block-adaptive-quail.md`
> 项目：NetDiagnostics（Qt 6 + QML 跨平台网络诊断工具，46项检测 G1-G5）
> 状态：草稿 — 待多轮交叉讨论后定稿

---

## 一、项目目标

把诊断结果页从"文本列表 + 旋转Spinner"重构为**活态诊断（Living Diagnostics）**体验：

- 方块格网格（类 iOS Widget / Fing / Speedtest）替代列表行
- 每个检测项一个**大图标方块**（有颜色、有质感，双色+渐变）
- 检测中显示**专属签名动画**（替代 Spinner，46项各不同）
- 完成后**状态染色 + 落定动画**
- 点击进入**全屏详情页**，用**动画 + 图表**展示结果（而非纯文本）

## 二、已锁定决策（7项）

| # | 决策 | 内容 |
|---|------|------|
| D1 | 图标质感 | 双色槽+渐变质感，扩展 `generate-colored-icons.py` 管线 |
| D2 | 布局 | **完全替换**列表 → 方块格网格（无列表回退） |
| D3 | 详情 | 46项全部数据可视化（模板化） |
| D4 | 动画 | 46项全部专属忙碌动画（6大类可复用组件） |
| D5 | Ping动画 | **水平往返**（左右节点间小球） |
| D6 | 详情交互 | **全屏详情页**（替代 overlay 弹层） |
| D7 | 方块密度 | **宽松大卡**：手机1列 / 平板2列 / 桌面3列，图标64px |

## 三、硬约束（不可违反）

1. **零运行时着色**：图标全部预生成静态SVG。AppIcon.qml 5WHY 记录——ShaderEffect（iOS无Metal）、MultiEffect（iOS aqt 无 QtQuick.Effects）、Image.color、Rectangle 遮罩**全部失败过**。只做"选文件+透明度"
2. **仅QML标准动画**：PropertyAnimation 体系（RotationAnimation/NumberAnimation/SequentialAnimation/ParallelAnimation/Scale/Opacity/PathAnimation），无 ShaderEffect
3. **ThemeEngine 不新增 QML 颜色/动画属性**（用 JS 对象；动画 token 走 JS 对象）——属性过多崩溃静态QML引擎
4. **不 shell out**（诊断纯 C++）
5. **报告不受影响**：新增 `data` 字段与 `rawOutput` 并行，ReportEngine 只读文本
6. **pre-commit 18项检查 + 三平台CI（桌面/iOS/Android）全绿**
7. **不冲突**工作区未提交改动（AppState/ResultsModel/DnsResolver/ProbeDatabase/DiagGroupPanel.qml/ReportEngine）
8. **单一图标源**：不引入第二套硬编码彩色图标目录（否决对方 icons-color/ 方案，理由见 §六）

## 四、架构（5层，L1为地基）

```
L5 详情页模板化  DetailPage + 6类模板 + 共享图表原语
L4 动画引擎      DiagAnimator 分发器 + 6个可复用动画组件类
L3 方块格UI      DiagBlock（图标区+信息区+关键指标）+ Flow 响应式网格
L2 图标管线      多哨兵色槽烘焙（主色随主题 + 语义强调色 + 渐变/阴影）
L1 数据层        DiagnosticResult.data（46项结构化输出）
```

依赖单向：L1 → L2 → L3 → L4 → L5。L1 无数据则 L5 无图可画。

## 五、L1 结构化数据层（P0，最优先）

### 现状问题
诊断函数只输出文本（rawOutput/details/summary），ResultsModel::getDetailResult() 只传文本。**可视化没有数据源**——这是整个重构的地基（对方方案缺失此层）。

### 改动
1. `src/Common/Model/DiagnosticResult.h`：新增 `QVariantMap data;` 字段
2. `src/app/ResultsModel.cpp`：`resultToVariantMap()` 加 `m["data"]=r.data`（includeProperties 时）；`getDetailResult()` 加 `m["data"]=r.data`
3. 46个诊断 `.cpp`：补结构化输出
4. 先做**数据审计**：逐项确认已采集 vs 需补采集

### 46项数据 schema（草案）
| 测试 | 关键字段 |
|------|---------|
| G4Ping | `rtts[]`, `loss`, `min/avg/max`, `sent/rcvd`, `tcpFallback` |
| G4Traceroute | `hops[]`(ttl, ip, host, rtt1-3) |
| G4PathPing | `hops[]` + 每跳统计 |
| G3InternetConnectivity | `down/up/jitter/ping`（速度测试结果） |
| G5HttpTiming | `dnsMs/connectMs/tlsMs/ttfbMs/totalMs`, `redirects[]` |
| G5SslCertificate | `issuer/subject/validFrom/validTo/daysLeft/fingerprint/chain[]` |
| G3GeoIPLoc | `lat/lon/city/region/isp/asn/country` |
| G3DnsServers | `servers[]`(ip, queryMs) |
| G4DnsResolution | `resolveMs/ipv4s[]/ipv6s[]` |
| G1WifiDiagnostics | `ssid/signalDb/channel/freq/linkSpeed/security` |
| G2RoutingTable | `routes[]`(dest, gw, iface, metric) |
| G2ArpTable | `entries[]`(ip, mac, type) |
| G5TcpConnect | `rttMs/port/bannerAvailable` |
| DB类(4个) | `connectMs/version/authOk` |
| G5Ftp/Ssh/Telnet/Mqtt | `connectMs/banner/version` |
| G5HttpHeaders | `headerCount/hsts/hpkp` |
| G5SecurityHeaders | `missing[]`（缺失安全头） |
| G5HttpCompression | `algorithm/ratio/before/after` |
| G4MtuDiscovery | `mtu/iterations` |
| G1ActiveConnections | `count/protocols[]/states[]` |
| G1NetworkAdapters | `adapters[]`(name, type, mac, ips, status) |
| G3DnsCache | `entries[]`(name, type, ttl) |
| G5HttpRedirect | `chain[]`(url, status) |
| G5EmailDiagnostics | `smtpMs/imapMs` |
| G5CurlVerbose | `httpVersion/statusCode/totalMs` |
| …其余 | 同类结构化（图标+横幅+响应时间） |

## 六、L2 图标管线（P1，融合后：我方架构 + 对方配色语义）

### 架构（我方，否决对方硬编码 icons-color/）
主SVG用4个哨兵色槽，生成时一次性烘焙（无运行时着色、无第二图标源、文件数不爆炸、主题自适应）：

| 哨兵 | 含义 | 生成时替换为 |
|------|------|-------------|
| `#FFFFFF` | 主色 | 调色板主色（随主题，同现状） |
| `#AAAAAA` | 渐变深端 | `darken(主色,30%)`（脚本内计算） |
| `#000000` | 强调色 | **语义强调色表**（见下，每图标固定） |
| `#777777` | 填充/阴影 | 固定柔和填充（烘焙进SVG） |

- 渐变 = `<linearGradient>` stop 用哨兵色，生成时替换 → 主色→主色暗端同色系渐变（有深度质感）
- 强调色 = 每图标从语义强调色表取一个，烘焙进 SVG → 双色，不随主题变（装饰性强调）
- 文件数 = 165×46 ≈ 当前量级，无组合爆炸
- `AppIcon.qml` 加可选 `color2`（默认用烘焙强调色）
- `scripts/pre-commit` 扩展：图标同步检查覆盖新哨兵色

### 配色语义（吸收对方）
- 网络/连通 = cyan/blue 系（ping/traceroute/tcp/dns…）
- 安全 = green 系（ssl/security-headers/proxy/netskope…）
- 警告/配置 = amber 系（dhcp/ip-config/proxy…）
- 数据库 = 产品品牌色（MySQL橙/Postgres蓝/Redis红/MongoDB绿）
- 系统硬件 = 紫/靛 系（cpu/network-card/adapters…）

### 重绘范围（吸收对方 P0/P1/P2 分批）
- **P0 首批8个**：ping, traceroute, tcp-connect, ssl-certificate, dns-server, wifi, cpu, network-card
- **P1**：G4全部 + G5关键（ssh/ftp/http）
- **P2**：G1-G3其余 + G5数据库类

## 七、L3 方块格UI（P2，融合后：对方块内结构 + D7尺寸）

### DiagBlock.qml
```
┌─────────────────────────┐
│ ┌───────────┐           │
│ │  图标井    │  Ping     │ ← 图标区(64px icon, tinted圆角方块+签名动画)
│ │ (动画)    │            │ ← 名称区: 名称(10px) 图标+名称两元素（L3规范）
│ └───────────┘            │
│ ════════════════════════ │ ← 状态色描边/微光
└─────────────────────────┘
```
- 卡片 radius≈16；**4状态**：Pending（textSecondary #94A3B8 @ 0.80 透度，与组标题图标令牌一致）→ Running（签名动画+边框脉冲+左上角单圆点秒数指示器）→ Done（状态色tint+落定pop，圆点保持显示）→ Disabled（暗淡）
- 运行时指示器：左上角单圆点 + 秒数文本，按耗时阈值着色（<5s 绿/5-9s 黄/10-20s 橙/>20s 红），执行结束后保持显示
- 落定动画：`Scale 0.9→1.0` + `OutBack` 缓动 + 状态色淡入（300ms）
- 关键指标字段来自 L1 的 `data`（如 Ping 块显示 avg 值）
- 无障碍：`Accessible.role: Button` + 键盘导航

### 网格改造 DiagGroupPanel.qml
- delegate 从 DiagResultItem（列表行）换成 DiagBlock
- 用 `Flow` 响应式列：**手机1 / 平板2 / 桌面3**；块宽 = (panelWidth - spacing×(cols-1)) / cols
- 保留组头（标题/徽章/进度条）
- `testRunning` 仍用 reactive 属性（非 JS 快照，沿用 5WHY 教训）
- 性能：仅当前运行组 + 可视区动画；块不可见即停（吸收对方 cacheBuffer 原则；GridView 因展开面板高度问题不用）

## 八、L4 动画引擎（P3，对方架构为主）

### 结构
`DiagAnimator.qml`（分发器，输入 diagId+running）+ **6个可复用动画组件类** + `diagAnimationType()`（DiagNames.h 单一数据源，新增）：

| 类 | 文件 | 适用(约) | 实现 |
|----|------|---------|------|
| Jiggle | JiggleAnimation.qml | ~20项 | ParallelAnimation: rotation ±2.5° + scale 0.96↔1.0 |
| Bounce | BounceAnimation.qml | 6项 | NumberAnimation x/y + Easing.InOutBounce |
| Path/Connect | PathAnimation.qml | 6项 | SequentialAnimation 节点依次出现+连线 |
| Pulse/Wave | PulseAnimation.qml | 5项 | opacity 脉冲 + scale 波纹 |
| Type/Sequence | TypeAnimation.qml | 5项 | 逐元素显示 |
| Lock/Stamp | LockAnimation.qml | 4项 | 印章下落 + scale 弹跳 |

### 46项分配（吸收对方表）
- **G4Ping = Bounce：小球左右端点间往返 + 端点闪烁** ✅D5
- **G4Traceroute = Path：节点从起点逐跳出现**（主打）
- **G5TcpConnect = Connect：SYN→SYN-ACK→ACK**（主打）
- **G5SslCertificate = Lock：证书印章下落+弹跳**（主打）
- G3InternetConnectivity = Lock：对勾中心放大弹出
- G1Wifi = Wave：信号弧线逐条外扩淡出
- G1Cellular = Wave：信号塔波纹扩散
- G1Dhcp = Bounce：令牌两节点间轮转
- G3GeoIP = Bounce：大头针落下反弹
- G2RoutingTable = Path：路径节点依次亮起
- G5HttpCompression = Jiggle+：大文件→压缩→小文件（scale缩）
- G5HttpTiming = Jiggle：秒表指针旋转
- G5HttpRedirect = Path：箭头 A→B
- G1DnsCache = Jiggle+：卡片 Y 轴翻转（Rotation axis，非 shader）
- …其余按对方 46 项表（已全部吸收）
- **兜底**：Jiggle 抖动

### 原则
- 仅运行中 + 可视区动画；完成即停，不留残影
- 动效叠加在元素上（小球/光点），不旋转整张图标
- `reduce-motion` 降级为透明度脉冲
- **AnimationTokens.js**（JS对象）：忙碌循环 800-1200ms / 落定 300ms / 过渡 200ms

## 九、L5 详情页（P4，对方结构 + 我方数据可视化）

### DetailPage.qml（全屏，替换 overlay）
```
← 返回   Ping    [分享/复制]
┌──────── Hero区 160px ────────┐  ← 大图标 + 完成/失败动画
│        [动画]               │
├────── 关键指标卡 ────────────┤  ← 大数字+单位+标签（数字递增 200ms）
│        23 ms RTT            │
├─ Sent ─ Recv ─ Loss ────────┤  ← 指标卡 Grid（错峰滑入 50ms）
├─ Min ─ Max ─────────────────┤
├── Properties（可折叠）───────┤  ← 属性错峰滑入
├── Terminal Output（打字动画）┤  ← 逐行 80ms + 语法高亮
└─────────────────────────────┘
```

### 6类详情模板 + 可视化
| 模板 | 适用 | Hero | 可视化 |
|------|------|------|--------|
| Ping/延迟 | Ping/DnsResolution/PathPing | 球往返+对勾 | RTT柱生长 + 丢包环 + 走势线 |
| 路径 | Traceroute/PathPing | 逐跳节点 | 跳数路径图（VisualRoute式） |
| 握手 | TcpConnect/Ssl | 握手/印章 | 时序图 + 证书链 + 剩余天数环 + 指纹 |
| 请求响应 | HTTP系 | 秒表/箭头 | 瀑布图(TTFB分解) + 重定向时间线 |
| 查询结果 | DB系/FTP/SSH | 终端/柱脉冲 | 横幅 + 响应时间 + 属性表 |
| 系统属性 | G1/G2系 | 属性/波形 | 属性卡 + 信号图 + 路由表 |

### 共享原语
`MetricCard.qml`（数字递增）、`TerminalBlock.qml`（打字+高亮）、`DetailHero.qml`、`Sparkline/BarChart/Gauge/PathGraph.qml`（图表原语，仅 QML 标准动画绘制）

## 十、实施阶段（融合：我方阶段 + 对方分批）

| 阶段 | 内容 | 验证 |
|------|------|------|
| **P0** | L1 数据层 + 数据审计 | 构建 + CI + 单测 |
| **P1** | L2 图标管线 + 首批8个彩色SVG → G4全+G5关键 → 其余 | pre-commit + 视觉截图 |
| **P2** | L3 DiagBlock + Flow 网格 + 落定动画 + 全屏导航 | 构建 + 手机/桌面截图 |
| **P3** | L4 6动画类 + 46项注册 + 兜底 + reduce-motion | 构建 + 录屏走查 |
| **P4** | L5 DetailPage + 6模板 + 共享原语 + 46项数据接入 | 构建 + UI走查 |
| **P5** | i18n(15语言新key) / 主题JS对象 / 无障碍 / 性能 / CI 三平台全绿 | pre-commit + CI |

依赖链：P0→P1→P2→P3→P4 严格顺序，P5 收尾。P3 与 P4 可部分并行（依赖 P2 导航）。

## 十一、关键文件

**新增**
- `src/Common/View/widgets/DiagBlock.qml`
- `src/Common/View/widgets/DiagAnimator.qml`
- `src/Common/View/widgets/animations/{Jiggle,Bounce,Path,Pulse,Type,Lock}Animation.qml`
- `src/Diagnostics/View/DetailPage.qml`
- `src/Diagnostics/View/DetailHero.qml` / `MetricCard.qml` / `TerminalBlock.qml`
- `src/Diagnostics/View/detail/viz/{Sparkline,BarChart,Gauge,PathGraph}.qml`
- `src/Common/View/theme/AnimationTokens.js`
- `resources/icons/ffffff/*.svg`（46个主SVG重绘）

**修改**
- `src/Common/Model/DiagnosticResult.h`（+data）
- 46个诊断 `.cpp`（结构化输出）
- `src/app/ResultsModel.cpp/.h`（data 透传）
- `src/Common/View/widgets/DiagGroupPanel.qml`（网格化）
- `src/Diagnostics/View/DiagnosticScreen.qml`（全屏导航替换 overlay）
- `src/Common/View/AppContent.qml`（DetailPage 路由 push）
- `src/Common/View/widgets/AppIcon.qml`（+color2）
- `src/Common/Model/DiagNames.h`（+diagAnimationType）
- `scripts/generate-colored-icons.py`（多色槽）
- `src/Common/View/theme/ThemeEngine.qml`（JS 对象扩展）
- `resources/translations.json`（新 UI 字符串，15语言）
- `resources/resources.qrc` + `resources/screens-qmldir`（新QML注册）
- `CMakeLists.txt`（如有新 cpp）
- `scripts/pre-commit`（图标同步检查扩展）

## 十二、验证清单

- [ ] 三平台 CI 绿 + pre-commit 18项
- [ ] iOS 真机46项全量运行，动画不卡顿不崩溃（静态 Qt 最严）
- [ ] Android 滚动流畅；桌面 resize 列数自适应
- [ ] 暗/亮主题下彩色图标均可辨识（哨兵烘焙保证）
- [ ] RTL 方块镜像正确；reduce-motion 降级生效
- [ ] 内存增量 <15MB；仅可视区动画
- [ ] 6类详情模板各抽查1项渲染正确

## 十三、待讨论问题（多轮交叉讨论驱动）

| # | 问题 | 状态 |
|---|------|------|
| Q1 | 强调色策略 | 已定：哨兵烘焙 + 语义强调色表（否决对方硬编码 icons-color/） |
| Q2 | 方块形状 | 已定：D7 宽松大卡，块内=图标区+信息区（吸收对方） |
| Q3 | 组展开/折叠 | **待定**：保留折叠 vs 常驻全显 |
| Q4 | 未开始方块外观 | **待定**：静态灰显 vs 静态彩色 |
| Q5 | 详情页返回 | **待定**：返回按钮 / 手势 / 两者 |
| Q6 | Dashboard 同步改 | **待定**：只改 Diagnostics vs 同步 |
| Q7 | 批次 | 已吸收对方 P0/P1/P2 分批（动画仍一步到位 D4） |
| Q8 | 节奏规范 | 已定：AnimationTokens.js（忙碌800-1200/落定300/过渡200ms） |
