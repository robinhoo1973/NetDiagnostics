// =============================================================================
// PageDisplay.qml — Page base class (UI layer core)
//
// Per review/refactor/ui/ui-refactor-architecture-guide.md §4.
// Three named content slots: headerContent / bodyContent / floatingContent.
// NEW-7: no built-in toast — pages use PageToastSection in floatingContent.
// NEW-8: overlayVisible must be wired by pages when a floating overlay opens.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Item {
    id: root

    // RTL：阿拉伯语等从右向左（dock 已单独禁用镜像，见 AppContent）
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    property alias headerContent: headerCol.data
    property alias bodyContent: bodyCol.data
    property alias floatingContent: floatingCol.data
    // 5WHY (复核 2026-08-19 viewport 门控): 内容 Flickable 暴露给页面——
    // 瓦片以之判定视口内（滚动出视口的运行瓦片停动画；裁剪不停动画，
    // 且 visible 不随滚动变化）。
    readonly property alias contentFlickable: bodyFlick

    // NEW-8: pages must set this true/false when a floating overlay opens/
    // closes (e.g. `overlayVisible: floatingOverlay.active`), else
    // AppContent.navBlocked stays false and navigation leaks under overlays.
    property bool overlayVisible: false
    signal sectionAction(string scope, string action, var payload)
    function emitSectionAction(scope, action, payload) { root.sectionAction(scope, action, payload) }
    // NEW-7: Toast 统一由 floatingContent 的 PageToastSection 提供（基类无内置 toast 状态）

    ColumnLayout {
        id: headerCol
        anchors { top: parent.top; left: parent.left; right: parent.right }
        spacing: 0
    }
    Flickable {
        id: bodyFlick
        anchors { top: headerCol.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        contentHeight: bodyCol.implicitHeight
        ColumnLayout {
            id: bodyCol
            width: parent.width
            spacing: 0
        }
    }
    Item {
        id: floatingCol
        anchors.fill: parent
        z: 1000
    }
    // NEW-7：Toast 由 floatingContent 的 PageToastSection 呈现（基类无内置 toastBar）
}
