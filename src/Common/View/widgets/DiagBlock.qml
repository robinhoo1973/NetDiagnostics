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
    signal clicked(var data)

    readonly property bool _isPending: itemData.isPending === true
    readonly property bool _isDisabled: itemData.isDisabled === true
    readonly property bool isDone: itemData.isDone === true
    readonly property int _status: itemData.status !== undefined ? itemData.status : -1
    // 5WHY (review round 4): 可见性经类型化 _status 判断（不再读未追踪的 var
    // 子属性）；仅隐藏 DiagStatus 3=Skipped。Cancelled 保持可见——groupStats
    // 仍计入取消项，一并隐藏会出现 "8/45 只见 6 瓦片" 的计数失配；基线行为
    // 是取消瓦片带徽标可见。
    readonly property bool _isSkipped: _status === 3
    readonly property string _statusIcon: isDone ? (ThemeEngine.statusIconNames[_status] || "badge-skip") : ""
    readonly property color _statusColor: isDone ? (ThemeEngine.statusColors[_status] || ThemeEngine.colors.skip) : "transparent"
    readonly property bool _isRunning: root.testRunning && !root._isDisabled && !root.isDone
    // C2：瓦片标签必须经 T.diagName（15 语言响应式）——直读 C++ 英文 label 会让
    // 语言切换对 45 个瓦片失效；label 作回退。
    readonly property string _label: (itemData.diagId !== undefined
        ? (T.diagName(itemData.diagId) || itemData.label || "")
        : (itemData.label || ""))
    // ── 瓦片级图标缩放（5WHY review round 4 — 用户诉求"图形以瓦片尺寸显示"）──
    // 图标/光晕垫随 blockSize 派生（M3 keyline 比经 ThemeEngine 令牌），
    // 不再用 compact 二值（原 80-160px 瓦片恒渲染 32/44px 小框）。
    readonly property int _iconSize: Math.max(ThemeEngine.tileIconMin,
                                              Math.round(root.blockSize * ThemeEngine.tileIconRatio))
    // 垫尺寸夹紧到卡片内（5WHY review round 4: 极小块调用方曾可溢出圆角）
    readonly property int _padSize: Math.min(root.blockSize - 10,
                                             Math.round(root._iconSize * ThemeEngine.tilePadRatio))
    // _iconName 中间属性：下游绑定只随 iconName 值变化（itemData 是 var 持
    // JS 对象，子属性访问不参与依赖追踪）
    readonly property string _iconName: itemData.iconName || "circle"
    // 终端协议图标（TELNET/SSH/FTP）：图标井内渲染闪烁下划线光标
    readonly property bool _isTerminalIcon: _iconName === "nd-diag-g5-telnet"
        || _iconName === "nd-diag-g5-ssh"
        || _iconName === "nd-diag-g5-ftp"
    // 光标几何与 SVG 静态下划线一致（M6.4 12.1 H9.8；5WHY review round 4:
    // 旧值 7.6/3.2 与再设计后母版脱节，形成错位双下划线）
    readonly property real _termCursorX: 6.4
    readonly property real _termCursorW: 3.4
    readonly property real _termCursorYOff: 0.1   // 12.1 - 12（24 空间中心偏移）
    // 禁用=灰色不变式（5WHY review round 4: Qt.alpha(primary,0.35) 在亮色
    // 主题近乎不可见；旧版为不透明 textMuted）。
    // 油墨色经 iconInk 令牌（5WHY review 2026-08-17, 用户诉求 light 可读:
    // light primary #0EA5E9 在白色瓦片上仅 ~2.8:1——light 用深蓝 #0C4A6E
    // ≈9.5:1；dark 与 primary 同值）
    readonly property color _iconColor: root._isDisabled
        ? ThemeEngine.colors.textMuted
        : ThemeEngine.colors.iconInk
    // 完成态：光晕垫改状态色（5WHY review round 4: 状态信号曾缩为 3px 细条
    // ——45 瓦片密集墙上失败几乎不可见；垫色随状态发光恢复醒目度）
    readonly property color _padTint: root.isDone
        ? root._statusColor
        : ThemeEngine.iconPadTint(_iconName)

    visible: _isPending || !_isSkipped
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
    property int _ranSeconds: 0   // 完成瞬间锁存（durationMs=0 的工厂结果也保留计时）
    Timer {
        id: elapsedTimer
        interval: 1000; repeat: true
        running: _isRunning
        onTriggered: root._elapsed++
        // 5WHY (verify 2026-08-17, Qt 6.8.2 实测): isDone 翻转时 onIsDoneChanged
        // 先于 runningChanged 触发（锁存成功），但当 testRunning 先于 isDone
        // 归 false（组先结束再落结果）时 _elapsed 已清零、锁存落空——停止路径
        // 同样锁存，两种完成时序都保留计时（委托重建路径见 _timerSecs 注释）。
        onRunningChanged: {
            if (!running) {
                if (root._elapsed > 0) root._ranSeconds = root._elapsed
                root._elapsed = 0
            }
        }
    }
    // 计时数据驱动（5WHY review round 4: 委托在每次进度事件被重建，本地
    // _elapsed 被清零——完成态计时改读 itemData.durationMs，重建后依然可见；
    // _showTimer/_displayElapsed 两个同步状态随之删除）
    // 完成态：Math.max(1, …) 防亚秒结果显示 "0"（GCommon/DiagnosticBase 最小
    // 1ms——round(150/1000)=0，圆点可见但显示 0 秒）。
    readonly property int _timerSecs: root.isDone
        ? Math.max(1, Math.round(((itemData.durationMs || 0) > 0 ? itemData.durationMs : root._ranSeconds * 1000) / 1000))
        : root._elapsed
    readonly property bool _timerVisible: root.isDone
        ? ((itemData.durationMs || 0) > 0 || root._ranSeconds > 0)
        : root._elapsed > 0
    readonly property string _timerColor: {
        if (_timerSecs > 20) return ThemeEngine.colors.fail
        if (_timerSecs >= 10) return ThemeEngine.colors.warningStrong
        if (_timerSecs >= 5) return ThemeEngine.colors.warning
        return ThemeEngine.colors.success
    }

    // settle：完成瞬间弹性缩放（5WHY review round 4: 直接动画 card.scale——
    // _settleScale 中间属性只为动画存在，且占用每瓦片属性预算）
    onIsDoneChanged: {
        if (isDone) {
            if (root._elapsed > 0) root._ranSeconds = root._elapsed
            card.scale = 0.94; settleAnim.restart()
        }
    }
    SequentialAnimation {
        id: settleAnim
        NumberAnimation { target: card; property: "scale"; to: 1.04; duration: 120; easing.type: Easing.OutQuad }
        NumberAnimation { target: card; property: "scale"; to: 1.0; duration: 140; easing.type: Easing.OutBack }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 3
        radius: ThemeEngine.radius.xl   // 瓦片 16px = xl 令牌
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: isDone ? Qt.alpha(_statusColor, 0.08)
                       : _isRunning ? Qt.alpha(ThemeEngine.colors.primary, 0.06)
                       : ThemeEngine.colors.surfaceContainerLow
            }
            GradientStop { position: 1.0; color: ThemeEngine.colors.surfaceContainerLow }
        }
        border { width: 1; color: _isRunning ? ThemeEngine.colors.primary : ThemeEngine.colors.outlineVariant }

        // ── 图标光晕垫（最先声明=底层；5WHY review round 4: 旧顺序垫盖住
        // 计时圆点——圆点/角标/状态条在其上绘制）──
        IconPad {
            id: iconWell
            anchors.centerIn: parent
            width: root._padSize; height: root._padSize
            iconName: root._iconName
            iconSize: root._iconSize
            iconColor: root._iconColor
            tint: root._padTint
            // 5WHY (verify 2026-08-17): 完成态 tint 是状态色（light 变体本身
            // 已加深，如 #DC2626）——再经 IconPad light 加深 1.5× 会变近黑，
            // 状态光晕退化为灰斑。仅非完成态的 IconTints 值需要 light 加深。
            darkenInLight: !root.isDone
            // 终端协议闪烁光标（仅运行中显示；几何对齐 SVG 静态下划线）
            Rectangle {
                id: terminalCursor
                visible: root._isTerminalIcon && root._isRunning
                width: root._termCursorW * root._iconSize / 24
                height: Math.max(1.5, root._iconSize * 1.2 / 24)
                x: iconWell.width / 2 + (root._termCursorX - 12) * root._iconSize / 24
                y: iconWell.height / 2 + root._termCursorYOff * root._iconSize / 24 - height / 2
                color: root._iconColor
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    // 5WHY (review 2026-08-17): 45 个瓦片仅 3 个终端图标显示
                    // 光标——未门控的无限动画在不可见瓦片上持续 tick
                    running: root._isTerminalIcon && root._isRunning && root.visible
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

        // ── 计时圆点（左上；光晕垫之上）──
        Rectangle {
            visible: root._timerVisible
            anchors { top: parent.top; left: parent.left; margins: 8 }
            width: 22; height: 22; radius: 11
            color: Qt.alpha(root._timerColor, 0.16)
            Label {
                anchors.centerIn: parent
                text: root._timerSecs
                font.family: ThemeEngine.monoFont; font.pixelSize: ThemeEngine.fontSize.micro; font.weight: Font.Bold
                color: root._timerColor
            }
        }

        // ── 底部状态条（完成=状态色 / 运行=主色；5WHY review round 4:
        // margins 10 防 3px 条刺出 16px 圆角轮廓）──
        Rectangle {
            anchors { left: card.left; right: card.right; bottom: card.bottom; leftMargin: 10; rightMargin: 10; bottomMargin: 10 }
            height: 3
            radius: 1.5
            visible: root.isDone || root._isRunning
            color: root.isDone ? root._statusColor : ThemeEngine.colors.primary
        }

        // ── 完成状态角标（右上）──
        AppIcon {
            visible: isDone
            anchors { top: parent.top; right: parent.right; margins: 7 }
            name: _statusIcon
            size: 14
            color: _statusColor
        }

        // hover 光晕（归档视觉）
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: Qt.alpha(ThemeEngine.colors.primary, 0.04)
            visible: hoverArea.containsMouse && !root._isDisabled && !root._isRunning
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
