# 自动化屏幕证据采集 — 完整设计文档

> **目标读者**：代码 Agent  
> **用途**：作为实施的唯一主蓝图，逐节对应实现步骤  
> **版本**：2.1  
> **日期**：2026-07-26  
> **基于**：`review/06_Capture_Architecture_Design.md` + `review/iOS_Android_Automated_Evidence_Capture_Implementation_Blueprint_CN.md` + 现有已实现的 CaptureFeatureGate / PlatformCapture / CaptureService
>  
> **v2.1 更新**：重构 CaptureRunningOverlay 从全屏模态卡片到紧凑浮动状态条；新增 `wantsScreenshot`/`isRecordingCapture` Q_PROPERTY 及 `captureModeChanged` 信号；新增 `wasRecordingSession()` Q_INVOKABLE 修复 `restoreSystemState()` 清除 `m_recording` 后的竞态；新增 Lucide SVG 图标（camera、list-checks）

---

## 目录

1. [功能需求概述](#1-功能需求概述)
2. [UX/UI 设计](#2-uxui-设计)
3. [架构总览](#3-架构总览)
4. [组件详细设计](#4-组件详细设计)
5. [状态机设计](#5-状态机设计)
6. [存储设计](#6-存储设计)
7. [数据流](#7-数据流)
8. [平台适配](#8-平台适配)
9. [实施步骤](#9-实施步骤)
10. [文件清单](#10-文件清单)

---

## 1. 功能需求概述

```
┌─────────────────────────────────────────────────────────────────┐
│                      需求要点                                   │
├─────────────────────────────────────────────────────────────────┤
│ R1 │ 隐藏入口激活（Settings > About > 双击 app 图标）✅ 已实现    │
│ R2 │ 激活后选择截屏 / 录屏 / 录屏+截屏 三种模式                   │
│ R3 │ 专注模式：防止通知干扰 + 防止自动锁屏/暗屏                    │
│ R4 │ 自动导航：切换每个 Tab 页面 → 截屏/录屏                      │
│ R5 │ 诊断自动化：填入网址 → 触发诊断 → 等待完成 → 截屏             │
│ R6 │ 详情采集：打开 InternetConnectivity → 截屏 → 录屏时滚动到底   │
│ R7 │ Dashboard 采集：切换到 Dashboard → 截屏 → 录屏时滚动到底      │
│ R8 │ Report 采集：打开 Report → 截屏/录屏                          │
│ R9 │ 录屏同时截屏：录屏过程中每步也保存截图                         │
│ R10│ 启动前提示用户勿触碰设备                                      │
│ R11│ 时间戳文件夹命名，与 Crash 报告同目录                          │
│ R12│ 文件名规则化、一目了然                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. UX/UI 设计

### 2.1 用户交互流程

```
用户双击 Settings App Icon (✅已实现)
         │
         ▼
  ┌─────────────────────────────┐
  │   🎬 捕获模式选择面板        │  ← CaptureModePanel
  │                             │
  │   📸 仅截屏                  │
  │   🎥 仅录屏                  │
  │   📸+🎥 录屏同时截屏 (推荐)   │
  │                             │
  │   [URL 输入框]              │  ← 预填当前 target
  │   [开始采集] [取消]          │
  └─────────────┬───────────────┘
                │ 点击 "开始采集"
                ▼
  ┌─────────────────────────────┐
  │   ⚠️  准备开始采集           │  ← CapturePreflightOverlay
  │                             │
  │   • 请勿触碰设备              │
  │   • 建议开启勿扰模式          │
  │   • 屏幕将保持常亮            │
  │   • 预计耗时 ~45秒            │
  │                             │
  │        [3] 秒后开始          │  ← 倒计时
  │        [取消]                │
  └─────────────┬───────────────┘
                │ 倒计时结束
                ▼
  ┌──────────────────────────────────────────────┐
  │  后台可见的自动导航+截屏/录屏执行              │
  │                                              │
  │  ┌──────────────────────────────────────┐    │
  │  │ ● 00:01:23 │ ⊙  7 │ ☰  5/12  ← 浮动条│   │  ← CaptureRunningOverlay (重构)
  │  └──────────────────────────────────────┘    │
  │                                              │
  │  [底层 UI 完全可见 + 可交互]                   │
  └─────────────┬────────────────────────────────┘
                │ 采集完成/取消
                ▼
  ┌─────────────────────────────┐
  │   ✅ 采集完成                │  ← CaptureResultSummary
  │                             │
  │   截图:  7 张                │
  │   录像:  1 段 (00:42)        │
  │   路径:  CaptureService/     │
  │          20260724-143052/    │
  │                             │
  │   [打开文件夹]  [关闭]        │
  └─────────────────────────────┘
```

**关键的 UX 设计变更** — `CaptureRunningOverlay` 从全屏模态卡片重构为紧凑浮动状态条：

- **旧设计（已移除）**：`anchors.fill: parent`、92% 不透明背景的居中卡片（~400×300px），完全遮挡底层 UI
- **新设计**：36px 高、透明（72% 不透明度）+ 投影、锚定右上角的浮动条。不阻挡底层 UI 交互，导航/诊断/滚动在状态条背后可见、可操作
- **设计理由（5WHY）**：旧模态遮挡了所有底层 UI——导航、诊断和滚动均被完全隐藏。用户无法验证采集进度，触摸事件被拦截。紧凑的浮动条让自动采集步骤在视觉上可验证

```
浮动条布局:

● 00:01:23 │ ⊙  7 │ ☰  5/12
↑ ↑         ↑  ↑   ↑    ↑ ↑
│ └ HH:MM:SS│  │   │    │ └ 总步数（2位右对齐，monospace）
│           │  │   │    └ 分隔符
│           │  │   └ 截图计数（2位右对齐，monospace，青色）
│           │  └ 截图图标（camera SVG, Lucide MIT, 青色）
│           └ 录像计时（固定最小宽度 72px，monospace，HH:MM:SS）
└ 闪烁红色录制圆点（仅 isRecordingCapture=true 时可见）
```

| 元素 | 实现 | 可见性约束 |
|---|---|---|
| 录制圆点 | 8×8px Rectangle，红色，闪烁动画（600ms） | `visible: isRecordingCapture`；仅录像/双模式 |
| 截图图标+计数 | Lucide `camera` SVG + 数字 | `visible: wantsScreenshot`；RecordingOnly 模式隐藏整组 |
| 任务图标+进度 | Lucide `list-checks` SVG + `current/total` | 始终可见 |
| 取消操作 | 整条 tappable（MouseArea.fill） | `enabled: root.opacity > 0`；截图隐藏期间禁用 |
```

### 2.2 视觉设计规范

| 元素 | 规范 |
|------|------|
| **模式选择面板** | 卡片式，居中弹出，圆角 20px，半透明背景遮罩 |
| **激活图标** | 青色 `ThemeEngine.cyan`，diagnostics 图标（✅已实现） |
| **倒计时** | 大号数字（64px），居中，脉冲动画 |
| **浮动状态条** | 右上角锚定，36px 高，圆角 10px，`surface` 色 @ 72% alpha + `MultiEffect` 投影（blurMax=10, shadowBlur=0.6, 偏移 1,2） |
| **录制指示器** | 8×8px 红色脉冲圆点（`failRed`），600ms 闪烁（SequentialAnimation），仅录像模式可见 |
| **截图图标** | Lucide `camera` SVG（MIT），15px，`cyan` 色，通过 `AppIcon`+`MultiEffect` colorization 渲染 |
| **任务图标** | Lucide `list-checks` SVG（MIT），15px，`textSecondary` 色 |
| **数字显示** | Monospace 13px DemiBold，右对齐，单数字前导空格（ThemeEngine.pad2），`cyan`/`textPrimary` |
| **耗时格式** | HH:MM:SS，ES5 padZero 零填充，1s 定时器轮询 `elapsedSeconds` |
| **截图隐藏** | `opacity: suppressOverlay ? 0 : 1`，`layer.enabled` 绑定 `opacity>0`（跳过 GPU 阴影），`MouseArea.enabled` 绑定 `root.opacity>0`（防止误触取消） |

---

## 3. 架构总览

```
┌──────────────────────────────────────────────────────────────────────┐
│                         QML Layer (View)                              │
│                                                                      │
│  CaptureModePanel.qml          CapturePreflightOverlay.qml           │
│  CaptureRunningOverlay.qml     CaptureResultSummary.qml              │
│                                                                      │
│  AppContent.qml  ←→  NavigationAdapter (C++ → QML switchToTab)       │
│  DiagnosticScreen.qml  ←→  CaptureService (进度指示器, 已实现)        │
├──────────────────────────────────────────────────────────────────────┤
│                    AppState (Facade)                                  │
│                                                                      │
│  captureOrchestrator ──→ CaptureOrchestrator (新增)                   │
│  captureService      ──→ CaptureService      (已实现)                 │
├──────────────────────────────────────────────────────────────────────┤
│                    C++ Core Layer                                     │
│                                                                      │
│  CaptureOrchestrator (新增)        — 总编排器，驱动状态机              │
│  CaptureStateMachine (新增)        — 捕获流程状态转换                 │
│  CaptureScenario (新增)            — 声明式步骤定义                   │
│  NavigationAdapter (新增)          — 封装 Tab 切换                    │
│  ScrollController (新增)           — 平滑滚动控制                     │
│  ScreenKeepAwake (新增)            — 防锁屏/暗屏                      │
│  CaptureFeatureGate (已实现)        — 功能开关                        │
│  CaptureService (已实现)            — 截屏会话管理                     │
├──────────────────────────────────────────────────────────────────────┤
│                    Platform Abstraction                               │
│                                                                      │
│  PlatformCapture.h/cpp (已实现)     — 截屏: QScreen::grabWindow        │
│  PlatformRecording.h/cpp (新增)    — 录屏平台抽象                    │
│    ├── DesktopRecording.cpp       — FFmpeg/libx264 (Linux)           │
│    ├── IOSRecording.mm            — RPScreenRecorder (iOS)           │
│    └── AndroidRecording.cpp       — MediaProjection (Android)         │
│  PlatformKeepAwake.h/cpp (新增)   — 屏幕常亮控制                      │
└──────────────────────────────────────────────────────────────────────┘
```

### 关键设计决策

| 决策 | 理由 |
|------|------|
| **编排器放 C++** | 状态机需精确控制时序；QML Timer 不够可靠 |
| **QML overlay 浮动于页面栈之上** | 右上角 36px 透明浮动条，不阻挡底层 UI 交互；z:2100 顶层显示采集状态；`layer.enabled: opacity>0` 按需开关 GPU 阴影 |
| **NavigationAdapter 通过 QMetaObject** | 不耦合 QML 类型；可被 C++ state machine 驱动 |
| **录屏平台隔离** | 与 PlatformShare/PlatformStore 模式一致 |
| **存储与 Crash 报告同目录** | 统一证据根目录（`<AppData>/Evidence/`），便于查找和管理 |

---

## 4. 组件详细设计

### 4.1 CaptureOrchestrator (核心编排器) ⭐

```
文件: src/EvidenceCapture/CaptureOrchestrator.h/.cpp
职责: 创建 Session → 驱动状态机 → 调度各组件 → 处理完成/失败/取消
```

```cpp
class CaptureOrchestrator : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)          // CaptureState enum
    Q_PROPERTY(int currentStep READ currentStep NOTIFY stepChanged)
    Q_PROPERTY(int totalSteps READ totalSteps NOTIFY stepChanged)
    Q_PROPERTY(QString currentAction READ currentAction NOTIFY actionChanged)
    Q_PROPERTY(int captureCount READ captureCount NOTIFY captureCountChanged)

public:
    enum CaptureMode { ScreenshotOnly = 0, RecordingOnly = 1, Both = 2 };

    explicit CaptureOrchestrator(AppState* appState, QObject* parent = nullptr);

    // ── 启动前检查 ──
    Q_INVOKABLE QVariantList runPreflight(const QString& diagUrl);
    // 返回: [{check:"focus_mode", ok:bool, hint:""}, ...]

    // ── 开始采集 ──
    Q_INVOKABLE void startCapture(int captureMode, const QString& diagUrl);

    // ── 取消 ──
    Q_INVOKABLE void cancel();

    // ── 内部: 步骤执行回调 ──
    void executeStep(const CaptureStep& step);
    void navigateTo(int tabIndex);
    void waitForPageReady(int tabIndex, int timeoutMs = 5000);
    void runDiagnostic(const QString& url);
    void waitForDiagnosticComplete(int timeoutMs = 120000);
    void openDiagnosticDetail(int diagId);
    void scrollPage(int durationMs = 3000);

signals:
    void stateChanged(int newState);             // CaptureStateMachine 状态
    void stepChanged(int current, int total);    // 步骤进度
    void actionChanged(const QString& action);   // 当前动作描述
    void captureCountChanged(int count);         // 已采集数
    void preflightComplete(const QVariantList& results);
    void captureCompleted(const QString& sessionPath);
    void captureCancelled();
    void captureFailed(const QString& errorCode, const QString& userMessage);

private:
    AppState* m_appState;
    CaptureStateMachine* m_stateMachine;   // 状态引擎
    CaptureScenario* m_scenario;           // 步骤定义
    NavigationAdapter* m_navAdapter;       // Tab 切换
    ScreenKeepAwake* m_keepAwake;          // 防锁屏
    int m_captureMode = 0;
    int m_captureCount = 0;
    QString m_sessionDir;
};
```

#### Q_PROPERTY 补充（v2.1 新增）

| 属性 | 类型 | NOTIFY | 用途 |
|---|---|---|---|
| `wantsScreenshot` | `bool` | `captureModeChanged` | RecordingOnly 模式下隐藏截图图标组 |
| `isRecordingCapture` | `bool` | `captureModeChanged` | 控制闪烁录制圆点可见性 |
| `suppressOverlay` | `bool` | `suppressOverlayChanged` | 截图瞬间隐藏浮动条 |

**`wasRecordingSession()` Q_INVOKABLE**：读取 `m_captureMode`（而非 `m_recording`）判断是否进行了录屏。`restoreSystemState()` 在 `captureCompleted` 信号延迟发射前就清除了 `m_recording`，导致 `isRecordingCapture()` 在 ResultSummary 中永远返回 false。`wasRecordingSession()` 通过读取 `m_captureMode`（不受 `restoreSystemState()` 影响）修复此竞态。

**`captureModeChanged()` 信号**：替代原来的 `stateChanged`（每 session ~11 次发射）。`wantsScreenshot` 和 `isRecordingCapture` 的值在 `startCapture()` 中设定一次后整个 session 不变，专用的 NOTIFY 信号仅在 `startCapture()` 中发射一次。

### 4.2 CaptureStateMachine (状态机)

```
文件: src/EvidenceCapture/CaptureStateMachine.h/.cpp
职责: 管理采集流程的所有合法状态转换
```

```
                    ┌──────────┐
                    │   Idle   │
                    └────┬─────┘
                         │ startCapture()
                         ▼
                    ┌──────────┐
                    │Preflight │ ← 检查前置条件
                    └────┬─────┘
                         │ preflight OK
                         ▼
              ┌─────────────────────┐
              │   CountdownToStart  │ ← 3-2-1 倒计时
              └─────────┬───────────┘
                        │ countdown=0
                        ▼
              ┌─────────────────────┐
              │   CreatingSession   │ ← 创建目录 + manifest
              └─────────┬───────────┘
                        │
                        ▼
              ┌─────────────────────┐
              │  StartingRecording  │ ← [录屏模式] 启动录屏
              └─────────┬───────────┘
                        │
                        ▼
              ┌─────────────────────┐
              │  ExecutingSteps     │ ← 遍历 Scenario.steps[]
              │  ┌───────────────┐  │
              │  │ navigate      │  │
              │  │ wait_ready    │  │
              │  │ capture       │  │  ← 每步都可能截图
              │  │ scroll        │  │  ← 录屏时滚动
              │  │ run_diag      │  │
              │  │ open_detail   │  │
              │  └───────────────┘  │
              └─────────┬───────────┘
                        │ all steps done
                        ▼
              ┌─────────────────────┐
              │  StoppingRecording  │ ← [录屏模式] 停止录屏
              └─────────┬───────────┘
                        │
                        ▼
              ┌─────────────────────┐
              │    Finalizing       │ ← 写 manifest + 日志
              └─────────┬───────────┘
                        │
              ┌─────────┼──────────┐
              ▼                    ▼
        ┌──────────┐        ┌──────────┐
        │Completed │        │  Failed  │
        └──────────┘        └──────────┘

        ┌──────────┐ (任意状态 → cancel)
        │Cancelled │
        └──────────┘
```

```cpp
enum class CaptureState {
    Idle = 0,
    Preflight,
    CountdownToStart,
    CreatingSession,
    StartingRecording,
    ExecutingSteps,
    StoppingRecording,
    Finalizing,
    Completed,
    Cancelled,
    Failed
};

class CaptureStateMachine : public QObject {
    Q_OBJECT
public:
    CaptureState state() const { return m_state; }
    bool canTransitionTo(CaptureState target) const;
    bool transitionTo(CaptureState target);  // 返回 false 如果非法转换

signals:
    void stateChanged(CaptureState from, CaptureState to);
    void invalidTransition(CaptureState from, CaptureState to);

private:
    CaptureState m_state = CaptureState::Idle;
    // 合法转换表
    static const QMap<CaptureState, QSet<CaptureState>> kValidTransitions;
};
```

### 4.3 CaptureScenario (场景定义)

```
文件: src/EvidenceCapture/CaptureScenario.h/.cpp
职责: 声明式定义采集步骤序列 — 纯数据，不包含执行逻辑
```

```cpp
enum class StepAction {
    Navigate,           // 切换到指定 Tab
    WaitPageReady,      // 等待页面渲染完成
    Capture,            // 截屏 (录屏模式下也截图如果 mode==Both)
    Scroll,             // 缓慢滚动 (仅录屏模式)
    SetUrl,             // 填入诊断 URL
    RunDiagnostic,      // 触发诊断
    WaitDiagComplete,   // 等待诊断完成
    OpenDetail,         // 打开诊断详情 (InternetConnectivity)
    OpenReport,         // 打开 Report 预览
    CloseOverlay,       // 关闭当前页面的详情/预览浮层 (截图后清理)
};

struct CaptureStep {
    StepAction action;
    QString param;         // Tab名 / URL / DiagId / 滚动时长ms
    QString description;   // 用于 UI 显示的步骤名
    bool captureBefore;    // 执行动作前先截图
};

// ══════════════════════════════════════════════════════════════════
// 默认 Scenario — 覆盖所有 Tab + 诊断 + 详情 + Dashboard + Report
// ══════════════════════════════════════════════════════════════════
inline QVector<CaptureStep> buildDefaultScenario(const QString& diagUrl) {
    return {
        // Phase 1: 所有 Tab 页面截图
        {Navigate,       "0", "Dashboard",        true},   // → Dashboard + 截屏
        {WaitPageReady,  "0", "",                  false},
        {Capture,        "01_dashboard",           "",      false},  // 纯截屏模式

        {Navigate,       "1", "Diagnostics",       true},   // → Diagnostics + 截屏
        {WaitPageReady,  "1", "",                  false},
        {Capture,        "02_diagnostic",          "",      false},

        {Navigate,       "2", "Config",            true},   // → Config + 截屏
        {WaitPageReady,  "2", "",                  false},
        {Capture,        "03_config",              "",      false},

        {Navigate,       "3", "Settings",          true},   // → Settings + 截屏
        {WaitPageReady,  "3", "",                  false},
        {Capture,        "04_settings",            "",      false},

        // Phase 2: 诊断自动化
        {Navigate,       "1", "Diagnostics (run)", true},
        {SetUrl,         diagUrl, "",              false},
        {RunDiagnostic,  "", "",                   false},
        {WaitDiagComplete,"120000", "",            false},
        {Capture,        "05_diag_complete",       "",      false},

        // Phase 3: InternetConnectivity 详情
        {OpenDetail,     "19", "InternetConnectivity", true},  // DiagId 19
        {WaitPageReady,  "", "",                   false},
        {Capture,        "06_connectivity_detail", "",      false},
        {Scroll,         "3000", "Scroll detail",  false},  // 录屏时 3s 滚动

        // Phase 4: Dashboard
        {Navigate,       "0", "Dashboard",         true},
        {WaitPageReady,  "0", "",                  false},
        {Capture,        "07_dashboard",           "",      false},
        {Scroll,         "5000", "Scroll dashboard",false},  // 录屏时 5s 滚动

        // Phase 5: Report
        {OpenReport,     "", "Report Preview",     true},
        {WaitPageReady,  "", "",                   false},
        {Capture,        "08_report",              "",      false},
    };
}
```

### 4.4 NavigationAdapter (导航适配器)

```
文件: src/EvidenceCapture/NavigationAdapter.h/.cpp
职责: 封装从 C++ 到 QML 的 Tab 切换调用
```

```cpp
class NavigationAdapter : public QObject {
    Q_OBJECT
public:
    explicit NavigationAdapter(QObject* appContent, QObject* parent = nullptr);

    // 切换到 Tab (0=dashboard, 1=diagnostic, 2=config, 3=settings)
    void switchToTab(int index);

    // 等待页面渲染完成 (通过 objectName 检测)
    void waitForPageReady(int tabIndex, int timeoutMs = 5000);

    // 打开诊断详情 overlay
    void openDiagnosticDetail(int diagIdInt);

    // 触发诊断运行 (填入 URL + 点击 RUN)
    void runDiagnostic(const QString& url);

    // 打开 Report 预览
    void openReportPreview();

    // 滚动当前页面的 Flickable
    void scrollFlickable(int durationMs);

signals:
    void pageReady(int tabIndex);
    void diagnosticComplete();
    void detailOpened();
    void scrollFinished();

private:
    QObject* m_appContent;  // AppContent QML 对象引用
    QTimer* m_timeoutTimer;
};
```

### 4.5 ScrollController (平滑滚动)

```
文件: src/EvidenceCapture/ScrollController.h/.cpp
职责: 以恒定速度将 Flickable 从顶部滚动到底部
```

```cpp
class ScrollController : public QObject {
    Q_OBJECT
public:
    // 从顶部开始，以恒定速度滚动到底部
    // durationMs: 总滚动时长 (默认 3000ms, 录屏时推荐 3000-5000ms)
    void scrollToBottom(int durationMs = 3000);

    // 停止当前滚动
    void cancel();

    // 注入目标 Flickable
    void setFlickable(QObject* flickable);

signals:
    void scrollFinished();
    void scrollProgress(qreal fraction);  // 0.0 → 1.0

private:
    QObject* m_flickable = nullptr;
    QTimer* m_scrollTimer;
    int m_durationMs = 0;
    qreal m_startPos = 0;
    qreal m_targetPos = 0;
};
```

### 4.6 ScreenKeepAwake (防锁屏)

```
文件: src/Common/Platform/PlatformKeepAwake.h   ← 跨平台头文件
      src/Common/Platform/PlatformKeepAwake.cpp  ← 桌面实现
      .../Apple/IOS/PlatformKeepAwake.mm         ← iOS 实现
      .../Android/PlatformKeepAwake.cpp          ← Android 实现
```

```cpp
// ── 跨平台抽象 ──
#pragma once
void platformSetKeepAwake(bool enable);
bool platformIsKeepAwake();

// ── Linux/桌面实现 ──
// 使用 X11/XSync 或 D-Bus (org.freedesktop.ScreenSaver)
// 简单方案: 周期性模拟按键 (不推荐)
// 推荐方案: D-Bus Inhibit
void platformSetKeepAwake(bool enable) {
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
    // D-Bus: org.freedesktop.ScreenSaver.Inhibit / UnInhibit
    // 或 org.gnome.SessionManager.Inhibit
#endif
}

// ── iOS 实现 ──
// [[UIApplication sharedApplication] setIdleTimerDisabled:enable]

// ── Android 实现 ──
// getWindow().addFlags(FLAG_KEEP_SCREEN_ON)
```

### 4.7 PlatformRecording (录屏抽象)

```
文件: src/Common/Platform/PlatformRecording.h    ← 跨平台头文件
      .../Desktop/DesktopRecording.cpp           ← 桌面实现 (FFmpeg)
      .../Apple/IOS/IOSRecording.mm              ← iOS (ReplayKit)
      .../Android/AndroidRecording.cpp            ← Android (MediaProjection)
```

```cpp
// ── 跨平台抽象 ──
#pragma once
#include <QString>
#include <functional>

using RecordingCallback = std::function<void(bool ok, const QString& filePathOrError)>;

// 开始录屏。filePath 不含扩展名，平台自动追加 .mp4/.mov
void platformStartRecording(const QString& filePath, RecordingCallback callback);

// 停止录屏。callback 返回最终文件路径
void platformStopRecording(RecordingCallback callback);

// 当前是否正在录屏
bool platformIsRecording();

// 录屏时同步截屏 (仅 iOS RPScreenRecorder 原生支持)
bool platformSupportsScreenshotWhileRecording();
```

**桌面 (Linux) 实现策略**:
```cpp
// 方案: FFmpeg x11grab (需要 ffmpeg 在 PATH 中)
// QProcess 启动 ffmpeg:
//   ffmpeg -f x11grab -video_size 1920x1080 -framerate 15
//          -i :0.0 -c:v libx264 -preset ultrafast
//          -pix_fmt yuv420p output.mp4
//
// stopRecording() → 发送 'q' 到 stdin → waitForFinished()
```

**iOS 实现策略**:
```objc
// RPScreenRecorder.sharedRecorder
// [recorder startRecordingWithHandler:^(NSError*)...]
// 支持: recorder.cameraPreviewView 叠加到 app window
// 警告: iOS 要求 Info.plist 中有 NSMicrophoneUsageDescription
//       (即使不录音，ReplayKit 也需要)
```

**Android 实现策略**:
```java
// MediaProjectionManager.createScreenCaptureIntent()
// → startActivityForResult(Intent, REQUEST_CODE)
// → MediaProjection + MediaRecorder
// 需要在 AndroidManifest.xml 声明 FOREGROUND_SERVICE_MEDIA_PROJECTION
```

---

## 5. 状态机设计

### 5.1 完整状态转换表

```
from \ to         Idle Pre  CntD Ses  StR  Exec StR  Fin  Comp Fail Canc
Idle               -    ✓    -    -    -    -    -    -    -    -    -
Preflight          -    -    ✓    -    -    -    -    -    -    ✓    ✓
CountdownToStart   -    -    -    ✓    -    -    -    -    -    ✓    ✓
CreatingSession    -    -    -    -    ✓*   -    -    -    -    ✓    ✓
StartingRecording  -    -    -    -    -    ✓    -    -    -    ✓    ✓
ExecutingSteps     -    -    -    -    -    -    ✓    -    -    ✓    ✓
StoppingRecording  -    -    -    -    -    -    -    ✓    -    ✓    ✓
Finalizing         -    -    -    -    -    -    -    -    ✓    ✓    -
Completed          ✓    -    -    -    -    -    -    -    -    -    -
Failed             ✓    -    -    -    -    -    -    -    -    -    -
Cancelled          ✓    -    -    -    -    -    -    -    -    -    -

✓* = 仅当 captureMode != ScreenshotOnly (即需要录屏)
```

### 5.2 超时和错误处理

| 状态 | 超时 | 处理 |
|------|------|------|
| Preflight | 10s | → Failed("preflight_timeout") |
| CountdownToStart | 用户取消 | → Cancelled |
| CreatingSession | 5s | → Failed("storage_error") |
| StartingRecording | 15s | → Failed("recording_start_failed") |
| ExecutingSteps (每步) | 30-120s | → Failed("step_timeout:{step}") |
| StoppingRecording | 30s | → Failed("recording_stop_failed") |
| Finalizing | 10s | 记录错误但标记 Completed |

---

## 6. 存储设计

### 6.1 目录结构

```
<AppData>/Evidence/                          ← 与 Crash 报告同级目录
├── NetDiagnostics_crash.log                 ← CrashHandler 现有文件
├── CaptureService/                          ← 手动触发的单次截屏 (已实现)
│   └── 20260724-143052/
│       ├── capture.log
│       ├── 00_session_start.png
│       ├── 01_G1_complete.png
│       └── ...
└── AutoCapture/                             ← 自动化采集 (本次新增)
    └── 20260724-150330_screenshot/          ← {ts}_{mode}
    │   ├── manifest.json                    ← Session metadata
    │   ├── execution.log                    ← Audit log
    │   ├── 01_dashboard.png
    │   ├── 02_diagnostic.png
    │   ├── 03_config.png
    │   ├── 04_settings.png
    │   ├── 05_diag_complete.png
    │   ├── 06_connectivity_detail.png
    │   ├── 07_dashboard.png
    │   └── 08_report.png
    └── 20260724-150445_recording/
        ├── manifest.json
        ├── execution.log
        ├── recording.mp4
        └── screenshots/                    ← Only if mode==Both
            ├── 01_dashboard.png
            └── ...
```

### 6.2 文件命名规则

```
格式: {seq2位}_{描述}_{子状态}.{ext}

规则:
  seq      — 2位数字序号, 01-99, 与 Scenario step 顺序对应
  描述     — 页面/动作名, 小写下划线, 英文
  子状态   — 可选, 如 before/after/scroll_mid/scroll_end

示例:
  01_dashboard.png              ← Dashboard 页面截图
  05_diag_complete.png          ← 诊断完成后截图
  06_connectivity_detail.png    ← InternetConnectivity 详情
  07_dashboard_scroll_mid.png   ← Dashboard 滚动中途截图
  recording.mp4                 ← 录屏文件 (唯一, 不编号)
```

### 6.3 manifest.json 格式

```json
{
  "session_id": "20260724-150330",
  "capture_mode": "screenshot",
  "diag_url": "https://httpbin.org",
  "started_at": "2026-07-24T15:03:30+08:00",
  "completed_at": "2026-07-24T15:04:15+08:00",
  "status": "completed",
  "captures": [
    {"seq": "01", "description": "dashboard",          "file": "01_dashboard.png",              "size": 245760},
    {"seq": "02", "description": "diagnostic",         "file": "02_diagnostic.png",             "size": 312400},
    {"seq": "03", "description": "config",             "file": "03_config.png",                 "size": 198200},
    {"seq": "04", "description": "settings",           "file": "04_settings.png",               "size": 220100},
    {"seq": "05", "description": "diag_complete",      "file": "05_diag_complete.png",          "size": 380500},
    {"seq": "06", "description": "connectivity_detail","file": "06_connectivity_detail.png",     "size": 420300},
    {"seq": "07", "description": "dashboard",          "file": "07_dashboard.png",              "size": 310200},
    {"seq": "08", "description": "report",             "file": "08_report.png",                 "size": 295800}
  ],
  "recording": null,
  "warnings": [],
  "errors": []
}
```

---

## 7. 数据流

### 7.1 截图流程 (每步)

```
CaptureOrchestrator::executeStep(step)
  │
  ├─ 1. captureBefore? → CaptureService::capture(step.description)
  │     └─ PlatformCapture::platformCaptureScreenshot(filePath)
  │         └─ QScreen::grabWindow(0) → pixmap.save(filePath, "PNG")
  │
  ├─ 2. 执行 step.action:
  │     ├─ Navigate → NavigationAdapter::switchToTab(index)
  │     │              → AppContent.switchToTab(index) [QML]
  │     │              → waitForPageReady()
  │     │
  │     ├─ SetUrl → AppState::setTarget(url)
  │     │
  │     ├─ RunDiagnostic → AppState::runDiagnostics()
  │     │
  │     ├─ WaitDiagComplete → 轮询 appState.runStatusInt() != 1
  │     │
  │     ├─ OpenDetail → DiagnosticScreen → detailOverlay.visible = true
  │     │
  │     ├─ Scroll → ScrollController::scrollToBottom(duration)
  │     │
  │     └─ OpenReport → AppContent → push Report preview
  │
  └─ 3. → AuditLogger::log(step_completed)
         → emit stepChanged(current, total)
```

### 7.2 录屏流程

```
CaptureOrchestrator::startCapture(mode=Recording, url)
  │
  ├─ Preflight → 检查磁盘空间 > 500MB, ffmpeg 可用 (桌面)
  ├─ Countdown → 3-2-1 倒计时, 用户看提示
  ├─ CreatingSession → 创建 {ts}_recording/ 目录
  ├─ StartingRecording → platformStartRecording(recordingPath)
  │     ├─ [Linux] QProcess → ffmpeg -f x11grab ...
  │     ├─ [iOS]   RPScreenRecorder.startRecording
  │     └─ [Android] MediaProjection + MediaRecorder
  │
  ├─ ExecutingSteps → 遍历 scenario steps:
  │     ├─ 每步: Capture (if mode==Both) → 截图到 screenshots/ 子目录
  │     ├─ Navigate → Switch tab
  │     ├─ WaitPageReady → 等待渲染
  │     ├─ [录屏] 每步完成后延时 2s → 让画面稳定
  │     ├─ Scroll → 3-5s 缓慢滚动 (录屏捕获滚动动画)
  │     └─ ...
  │
  ├─ StoppingRecording → platformStopRecording()
  │     ├─ [Linux] ffmpeg: write 'q' → waitForFinished()
  │     ├─ [iOS]   RPScreenRecorder.stopRecording
  │     └─ [Android] MediaRecorder.stop()
  │
  ├─ Finalizing → ManifestWriter::finalizeManifest()
  │             → ScreenKeepAwake::restore()
  │             → AuditLogger::log(session_completed)
  │
  └─ emit captureCompleted(sessionPath)
```

---

## 8. 平台适配

| 能力 | Linux Desktop | macOS | Windows | iOS | Android |
|------|:---:|:---:|:---:|:---:|:---:|
| **截屏** | QScreen | QScreen | QScreen | UIGraphicsImageRenderer | View.drawToBitmap |
| **录屏** | ffmpeg x11grab | AVFoundation | ffmpeg gdigrab | RPScreenRecorder | MediaProjection |
| **录屏+截屏** | ✅ 进程独立 | ✅ | ✅ | ✅ 原生支持 | ❌ 需分步 |
| **防锁屏** | D-Bus Inhibit | IOKit | SetThreadExecutionState | idleTimerDisabled | FLAG_KEEP_SCREEN_ON |
| **专注模式检测** | ❌ 用户引导 | ❌ | ❌ | ❌ 用户引导 | NotificationManager |

**桌面录屏依赖**: ffmpeg 需在系统 PATH 中。Preflight 阶段检测不可用时向用户提示安装命令。

---

## 9. 实施步骤

### Phase 1: 核心基础设施 (本次已完成)
- [x] CaptureFeatureGate — 功能开关
- [x] PlatformCapture — 桌面截屏
- [x] CaptureService — 截屏会话管理
- [x] SettingsScreen 双击激活

### Phase 2: 编排引擎 (下一步)
- [ ] CaptureStateMachine — 状态机
- [ ] CaptureScenario — 步骤定义
- [ ] CaptureOrchestrator — 编排器骨架
- [ ] NavigationAdapter — Tab 切换
- [ ] AppState 集成: 暴露 orchestrator 给 QML

### Phase 3: 平台能力
- [ ] PlatformKeepAwake — 防锁屏 (桌面/iOS/Android)
- [ ] PlatformRecording — 录屏 (桌面 ffmpeg)
- [ ] ScrollController — 平滑滚动

### Phase 4: QML UI
- [ ] CaptureModePanel — 模式选择面板
- [ ] CapturePreflightOverlay — 倒计时 + 提示
- [x] CaptureRunningOverlay — 紧凑浮动状态条（已从全屏模态卡片重构为右上角 36px 透明浮动条，支持截图隐藏、录制模式自适应显隐）
- [ ] CaptureResultSummary — 完成摘要

### Phase 5: 集成和测试
- [ ] main.cpp: 注入 captureOrchestrator 到 QML 上下文
- [ ] AppContent.qml: 添加 CaptureOverlay 层级
- [ ] CMakeLists.txt: 添加所有新文件
- [ ] 真机测试 (iOS/Android)

---

## 10. 文件清单

### 新增 C++ 文件

```
src/EvidenceCapture/
├── CaptureOrchestrator.h          (~120 行) ⭐ 核心编排器
├── CaptureOrchestrator.cpp        (~300 行)
├── CaptureStateMachine.h          (~60 行)
├── CaptureStateMachine.cpp        (~100 行)
├── CaptureScenario.h              (~60 行)
├── CaptureScenario.cpp            (~30 行)
├── NavigationAdapter.h            (~40 行)
├── NavigationAdapter.cpp          (~80 行)
├── ScrollController.h             (~30 行)
├── ScrollController.cpp           (~80 行)

src/Common/Platform/
├── PlatformKeepAwake.h            (~15 行)
├── PlatformKeepAwake.cpp          (~40 行) 桌面 D-Bus 实现
├── PlatformRecording.h            (~20 行)
├── PlatformRecording.cpp          (~80 行) 桌面 ffmpeg 实现
```

### 新增 QML 文件

```
src/EvidenceCapture/View/
├── CaptureModePanel.qml           (~150 行) 模式选择面板
├── CapturePreflightOverlay.qml    (~120 行) 倒计时覆盖层
├── CaptureRunningOverlay.qml      (~310 行) 紧凑浮动状态条（重构后）
├── CaptureResultSummary.qml       (~130 行) 完成摘要面板
```

### 修改文件

```
src/app/AppState.h                 +5 行  (orchestrator 访问器)
src/app/AppState.cpp               +3 行  (orchestrator 构造)
src/main.cpp                       +2 行  (orchestrator context property)
src/Common/View/AppContent.qml     +5 行  (overlay loader component)
CMakeLists.txt                     +15 行 (新源文件)
```

### 估算代码量

| 层级 | 文件数 | 估算行数 |
|------|--------|----------|
| C++ 核心 | 10 | ~950 |
| QML View | 4 | ~500 |
| 修改现有 | 5 | ~30 |
| **总计** | **19** | **~1,480** |

---

## 附录 A: 关键 QML 集成代码示例

```qml
// AppContent.qml 新增 overlay loader (加在 StackView 同级, z-index 最高)
Loader {
    id: captureOverlayLoader
    anchors.fill: parent
    z: 2000
    active: false
    source: ""
}
```

```qml
// CaptureModePanel.qml 核心结构
Rectangle {
    anchors.fill: parent
    color: Qt.alpha(ThemeEngine.colors.surface, 0.85)
    z: 2000

    Rectangle {
        anchors.centerIn: parent
        width: 360; height: modeCol.implicitHeight + 48; radius: 20
        color: ThemeEngine.colors.card

        ColumnLayout {
            id: modeCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            Label { text: "🎬 Capture Mode"; font.pixelSize: 18; font.weight: Font.Bold }

            // Mode selector: screenshot / recording / both
            ColumnLayout {
                Repeater {
                    model: [
                        { icon: "📸", label: "Screenshot Only",  mode: 0 },
                        { icon: "🎥", label: "Recording Only",   mode: 1 },
                        { icon: "📸+🎥", label: "Both (Recommended)", mode: 2 }
                    ]
                    delegate: Rectangle { /* ... radio-button style ... */ }
                }
            }

            // URL input
            TextField {
                placeholderText: "https://example.com"
                text: appState.target
            }

            // Action buttons
            RowLayout {
                Rectangle { /* Cancel */ }
                Rectangle { /* Start Capture */ }
            }
        }
    }
}
```

## 附录 B: 图形化 UI 流程示意

```
                    ┌──────────────────────────────────┐
                    │         Settings > About          │
                    │  ┌──────┐                         │
                    │  │  📱  │  NetDiagnostics         │
                    │  │  🟢  │  Version 0.0.1          │  ← 双击图标激活
                    │  └──────┘                         │
                    └──────────────┬───────────────────┘
                                   │ 双击
                                   ▼
              ┌─────────────────────────────────────────┐
              │         🎬 Capture Mode                  │
              │                                         │
              │  ○ 📸 Screenshot Only                    │
              │  ○ 🎥 Recording Only                     │
              │  ● 📸+🎥 Both (Recommended)              │
              │                                         │
              │  URL: [ https://httpbin.org    ]         │
              │                                         │
              │  [ Cancel ]    [ ▶ Start Capture ]       │
              └──────────────────┬──────────────────────┘
                                 │ Start
                                 ▼
              ┌─────────────────────────────────────────┐
              │         ⚠️  Prepare to Capture           │
              │                                         │
              │    🙏 Please do not touch the device     │
              │    🔕 Enable Do Not Disturb              │
              │    💡 Screen will stay awake             │
              │    ⏱️  Estimated time: ~45 seconds       │
              │                                         │
              │               [ 3 ]                     │  ← 倒计时
              │                                         │
              │          [ Cancel Capture ]              │
              └──────────────────┬──────────────────────┘
                                 │ countdown = 0
                                 ▼
              ┌─────────────────────────────────────────┐
              │         🔴 Capturing...                  │
              │  ──────────────────────────────          │
              │  ✅ 01_dashboard                         │
              │  ✅ 02_diagnostic                        │
              │  ✅ 03_config                            │
              │  ✅ 04_settings                          │
              │  ⏳ 05_running_diagnostics...            │
              │  ⬜ 06_connectivity_detail               │
              │  ⬜ 07_dashboard_scroll                  │
              │  ⬜ 08_report                            │
              │  ──────────────────────────────          │
              │  📸 Captures: 4    ⏱️ Elapsed: 18s       │
              │                                         │
              │          [ ✕ Cancel Capture ]            │
              └──────────────────┬──────────────────────┘
                                 │ all steps complete
                                 ▼
              ┌─────────────────────────────────────────┐
              │         ✅ Capture Complete              │
              │                                         │
              │    📸 Screenshots:  8                    │
              │    🎥 Recording:    1 (00:42)            │
              │    📁 Path:                              │
              │    Evidence/AutoCapture/                 │
              │    20260724-150330_both/                 │
              │                                         │
              │    [ 📂 Open Folder ]    [ ✓ Done ]      │
              └─────────────────────────────────────────┘
```

---

> **文档结束**  
> 本设计文档覆盖了从 UX 交互、架构设计、组件职责、状态机、存储命名到平台适配的全部内容。  
> Agent 可依此逐节实施 Phase 2-5 的代码。
