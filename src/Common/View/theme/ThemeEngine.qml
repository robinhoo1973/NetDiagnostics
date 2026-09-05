// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton, rebuilt foundation)
//
// Canonical tokens per review/refactor/ui/ui-refactor-architecture-guide.md.
// STRICT RULE: minimize QML property count (~97-property crash threshold in
// static builds) — palette lives in JS objects.
// =============================================================================
pragma Singleton
import QtQuick
import "Palette.js" as Palette
import "../widgets/IconTints.js" as IconTints

QtObject {
    readonly property int litMode: 1
    readonly property int drkMode: 2
    // 5WHY (2026-08-23 P0-1, review/ui-ux-audit-plan §4): 曾仅 Light/Dark 二态
    // 硬选——"跟随系统"是全行业标配（对标矩阵 G5）。mode 0=跟随系统：isDark 由
    // QStyleHints::colorScheme 推导，全部既有 isDark 消费方零改动。
    readonly property int sysMode: 0
    property int mode: drkMode
    // 5WHY (review 2026-08-23 修正两点):
    // ① Qt::ColorScheme = { Unknown=0, Light=1, Dark=2 }——曾写 === 1
    //    （Light 当深色、Dark 当浅色，跟随系统态明暗完全颠倒）。
    // ② Qt.styleHints 静态绑定为项目禁用模式（D.1 同类静态初始化顺序
    //    崩溃）——曾直接绑定 Qt.styleHints.colorScheme。改为可写属性 +
    //    onCompleted 空检赋值 + colorSchemeChanged 连接（fontUi 同模式）。
    property int systemScheme: 2   // 缺省 Dark（onCompleted 前安全值）
    readonly property bool systemIsDark: systemScheme === 2
    readonly property bool isDark: mode === sysMode
        ? systemIsDark
        : (mode !== litMode)
    // 5WHY (review round 4, D.1): Qt.platform 静态绑定与 Qt.application 同类的
    // C++ 后端初始化顺序风险——可写属性 + onCompleted 空检赋值（readonly 会
    // 静默丢弃赋值，round 3 教训）。
    property bool isMobile: false

    property var colors: Palette.Dark

    function applyTheme() {
        var dark = (mode === sysMode) ? systemIsDark : (mode !== litMode)
        var p = dark ? Palette.Dark : Palette.Light
        if (colors !== p)
            colors = Object.assign({}, p)
    }
    onModeChanged: applyTheme()
    // 跟随系统模式下响应 OS 深浅切换（colorSchemeChanged → systemScheme → 触发）
    onSystemIsDarkChanged: if (mode === sysMode) applyTheme()
    Component.onCompleted: {
        applyTheme()
        // 5WHY (review 2026-08-17, D.1): Qt.application 静态绑定违反项目规则
        // ——onCompleted 空检赋值（并入既有初始化点，避免第二个 handler）
        if (Qt.application) fontUi = Qt.application.font.family
        if (Qt.platform) isMobile = (Qt.platform.os === "ios" || Qt.platform.os === "android")
        // 5WHY (review 2026-08-23 同款空检赋值): 系统深浅色初始化 + 运行时
        // 跟随（colorSchemeChanged 信号）；静态绑定禁止规则见上。
        if (Qt.styleHints) {
            systemScheme = Qt.styleHints.colorScheme
            Qt.styleHints.colorSchemeChanged.connect(function (cs) { systemScheme = cs })
        }
        // M8 (5WHY): statusColors/statusIconNames 按枚举序维护——修改一个数组
        // 而忘记另一个会导致 Repeater 消费方索引错位（颜色与图标不匹配）。
        // 完成时长度断言在开发期暴露漂移。
        // 5WHY (2026-09-04 iOS 启动闪退): 此断言曾作为第二个
        // Component.onCompleted 独立声明——同一对象重复信号处理器在
        // Qt 6.8 编译型 QML（iOS 静态构建 qmlcachegen）下是致命编译错误
        // （"Property value set multiple times" → ThemeEngine 加载失败 →
        // 通用崩溃链 → 闪退）。同一信号只允许一个处理器，断言并入此处。
        if (statusColors.length !== statusIconNames.length)
            console.warn("ThemeEngine: statusColors.length(" + statusColors.length
                         + ") !== statusIconNames.length(" + statusIconNames.length + ")")
    }

    readonly property var statusColors: [
        colors.success,   colors.warning,  colors.fail,
        colors.skip,      colors.error,    colors.info,
        colors.outline                          // DiagStatus::Cancelled=6
        // 5WHY (复核 2026-08-18): Cancelled 曾复用 skip 灰——Skipped/Cancelled
        // 徽标相邻同色同图标，取消数不可区分。改用 outline 石板色（#64748B，
        // 两主题同值，比 skip 深一档）+ close 图标（X=中止语义）。
    ]
    readonly property var statusIconNames: [
        "badge-check", "badge-warning", "badge-close",
        "badge-skip",  "badge-error",   "badge-info",
        "close"                                 // DiagStatus::Cancelled=6（DiagId.h 同映射）
    ]
    // 5WHY (复核 2026-08-18 五表漂移): 状态展示顺序/计数键/标签键曾同时维护在
    // StatusBadgeCluster（7 行字面量）与 DashboardSummaryComp（7 行表）——
    // 加 Cancelled 时两处都改过且顺序相同纯属巧合。此处为单一来源，两个
    // Repeater 消费；statusColors/statusIconNames 仍按枚举序（DiagId.h）。
    readonly property var statusRows: [
        { code: 0, labelKey: "summaryPass",      countKey: "pass" },
        { code: 5, labelKey: "summaryInfo",      countKey: "info" },
        { code: 1, labelKey: "summaryWarning",   countKey: "warn" },
        { code: 2, labelKey: "summaryFail",      countKey: "fail" },
        { code: 3, labelKey: "summarySkipped",   countKey: "skip" },
        { code: 4, labelKey: "summaryError",     countKey: "error" },
        { code: 6, labelKey: "summaryCancelled", countKey: "cancelled" }
    ]

    readonly property int toastDurationMs: 3500
    // 窄屏断点（5WHY review round 3: 组头两行/tab 前缀曾各用一套门限，
    // 平板 700px 出现短 tab 配完整组头——统一单一断点）
    readonly property int compactUiWidth: 600
    // 瓦片图标度量令牌（5WHY review round 4: 图标曾硬编码 32/44、垫 48/60
    // 且与瓦片尺寸无关——"瓦片内小框"的根源；M3 keyline 比随 blockSize 派生。
    // 5WHY (review 2026-08-17, 用户诉求"图形以瓦片尺寸显示"): 0.55 仍偏小且
    // 垫(0.75×瓦片)仍是"小一号方框"——图标升到 0.66×瓦片、垫≈0.92×瓦片
    // （贴满瓦片，方框感消失；80px 紧凑瓦片上垫=卡片全幅）。
    // 5WHY (复核 2026-08-18 几何复核): 0.54 的收缩基于包围盒计算——图标井是
    // 圆形（r=iconSize/2）、计时圆点也是圆形，圆心距在 0.66 比下最小 5.2px
    // （80px 瓦片）至 15.9px（108px），圆与圆从未重叠；重叠的是方形包围盒
    // 边角（~4.5px），而描边字形不触达方形角落。恢复 0.66 满足"图形以瓦片
    // 尺寸显示"诉求；计时圆点保持在左上角 4px 边距（圆形无碰撞）。
    readonly property real tileIconRatio: 0.66
    // 图标最小可读尺寸（5WHY review round 4: 40px 底线曾是组件内魔法数——
    // 尺寸规则整体归令牌层）
    readonly property int tileIconMin: 40
    // 窄屏判定（5WHY review round 3 修复：isMobile && width < compactUiWidth
    // 三元曾两处复制——组头条与 tab 前缀漂移风险；单一断点配套单一判定）
    function isCompactUi(width) {
        return isMobile && width < compactUiWidth
    }
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property var spacing: ({ xs: 4, sm: 8, md: 12, lg: 16, xl: 24 })
    // R5-4：字阶令牌（caption/body/subhead/title/headline + mono）——组件禁止魔法字号。
    readonly property var fontSize: ({ caption: 11, micro: 9, body: 13, subhead: 15, title: 17, headline: 22, mono: 12 })
    // 5WHY (review 2026-08-17, D.1): Qt.application 静态绑定违反项目自身规则
    // （Qt.styleHints 同类静态初始化顺序崩溃）——默认安全值，onCompleted 空检赋值。
    // 注意：必须可写——QML 会静默丢弃对 readonly 属性的赋值（review round 3
    // 曾误声明 readonly 导致全部界面回退 sans-serif）。
    property string fontUi: "sans-serif"
    readonly property string monoFont: "JetBrains Mono"

    function formatDuration(ms) {
        if (ms < 1000) return ms + "ms"
        if (ms < 60000) return (ms/1000).toFixed(1) + "s"
        var min = Math.floor(ms / 60000)
        var sec = Math.round((ms % 60000) / 1000)
        return min + "m " + sec + "s"
    }
    // 5WHY (Reuse 2026-09-05): color → 6 位大写十六进制曾在 AppIcon /
    // WifiWaveAnimation / BarsCycleAnimation 三处实现且已现分歧（toString
    // 切片 vs 通道取整）——颜色格式规则改动需三处同步。收敛单点：
    // 通道取整版本（对 #RRGGBB/#AARRGGBB/透明色均精确，不依赖
    // toString() 输出格式）。
    function colorToHex(c) {
        function h(n) {
            var s = Math.round(Math.max(0, Math.min(1, n)) * 255).toString(16)
            return s.length < 2 ? "0" + s : s
        }
        return (h(c.r) + h(c.g) + h(c.b)).toUpperCase()
    }
    function pad2(n) { return (n < 10 ? " " : "") + n }
    // 5WHY (复核 2026-08-18 Reuse C4): "X/Y" 组合 + 零守卫曾在 StatusBadgeCluster
    // 标签与状态头两分支三处手写——格式变更（如补零、分隔符）须三处同步。
    function xyLabel(completed, total) {
        if (total > 0) return (completed || 0) + "/" + total
        return (completed || 0) + ""
    }
    // 组图标单一映射（G1-G5；UI 评审：消除各组件重复硬编码数组）
    function groupIconName(idx) {
        return ["network-card", "shield-network", "internet-globe", "remote-host", "protocol-stack"][idx] || "circle"
    }
    // 组色调单一映射（G1-G5；5WHY review 2026-08-17：PageGroupPanelSection 与
    // DashboardScreen 各自复制守卫三元，回退策略改动只落一处 → 组头条与仪表盘
    // 分层行渲染不同色。5WHY (2026-08-17, 用户诉求 light 可读): groupHues 不再
    // 主题无关——Light 块拥有加深变体数组（白面 4.4-9.9:1），dark 保持亮色系）
    function groupHue(idx) {
        // 经 colors 单一访问路径（Light 块自有加深 groupHues 数组）
        var hues = colors.groupHues
        return (hues !== undefined && idx >= 0 && idx < hues.length) ? hues[idx] : colors.secondary
    }
    // 45 图标常显主导色（瓦片光晕垫 tint；烘焙生成 IconTints.js —— 唯一访问点）
    function iconPadTint(name) {
        return IconTints.tintFor(name || "")
    }
    // 5WHY (复核 2026-08-18 重复收敛): light 加深 1.5× 规则曾同时在 IconPad 与
    // DiagBlock._glowColor 维护——(isDark || !darken) 三元 + Qt.darker(t,1.5)
    // 的语义只有 IconTints 烘焙值适用（完成态状态色 light 变体本身已加深）。
    // 提取单一 helper，两处消费。
    function effTint(tint, darkenInLight) {
        return (!isDark && darkenInLight) ? Qt.darker(tint, 1.5) : tint
    }
    // 导航/工具按钮悬停底色配方（NavArrowButton/ZoomBar 共用；5WHY review
    // round 3: 该 0.2/0.08 三元曾散落 4 处，调悬停色需 4 处同步）
    function navHoverTint(hovered) {
        return hovered ? Qt.alpha(colors.tertiary, 0.2)
                       : Qt.alpha(colors.tertiary, 0.08)
    }
    // 运行状态呈现表（Dashboard 主状态区与状态头共用；5WHY simplify 2026-08-17：
    // 原先颜色已集中但图标/标签仍是两份平行三元链，新增状态值需 ≥3 处同步）。
    // 返回 null 表示常规状态（调用方回退）。
    function runStatusInfo(status) {
        // dimmed：标题弱化（5WHY review round 3: 取消态标题弱化规则曾散落在
        // 调用方三元里，与呈现表脱节）
        // 5WHY (复核 2026-08-18 用户诉求 "孤立成功图标" 根因): Completed(2) 曾
        // 无表项——两个消费方走 `|| fallback` 回退到成功绿勾，导致"未覆盖状态
        // 一律渲染成功图标"：运行完成后（含大量失败项）标题栏仍显示孤立绿勾，
        // 与中性 "X/Y completed" 标签/红绿徽标脱节。补齐全 5 态呈现表：2→中性
        // 勾（颜色与标签同为 onSurfaceVariant）；labelKey 留空 = 消费方各自保
        // 持 X/Y 标签（状态头）与 diagRunComplete（Dashboard）。Idle→null。
        if (status === 2) return { color: colors.onSurfaceVariant, iconName: "badge-check", labelKey: "", dimmed: false }
        if (status === 3) return { color: colors.warning, iconName: "badge-close", labelKey: "cancelled", dimmed: true }
        if (status === 4) return { color: colors.fail,    iconName: "badge-error", labelKey: "errorStatus", dimmed: false }
        return null
    }
    // 5WHY (复核 2026-08-18 终态判定单一来源): 消费方曾各自手写 `>= 2`/`!== 1`
    // 数值范围判定——RunStatus 枚举重排时静默翻转图标可见性。白名单终态集
    // （2/3/4 显式枚举）由两处消费方共用，加状态值只需改此一处。
    function isTerminalRunStatus(s) {
        return s === 2 || s === 3 || s === 4
    }
    // （runStatusColor/runStatusIcon 包装已删除——review round 4：纯字段
    // 间接层，调用方直接读缓存的 runStatusInfo 对象 + || fallback）
    // 终端底色（TerminalBlock/PageTerminalSection 共用）：暗=surface，
    // 亮=surfaceContainerHighest（7-5 修复：亮色下硬编码深藏青导致文字几乎
    // 不可读）。必须是属性而非函数：无参函数调用绑定没有依赖追踪，
    // 运行时切主题后终端背景不会更新（review round 3）。
    readonly property color terminalBg: isDark ? colors.surface
                                               : colors.surfaceContainerHighest
}
