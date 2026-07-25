# 截屏/录屏完整流程详解

> **版本**: 1.0 | **日期**: 2026-07-26 | **适用平台**: iOS / Android

---

## 目录

1. [架构总览](#1-架构总览)
2. [激活入口](#2-激活入口)
3. [模式选择面板](#3-模式选择面板-capturemodepanel)
4. [预检覆盖层](#4-预检覆盖层-capturepreflightoverlay)
5. [采集运行覆盖层](#5-采集运行覆盖层-capturerunningoverlay)
6. [结果摘要覆盖层](#6-结果摘要覆盖层-captureresultsummary)
7. [场景步骤详解](#7-场景步骤详解)
8. [iOS 平台实现](#8-ios-平台实现)
9. [Android 平台实现](#9-android-平台实现)
10. [存储与文件结构](#10-存储与文件结构)
11. [状态机完整转换表](#11-状态机完整转换表)

---

## 1. 架构总览

```
┌──────────────────────────────────────────────────────────────────┐
│                     QML Layer (用户界面)                          │
│                                                                  │
│  SettingsScreen.qml  ──→  双击 App 图标激活                       │
│  CaptureModePanel.qml ──→ 模式选择 (截图/录屏)                    │
│  CapturePreflightOverlay.qml ──→ 倒计时 + DND 提示                │
│  CaptureRunningOverlay.qml  ──→ 采集进度显示                      │
│  CaptureResultSummary.qml   ──→ 采集结果摘要                      │
│                                                                  │
│  AppContent.qml  ──→  Loader 统一管理覆盖层生命周期                │
├──────────────────────────────────────────────────────────────────┤
│                     C++ Core Layer                                │
│                                                                  │
│  CaptureOrchestrator     ──→ 总编排器，驱动状态机                  │
│  CaptureStateMachine     ──→ 11 状态 FSM                          │
│  CaptureScenario         ──→ 声明式步骤定义 (27 步)               │
│  NavigationAdapter       ──→ Tab 切换 + 页面就绪等待              │
│  ScrollController        ──→ Flickable 平滑滚动                   │
│                                                                  │
│  AppState                ──→ 外观层，暴露 captureOrchestrator      │
├──────────────────────────────────────────────────────────────────┤
│                     Platform Layer                                │
│                                                                  │
│  PlatformCapture   ──→ 截屏: iOS UIGraphicsImageRenderer          │
│                           Android View.drawToBitmap               │
│  PlatformRecording ──→ 录屏: iOS RPScreenRecorder + AVAssetWriter │
│                           Android MediaProjection + MediaRecorder │
│  PlatformFocus     ──→ DND/亮度/方向/防锁屏                        │
│  PlatformKeepAwake ──→ 屏幕常亮                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 2. 激活入口

### 2.1 触发路径

```
用户操作: Settings > About 页面
         ┌──────────┐
         │  📱 Logo  │ ← 双击
         └─────┬────┘
               │
    ┌──────────▼──────────┐
    │ onDoubleClicked()    │  SettingsScreen.qml:329
    │                      │
    │ if (captureFeature   │  首次双击: 启用功能 + 弹出面板
    │     Enabled == false)│  再次双击: 切换启用/禁用
    │   → enableCapture   │
    │   → requestModeSel  │
    └──────────┬──────────┘
               │
    ┌──────────▼──────────┐
    │ CaptureOrchestrator  │
    │ ::requestModeSelec   │
    │                      │
    │ → enableCaptureFeature│
    │ → emit modeSelection │
    │   Requested()        │
    └──────────┬──────────┘
               │
    ┌──────────▼──────────┐
    │ AppContent.qml       │
    │ Loader (captureOvly) │
    │ source = CaptureMode │
    │ Panel.qml            │
    └──────────────────────┘
```

### 2.2 关键代码位置

| 文件 | 行号 | 作用 |
|------|------|------|
| `SettingsScreen.qml` | 329 | `onDoubleClicked` 处理器 |
| `CaptureOrchestrator.cpp` | 243 | `requestModeSelection()` |
| `AppContent.qml` | 108-114 | `onModeSelectionRequested` → Loader |

---

## 3. 模式选择面板 (CaptureModePanel)

### 3.1 iOS 模式选择 (多选 checkbox)

```
┌─────────────────────────────────────┐
│         🎬 Capture Mode             │
│                                     │
│  Select capture options and enter   │
│  a diagnostic URL.                  │
│                                     │
│  ┌─────────────────────────────┐    │
│  │ [✓] 📸 Screenshots          │    │  ← 独立勾选
│  │     Capture screenshots     │    │
│  └─────────────────────────────┘    │
│                                     │
│  ┌─────────────────────────────┐    │
│  │ [✓] 🎥 Screen Recording     │    │  ← 独立勾选
│  │     Record video of flow    │    │
│  └─────────────────────────────┘    │
│                                     │
│  📸+🎥 Both modes enabled —         │  ← 两者都选时显示
│  recommended for complete evidence  │
│                                     │
│  Diagnostic URL:                    │
│  ┌─────────────────────────────┐    │
│  │ https://httpbin.org          │    │
│  └─────────────────────────────┘    │
│                                     │
│  [ Cancel ]    [ ▶ Start Capture ]  │
└─────────────────────────────────────┘

选择逻辑:
  ☐ ☐ → Start 按钮禁用 (无效选择)
  ☑ ☐ → mode=0 (ScreenshotOnly)
  ☐ ☑ → mode=1 (RecordingOnly)
  ☑ ☑ → mode=2 (Both)
```

### 3.2 Android 模式选择 (单选 radio)

```
┌─────────────────────────────────────┐
│         🎬 Capture Mode             │
│                                     │
│  Select capture options and enter   │
│  a diagnostic URL.                  │
│                                     │
│  ┌─────────────────────────────┐    │
│  │ [●] 📸 Screenshots          │    │  ← 点击自动取消 Recording
│  │     Capture screenshots     │    │
│  └─────────────────────────────┘    │
│                                     │
│  ┌─────────────────────────────┐    │
│  │ [○] 🎥 Screen Recording     │    │  ← 点击自动取消 Screenshots
│  │     Record video of flow    │    │
│  └─────────────────────────────┘    │
│                                     │
│  (无 Both 提示 — Android 不支持)     │
│                                     │
│  Diagnostic URL:                    │
│  ┌─────────────────────────────┐    │
│  │ https://httpbin.org          │    │
│  └─────────────────────────────┘    │
│                                     │
│  [ Cancel ]    [ ▶ Start Capture ]  │
└─────────────────────────────────────┘

选择逻辑:
  ☐ ☐ → Start 按钮禁用
  ☑ ☐ → mode=0 (ScreenshotOnly)
  ☐ ☑ → mode=1 (RecordingOnly)
  ☑ ☑ → 不可能 (互斥)
```

### 3.3 平台差异实现

```cpp
// CaptureOrchestrator.h
Q_PROPERTY(bool supportsBothModes READ supportsBothModes CONSTANT)

bool CaptureOrchestrator::supportsBothModes() const {
#if defined(PLATFORM_IOS)
    return true;   // ReplayKit 支持边录边截
#else
    return false;  // MediaProjection 不支持
#endif
}
```

```qml
// CaptureModePanel.qml — 截图点击 (Android 互斥)
onClicked: {
    if (!captureOrchestrator.supportsBothModes) {
        root.wantsScreenshot = !root.wantsScreenshot
        if (root.wantsScreenshot) root.wantsRecording = false  // 取消录屏
    } else {
        root.wantsScreenshot = !root.wantsScreenshot
    }
}
```

---

## 4. 预检覆盖层 (CapturePreflightOverlay)

### 4.1 iOS 专注模式手动设置流程

```
用户点击 "▶ Start Capture"
         │
         ▼
┌─────────────────────────────────────┐
│         ⚠️  Prepare to Capture       │
│                                     │
│  ┌─ Focus/DND 设置引导 ──────────┐  │
│  │ ⚠️ Focus / Do Not Disturb     │  │
│  │ must be enabled manually.     │  │
│  │                               │  │
│  │ 1. Tap → Settings → Focus    │  │
│  │ 2. Enable any Focus mode     │  │
│  │ 3. Return and tap I'm Ready  │  │
│  │                               │  │
│  │ [Open Settings] [✓ I'm Ready] │  │
│  └───────────────────────────────┘  │
│                                     │
│  • Screen will stay awake            │
│  • Estimated time: ~45 seconds       │
│                                     │
│  (倒计时隐藏，等待用户确认)            │
│                                     │
│  (Cancel 按钮隐藏)                    │
└─────────────────────────────────────┘
         │ 用户点击 "✓ I'm Ready"
         ▼
┌─────────────────────────────────────┐
│         ⚠️  Prepare to Capture       │
│                                     │
│  • Screen will stay awake            │
│  • Estimated time: ~45 seconds       │
│                                     │
│              [ 5 ]                   │  ← 5 秒倒计时开始
│                                     │
│         [ Cancel Capture ]           │
└─────────────────────────────────────┘
         │ 倒计时 → 0
         ▼
    进入自动采集流程
```

### 4.2 Android 预检流程

```
用户点击 "▶ Start Capture"
         │
         ▼
┌─────────────────────────────────────┐
│         ⚠️  Prepare to Capture       │
│                                     │
│  • Please do not touch the device    │
│  • Screen will stay awake            │
│  • Estimated time: ~45 seconds       │
│                                     │
│  (DND 提示仅在未授权时显示)           │
│                                     │
│              [ 5 ]                   │  ← 5 秒倒计时（DND 未授权时）
│              [ 3 ]                   │  ← 3 秒倒计时（DND 已授权时）
│                                     │
│         [ Cancel Capture ]           │
└─────────────────────────────────────┘
         │ 倒计时 → 0
         ▼
    进入自动采集流程
```

### 4.3 倒计时完成后的系统状态配置

```
onStateChanged(CountdownToStart):
  ┌──────────────────────────────┐
  │ 1. platformSetKeepAwake(true) │  ← 禁止屏幕休眠
  │ 2. platformEnableFocusMode()  │  ← 启用 DND (Android) / 静音 (iOS)
  │ 3. platformSetMaxBrightness() │  ← 亮度设为最大
  │ 4. platformLockOrientation()  │  ← 锁定屏幕方向
  │ 5. 启动 10 秒安全定时器       │  ← QML 覆盖层加载失败时的兜底
  │ 6. emit stateChanged()        │  ← 通知 QML 加载运行覆盖层
  └──────────────────────────────┘
```

---

## 5. 采集运行覆盖层 (CaptureRunningOverlay)

```
┌─────────────────────────────────────┐
│  🔴 Capturing...                    │
│  ───────────────────────────────    │
│                                     │
│  Dashboard          ████████░░ 2/8  │  ← 进度条
│                                     │
│  📸 Screenshots: 3                  │  ← 实时计数
│  ⏱ Elapsed: 18s                    │  ← 每秒更新
│                                     │
│         [ ✕ Cancel Capture ]        │
└─────────────────────────────────────┘
```

触发时机：`onStateChanged(ExecutingSteps)` → Loader 加载此覆盖层
更新机制：
- `stepChanged(current, total)` → 进度条和步数
- `captureCountChanged(count)` → 截图计数
- `Timer(1000ms)` 轮询 `elapsedSeconds` → 耗时显示

---

## 6. 结果摘要覆盖层 (CaptureResultSummary)

```
采集完成时:                      采集失败时:
┌─────────────────────┐         ┌─────────────────────┐
│   ✅ Capture Complete│         │   ❌ Capture Failed  │
│                     │         │                     │
│  📸 Screenshots: 8  │         │  NO_FFMPEG          │
│  🎥 Recording: ✓    │         │  ffmpeg is required │
│  ⏱ Duration: 42s    │         │                     │
│                     │         │                     │
│  📁 /path/to/session │         │  [ ✕ Dismiss ]      │
│                     │         └─────────────────────┘
│  ⚠ DND reminder     │
│                     │
│  [ ✓ Done ]         │
└─────────────────────┘
```

---

## 7. 场景步骤详解

### 7.1 完整步骤序列 (27 步)

```
Phase 1: 所有 Tab 页面截图 (步骤 0-11)
────────────────────────────────────────────────────
 0 │ Navigate    │ tab 0 (Dashboard)    │ (无 captureBefore)
 1 │ WaitReady   │ 3000ms               │
 2 │ Capture     │ "Dashboard_Main"     │ 截图: 001_Dashboard_Main.png
───┼─────────────┼──────────────────────┼─────────────────────────
 3 │ Navigate    │ tab 1 (Diagnostics)  │ (无 captureBefore)
 4 │ WaitReady   │ 3000ms               │
 5 │ Capture     │ "Diagnostic"         │ 截图: 002_Diagnostic.png
───┼─────────────┼──────────────────────┼─────────────────────────
 6 │ Navigate    │ tab 2 (Config)       │
 7 │ WaitReady   │ 3000ms               │
 8 │ Capture     │ "Config"             │ 截图: 003_Config.png
───┼─────────────┼──────────────────────┼─────────────────────────
 9 │ Navigate    │ tab 3 (Settings)     │
10 │ WaitReady   │ 3000ms               │
11 │ Capture     │ "Settings"           │ 截图: 004_Settings.png

Phase 2: 诊断自动化 (步骤 12-17)
────────────────────────────────────────────────────
12 │ Navigate    │ tab 1 (Diagnostics)  │
13 │ SetUrl      │ diagUrl              │ 设置测试 URL
14 │ Capture     │ "Diagnostic_Input"   │ 截图: 005_Diagnostic_Input.png
15 │ RunDiag     │ (触发诊断)            │
16 │ WaitDiag    │ 120000ms (超时)      │ 轮询 runStatusInt() != 1
17 │ Capture     │ "Diagnostic_Result"  │ 截图: 006_Diagnostic_Result.png

Phase 3: InternetConnectivity 详情 (步骤 18-21)
────────────────────────────────────────────────────
18 │ OpenDetail  │ DiagId::G3InternetConnectivity │
19 │ WaitReady   │ 2000ms                          │
20 │ Capture     │ "InternetConnectivity_Top"      │ 截图: 007_...Top.png
21 │ Scroll      │ 3000ms (recordingOnly)          │ 录屏时滚动
22 │ Capture     │ "InternetConnectivity_Bottom"   │ 截图: 008_...Bottom.png
   │             │ (recordingOnly)                 │ 仅在录屏模式

Phase 4: Dashboard 回访 (步骤 23-27)
────────────────────────────────────────────────────
23 │ Navigate    │ tab 0 (Dashboard)               │
24 │ WaitReady   │ 3000ms                          │
25 │ Capture     │ "Dashboard_AfterDiagnostic"     │
26 │ Scroll      │ 5000ms (recordingOnly)          │ 录屏时滚动
27 │ Capture     │ "Dashboard_End" (recordingOnly) │ 仅在录屏模式

Phase 5: Report 预览 (步骤 28-30)
────────────────────────────────────────────────────
28 │ OpenReport  │ (500ms 延迟后检查 StackView)     │
29 │ WaitReady   │ 3000ms                          │
30 │ Capture     │ "Report_Summary"                │
```

### 7.2 步骤过滤逻辑

```cpp
// startCapture() 中
m_filteredScenario.clear();
for (const auto& step : scenario.steps()) {
    if (step.recordingOnly && !m_recording) continue;  // 截图模式跳过录制专属步骤
    m_filteredScenario.addStep(step);
}
```

**ScreenshotOnly 模式**: 跳过 3 个 `recordingOnly=true` 的步骤（21, 26, 27）→ 执行 24 步
**RecordingOnly 模式**: 跳过截图操作 (`if (!m_doScreenshot)` 守卫) → 只执行导航和滚动
**Both 模式**: 执行所有 27 步，包括截图和录制专属步骤

---

## 8. iOS 平台实现

### 8.1 截屏: UIGraphicsImageRenderer

```
platformCaptureScreenshot(filePath)
         │
         ▼
┌─────────────────────────────────────┐
│ 1. QDir().mkpath(parentDir)         │  ← 确保目录存在
│ 2. 检测当前线程                     │
│    ├─ 主线程 → 直接执行             │
│    └─ 后台 → dispatch_async(main)   │
│         + dispatch_semaphore(30s)   │
└──────────────┬──────────────────────┘
               ▼
┌─────────────────────────────────────┐
│ captureKeyWindowImage()             │
│                                     │
│ 1. 遍历 UIApplication.windows       │
│    找到 keyWindow                   │
│ 2. 创建 UIGraphicsImageRenderer     │
│    scale = mainScreen.scale (2x/3x) │
│ 3. drawViewHierarchyInRect:         │
│    afterScreenUpdates:YES           │
│    → 捕获完整渲染输出 (含模糊/毛玻璃) │
│ 4. 返回 UIImage                     │
└──────────────┬──────────────────────┘
               ▼
┌─────────────────────────────────────┐
│ UIImagePNGRepresentation → NSData   │
│ [pngData writeToFile:atomically:]   │
│ → 返回 true/false                   │
└─────────────────────────────────────┘
```

### 8.2 录屏: RPScreenRecorder + AVAssetWriter

```
platformStartRecording(filePath, callback)
         │
         ▼
┌─────────────────────────────────────┐
│ 1. s_stopping = false (重置)        │
│ 2. 清除 s_lastError                 │
│ 3. 确保输出目录存在                  │
│ 4. 检查 RPScreenRecorder.available  │
└──────────────┬──────────────────────┘
               ▼
┌─────────────────────────────────────┐
│ AVAssetWriter 配置                  │
│                                     │
│ • fileType: AVFileTypeMPEG4         │
│ • codec: H.264 (High Auto Level)    │
│ • bitrate: 3 Mbps                  │
│ • resolution: screenSize × scale   │
│ • realtime: YES                     │
└──────────────┬──────────────────────┘
               ▼
┌─────────────────────────────────────┐
│ [s_writer startWriting]             │
│                                     │
│ 定义 cleanupAfterError 块:          │
│   1. dispatch_sync(stateQueue):     │
│      - 提取 s_startCb               │
│      - 清除 writer/input/outputPath │
│   2. dispatch_async(main): 回调     │
└──────────────┬──────────────────────┘
               ▼
┌─────────────────────────────────────┐
│ [s_recorder startCaptureWithHandler │
│  :^(CMSampleBuffer, type, error) {  │
│                                     │
│  ┌─ 错误 → cleanupAfterError ──────┤
│  │                                   │
│  ├─ type != Video → return          │
│  │                                   │
│  ├─ s_stopping → return (防止启动)   │
│  │                                   │
│  ├─ 首帧: startSessionAtSourceTime  │
│  │   s_recording = true             │
│  │   回调 s_startCb(true, path)     │  ← 通知 Orchestrator
│  │                                   │
│  └─ 后续帧: appendSampleBuffer      │
│  }                                   │
│  completionHandler:                  │
│    error → cleanupAfterError         │
└─────────────────────────────────────┘
```

### 8.3 录屏停止流程

```
platformStopRecording(callback)
         │
         ├─ !s_recording || s_stopping?
         │   ├─ YES → s_stopping = true (防 TOCTOU)
         │   │        清除 s_startCb (防回调泄漏)
         │   │        投递 s_lastError (如存在)
         │   │        return
         │   │
         │   └─ NO → s_stopping = true
         │            [s_recorder stopCaptureWithHandler:]
         │              │
         │              ├─ error → 回调(false, error)
         │              │
         │              ├─ tornDown? (s_input/s_writer 已被并发错误清理)
         │              │   → 回调(false, s_lastError)
         │              │
         │              └─ 正常:
         │                  [s_input markAsFinished]
         │                  [s_writer finishWritingWithCompletionHandler:]
         │                    │
         │                    ├─ outPath? → 回调(true/ok, pathOrError)
         │                    └─ !outPath? → 回调(false, "lost")
```

### 8.4 线程安全: s_stateQueue

iOS 录屏模块使用专用串行派发队列保护 ObjC 对象指针：

```
状态变量              │ 读写线程                      │ 保护机制
─────────────────────┼──────────────────────────────┼─────────────
s_recording          │ 主线程 + ReplayKit 队列       │ std::atomic<bool>
s_stopping           │ 主线程 + ReplayKit 队列       │ std::atomic<bool>
s_writer, s_input    │ 主线程写入, ReplayKit 队列读写 │ s_stateQueue
s_lastError          │ ReplayKit 队列写入, 主线程读取 │ s_stateQueue
s_startCb            │ 主线程写入, ReplayKit 读写    │ s_stateQueue
s_outputPath         │ 主线程写入, 多队列读取         │ s_stateQueue

帧处理热路径 (30/60fps): 直接读取 s_recording(atomic) + s_input(稳定指针)
                         — 不经过 s_stateQueue (零锁开销)
```

### 8.5 方向锁定: UIDevice KVO

```
platformLockOrientation()
         │
         ▼
┌─────────────────────────────────────┐
│ 1. beginGeneratingDeviceOrientation │
│    Notifications                    │
│ 2. 读取 currentDevice.orientation   │
│    ├─ Known → 使用                  │
│    └─ Unknown → 回退:               │
│       iOS 13+: windowScene          │
│         .interfaceOrientation       │
│       iOS 12: statusBarOrientation  │
│ 3. 保存到 s_savedOrientation        │
│ 4. setValue:@(orientation)          │
│    forKey:@"orientation" (KVO)      │
└─────────────────────────────────────┘

platformUnlockOrientation()
         │
         ▼
┌─────────────────────────────────────┐
│ 1. setValue:@(Unknown) forKey:      │
│    @"orientation"                   │
│ 2. 重置 s_savedOrientation = -1     │
│ 3. endGeneratingDeviceOrientation   │
│    Notifications (async)            │
└─────────────────────────────────────┘
```

### 8.6 专注模式: AVAudioSession

```
platformEnableFocusMode()
         │
         ▼
┌─────────────────────────────────────┐
│ [AVAudioSession setActive:NO        │
│  error:&err]                        │
│                                     │
│ iOS 不提供程序化 DND API             │
│ → 仅静音应用音频                     │
│ → 始终返回 true                      │
│ → 用户必须通过预检覆盖层手动设置      │
└─────────────────────────────────────┘
```

---

## 9. Android 平台实现

### 9.1 截屏: View.drawToBitmap

```
platformCaptureScreenshot(filePath)
         │
         ▼
┌─────────────────────────────────────┐
│ 1. 获取 Activity                    │
│    QtNative::activity()             │
│ 2. activity.getWindow().getDecorView│
│ 3. view.drawToBitmap(bitmap)        │
│ 4. bitmap.compress(PNG, 100, stream)│
│ 5. 写入文件                          │
│ → 返回 true/false                   │
└─────────────────────────────────────┘
```

### 9.2 录屏: MediaProjection + MediaRecorder

```
platformStartRecording(filePath, callback)
         │
         ▼
┌─────────────────────────────────────┐
│ 1. 检查 s_recording                 │
│ 2. 获取 MediaProjectionManager      │
│ 3. createScreenCaptureIntent()      │
│ 4. QtAndroidPrivate::startActivity  │
│    → 系统权限对话框                  │
│ 5. 用户授权后 → setupRecorder()      │
└──────────────┬──────────────────────┘
               ▼
┌─────────────────────────────────────┐
│ setupRecorder()                     │
│                                     │
│ 1. MediaRecorder 配置:              │
│    • audioSource: MIC              │
│    • videoSource: SURFACE          │
│    • outputFormat: MPEG_4          │
│    • videoEncoder: H264            │
│    • audioEncoder: AAC             │
│    • videoSize: 1280x720           │
│    • frameRate: 30                 │
│    • bitRate: 3Mbps                │
│                                     │
│ 2. setOutputFile(path)              │
│ 3. prepare() → start()             │
│ 4. getSurface()                     │
│ 5. createVirtualDisplay()           │
│    (FLAG_AUTO_MIRROR)               │
│                                     │
│ 6. s_recording = true               │
│ 7. 回调 s_startCb(true, path)      │
└─────────────────────────────────────┘
```

### 9.3 平台函数实现

```
DND 控制:
  platformEnableFocusMode()  → NotificationManager.setInterruptionFilter(PRIORITY)
  platformDisableFocusMode() → 恢复 s_originalFilter
  需要 ACCESS_NOTIFICATION_POLICY 权限

亮度控制:
  platformSetMaxBrightness() → WindowManager.LayoutParams.screenBrightness = 1.0f
  platformRestoreBrightness() → 恢复 s_savedBrightness
  JNI 失败时保留保存的值以支持重试

方向锁定:
  platformLockOrientation()   → setRequestedOrientation(SCREEN_ORIENTATION_LOCKED=14)
  platformUnlockOrientation() → setRequestedOrientation(SCREEN_ORIENTATION_USER=2)
  JNI 异常检测 (checkAndClearExceptions)
```

---

## 10. 存储与文件结构

### 10.1 目录结构

```
<captureBasePath()>/NetDiagnostics/Capture/
│
├── 20260726_143052_789/          ← Session ID (时间戳)
│   ├── Screenshots/
│   │   ├── 001_Dashboard_Main.png
│   │   ├── 002_Diagnostic.png
│   │   ├── 003_Config.png
│   │   ├── 004_Settings.png
│   │   ├── 005_Diagnostic_Input.png
│   │   ├── 006_Diagnostic_Result.png
│   │   ├── 007_InternetConnectivity_Top.png
│   │   ├── 008_InternetConnectivity_Bottom.png
│   │   ├── 009_Dashboard_AfterDiagnostic.png
│   │   ├── 010_Dashboard_End.png
│   │   └── 011_Report_Summary.png
│   ├── Videos/
│   │   └── Capture_AutoDemo.mp4   ← 录屏文件
│   ├── Logs/
│   │   └── execution.log          ← 审计日志
│   └── Metadata/
│       └── manifest.json          ← 会话清单
│
├── 20260726_150445_789/          ← 下一次采集
│   └── ...
```

### 10.2 iOS vs Android 路径差异

| 平台 | captureBasePath() | 完整路径 |
|------|-------------------|---------|
| iOS | `DocumentsLocation` | `Documents/NetDiagnostics/Capture/{ts}/` |
| Android | `AppDataLocation` | `AppData/NetDiagnostics/Capture/{ts}/` |

### 10.3 manifest.json 格式

```json
{
  "session_id": "20260726_143052_789",
  "capture_mode": "Video+Screenshot",
  "diag_url": "https://httpbin.org",
  "started_at": "2026-07-26T14:30:52",
  "status": "running",
  "device": "iPhone15",
  "os": "ios 26.5",
  "captures": [
    {"seq": "001", "description": "Dashboard_Main",
     "file": "001_Dashboard_Main.png", "size": 245760,
     "timestamp": "2026-07-26T14:30:55"},
    {"seq": "002", "description": "Diagnostic",
     "file": "002_Diagnostic.png", "size": 312400,
     "timestamp": "2026-07-26T14:30:58"}
  ],
  "completed_at": "2026-07-26T14:31:35",
  "status": "completed",
  "total_captures": 11,
  "duration_s": 43
}
```

### 10.4 execution.log 格式

```
=== Automated Capture Session ===
Session:  20260726_143052_789
Mode:     Video+Screenshot
URL:      https://httpbin.org
Started:  2026-07-26T14:30:52

[14:30:55] Step 1/27  Navigate       → Dashboard              | no captureBefore
[14:30:55] Step 2/27  WaitPageReady  →                        | settle 3000ms
[14:30:58] Step 3/27  Capture        → Dashboard_Main          | screenshot ✓
[14:30:58] Step 4/27  Navigate       → Diagnostics            | no captureBefore
[14:30:59] Step 5/27  WaitPageReady  →                        | settle 3000ms
...
[14:31:32] Step 30/27 Capture        → Report_Summary         | screenshot ✓

--- Session complete ---
Total captures: 11
Ended: 2026-07-26T14:31:35
```

---

## 11. 状态机完整转换表

### 11.1 状态转换矩阵

```
from \ to         Idle Pre  CntD Ses  StR  Exec StR  Fin  Comp Fail Canc
Idle               -    ✓    -    -    -    -    -    -    -    -    -
Preflight          -    -    ✓    -    -    -    -    -    -    ✓    ✓
CountdownToStart   -    -    -    ✓    -    -    -    -    -    ✓    ✓
CreatingSession    -    -    -    -    ✓    ✓    -    -    -    ✓    ✓
StartingRecording  -    -    -    -    -    ✓    -    -    -    ✓    ✓
ExecutingSteps     -    -    -    -    -    -    ✓    ✓    -    ✓    ✓
StoppingRecording  -    -    -    -    -    -    -    ✓    -    ✓    ✓
Finalizing         -    -    -    -    -    -    -    -    ✓    ✓    ✓
Completed          ✓    -    -    -    -    -    -    -    -    -    -
Failed             ✓    -    -    -    -    -    -    -    -    -    -
Cancelled          ✓    -    -    -    -    -    -    -    -    -    -
```

### 11.2 完整采集流程 (Both 模式)

```
    Idle
     │ startCapture(Both, url)
     ▼
  Preflight
     │ 检查磁盘空间 (>100MB)
     ▼
  CountdownToStart
     │ QML 倒计时 (iOS: Focus确认 → 5s, Android: 3-5s)
     │ 配置: keepAwake + DND + 亮度 + 方向锁定
     ▼
  CreatingSession
     │ 创建目录 + 写入 manifest + execution.log
     ▼
  StartingRecording
     │ iOS: RPScreenRecorder.startCapture
     │ Android: MediaProjection 权限对话框
     │ 回调 → ExecutingSteps
     ▼
  ExecutingSteps ──────────────────────────────────┐
     │ 遍历 27 个场景步骤                           │
     │ ├─ Navigate → tab 页面切换                   │
     │ ├─ WaitPageReady → 等待渲染 (3s)             │
     │ ├─ Capture → 截图 + manifest 追加            │
     │ ├─ SetUrl → 填入诊断 URL                     │
     │ ├─ RunDiagnostic → 触发诊断运行              │
     │ ├─ WaitDiagComplete → 轮询 (120s 超时)       │
     │ ├─ OpenDetail → 打开诊断详情                 │
     │ ├─ Scroll → Flickable 滚动 (录制模式)        │
     │ └─ OpenReport → 打开 Report 预览             │
     │                                              │
     │ 全部步骤完成                                  │
     ▼                                              │
  StoppingRecording                                 │
     │ iOS: RPScreenRecorder.stopCapture             │
     │ Android: MediaRecorder.stop + release         │
     │ 回调 → Finalizing                            │
     ▼                                              │
  Finalizing                                        │
     │ 更新 manifest (completed_at, status)          │
     │ 追加 execution.log 尾部                       │
     ▼                                              │
  Completed                                         │
     │ restoreSystemState()                          │
     │ emit captureCompleted(sessionPath)            │
     ▼                                              │
    Idle (reset)                                    │

  任意运行状态 ──→ Cancelled (用户取消)
                    │ restoreSystemState()
                    │ emit captureCancelled()

  任意运行状态 ──→ Failed (错误)
                    │ restoreSystemState()
                    │ emit captureFailed(code, msg)
```

### 11.3 系统状态恢复 (restoreSystemState)

每次采集结束（完成/失败/取消/析构）都会执行：

```
restoreSystemState()
     │
     ├─ 1. platformSetKeepAwake(false)     恢复屏幕休眠
     ├─ 2. platformRestoreBrightness()     恢复原始亮度
     ├─ 3. platformUnlockOrientation()     恢复自动旋转
     ├─ 4. platformDisableFocusMode()      恢复通知 (Android) / 恢复音频 (iOS)
     ├─ 5. emit needsFocusModeSetupChanged 通知 QML
     └─ 6. 安全网: if (m_recording)
              → platformStopRecording(nullptr)  强制停止录制
            m_recording = false
```

---

> **文档结束** — 本指南覆盖了从用户双击激活到采集完成的完整代码逻辑，包含 iOS 和 Android 两个平台的详细平台实现。
