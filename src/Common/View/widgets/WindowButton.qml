// =============================================================================
// WindowButton.qml — 桌面无边框窗口按钮（最小化/最大化/关闭 共用）
//
// 5WHY (simplify 2026-08-17)：main.qml 的三个窗口按钮是 3 份近同代码
// （38×38、悬停才显底色、图标悬停变色、关闭钮悬停红色）。合并为单组件，
// 仅以 destructive 区分关闭按钮。
// =============================================================================
import QtQuick
import "../theme"

Rectangle {
    id: root
    property string iconName: ""
    property string accName: ""
    property bool destructive: false   // true = 关闭按钮（悬停红色 + 白色图标）
    signal clicked()

    width: 38; height: 38; radius: 6

    color: ma.containsMouse
           ? (root.destructive ? ThemeEngine.colors.fail
                               : Qt.alpha(ThemeEngine.colors.outlineVariant, 0.6))
           : "transparent"

    AppIcon {
        anchors.centerIn: parent
        name: root.iconName; size: 14
        // 5WHY (review round 3): 裸 #FFFFFF 违反 M3 单一事实源——用 onError
        // （错误面内容色；暗 #0F172A 对 fail 红 ≈6.4:1，亮 #FFFFFF ✓）
        // 5WHY (review round 4 复核): onError 暗色 #0F172A——黑图标在 fail
        // 红上视觉倒置；onFail 两主题一致白（fail 红上的内容色）
        color: ma.containsMouse
               ? (root.destructive ? ThemeEngine.colors.onFail : ThemeEngine.colors.onSurface)
               : ThemeEngine.colors.onSurfaceVariant
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: root.clicked()
    }

    Accessible.name: root.accName
    Accessible.role: Accessible.Button
}
