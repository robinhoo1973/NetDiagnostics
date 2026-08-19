// PageTileGridSection.qml — 瓦片网格壳（§2.5，算法在 DiagTileGrid）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain

    property var model: []
    property bool compact: false
    property bool usePerItemRunning: false
    property bool groupRunning: false
    // 5WHY (复核 2026-08-19): 屏幕可见性下传（面板→网格壳→网格→瓦片）。
    property bool screenVisible: true
    signal tileClicked(var data)

    sectionImplicitHeight: grid.implicitHeight

    DiagTileGrid {
        id: grid
        Layout.fillWidth: true
        model: root.model
        compact: root.compact
        usePerItemRunning: root.usePerItemRunning
        groupRunning: root.groupRunning
        screenVisible: root.screenVisible
        onTileClicked: function(data) { root.tileClicked(data) }
    }
}
