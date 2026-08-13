// DiagBlock.qml — 诊断瓦片（归档视觉的紧凑忠实移植：分层卡 + 状态着色 + 计时圆点
// + 图标井 + 无障碍 NEW-22 + settle 弹跳 + 五动画）
import QtQuick
import QtQuick.Controls
import theme
import widgets

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
    // C2：瓦片标签必须经 T.diagName（15 语言响应式）——直读 C++ 英文 label 会让
    // 语言切换对 45 个瓦片失效；label 作回退。
    readonly property string _label: (itemData.diagId !== undefined
        ? (T.diagName(itemData.diagId) || itemData.label || "")
        : (itemData.label || ""))

    visible: _isPending || (itemData.status !== 3)
    implicitWidth: visible ? blockSize : 0
    implicitHeight: visible ? blockSize : 0

    // NEW-22：可聚焦交互瓦片（Tab 可达 + 回车/空格激活）
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: _label
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
            || event.key === Qt.Key_Space) {
            root.clicked(root.itemData)
            event.accepted = true
        }
    }

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

    // settle：完成瞬间弹性缩放（归档完成态弹跳）
    property real _settleScale: 1.0
    on_IsDoneChanged: {
        if (_isDone) { _settleScale = 0.94; settleAnim.restart() }
    }
    SequentialAnimation {
        id: settleAnim
        NumberAnimation { target: root; property: "_settleScale"; to: 1.04; duration: 120; easing.type: Easing.OutQuad }
        NumberAnimation { target: root; property: "_settleScale"; to: 1.0; duration: 140; easing.type: Easing.OutBack }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 3
        scale: root._settleScale
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

        // hover 光晕（归档视觉）
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: Qt.alpha(ThemeEngine.colors.primary, 0.04)
            visible: hoverArea.containsMouse && !root._isDisabled && !root._isRunning
        }

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
        // 图标井：圆底容器（归档视觉；无文本版——图标居中即瓦片主视觉）
            Rectangle {
                id: iconWell
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.compact ? 44 : 56; height: root.compact ? 44 : 56
                radius: width / 2
                color: Qt.alpha(_isDone ? _statusColor
                       : _isRunning ? ThemeEngine.colors.primary
                       : _isDisabled ? ThemeEngine.colors.textMuted
                       : ThemeEngine.colors.textSecondary, 0.12)
                AppIcon {
                    anchors.centerIn: parent
                    name: itemData.iconName || "circle"
                    size: root.compact ? 28 : 36
                    color: _isDone ? _statusColor
                           : _isRunning ? ThemeEngine.colors.primary
                           : _isDisabled ? ThemeEngine.colors.textMuted
                           : ThemeEngine.colors.textSecondary
                }
                // 运行动画（animType → 五动画，DiagAnimator 调度）
                DiagAnimator {
                    anchors.fill: parent
                    diagId: itemData.diagId !== undefined ? itemData.diagId : -1
                    running: root._isRunning
                    targetItem: iconWell
                }
            }
        }

        // 完成状态角标（右上）
        AppIcon {
            visible: _isDone
            anchors { top: parent.top; right: parent.right; margins: 7 }
            name: _statusIcon
            size: 14
            color: _statusColor
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked(root.itemData)
            hoverEnabled: true
            // H6：pending/disabled 态不可点（归档语义）
            enabled: !root._isPending && !root._isDisabled
        }
    }
}
