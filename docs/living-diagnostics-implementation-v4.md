# NetDiagnostics — 活态诊断 UI 重构 实施报告

> 日期：2026-08-10
> 分支：master
> 基于规划文档：[living-diagnostics-plan-v4-merged.md](../../.claude/plans/living-diagnostics-plan-v4-merged.md)

---

## 一、项目概述

将诊断结果页从"文本列表 + 旋转Spinner"重构为**活态诊断（Living Diagnostics）**体验，覆盖5个架构层（L1-L5），涉及 ~80 文件的新增/修改。

### 设计决策（16项确认）

| # | 决策 | 选择 |
|---|------|------|
| D1 | 布局 | 保留G1-G5折叠面板，展开内部方块Grid |
| D2 | 动画 | 46项差异化（6大类可复用组件） |
| D3 | 详情页 | 全屏StackView push |
| D4 | 图标 | 双色渐变质感，4哨兵色槽烘焙 |
| D5 | Ping动画 | 水平往返弹跳 |
| D6 | 方块密度 | 手机1列/平板2列/桌面3列，图标64px |
| D7 | 图标管线 | 单一线管，4哨兵色槽 |
| D8 | 组面板 | 保留折叠 |
| D9 | Pending外观 | 静态灰显 |
| D10 | 详情返回 | ←按钮 + 右滑手势 |
| D11 | Dashboard | 完全复用方块Grid |
| D12 | 图标风格 | Apple微渐变风 |
| D13 | 卡片质感 | 默认card+border，hover浮起 |
| D14 | 详情入场 | StackView slide transition |
| D15 | 模板策略 | 6模板共用统一框架 |
| D16 | 图表密度 | 默认折叠 |

---

## 二、架构分层实现

### L1 — 数据层（P0）

**改动：** `DiagnosticResult` 新增 `QVariantMap data` 字段，46项诊断全部填充结构化输出。

| 文件 | 改动 |
|------|------|
| `src/Common/Model/DiagnosticResult.h` | + `QVariantMap data` |
| `src/app/ResultsModel.cpp` | `resultToVariantMap()` / `getDetailResult()` 透传 `data` |
| 46个 `src/Diagnostics/Model/G*/*.cpp` | 各补结构化 `r.data` 输出 |

**关键数据示例（Ping）：**
```cpp
r.data = {
    "target": "8.8.8.8", "packetsSent": 4, "packetsReceived": 4,
    "lossPercent": 0.0, "rttMinMs": 22.0, "rttMaxMs": 25.0, "rttAvgMs": 23.0,
    "rttJitterMs": 1.2, "tcpFallback": false,
    "individualRtts": [22, 25, 23, 23]
};
```

### L2 — 图标管线（P1）

**改动：** 4哨兵色槽烘焙管线 + 8个P0彩色主SVG。

| 哨兵色 | 含义 | 替换逻辑 |
|--------|------|---------|
| `#FFFFFF` | 主色 | 调色板颜色（主题自适应） |
| `#AAAAAA` | 渐变深端 | `darken(主色, 30%)` |
| `#000000` | 语义强调色 | `SEMANTIC_ACCENT` 表查找（固定） |
| `#777777` | 柔和填充 | `#64748B`（固定） |

**P0彩色图标（8个）：** ping, traceroute, tcp-connect, certificate, dns-server, wifi, cpu, network-card

**脚本改进：**
- `darken_hex()` — HSL空间降亮度 + min-lightness guard
- SVG正文缓存 — 33色×105图标从~1900次磁盘读取减为105次
- 未映射 `#000000` sentinel 警告输出
- `round()` 替代 `int()` 避免浮点截断

### L3 — 方块格 UI（P2）

**新增文件：**
- `DiagBlock.qml` — 正方形方块卡片（120-140px），4状态驱动
- `DiagGroupPanel.qml` 改造 — 内部 Flow 响应式网格替代列表行

**DiagBlock 状态机：**

| 状态 | 外观 | 动画 |
|------|------|------|
| Pending | textSecondary(#94A3B8) @ 0.80 透度，静态图标（与组标题图标令牌一致） | 无 |
| Running | 彩色图标 + L4动画 + primary边框脉冲 | DiagAnimator |
| Done | 状态色tint + 落定pop | Scale 0.9→1.0 OutBack 300ms |
| Disabled | 暗淡灰，不可交互 | 无 |

**运行时指示器：** 左上角单圆点 + 秒数文本。彩色编码按耗时分档：<5s 绿 → 5-9s 黄 → 10-20s 橙 → >20s 红。执行结束后圆点保持显示（无消隐定时器），重跑时归零重置。

**方块信息策略：** 磁贴为图标+名称两元素（L3 规范），底线指标行已移除。详情页全量展示指标和图表。

### L4 — 动画引擎（P3）

**新增文件：**

| 文件 | 用途 |
|------|------|
| `DiagAnimator.qml` | 分发器 — C++ `diagAnimationUrl()` 直接返回QRC URL |
| `JiggleAnimation.qml` | 🌀 抖动 — ~15项（默认），shimmer环旋转±2.5°+脉冲 |
| `BounceAnimation.qml` | 🏓 弹跳 — 6项（Ping/DHCP/GeoIP/MQTT等），小球左右往返 |
| `PathAnimation.qml` | 🔗 路径 — 6项（Traceroute/Route/TCP等），节点逐跳出现 |
| `PulseAnimation.qml` | 🌊 脉冲 — 9项（CPU/DNS/DB等），透明度呼吸 |
| `TypeAnimation.qml` | ⌨️ 序列 — 6项（ARP/URL/Headers等），逐行显示 |
| `LockAnimation.qml` | 🔒 锁定 — 4项（SSL/DNS Integrity等），印章下落+弹跳 |
| `AnimationTokens.js` | 动画时长token |

**C++ 支持：**
- `DiagAnimType` 枚举（DiagNames.h）
- `diagAnimationType(DiagId)` — DiagId→动画类别
- `AppState::diagAnimationUrl()` — Q_INVOKABLE，返回QRC URL字符串
- `DiagTemplateType` 枚举 — DiagId→详情模板类别

### L5 — 详情页（P4）

**新增文件：**

| 文件 | 用途 |
|------|------|
| `DetailPage.qml` | 全屏详情页（StackView push），统一框架+6类模板 |
| `MetricCard.qml` | 关键指标卡片（数字递增动画） |
| `TerminalBlock.qml` | 终端输出（打字逐行动画） |
| `viz/Gauge.qml` | 水平条形仪表 |
| `viz/BarChart.qml` | 柱状图（共享Timer交错入场） |

**6类详情模板（按 `DiagTemplateType` 分派）：**

| 模板 | 适用 | Hero | 可视化 |
|------|------|------|--------|
| Ping (0) | Ping/Dns/PathPing | 球往返 | RTT柱 + 丢包率 |
| Path (1) | Traceroute/Routing | 逐跳节点 | 路径图 |
| Handshake (2) | SSL/DNS/Security | 印章/锁定 | 证书链 + 环图 |
| Request (3) | HTTP系列 | 秒表/箭头 | 瀑布图 |
| Query (4) | DB/FTP/SSH等 | 终端/柱 | 连接信息 |
| System (5) | G1/G2默认 | 属性卡 | 适配器/路由表 |

---

## 三、代码审查与修复

经过10角度全面审查（行级扫描、跨文件追踪、语言陷阱、包装器正确性、复用、简化、效率、架构、约定合规、移除行为审计），共发现并修复 **24项问题**：

### 可见性/正确性 Bug（5项）

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | JiggleAnimation.qml | 无可见输出（动画目标为不可见Loader） | 重写为shimmer环 |
| 2 | G4DnsResolution.cpp | 变量遮蔽：`int rcode` 重新声明，外部变量始终 -1 | 移除 `int`，赋值到外部变量 |
| 3 | DetailPage.qml | `diagId \|\| -1` 将 diagId=0 当作 falsy | `!== undefined` 严格检查 |
| 4 | TypeAnimation.qml | `parent.children[index]` off-by-one（跳过Repeater） | `children[index + 1]` |
| 5 | DiagBlock.qml | 缺少skipped可见性门控 | 添加 `visible: isPending \|\| status !== 3` |

### iOS安全/5WHY合规（3项）

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 6 | DetailPage.qml | `sourceComponent` 饥饿编译 | `Loader + source URL` |
| 7 | G1NetworkAdapters/G2ArpTable/G2DefaultGateway | `#elif` 违反 CLAUDE.md check#4 | 嵌套 `#else + #if` |
| 8 | DetailPage.qml | QRC路径 `../../Common/` 无法解析 | `"../"` 单级路径 |

### 性能（4项）

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 9 | DiagnosticScreen.qml | `Qt.createComponent` 同步编译阻塞UI | `Component.Asynchronous` + Loading处理 |
| 10 | generate-icons.py | SVG每色循环重复读取（~1900次） | 预缓存 `cached_bodies` |
| 11 | BarChart.qml | N个Timer QObject（每个bar一个） | 单共享Timer + `_revealIndex` 计数器 |
| 12 | DetailPage.qml | `Object.keys()` 三处独立调用 | 提升为单一 `_hasData` 属性 |

### 架构简化（6项）

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 13 | DiagNames.h + ResultsModel + DetailPage | `_template` 鸭子类型推断 | `DiagTemplateType` 枚举 + 自动注入 |
| 14 | AppState + DiagAnimator | enum→int→QML switch→URL 双重分发 | 合并为 `diagAnimationUrl()` 直接返回URL |
| 15 | AnimationTokens.js | 7个死字段 | 精简为8个被消费token |
| 16 | resources.qrc | DiagResultItem.qml 孤儿条目 | 移除 |
| 17 | DiagBlock.qml | `_isRunning` 位置不一致 | 提升到 root Item |
| 18 | DiagGroupPanel.qml | `blockSize` 双重求值 | 内联columns逻辑 |

### 代码质量（6项）

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 19 | 6动画.qml | import路径 `../theme` 解析到不存在目录 | `../../theme` |
| 20 | DiagnosticScreen.qml | `getDetailResult` 双次调用 | 信号处理器只传 `{diagId}` |
| 21 | generate-icons.py | `int()` 浮点截断 | `round()` |
| 22 | generate-icons.py | 未映射#000000静默黑 | WARNING输出 |
| 23 | DiagBlock.qml | `_hasData` 计算但未引用 | 删除 |
| 24 | generate-icons.py | `darken_hex` 深色输入产生不可见渐变 | min-lightness guard (0.05) |

### 本会话补充修复（3项，2026-08-10）

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 25 | DiagBlock.qml | 运行指示器用硬币找零多圆点（50/20/10/5/1），在小磁贴上密集杂乱 | 替换为单圆点+秒数文字，按耗时阈值着色，执行后保持显示（移除2s消隐） |
| 26 | DiagBlock.qml | 底线 metricLine 显示执行时长/缩略结果，与左上角新指示器冗余且违反L3图标+名称规范 | 删除 metricLine，nameLabel 直接锚定卡片底部 |
| 27 | DiagBlock.qml | 待执行态图标 textMuted(#64748B) @ 0.55 在深色卡片上仅~7%亮度对比（数学上不可见）。10+次过往修复均处理渲染引擎问题（ShaderEffect/FBO/Z-order），从未审视色彩令牌选择 | 改为 textSecondary(#94A3B8) @ 0.80，与组标题图标令牌一致；5WHY 根因是使用了错误语义颜色变量 |

---

## 四、文件清单

### 新增文件（~30个）

```
src/Common/Model/DiagNames.h                         (+ DiagAnimType, DiagTemplateType 枚举)
src/Common/View/widgets/DiagBlock.qml                (L3 方块卡片)
src/Common/View/widgets/DiagAnimator.qml             (L4 动画分发器)
src/Common/View/widgets/animations/
    JiggleAnimation.qml                               (🌀 抖动)
    BounceAnimation.qml                               (🏓 弹跳)
    PathAnimation.qml                                 (🔗 路径)
    PulseAnimation.qml                                (🌊 脉冲)
    TypeAnimation.qml                                 (⌨️ 序列)
    LockAnimation.qml                                 (🔒 锁定)
src/Common/View/theme/AnimationTokens.js              (动画时长token)
src/Common/View/detail/
    MetricCard.qml                                    (指标卡片)
    TerminalBlock.qml                                 (终端输出)
    viz/Gauge.qml                                     (条形仪表)
    viz/BarChart.qml                                  (柱状图)
src/Diagnostics/View/DetailPage.qml                   (L5 全屏详情页)
resources/icons/ffffff/
    ping.svg                 (重绘，4哨兵色槽)
    traceroute.svg           (重绘)
    tcp-connect.svg          (重绘)
    certificate.svg          (重绘)
    dns-server.svg           (重绘)
    wifi.svg                 (重绘)
    cpu.svg                  (重绘)
    network-card.svg         (重绘)
```

### 修改文件（~50个）

```
src/Common/Model/DiagnosticResult.h                   (+ QVariantMap data)
src/Common/Model/DiagNames.h                          (+ diagAnimationType, diagTemplateType)
src/app/AppState.h/.cpp                               (+ diagAnimationUrl)
src/app/ResultsModel.cpp                              (+ data 透传 + templateType 注入)
src/Diagnostics/View/DiagnosticScreen.qml             (DetailPage push + async createComponent)
src/Common/View/widgets/DiagGroupPanel.qml            (Flow网格 + blockSize优化)
src/Common/View/widgets/DiagBlock.qml                 (可见性门控 + _isRunning提升)
scripts/generate-colored-icons.py                     (4哨兵 + 缓存 + round + 警告 + guard)
resources/resources.qrc                               (+ 新QML条目, - DiagResultItem)
46个 src/Diagnostics/Model/G*/*.cpp                  (各补 r.data 结构化输出)
```

---

## 五、构建验证

所有修改通过 `ninja -j4` 编译（aarch64 Linux, Qt 6, CMake 3.31），链接成功生成 `net_diagnostics` 可执行文件。

图标管线生成：33色 × 105图标 = 3465文件（48双色）。
