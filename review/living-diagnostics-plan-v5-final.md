# 活态诊断 UI 重构 — 规划文档 v5.0（最终版）

> **状态：已实施** | 日期：2026-08-11 | 分支：master
>
> 融合来源：
> - v2.0 规划（46项动画映射 + 彩色图标 + 方块卡片 + 详情页 + 竞品分析）
> - v3.0 方案（L1-L5分层架构 + 数据层 + 哨兵色槽 + 图表原语 + 无障碍）
> - v4.0 多轮讨论确认（16项设计决策）
> - v5.0 实施后更新（DiagnosticMeta 注册表 + Gap-Ratio 算法 + SVG 修复 + 审查修正）

---

## 一、项目概述

将诊断结果页从"文本列表 + 旋转Spinner"重构为**活态诊断（Living Diagnostics）**体验。

| 从 | 到 |
|----|----|
| 32px 文本列表行 | 方块格网格（Gap-Ratio 动态 tile 算法） |
| 旋转 Spinner SVG | 46项专属签名动画（6大类可复用组件） |
| 单色线条图标 | 双色渐变质感图标（4哨兵色槽烘焙） |
| 半透明文本覆盖层 | 全屏详情页（动画 + 图表 + 终端） |
| 纯文本诊断输出 | 结构化数据驱动可视化（DiagnosticMeta 注册表） |

---

## 二、架构设计（L1-L5 + 元数据注册表）

### 2.1 DiagnosticMeta — 新增元数据注册表层

**文件：** `src/Common/Model/DiagnosticMeta.h` + `DiagnosticMeta.cpp`

集中管理每项检测的**所有**元数据，替代分散在 5+ 文件中的 switch 语句：

```cpp
struct DetailProfile {
    bool showErrorOutput : 1;    // 红色错误块
    bool showProperties  : 1;    // 属性键值对
    bool showCharts      : 1;    // 图表可视化
    bool showTerminal    : 1;    // 终端输出
    const char* keyMetricField;  // 关键指标字段名
    const char* keyMetricUnit;   // 单位
    int  keyMetricPrecision;     // 小数位数
    ChartType chartType;         // BarChart / Gauge / NoChart
};

struct DiagnosticMeta {
    DiagId       id;
    const char*  displayName;
    const char*  iconName;
    uint8_t      platforms;      // Platform::Flag 位掩码
    DiagAnimType animType;       // L4 动画
    DiagTemplateType tmplType;   // L5 模板
    DetailProfile detail;        // 详情页配置
};
```

**46 项检测注册表**：`kDiagMeta[]` 数组，`static_assert` 强制恰好 46 项，按 `DiagId` 枚举索引。

**平台可用性位掩码**：
| 平台 | 位 |
|------|-----|
| iOS | `1<<0` |
| Android | `1<<1` |
| Windows | `1<<2` |
| macOS | `1<<3` |
| Linux | `1<<4` |
| Desktop | `Windows\|macOS\|Linux` |
| Mobile | `iOS\|Android` |

**ResultsModel 注入**：`resultToVariantMap()` 和 `getDetailResult()` 自动将元数据字段注入 QML 数据 map。

### 2.2 L1 — 数据层

`DiagnosticResult` 新增 `QVariantMap data` 字段，46 项诊断全部填充结构化输出。

**关键数据示例（Ping）**：
```cpp
r.data = {
    "target": "8.8.8.8", "packetsSent": 4, "packetsReceived": 4,
    "lossPercent": 0.0, "rttMinMs": 22.0, "rttMaxMs": 25.0,
    "rttAvgMs": 23.0, "rttJitterMs": 1.2,
    "tcpFallback": false, "individualRtts": [22, 25, 23, 23]
};
```

### 2.3 L2 — 图标管线

**4 哨兵色槽烘焙**：

| 哨兵色 | 含义 | 替换逻辑 |
|--------|------|---------|
| `#FFFFFF` | 主色 | 调色板颜色（主题自适应） |
| `#AAAAAA` | 渐变深端 | `darken(主色, 30%)` |
| `#000000` | 语义强调色 | `SEMANTIC_ACCENT` 表 |
| `#777777` | 柔和填充 | `#64748B` |

**P0 彩色图标**：ping, traceroute, tcp-connect, certificate, dns-server, wifi, cpu, network-card（8 个）

**iOS/Android SVG 修复**：`Q_IMPORT_PLUGIN(QSvgPlugin)` 用于静态 Qt 构建（`PLATFORM_IOS` + `ND_STATIC_QT`）

### 2.4 L3 — 方块格 UI

**Tile 尺寸算法 — Gap-Ratio v3**：

```
k = gap / tile  (full: 0.08, compact: 0.06)
block_width = n × tile + (n+1) × gap
            = tile × (n + (n+1) × k)
→ tile = block_width / (n + (n+1) × k)
```

- 列数 n：最大列数使得 `tile >= minTile`
- 边缘间距：(n+1) 个 gap（含两端），遵循 iOS Home Screen / M3 标准
- Tile 大小钳位：compact `[80, 160]`，full `[100, 160]`

**DiagBlock 状态机**：
| 状态 | 外观 |
|------|------|
| Pending | textSecondary(#94A3B8) @ 0.80，静态图标 |
| Running | 彩色图标 + L4 动画 + primary 边框脉冲 + 左上角计时圆点 |
| Done | 状态色 tint + 落定 pop（Scale 0.9→1.0 OutBack 300ms） |
| Disabled | 暗淡灰，不可交互 |

**运行时计时器**：左上角 22px 彩色圆点，内部显示秒数。颜色按耗时分档：<5s 绿 → 5-9s 黄 → 10-20s 橙 → >20s 红。执行结束后保持显示。

**DiagTileGrid**：Grid + 动态列数 + 动态 tile 大小，两屏（Diagnostic/Dashboard）共享同一组件。

### 2.5 L4 — 动画引擎

**6 类动画**：Jiggle（默认，~15项）、Bounce（6项）、Path（6项）、Pulse（9项）、Type（6项）、Lock（4项）

**DiagAnimator**：Loader + source URL 延迟加载，`appState.diagAnimationUrl()` 返回 QRC URL。

**AnimationTokens.js**：集中管理动画时长（jigglePeriod, bouncePeriod, settleDuration 等）。

### 2.6 L5 — 详情页

**Header**：单行 42px ToolBar：`<| test name | duration | COPY`

**Body 间距层级**（M3 + Apple HIG 最佳实践）：
| 间距 | Token | 用途 |
|------|-------|------|
| 16px | `spacing.lg` | Header-body 间距 + 独立区块边界 |
| 8px | `spacing.sm` | 相关组内间隔（Hero+MetricCard+Error） |
| 12px | `spacing.md` | 卡片内部 padding |

**条件显示逻辑**：
| 区块 | 显示条件 |
|------|---------|
| Hero | 始终显示（状态 disc + summary + meta line） |
| MetricCard | `_keyMetric.ok`（有结构化指标数据） |
| Error Output | `_hasErrorOutput`（C++ errorOutput 非空） |
| Properties | `_hasProperties`（属性列表非空） |
| Detailed Data | `chartView.hasChart`（图表数据可用） |
| Terminal Output | `_hasTerminalOutput`（details/rawOutput 非空） |

**效率优化**：
- `_terminalText`：`details || rawOutput` 表达式缓存一次
- `_terminalLines`：O(n) 字符循环替代 `split()` 数组分配
- `_hasTerminalOutput` / `_hasErrorOutput` / `_hasProperties` 预计算
- MetricCard 计数动画（数字递增）

---

## 三、G5 协议测试 errorOutput 修复

### 3.1 架构修复

**`g5Result()` 自动 errorOutput**：`status == Fail/Warning/Error` 时 `errorOutput = summary`

**`autoErrorOutput()` 辅助函数**：供 `g5ProbeResult()` 调用者在状态从 Pass 改为 Warning 后调用

**`g5ProbeResult()` 错误详情**：`NetworkProbe::TcpProbeResult` 新增 `QString error` 字段，捕获 `sock.errorString()`

### 3.2 双渲染防止

- `g5ProbeResult()` 失败时**不**将 `details` 设为 `errorOutput`（终端保持清空）
- `result()` 不自动设置 `errorOutput = details`
- G5Ldap/MongoDB：连接失败时传递 `{}` 给 details，仅设置 errorOutput
- G5TcpConnect：成功时设置 details，失败时仅设置 errorOutput

---

## 四、代码审查与修复 — 24 项基础 + 25 项补充

### 基础修复（24 项，来自初始实现）

| 类别 | 数量 |
|------|------|
| 可见性/正确性 Bug | 5 项 |
| iOS 安全/5WHY 合规 | 3 项 |
| 性能 | 4 项 |
| 架构简化 | 6 项 |
| 代码质量 | 6 项 |

### 补充修复（25 项，来自审查迭代）

| 轮次 | 数量 | 代表修复 |
|------|------|---------|
| SVG 导入 | 1 | `Q_IMPORT_PLUGIN(QSvgPlugin)` 用于 iOS |
| #elif 合规 | 3 | G1DhcpStatus + G1WifiDiagnostics |
| 动画修复 | 6 | JiggleAnimation 可见输出、AnimationTokens.js 死代码、import 路径 |
| 数据层修复 | 5 | G4DnsResolution 变量遮蔽、returnCode/resultCode、jitterMs |
| 详情页修复 | 10 | Header 单行、间距层级、条件间隙折叠、errorOutput 双渲染、typewriter 阈值 |

---

## 五、设计决策（16 项确认）

| # | 决策 | 选择 |
|---|------|------|
| D1 | 布局 | 保留 G1-G5 折叠面板，展开内部方块 Grid |
| D2 | 动画 | 46 项差异化（6 大类可复用组件） |
| D3 | 详情页 | 全屏 StackView push |
| D4 | 图标 | 双色渐变质感，4 哨兵色槽烘焙 |
| D5 | Ping 动画 | 水平往返弹跳 |
| D6 | 方块密度 | 动态列数 + 动态 tile 大小（Gap-Ratio 算法） |
| D7 | 图标管线 | 单一线管，4 哨兵色槽 |
| D8 | 组面板 | 保留折叠 |
| D9 | Pending 外观 | 静态灰显 |
| D10 | 详情返回 | ← 按钮 + 右滑手势 |
| D11 | Dashboard | 完全复用 DiagTileGrid |
| D12 | 图标风格 | Apple 微渐变风 |
| D13 | 卡片质感 | 默认 card+border，hover 浮起 |
| D14 | 详情入场 | StackView slide transition |
| D15 | 模板策略 | 6 模板（Ping/Path/Handshake/Request/Query/System） |
| D16 | 图表密度 | 短数据(≤8)默认展开，长数据折叠 |

---

## 六、文件清单

### 新增文件（~35 个）

```
src/Common/Model/DiagnosticMeta.h             元数据注册表头文件
src/Common/Model/DiagnosticMeta.cpp           46 项检测元数据表
src/Common/View/widgets/DiagBlock.qml         L3 方块卡片
src/Common/View/widgets/DiagTileGrid.qml      Gap-Ratio 动态 tile 网格
src/Common/View/widgets/DiagAnimator.qml      L4 动画分发器
src/Common/View/widgets/animations/
    JiggleAnimation.qml / BounceAnimation.qml / PathAnimation.qml
    PulseAnimation.qml / TypeAnimation.qml / LockAnimation.qml
src/Common/View/theme/AnimationTokens.js       动画时长 token
src/Common/View/detail/
    MetricCard.qml / TerminalBlock.qml
    viz/Gauge.qml / viz/BarChart.qml / viz/ResultChart.qml
src/Diagnostics/View/DetailPage.qml            L5 全屏详情页
resources/icons/ffffff/
    ping.svg / traceroute.svg / tcp-connect.svg / certificate.svg
    dns-server.svg / wifi.svg / cpu.svg / network-card.svg
```

### 修改文件（~80 个）

```
src/Common/Model/DiagnosticResult.h           + QVariantMap data
src/Common/Model/DiagNames.h                  + diagAnimationType, diagTemplateType
src/Common/Model/DiagId.h                     _G3Reserved17_Deprecated (Netskope removed)
src/Common/Model/DiagCapability.h             平台能力声明
src/Configuration/Model/DiagnosticConfig.h    isValidDiagId 修复
src/app/AppState.h/.cpp                       diagAnimationUrl()
src/app/ResultsModel.h/.cpp                   data + meta 注入
src/Common/View/widgets/DiagGroupPanel.qml    网格化 + testRunning 修复
src/Dashboard/View/DashboardScreen.qml        DiagTileGrid 复用 + 间距统一
src/Diagnostics/View/DiagnosticScreen.qml     DetailPage push + async 编译
src/Diagnostics/View/DetailPage.qml           Header 重设计 + 间距层级
46 个 src/Diagnostics/Model/G*/*.cpp          各补 r.data 结构化输出
scripts/generate-colored-icons.py             4 哨兵 + 缓存 + 精度修复
cmake/dependencies.cmake                     Qt6Svg 可选
src/main.cpp                                  Q_IMPORT_PLUGIN(QSvgPlugin)
```

---

## 七、构建验证

所有修改通过 `ninja -j4` 编译（aarch64 Linux, Qt 6, CMake 3.31），18 项 pre-commit 检查全绿。

图标管线生成：33 色 × 105 图标 = 3465 文件（48 双色）。
