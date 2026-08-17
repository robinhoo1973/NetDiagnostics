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
    property int mode: drkMode
    readonly property bool isDark: mode !== litMode
    // 5WHY (review round 4, D.1): Qt.platform 静态绑定与 Qt.application 同类的
    // C++ 后端初始化顺序风险——可写属性 + onCompleted 空检赋值（readonly 会
    // 静默丢弃赋值，round 3 教训）。
    property bool isMobile: false

    property var colors: Palette.Dark

    function applyTheme() {
        var p = (mode === litMode) ? Palette.Light : Palette.Dark
        if (colors !== p)
            colors = Object.assign({}, p)
    }
    onModeChanged: applyTheme()
    Component.onCompleted: {
        applyTheme()
        // 5WHY (review 2026-08-17, D.1): Qt.application 静态绑定违反项目规则
        // ——onCompleted 空检赋值（并入既有初始化点，避免第二个 handler）
        if (Qt.application) fontUi = Qt.application.font.family
        if (Qt.platform) isMobile = (Qt.platform.os === "ios" || Qt.platform.os === "android")
    }

    readonly property var statusColors: [
        colors.success,   colors.warning,  colors.fail,
        colors.skip,      colors.error,    colors.info
    ]
    readonly property var statusIconNames: [
        "badge-check", "badge-warning", "badge-close",
        "badge-skip",  "badge-error",   "badge-info"
    ]

    readonly property int toastDurationMs: 3500
    // 窄屏断点（5WHY review round 3: 组头两行/tab 前缀曾各用一套门限，
    // 平板 700px 出现短 tab 配完整组头——统一单一断点）
    readonly property int compactUiWidth: 600
    // 瓦片图标度量令牌（5WHY review round 4: 图标曾硬编码 32/44、垫 48/60
    // 且与瓦片尺寸无关——"瓦片内小框"的根源；M3 keyline 比随 blockSize 派生。
    // 5WHY (review 2026-08-17, 用户诉求"图形以瓦片尺寸显示"): 0.55 仍偏小且
    // 垫(0.75×瓦片)仍是"小一号方框"——图标升到 0.66×瓦片、垫≈0.92×瓦片
    // （贴满瓦片，方框感消失；80px 紧凑瓦片上垫=卡片全幅）
    readonly property real tileIconRatio: 0.66
    readonly property real tilePadRatio: 1.39
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
    function pad2(n) { return (n < 10 ? " " : "") + n }
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
        if (status === 3) return { color: colors.warning, iconName: "badge-close", labelKey: "cancelled", dimmed: true }
        if (status === 4) return { color: colors.fail,    iconName: "badge-error", labelKey: "errorStatus", dimmed: false }
        return null
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
