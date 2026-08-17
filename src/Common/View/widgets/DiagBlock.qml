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
    readonly property color _statusColor: _isDone ? (ThemeEngine.statusColors[_status] || ThemeEngine.colors.skip) : "transparent"
    readonly property bool _isRunning: root.testRunning && !root._isDisabled && !root._isDone
    // C2：瓦片标签必须经 T.diagName（15 语言响应式）——直读 C++ 英文 label 会让
    // 语言切换对 45 个瓦片失效；label 作回退。
    readonly property string _label: (itemData.diagId !== undefined
        ? (T.diagName(itemData.diagId) || itemData.label || "")
        : (itemData.label || ""))
    // 终端协议图标（TELNET/SSH/FTP）：图标井内渲染闪烁下划线光标。
    // SVG 无 SMIL 动画支持（QtSvg 不渲染 animate），光标由 QML 层驱动——
    // 主形 SVG 只画 ">" 提示符，下划线由 terminalCursor 补画并闪烁。
    // _iconName 中间属性：下游绑定只随 iconName 值变化（5WHY review
    // 2026-08-17：itemData 是 var 持 JS 对象，子属性访问不参与依赖追踪，
    // 直接绑定会在每次 itemData 重赋时触发多余求值）。
    readonly property string _iconName: itemData.iconName || "circle"
    readonly property bool _isTerminalIcon: _iconName === "nd-diag-g5-telnet"
        || _iconName === "nd-diag-g5-ssh"
        || _iconName === "nd-diag-g5-ftp"
    // 各协议下划线几何（24 空间）：宽度=提示符宽度 3.2，位于首字母正下方，中心 y=16.4
    readonly property var _termCursorGeom: ({
        "nd-diag-g5-telnet": { x: 7.6, w: 3.2 },
        "nd-diag-g5-ssh":    { x: 7.6, w: 3.2 },
        "nd-diag-g5-ftp":    { x: 7.6, w: 3.2 }
    })
    // 光标几何一次查表（5WHY simplify 2026-08-17：宽度/x 各自重复
    // _termCursorGeom[itemData.iconName] 守卫+取值两次，且依赖未追踪的 var 对象）
    readonly property var _termCursor: _termCursorGeom[_iconName]
    // 5WHY (review 2026-08-17): Qt.rgba(colors.primary.r, ...) 取的是 JS 字符串
    // 的 .r/.g/.b — undefined → NaN → rgba(0,0,0,0.35) 黑色半透明图标。
    // Qt.alpha() 才在类型边界做 string→color 转换。
    readonly property color _iconColor: root._isDisabled
        ? Qt.alpha(ThemeEngine.colors.primary, 0.35)
        : ThemeEngine.colors.primary
    readonly property int _iconSize: root.compact ? 32 : 44

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
        if (_elapsed > 20) return ThemeEngine.colors.fail
        if (_elapsed >= 10) return ThemeEngine.colors.warningStrong
        if (_elapsed >= 5) return ThemeEngine.colors.warning
        return ThemeEngine.colors.success
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
                       : ThemeEngine.colors.surfaceContainerLow
            }
            GradientStop { position: 1.0; color: ThemeEngine.colors.surfaceContainerLow }
        }
        border { width: 1; color: _isRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.outlineVariant }

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
        // 图标光晕垫：squircle 圆角方垫（同色低透明光晕，无硬染色；
        // 共享组件 IconPad——光晕规格只在组件内维护）
            IconPad {
                id: iconWell
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.compact ? 48 : 60; height: root.compact ? 48 : 60
                iconName: root._iconName
                iconSize: root._iconSize
                iconColor: root._iconColor
                // 终端协议闪烁光标（几何随协议：SVG 内为静态光标块，此下划线常显并闪烁叠加）
                Rectangle {
                    id: terminalCursor
                    visible: root._isTerminalIcon
                    width: (root._termCursor ? root._termCursor.w : 8) * root._iconSize / 24
                    height: Math.max(1.5, root._iconSize * 1.2 / 24)
                    x: iconWell.width / 2 + ((root._termCursor ? root._termCursor.x : 11.4) - 12) * root._iconSize / 24
                    y: iconWell.height / 2 + (12.1 - 12) * root._iconSize / 24 - height / 2
                    color: root._iconColor
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        // 5WHY (review 2026-08-17): 45 个瓦片仅 3 个终端图标显示
                        // 光标——未门控的无限动画在不可见瓦片上持续 tick（移动端
                        // 空耗 CPU/唤醒）。
                        running: root._isTerminalIcon && root.visible
                        NumberAnimation { from: 1.0; to: 0.15; duration: 530; easing.type: Easing.Linear }
                        NumberAnimation { from: 0.15; to: 1.0; duration: 530; easing.type: Easing.Linear }
                    }
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

        // 底部状态条（完成=状态色 / 运行=主色；状态不再染图标本身）
        Rectangle {
            anchors { left: card.left; right: card.right; bottom: card.bottom; leftMargin: 3; rightMargin: 3; bottomMargin: 3 }
            height: 3
            radius: 1.5
            visible: root._isDone || root._isRunning
            color: root._isDone ? root._statusColor : ThemeEngine.colors.primary
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
