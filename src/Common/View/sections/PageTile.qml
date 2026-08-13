// PageTile.qml — 单个瓦片（§2.6，包 DiagBlock）
import QtQuick
import QtQuick.Controls
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain

    property var itemData: ({})
    property bool testRunning: false
    property real blockSize: 108
    property bool compact: false
    signal clicked(var data)

    sectionImplicitHeight: blockSize
    implicitWidth: blockSize
    active: itemData.isPending === true || (itemData.status !== 3)

    DiagBlock {
        anchors.fill: parent
        itemData: root.itemData
        testRunning: root.testRunning
        blockSize: root.blockSize
        compact: root.compact
        onClicked: function(data) { root.clicked(data) }
    }
}
