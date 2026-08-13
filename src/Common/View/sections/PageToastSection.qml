// =============================================================================
// PageToastSection.qml — Unified toast (NEW-7, PageSection 派生)
//
// Placed in PageDisplay.floatingContent.  Page owns the trigger: set
// toastText (or expose showToast()), keep page-local Timer/Connections for
// multi-toast sequencing (e.g. language + restore in Settings).  Base
// PageDisplay no longer carries a built-in toast.
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

    backgroundStyle: PageSection.Plain
    z: 2000
    anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
    // 长文案宽度帽：防止 toast 超出窗口（reportShareOk 等长键）
    implicitWidth: Math.min(label.implicitWidth + 24, 440)
    implicitHeight: 36

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: ThemeEngine.radius.full   // 胶囊 toast
        visible: root.toastText !== ""
        color: ThemeEngine.colors.card
        border { width: 1; color: ThemeEngine.colors.borderFocused }
    }

    Label {
        id: label
        Layout.alignment: Qt.AlignCenter
        text: root.toastText
        font.family: ThemeEngine.monoFont
        font.pixelSize: 12
        color: ThemeEngine.colors.textPrimary
        visible: root.toastText !== ""
        maximumLineCount: 1
        elide: Text.ElideMiddle
    }
}
