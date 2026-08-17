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
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"   // R4-1: 从平台推导，不硬编码

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
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property var spacing: ({ xs: 4, sm: 8, md: 12, lg: 16, xl: 24 })
    // R5-4：字阶令牌（caption/body/subhead/title/headline + mono）——组件禁止魔法字号。
    readonly property var fontSize: ({ caption: 11, micro: 9, body: 13, subhead: 15, title: 17, headline: 22, mono: 12 })
    // 5WHY (review 2026-08-17, D.1): Qt.application 静态绑定违反项目自身规则
    // （Qt.styleHints 同类静态初始化顺序崩溃）——默认安全值，onCompleted 空检赋值。
    readonly property string fontUi: "sans-serif"
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
    // 分层行渲染不同色。groupHues 主题无关，单一来源在 Palette.Dark。）
    function groupHue(idx) {
        // 经 colors 单一访问路径（groupHues 主题无关：Light 块引用 Dark 同一数组）
        var hues = colors.groupHues
        return (hues !== undefined && idx >= 0 && idx < hues.length) ? hues[idx] : colors.secondary
    }
    // 45 图标常显主导色（瓦片光晕垫 tint；烘焙生成 IconTints.js —— 唯一访问点）
    function iconPadTint(name) {
        return IconTints.tintFor(name || "")
    }
    // 运行状态呈现表（Dashboard 主状态区与状态头共用；5WHY simplify 2026-08-17：
    // 原先颜色已集中但图标/标签仍是两份平行三元链，新增状态值需 ≥3 处同步）。
    // 返回 null 表示常规状态（调用方回退）。
    function runStatusInfo(status) {
        if (status === 3) return { color: colors.warning, iconName: "badge-close", labelKey: "cancelled" }
        if (status === 4) return { color: colors.fail,    iconName: "badge-error", labelKey: "errorStatus" }
        return null
    }
    function runStatusColor(status, fallback) {
        var info = runStatusInfo(status)
        return info ? info.color : fallback
    }
    function runStatusIcon(status, fallbackName) {
        var info = runStatusInfo(status)
        return info ? info.iconName : (fallbackName || "check")
    }
    // 终端底色（TerminalBlock/PageTerminalSection 共用）：暗=surface，
    // 亮=surfaceContainerHighest（7-5 修复：亮色下硬编码深藏青导致文字几乎
    // 不可读）。注：曾尝试 Palette.js getter 派生令牌（前景/背景同令牌系统），
    // 但 qmllint 不支持对象字面量 getter——退回函数式单点（simplify 2026-08-17）。
    function terminalBg() {
        return isDark ? colors.surface : colors.surfaceContainerHighest
    }
}
