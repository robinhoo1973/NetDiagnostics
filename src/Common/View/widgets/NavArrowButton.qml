// =============================================================================
// NavArrowButton.qml — PDF 翻页箭头按钮（PdfPreviewView / NativePdfPageView 共用）
//
// 5WHY (simplify 2026-08-17)：两个 PDF 查看器的上一页/下一页按钮是 4 份
// 近同代码（32/44px 尺寸、悬停 tertiary 0.2/0.08、play 图标旋转 180°），
// 任何一处改交互（如悬停色、可访问性名）都需同步 4 处。合并为单组件。
// =============================================================================
import QtQuick
import "../theme"

Rectangle {
    id: root
    property bool prev: false          // true = 上一页（左箭头，图标旋转 180°）
    property int btnSize: 32
    // 5WHY (review round 3): 7/16 派生默认在所有调用点都被显式覆盖且 44px 处
    // 与实况不一致（派生 19 vs 实传 18）——双定义。回退为朴素默认。
    property int iconSize: 14
    signal activated()

    width: btnSize; height: btnSize
    // 尺寸适配：32px→radius 6，44px→radius 8
    radius: Math.round(btnSize * 3 / 16)

    // 悬停底色配方统一走 ThemeEngine.navHoverTint（review round 3：该
    // 0.2/0.08 三元曾散落 4 处）
    color: ThemeEngine.navHoverTint(ma.containsMouse)

    AppIcon {
        anchors.centerIn: parent
        name: "play"; size: root.iconSize
        color: ThemeEngine.colors.tertiary
        rotation: root.prev ? 180 : 0
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: root.activated()
    }

    Accessible.name: root.prev ? T.tr("accPrevPage") : T.tr("accNextPage")
    Accessible.role: Accessible.Button
}
