// main.qml — UI 壳：AppContent（dock 导航 + 四页面栈）
import QtQuick
import QtQuick.Window
import core
import theme

Window {
    id: win
    width: 480
    height: 720
    minimumWidth: 360
    minimumHeight: 480
    visible: true
    title: T.tr("appName")
    color: ThemeEngine.colors.surface

    AppContent {
        anchors.fill: parent
    }
}
