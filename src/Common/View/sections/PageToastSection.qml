// =============================================================================
// PageToastSection.qml — Unified toast (NEW-7, PageSection 派生)
//
// Placed in PageDisplay.floatingContent.  Page owns the trigger: set
// toastText (or expose showToast()), keep page-local Timer/Connections for
// multi-toast sequencing (e.g. language + restore in Settings).  Base
// PageDisplay no longer carries a built-in toast.
//
// 7-8（2026-08-13）：toast 改为 Popup 承载——Popup 渲染于窗口 overlay 层，
// 高于 main.qml 的窗口控制按钮；原普通 Item + z:2000 仅在页面兄弟层级内
// 生效，被窗口按钮（Window 直接子级、声明在后）盖住。
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import core

PageSection {
    id: root
    property string toastText: ""
    property int durationMs: ThemeEngine.toastDurationMs
    // 5WHY (Reuse 2026-09-05): showToast/自动隐藏 Timer 曾逐字复制于
    // DetailPage / SettingsScreen / DiagnosticScreen 三页——toast 时序、
    // 多 toast 队列化、无障碍播报的改动须三处同步。触发与定时收敛进
    // section 本体，页面只保留触发点（page.showToast 一行委托）。
    function showToast(msg) {
        toastText = msg
        if (msg !== "") toastTimer.restart()
    }
    Timer {
        id: toastTimer
        interval: root.durationMs
        onTriggered: root.toastText = ""
    }

    backgroundStyle: PageSection.Plain
    anchors.fill: parent   // 占据整页，供 Popup 定位（浮层容器模式）

    Popup {
        id: toastPopup
        x: Math.round((root.width - width) / 2)
        y: root.height - height - 24
        width: Math.min(toastLabel.implicitWidth + 48, 440)
        height: 36
        visible: root.toastText !== ""
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose
        padding: 0
        background: Rectangle {
            radius: ThemeEngine.radius.full   // 胶囊 toast
            color: ThemeEngine.colors.surfaceContainerLow
            border { width: 1; color: ThemeEngine.colors.primary }
        }
        contentItem: Label {
            id: toastLabel
            text: root.toastText
            font.family: ThemeEngine.monoFont
            font.pixelSize: 12
            color: ThemeEngine.colors.onSurface
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            maximumLineCount: 1
            elide: Text.ElideMiddle
        }
    }
}
