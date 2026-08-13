// DiagBlock.qml — 诊断瓦片（归档视觉的紧凑忠实移植：分层卡 + 状态着色 + 计时圆点）
import QtQuick
import QtQuick.Controls
import theme

Item {
    id: root

    property var itemData: ({})
    property bool testRunning: false
    property real blockSize: 108
    property bool compact: false
    signal clicked(var data)

    readonly property bool _isPending: itemData.isPending === true
    readonly property bool _isDisabled: itemData.isDisabled === true
    readonly property bool _isDone: itemData.isDone === true
    readonly property int _status: itemData.status !== undefined ? itemData.status : -1
    readonly property string _statusIcon: _isDone ? (ThemeEngine.statusIconNames[_status] || "badge-skip") : ""
    readonly property color _statusColor: _isDone ? (ThemeEngine.statusColors[_status] || ThemeEngine.colors.skipGray) : "transparent"
    readonly property bool _isRunning: root.testRunning && !root._isDisabled && !root._isDone

    visible: _isPending || (itemData.status !== 3)
    implicitWidth: visible ? blockSize : 0
    implicitHeight: visible ? blockSize : 0

    property int _elapsed: 0
    property bool _showTimer: false
    Timer {
        id: elapsedTimer
        interval: 1000; repeat: true
        running: _isRunning
        onTriggered: { root._elapsed++; if (!root._showTimer) root._showTimer = true }
        onRunningChanged: {
            if (running) { root._elapsed = 0; root._showTimer = false }
            else if (root._elapsed > 0) root._showTimer = true
        }
    }
    readonly property int _displayElapsed: Math.max(1, root._elapsed)
    readonly property string _timerColor: {
        if (_elapsed > 20) return ThemeEngine.colors.failRed
        if (_elapsed >= 10) return ThemeEngine.colors.warnOrange
        if (_elapsed >= 5) return ThemeEngine.colors.warnYellow
        return ThemeEngine.colors.passGreen
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 3
        radius: ThemeEngine.radius.xl   // 瓦片 16px = xl 令牌
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: _isDone ? Qt.alpha(_statusColor, 0.08)
                       : _isRunning ? Qt.alpha(ThemeEngine.colors.primary, 0.06)
                       : ThemeEngine.colors.card
            }
            GradientStop { position: 1.0; color: ThemeEngine.colors.card }
        }
        border { width: 1; color: _isRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.borderCard }

        // 计时圆点（左上）
        Rectangle {
            visible: root._showTimer
            anchors { top: parent.top; left: parent.left; margins: 8 }
            width: 22; height: 22; radius: 11
            color: Qt.alpha(root._timerColor, 0.16)
            Label {
                anchors.centerIn: parent
                text: root._displayElapsed
                font.family: ThemeEngine.monoFont; font.pixelSize: ThemeEngine.fontSize.micro; font.weight: Font.Bold
                color: root._timerColor
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 6
            AppIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: itemData.iconName || "circle"
                size: root.compact ? 28 : 34
                color: _isDone ? _statusColor
                       : _isRunning ? ThemeEngine.colors.primary
                       : _isDisabled ? ThemeEngine.colors.textMuted
                       : ThemeEngine.colors.textSecondary
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: itemData.label || ""
                width: card.width - 20
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                font.family: ThemeEngine.fontUi
                font.pixelSize: root.compact ? ThemeEngine.fontSize.caption : ThemeEngine.fontSize.body
                color: _isDone ? _statusColor
                       : _isDisabled ? ThemeEngine.colors.textMuted
                       : ThemeEngine.colors.textPrimary
            }
            // 完成状态角标
            AppIcon {
                visible: _isDone
                anchors.horizontalCenter: parent.horizontalCenter
                name: _statusIcon
                size: 16
                color: _statusColor
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked(root.itemData)
            hoverEnabled: true
        }
    }
}
