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
    Component.onCompleted: applyTheme()

    readonly property var statusColors: [
        colors.passGreen,   colors.warnYellow,  colors.failRed,
        colors.skipGray,    colors.errorRed,    colors.infoBlue
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
    readonly property string fontUi: Qt.application.font.family
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
}
