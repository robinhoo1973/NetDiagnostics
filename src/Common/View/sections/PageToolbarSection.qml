// PageToolbarSection.qml — DiagnosticToolbar 壳（§2.2）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Bar
    fixedHeight: toolbar.implicitHeight   // 5WHY UI-1

    property bool wide: true
    signal runRequested()
    signal cancelRequested()

    DiagnosticToolbar {
        id: toolbar
        Layout.fillWidth: true
        wide: root.wide
        onRunRequested: root.runRequested()
        onCancelRequested: root.cancelRequested()
    }
}
