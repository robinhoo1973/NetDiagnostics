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
    // 5WHY (复核 2026-08-18 动画批量抖动): 网格序号——完成动画错峰延迟用。
    property int staggerIndex: 0
    // 5WHY (复核 2026-08-19 效率): 屏幕可见性由面板经网格链注入（StackView
    // 隐藏页不销毁瓦片，且 Item.visible 是局部属性、不随祖先隐藏传播——
    // 仅靠 root.visible 门控抓不到离屏页）。默认 true 保持独立使用兼容。
    property bool screenVisible: true
    // 5WHY (复核 2026-08-19 viewport 门控): 内容 Flickable 注入——滚动出
    // 视口的运行瓦片停动画（Flickable 裁剪不停动画，visible 也不随滚动
    // 变化，此前是残余空转路径）。
    property var viewportItem: null
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
    // 5WHY (复核 2026-08-19 效率): 无可见性门控的无限动画在离屏瓦片上持续
    // tick——StackView 隐藏整屏时运行中瓦片的 DiagAnimator（GeoLocate 3 路
    // 无限 SequentialAnimation、Meter 60fps 定时器）在低功耗 ARM 板上空耗
    // CPU。screenVisible（屏幕注入）+ root.visible（局部）+ _inViewport
    // （滚动视口）三级门控：隐藏页/显式隐藏/滚出视口即停。
    readonly property bool _inViewport: {
        // 5WHY (复核 2026-08-19 空转早退): 无运行语义时直接假——空闲滚动
        // 不为 ~45 瓦片 × mapToItem 祖先链遍历付费（视口门控的成本只在
        // 运行中发生）。
        if (!root.testRunning) return false
        var vp = root.viewportItem
        if (!vp) return true
        var cy = vp.contentY   // 依赖读取：滚动时重估本绑定
        // 5WHY (复核 2026-08-19 依赖缺口): mapToItem 是函数调用、不参与绑定
        // 依赖追踪——面板头增删（首条结果落地徽标行出现）把网格整体下移时
        // contentY 与 root.y（瓦片→网格局部坐标）均不变，视口判定残留旧值：
        // 刚移出视口的运行瓦片继续空转、刚移入的停摆，直到下一次滚动。
        // contentHeight 随 bodyCol 隐式高变化（头增删必经），一并读取补齐
        // 该重估触发；min(height, contentHeight) 同时覆盖内容未满视口情形。
        var ch = vp.contentHeight   // 依赖读取：上方内容增删改变布局时重估本绑定
        var ty = root.y        // 依赖读取：网格重排时重估本绑定
        var top = root.mapToItem(vp, 0, 0).y
        // 5WHY (复核 2026-08-19 边界迟滞): 边界精确判定在滚动震荡下反复翻转
        // _isRunning——运行中动画的 Loader 每越过边界一次就销毁/重建并
        // 从 0 相位重放（qrc 重载 + 重实例化成本高于被省下的空转）。两侧
        // 各加 80px（≈1 瓦片）迟滞带：震荡不翻转，仅少量越界瓦片继续
        // 空转（可接受换取稳定）。
        var hys = 80
        return top + root.height + hys > 0 && top < Math.min(vp.height, ch) + hys
    }
    readonly property bool _isRunning: root.testRunning && !root._isDisabled
        && !root.isDone && root.visible && root.screenVisible && root._inViewport
    // C2：瓦片标签必须经 T.diagName（15 语言响应式）——直读 C++ 英文 label 会让
    // 语言切换对 45 个瓦片失效；label 作回退。
    readonly property string _label: (itemData.diagId !== undefined
        ? (T.diagName(itemData.diagId) || itemData.label || "")
        : (itemData.label || ""))
    // ── 瓦片级图标缩放（5WHY review round 4 — 用户诉求"图形以瓦片尺寸显示"）──
    // 图标随 blockSize 派生（M3 keyline 比经 ThemeEngine 令牌），不再用 compact
    // 二值（原 80-160px 瓦片恒渲染 32/44px 小框）。
    // 5WHY (复核 2026-08-18, 用户诉求"检测项图片外不需要圆角方框按钮区"):
    // 0.66×瓦片恢复——几何复核证明图标井圆与计时圆点圆从不重叠（最小圆心
    // 距差 5.2px@80px 瓦片），此前 0.54 收缩基于错误的包围盒计算，详见
    // ThemeEngine.tileIconRatio 令牌注释。
    readonly property int _iconSize: Math.max(ThemeEngine.tileIconMin,
                                              Math.round(root.blockSize * ThemeEngine.tileIconRatio))
    // 左上角指示（计时圆点/完成结果图标）尺寸：随瓦片缩放、夹紧 16-24px
    readonly property int _timerDotSize: Math.min(24, Math.max(16, Math.round(root.blockSize * 0.185)))
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
    // 圆光晕合成色（复核 2026-08-18: 圆角方垫→归档圆形光晕，半径 width/2；
    // light 加深仅用于 IconTints 烘焙值——完成态状态色本身是加深变体）
    // 5WHY (复核 2026-08-18): _padTint 随 IconPad 移除后成死属性（零读取），
    // 且每次求值重复一次 IconTints 字符串查找——已删除。
    readonly property color _glowColor: {
        if (root.isDone) return Qt.alpha(root._statusColor, 0.14)
        if (root._isRunning) return Qt.alpha(ThemeEngine.colors.primary, 0.10)
        // 5WHY (复核 2026-08-18 重复收敛): light 加深规则消费 ThemeEngine.effTint
        // 单一 helper（IconPad 同源）——规则改动只落一处。
        var t = ThemeEngine.effTint(ThemeEngine.iconPadTint(root._iconName), true)
        return Qt.alpha(t, ThemeEngine.colors.iconPadAlpha)
    }

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
    Timer {
        id: elapsedTimer
        interval: 1000; repeat: true
        running: _isRunning
        onTriggered: root._elapsed++
        // 5WHY (复核 2026-08-18): 运行状态翻转即清零——完成态计时圆点已让位
        // 给结果图形（用户诉求），完成后 _elapsed 无任何消费方；停止路径
        // （中断/异常未落结果）同样清零，冻结计数不再残留。
        onRunningChanged: {
            // 5WHY (复核 2026-08-19 离屏续计): 离屏门控（screenVisible/visible）
            // 暂停计时器时同样触发本处理器——切页往返把运行中瓦片计时归零
            // （与 startedAtMs 真实起点设计相悖）。仅当运行语义真正结束
            // （testRunning 归假）才清零；离屏暂停保留进度，回屏续计。
            if (!root.testRunning) root._elapsed = 0
        }
    }
    // 5WHY (复核 2026-08-19 冻结徽标): 瓦片离屏（视口/切页）期间运行语义
    // 结束（取消/异常无结果）时，计时器本已暂停——running 无翻转，上述
    // 处理器不触发，_elapsed 冻结值 + startedAtMs>0 令计时圆点永久残留
    // （"中断/异常未落结果…冻结计数不再残留"契约落空）。补 testRunning
    // 归假路径直接清零（离屏暂停不改变 testRunning，续计不受影响）。
    onTestRunningChanged: {
        if (!root.testRunning) root._elapsed = 0
    }
    // 5WHY (复核 2026-08-18): 委托重建（reloadModel 换模型身份）会把 _elapsed
    // 清零。真实起点经模型注入 startedAtMs（C++ DiagnosticBase 墙钟）——重建
    // 后计时从真实起点反推，并行 Suite 兄弟结果落地不再重置显示；无起点
    // （模型未注入时）回退本地计数。
    Component.onCompleted: {
        var ms = root.itemData.durationMs
        if (ms !== undefined && ms > 0) root._elapsed = Math.round(ms / 1000)
    }
    readonly property real _startedAtMs: root.itemData.startedAtMs !== undefined
        ? root.itemData.startedAtMs : 0
    // 5WHY (复核 2026-08-20 Date.now 不可追踪): startedAtMs 注入后本绑定
    // 只读 Date.now()/startedAtMs——两者均不参与 QML 依赖追踪，每秒 _elapsed
    // tick 不再触发重估，长探针（180s）计时圆点冻结在注入时刻值。显式读
    // _elapsed 作依赖钩（与 _inViewport 同习语）。
    readonly property int _timerSecs: {
        var tick = root._elapsed   // 依赖读取：每秒重估
        if (root._startedAtMs > 0)
            return Math.max(1, Math.floor((Date.now() - root._startedAtMs) / 1000))
        return Math.max(1, tick)
    }
    readonly property bool _timerVisible: !root.isDone && (root._startedAtMs > 0 || root._elapsed > 0)
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
            card.scale = 0.94; settleAnim.restart()
        }
    }
    SequentialAnimation {
        id: settleAnim
        // 5WHY (复核 2026-08-18 动画批量抖动): 首段 PauseAnimation 按网格序号
        // 错峰（30ms×序号，封顶 300ms）——并行 Suite 完成 burst 时 ~45 瓦片
        // 的 settle+光晕动画不再同帧齐发。
        PauseAnimation { duration: Math.min(root.staggerIndex, 10) * 30 }
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

        // ── 图标圆形光晕（最先声明=底层；5WHY review round 4: 旧顺序垫盖住
        // 计时圆点——圆点/结果图标/状态条在其上绘制）
        // 5WHY (复核 2026-08-18, 用户诉求): 圆角方框按钮区（IconPad squircle
        // ≈0.92×瓦片）删除——回归归档视觉，检测项图形直接用圆形光晕承载
        // （半径 width/2，无边框），不再有"小一号方框"感。
        Rectangle {
            id: iconWell
            anchors.centerIn: parent
            width: root._iconSize; height: root._iconSize
            radius: width / 2
            color: root._glowColor
            Behavior on color { ColorAnimation { duration: 200 } }

            // 检测项图形（归档：井内死居中、最上层 z:2；瓦片级缩放）
            AppIcon {
                id: blockIcon
                z: 2
                anchors.centerIn: parent
                name: root._iconName
                size: root._iconSize
                color: root._iconColor
                // 5WHY (verify 2026-08-17): 完成态图标保持油墨色——状态信号由
                // 光晕色 + 底部状态条 + 左上结果图标三重呈现，图标变状态色会
                // 削弱 light 主题可读性（fail 红黑混叠）。
            }
            // 终端协议闪烁光标（仅运行中显示；几何对齐 SVG 静态下划线）
            Rectangle {
                id: terminalCursor
                visible: root._isTerminalIcon && root._isRunning
                width: root._termCursorW * root._iconSize / 24
                height: Math.max(1.5, root._iconSize * 1.2 / 24)
                x: iconWell.width / 2 + (root._termCursorX - 12) * root._iconSize / 24
                y: iconWell.height / 2 + root._termCursorYOff * root._iconSize / 24 - height / 2
                // 5WHY (复核 2026-08-19 浅色可读): 曾用 _iconColor——light 下
                // iconInk #0C4A6E 压在深屏 #0F172A 上仅 1.89:1，光标不可见。
                // terminalInk 与图标管线终端文字槽同源（Palette.js 单一事实源）。
                color: ThemeEngine.colors.terminalInk
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    // 5WHY (review 2026-08-17): 45 个瓦片仅 3 个终端图标显示
                    // 光标——未门控的无限动画在不可见瓦片上持续 tick
                    running: root._isTerminalIcon && root._isRunning && root.visible
                    NumberAnimation { from: 1.0; to: 0.15; duration: 530; easing.type: Easing.Linear }
                    NumberAnimation { from: 0.15; to: 1.0; duration: 530; easing.type: Easing.Linear }
                }
            }
            // 运行动画（animType → 九动画，DiagAnimator 调度）
            DiagAnimator {
                anchors.fill: parent
                diagId: itemData.diagId !== undefined ? itemData.diagId : -1
                running: root._isRunning
                targetItem: iconWell
            }
        }

        // ── 左上角指示区 ──
        // 5WHY (复核 2026-08-18, 用户诉求): 计时圆点贴死左上角（边距 8→4，
        // 尺寸随瓦片缩放 16-24px），且仅运行中显示；检测结束该位置由结果
        // 图形取代（右上角状态角标随之删除——同一信息不再双处呈现）。
        Rectangle {
            visible: root._timerVisible
            anchors { top: parent.top; left: parent.left; margins: 4 }
            width: root._timerDotSize; height: root._timerDotSize; radius: width / 2
            color: Qt.alpha(root._timerColor, 0.16)
            Label {
                anchors.centerIn: parent
                text: root._timerSecs
                font.family: ThemeEngine.monoFont
                font.pixelSize: root._timerDotSize >= 20 ? ThemeEngine.fontSize.micro : 8
                font.weight: Font.Bold
                color: root._timerColor
            }
        }
        // ── 完成态结果图形（占用原计时圆点位置；与圆点同尺寸同锚点）──
        AppIcon {
            visible: root.isDone
            anchors { top: parent.top; left: parent.left; margins: 4 }
            name: root._statusIcon
            size: root._timerDotSize
            color: root._statusColor
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
