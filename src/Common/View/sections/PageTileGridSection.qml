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
    signal tileClicked(var data)

    sectionImplicitHeight: grid.implicitHeight

    DiagTileGrid {
        id: grid
        Layout.fillWidth: true
        model: root.model
        compact: root.compact
        usePerItemRunning: root.usePerItemRunning
        groupRunning: root.groupRunning
        onTileClicked: function(data) { root.tileClicked(data) }
    }
}
