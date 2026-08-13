// PageListSection.qml — 通用 ListView 壳（page-config.md §2.3：包装原 ListView）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain

    property var model: []
    property Component delegate: null
    property int spacing: 0

    sectionImplicitHeight: Math.max(120, listView.contentHeight)

    ListView {
        id: listView
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: root.model
        delegate: root.delegate
        spacing: root.spacing
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    }
}
