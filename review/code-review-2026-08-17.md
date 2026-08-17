# 代码审查与修复报告（2026-08-17，两轮多角色审查）

> 生成方式：10 个独立审查代理（第一轮）+ 6 个独立审查代理（第二轮），
> 独立验证去重后实施修复；全部 27 项 pre-commit 检查通过，qmllint 15/15。
> 角色视角：资深 UI/UX、资深 C++/Qt 程序员、资深架构师、资深产品经理。

## 一、审查规模

| 轮次 | 代理数 | 视角 | 发现 | 采纳修复 | 驳回 |
|---|---|---|---|---|---|
| 第一轮 | 10 | 逐行扫描/语言陷阱/跨文件追踪/删除行为/包装层/复用/简化/效率/分层/约定 | 30 | 20 项去重结论 | 1 项（"19 个死角色"——实际 18/19 被 ReportEngine.cpp 使用，仅 `shadow` 真死） |
| 第二轮 | 6 | 修复验证/UI+UX/C++/架构/产品/QML 运行时安全 | 51 | 见下 | 若干降级为建议 |

## 二、第一轮：主题/调色板/图标子系统

### 关键缺陷（5WHY 根因 → 修复）

1. **禁用瓦片图标 NaN**（[DiagBlock.qml](src/Common/View/widgets/DiagBlock.qml)）
   - 现象：`Qt.rgba(colors.primary.r, .g, .b, 0.35)` 取 JS 字符串的 `.r` → undefined → NaN → 黑色半透明图标。
   - 5WHY：`ThemeEngine.colors` 是 var 持 JS 对象；QML 仅在类型边界（属性赋值/Qt.alpha）做 string→color 转换，子属性访问不转换。
   - 修复：`Qt.alpha(ThemeEngine.colors.primary, 0.35)`。

2. **详情页 45 图标全彩头部从未激活**（[AppState.cpp](src/app/AppState.cpp)）
   - 现象：`resultFor()` 不产 `iconName`，DetailPage/PageDetailSheet/PageHeroSection 三个回退分支永远命中。
   - 5WHY：只有 `itemFor()` 注入该键；头部/hero 由 `resultFor` 数据驱动；新功能没有跨层键契约检查。
   - 修复：`resultFor()` 补 `iconName = diagnosticMeta(it->id).iconName`。

3. **浅色 geoip 图标泄漏 `#B00001` 哨兵色**（[generate-colored-icons.py](scripts/generate-colored-icons.py)）
   - 现象：`resources/icons/10b981/nd-diag-g3-geoip.svg` 含 4 处 `#B00001` 暗红字面量。
   - 5WHY：双固定色表改造后 `FIXED_COLORS_LIGHT.get(stem, [])` 返回空表 → 哨兵不被替换 → 哨兵是合法 hex，无校验能拦。
   - 修复：浅色表缺失时回退 dark 表；新增"未替换 #B0000n 哨兵"生成期警告。

4. **终端瓦片光晕垫隐形**（[IconTints.js](src/Common/View/widgets/IconTints.js) 生成侧）
   - 现象：ssh/ftp/telnet tint 为 `#1E293B`（对同色卡片 1.0:1）。
   - 5WHY：0.15 亮度门使用无 gamma 校正的线性亮度（#1E293B=0.157 侥幸过关），且同轮把终端屏色从 #0F172A 改成 #1E293B。
   - 修复：改用 WCAG gamma 校正亮度（共享自对比度审计；#1E293B≈0.022 → 回退 DIAG_ACCENT：#4ADE80/#F59E0B/#60C8F8）。

5. **13 个 DB/HTTP 瓦片光晕垫隐形（第二轮追加）**
   - 现象：`#007CC9`（L=0.186 过关）0.22 边框合成后对卡片仅 1.26:1。
   - 5WHY：门限只为近黑屏校准，未验证"合成后对比度"。
   - 修复：合成对比度门（tint@0.22 对 surfaceContainerLow 两主题 ≥1.3:1），13 瓦片回退 DIAG_ACCENT（amber/indigo/sky/green）。

6. **e0f2fe 22 个图标与生成器矛盾**（预热 stale）
   - 现象：M3 重命名把 `#E0F2FE` 从仅 Light 变为 Dark.onPrimaryContainer + Light.primaryContainer 共享 → 双套启发式重分类。
   - 5WHY：staleness 检查只是 staging 模式启发式，从未做过内容比对。
   - 修复：生成器新增 `--check`（镜像树重建 + 字节比对，~1.4s）；pre-commit 检查 19 无条件执行；共享 hex 生成期 NOTE 提示。

7. **pre-commit 检查 25 阻断全新克隆**
   - 5WHY：`--check` 对 gitignored 且从不提交的报告硬失败 → 任何新克隆任何提交都被拦。
   - 修复：报告缺失时警告跳过；存在但过期仍失败。

8. **info.svg 被清扫删除但仍被引用**（[PremiumCard.qml](src/Common/View/widgets/PremiumCard.qml)、[SettingsScreen.qml](src/Settings/View/SettingsScreen.qml)）
   - 修复：从 HEAD 恢复 master + 全变体重烘焙（37 个 qrc 条目）。

### 架构性修复（去镜像/单一事实源）

- 新增 [palette_common.py](scripts/palette_common.py)：统一 Palette.js 解析 + WCAG 数学（原 3 份正则解析器 + 1 份弱化亮度公式）。
- [VerifyPaletteSync.cmake](cmake/VerifyPaletteSync.cmake) 重写：78 对手写表 → 单遍派生（80 令牌/configure，O(n)），加完整性断言（块检测锚点失效 → FATAL）。**并修复其从未被 CMakeLists 包含的问题**（1acc2d62 曾移除 include）。
- [generate-appcolors.py](scripts/generate-appcolors.py)：12 个 `APPC_*_RGB` 宏从手写字面量改为派生；ROLES↔Palette.js 双向校验（新角色漏 ROLES → 点名报错；删角色 → 点名报错而非 KeyError）；新增 `terminalText` 角色。
- DIAG_ACCENT 表中 ~20 个镜像调色板角色的条目改为 `"Theme.role"` 引用（生成期解析；其余为真实美术规范保留字面量）。
- [audit-palette-contrast.py](scripts/audit-palette-contrast.py)：5WHY 偏差文字中的比率改为生成时计算（对纯白 #FFFFFF，与"白底"表述一致）；PAIRS 引用缺失角色 → 警告 + `--check` 硬失败。
- [Palette.js](src/Common/View/theme/Palette.js)：`shadow`（唯一真死角色）移除；`iconPadAlpha/iconPadBorderAlpha/groupHues` Light 块改为引用 Dark 单一来源；新增 `terminalText`（暗 #4ADE80/亮 #047857——亮色终端文字实测 5.01:1 AA）。
- 新 pre-commit 检查 27：QML `colors.<token>`/`Palette.<token>` 引用与 Palette.js 键名比对（qmllint 无法校验动态 JS 对象成员——角色重命名即崩溃类，此前无任何方向检查）。

## 三、第二轮：C++/Qt 正确性（资深 C++/Qt 视角）

| 位置 | 缺陷 | 5WHY 根因摘要 | 修复 |
|---|---|---|---|
| [main.cpp](src/main.cpp):175 | `qInstallMessageHandler` 静默替换 CrashHandler 的处理器 | CrashHandler 已装 qtMessageHandler 且自带 stderr 回显；lambda 不链式调用 | 删除冗余块（qFatal 根因文本重新可入 crash log） |
| [GeoProbe.cpp](src/Diagnostics/Model/GeoProbe.cpp):139 | 退出时 UAF：15s 等停超时后仍 delete 运行中的 QThread 与数据库 | 批次可达 ~33s > 15s；泄漏线程仍写 m_db | `requestStop()` 返回 bool；未停则三者一并泄漏（换取无 UAF） |
| [G4/Adapters.cpp](src/Diagnostics/Model/G4/Adapters.cpp):789 | traceroute 每跳同步无界反查 DNS（30-120s）拖死池线程与主线程 | 违反本文件 H4 规则；看门狗判超时后 worker 仍卡在 getaddrinfo | 局部事件循环 + 5s 截止 |
| [DiagnosticSuite.cpp](src/Diagnostics/Model/DiagnosticSuite.cpp):59 | start 循环中同步 finished→removeOne 迭代器失效 | 瞬时完成探针经 setFuture 直接回调 | 迭代本地拷贝 |
| [ProbeExecutor.cpp](src/Diagnostics/Model/ProbeExecutor.cpp)/[DnsResolver.cpp](src/Common/Services/DnsResolver.cpp) | `std::thread` 创建无 try/catch（EAGAIN → terminate） | 低 RLIMIT_NPROC/fd 耗尽 | try/catch + 降级跳过/解析失败 |
| [G5/Adapters.cpp](src/Diagnostics/Model/G5/Adapters.cpp):119 | `scheme.endsWith('s')` 把 sftp 当 TLS | 后缀启发式混淆协议族 | 隐式 TLS 白名单（ftps/smtps/imaps/pop3s/mqtts/ldaps） |
| [AppState.cpp](src/app/AppState.cpp):291 | IPv6 字面量目标显式端口被静默丢弃 | `contains(':')` 把 IPv6 冒号误判为已有端口 | 括号化 + 端口追加 |
| [AppState.cpp](src/app/AppState.cpp):426 | `groupStats().durationMs` 无组过滤——Layer Timings 五行同值 | 时长循环未与计数循环同源过滤 | `r.group != g` 过滤（产品级准确计时承诺） |
| [GCommon.cpp](src/Diagnostics/Model/GCommon.cpp):334 | worker 线程无事件循环下 `deleteLater()` 永不处理 → 每超时泄漏一个 QNetworkReply | DeferredDelete 需要持久事件循环 | 局部循环退出后直接 `delete reply` |
| [GatewayDhcpRouting.mm](src/Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.mm) | "EV-iO"/"EiGE"/"��" 文本污染 | 历史 d→i 全局替换残留 | 恢复 EV-DO/EDGE/— |
| [main.cpp](src/main.cpp):220 | NativePdfDocument 从未注册到 QML 引擎 | 注册在重构中被丢失 | `qmlRegisterType`（IOS/APPLE 构建） |

## 四、第二轮：UI/UX 视角

| 位置 | 缺陷 | 修复 |
|---|---|---|
| [PageDetailHeaderSection.qml](src/Common/View/sections/PageDetailHeaderSection.qml) | 返回/复制命中区仅 ~20px 图标、无键盘可达（44px 最低触控 + Tab 均失败） | 44×56 命中区 + `focusPolicy` + Enter/Space |
| 详情页 | 同一诊断图标在头部(18px)+hero 垫(40px) 双渲染（spec §2.1/§2.2 无头部图标）；缺失时回退徽标与状态圆盘同图形异色冲突 | 头部去图标；hero 垫仅真实图标时可见（`_hasDiagIcon`） |
| [IconPad.qml](src/Common/View/widgets/IconPad.qml) | hero 用 `radius: 16` 覆盖组件公式，破坏"光晕规格只在组件内"契约 | 删除覆盖（15.7px 视觉无差） |
| [PremiumCard.qml](src/Common/View/widgets/PremiumCard.qml) | 解锁态绿字在 success@0.16 浅底仅 2.19:1（AA 失败） | 用 `terminalText`（亮=深翡翠 ≈4.7:1，暗=success） |
| [TerminalBlock.qml](src/Common/View/detail/TerminalBlock.qml) | 长行（TLS 证书/curl verbose）在手机上永久截断无横向滚动；阿拉伯语下终端右对齐 | contentWidth=最长行（TextMetrics）+ AutoFlickIfNeeded；LayoutMirroring 关 + AlignLeft（LTR-forever 内容） |
| [PageOverlaySection.qml](src/Common/View/sections/PageOverlaySection.qml) | 遮罩点击 `visible=false` 命令式赋值**永久删除调用方绑定**——二次打开不可见 + 导航被 overlayVisible 卡死 | `closeRequested()` 信号；DiagnosticScreen/DashboardScreen 改页面状态 |
| [AppContent.qml](src/Common/View/AppContent.qml) | 报告预览浮层无 close 钩子 → dock 点击被吞 | `previewVisible` 兜底分支 |
| [PremiumDialog.qml](src/Common/View/dialogs/PremiumDialog.qml) | RTL 缺镜像；关闭用旋转 90° chevron | LayoutMirroring + 标准 `close` 字形 + 44px 命中区 |
| [DiagBlock.qml](src/Common/View/widgets/DiagBlock.qml) | 45 瓦片仅 3 个显示光标却全部常跑无限闪烁动画（移动端空耗） | `running: _isTerminalIcon && visible` |
| [ThemeEngine.qml](src/Common/View/theme/ThemeEngine.qml) | `Qt.application` 静态绑定违反项目 D.1 规则 | 安全默认值 + onCompleted 空检赋值 |
| [ResultChart.qml](src/Common/View/detail/viz/ResultChart.qml) | 图表 Loader URL 指向已不存在的 `qrc:/qml/...`——**所有详情页图表从未渲染**（静默空白卡） | 改 `qrc:/qt/qml/...` + Loader.Error 日志（F.1 规则） |

## 五、第二轮：架构师视角（工具链完整性）

| 缺陷 | 修复 |
|---|---|
| VerifyPaletteSync.cmake 从未被 CMakeLists 包含（CI 从未校验） | 重新 include |
| ROLES 与 Palette.js 无双向校验（新增角色 --check 假通过；删除 KeyError） | 生成器内点名校验 |
| pre-commit 检查 19 输入模式漏掉生成器脚本自身 | 无条件执行 --check（1.4s） |
| Python 3.10 下限无守卫（macOS 自带 3.9.6） | 四个脚本 `from __future__ import annotations` |
| QML 角色引用无反向检查 | 新 pre-commit 检查 27（对抗测试：phantomRole 被检出 ✓） |
| `scripts/*` 忽略规则让新脚本对 git 隐形（palette_common.py 曾无法被提交） | .gitignore 白名单条目 |
| 生成器原地写入无事务（中断留半再生树） | 先烘后清 + 过期目录删除警告 |
| CMake 派生校验缺完整性断言（重排版后 "0 verified" 假通过） | AppColors.h 宏数断言（对抗测试：块格式漂移被 FATAL ✓） |

## 六、第二轮：产品经理视角

- **已修复**：详情页图标头部/hero 冗余（与 UX 共识：hero 唯一身份元素）；终端/DB/HTTP 瓦片光晕垫可见性；e0f2fe 语义评估（dark 表为共享 hex 的正确选择，当前无用户可见回归）。
- **建议（需产品决策，未改动）**：
  1. **隐藏 vs 禁用瓦片**：scheme 门控诊断（MySQL/Redis/MQTT/LDAP 等）在桌面端应显示为禁用瓦片（35% 图标 + 原因 tooltip），否则用户不知道它们存在。`isDisabled` 目前硬编码 false（spec §2.4 的禁用态是死代码）。
  2. **groupHues 区分度**：G2/G3/G4 三蓝几乎不可区分（成对对比度 <2:1），色盲用户完全失效。需设计决策（重命名轮冻结了数值）。
  3. **geoip 光晕垫 tint**：#F43F5E 跟随 4 处小强调点而非主形（17 处主题自适应 pin）——1/45 的轻微品牌偏差。
  4. **RTL 字符串**：DashboardScreen:181 `': '` 拼接、SettingsScreen:134 `' ✓'` 后缀应移入翻译（15 语言文件联动，需单独变更）。
  5. **令牌字面量**：若干像素字号/半径不在令牌刻度（16/14/46/10/2）——建议后续映射 + pre-commit 字面量检查。

## 七、建议（技术债，未实施——超出本轮修复范围）

1. **凭据明文持久化**（[AppState.cpp:627](src/app/AppState.cpp)）：FTP/SSH/DB 密码明文进 QSettings——应改 OS 凭据库（Keychain/DPAPI/libsecret）或至少提供 opt-in"记住凭据"。
2. **CrashHandler 信号处理器非异步安全**（[CrashHandler.h:398](src/Common/Utils/CrashHandler.h)）：堆损坏场景下写报告本身会二次崩溃——应 raw write(2) 暂存 + 下次启动后处理。
3. **Android DNS JNI 阻塞全局池**（[NetworkDiagnostics.cpp:1053](src/Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.cpp)）：违反项目规则 10.3，应改 detached std::thread + promise（DnsResolver 桌面模式）。
4. **iosHttpTiming `__block QString err`**（[HttpDiagnostics.mm:340](src/Diagnostics/Model/G5/Platform/IOS/HttpDiagnostics.mm)）：应迁移到兄弟函数的 shared_ptr<Ctx> 模式（当前休眠态，重构即复发）。
5. **检查 25 进程决策**：报告 gitignored → 新克隆对 WCAG 回归永久静默。备选：提交生成报告（漂移 diff 可见 + 硬失败）。

## 八、验证记录

- 26→27 项 pre-commit 检查全绿；qmllint 15/15 变更文件通过；三个生成器 `--check` 全过。
- CMake 校验器对抗测试：损坏宏值 → FATAL ✓；块格式漂移 → 完整性断言 FATAL ✓；恢复后 40+40 令牌 ✓。
- 图标 `--check` 对抗测试：故意损坏文件 → 检出 ✓。
- 检查 27 对抗测试：注入 `colors.phantomRole` → 检出 ✓。
- 角色引用转换值保持验证：烘焙输出与转换前逐值一致 ✓。
- 回归自查：实现过程中自身引入的 3 个 bug（ROLES 键集合误用、mkdir 冲突、辅形目录误删）均由验证链路当场捕获并修复。
