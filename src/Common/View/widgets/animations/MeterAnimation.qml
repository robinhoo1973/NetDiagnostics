import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── MeterAnimation.qml — 表针左右摆动（Internet Connectivity & Speed）──
// 表针底部固定在表盘圆心，模拟检测采样：每轮随机取一个左侧角度，从左
// 摆到右侧随机角度，到位后小幅回摆稳定，保持片刻再开新一轮。
// 纯 Rectangle + Rotation；运动由 Timer 逐帧驱动（无 QML NumberAnimation
// 绑定——对随机变化目标值会产生绑定环/递归重启，见 5WHY）。
// 无 Canvas/ShapePath/ShaderEffect（iOS 静态 Qt 安全）。
// 表盘圆心对齐 internet 母版 gauge 表盘（viewBox ≈(19.2,10.6)/24）。
//
// Usage: MeterAnimation { anchors.fill: parent; running: testRunning }

Item {
    id: root
    property bool running: false
    // Unused by Meter (draws own content) — see BounceAnimation 5WHY.
    property var targetItem: null
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"
    property int sweepDuration: Tokens.tokens.meterSweepDuration

    // 表盘圆心（internet 母版 gauge 表盘中心，非图标中心）
    readonly property real _cx: parent.width * 0.80
    readonly property real _cy: parent.height * 0.44
    readonly property real _needleLen: parent.width * 0.30
    readonly property real _stroke: Math.max(1.6, parent.width * 2.4 / 24)
    readonly property real _hub: Math.max(2.4, parent.width * 3.2 / 24)

    // 状态机：_phaseSwing 扫掠 → _phaseSettle 回摆稳定 → 保持 → 新一轮
    property real _angle: 0
    property real _fromAngle: -40
    property real _toAngle: 40
    property bool _phaseSwing: false
    property bool _phaseSettle: false
    property real _t0: 0

    function _easeInOutQuad(x) {
        return x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2
    }

    function _startSwing() {
        // 每轮随机"从左到右"的采样角度（左 -55..-18，右 18..55）
        root._fromAngle = -(18 + Math.random() * 37)
        root._toAngle   =  18 + Math.random() * 37
        root._angle = root._fromAngle
        root._phaseSwing = true
        root._phaseSettle = false
        root._t0 = 0
    }

    // ── 指针：底部锚定表盘圆心，绕底中旋转 ────────────────────────────
    Rectangle {
        id: needle
        width: root._stroke
        height: root._needleLen
        radius: root._stroke / 2
        color: root.accentColor
        x: root._cx - width / 2
        y: root._cy - height
        transform: Rotation {
            origin.x: width / 2
            origin.y: height
            angle: root._angle
        }
    }

    // ── 轴心 ───────────────────────────────────────────────────────────
    Rectangle {
        width: root._hub; height: root._hub
        radius: root._hub / 2
        color: root.accentColor
        x: root._cx - width / 2
        y: root._cy - height / 2
    }

    // 5WHY: 不用 NumberAnimation.to 绑定随机目标——序列内 ScriptAction 改 to
    // 会触发动画重启 → 重入 ScriptAction → 栈溢出；外部 Timer 改 to → 绑定环
    // 警告。改为纯 Timer 逐帧插值，零绑定、零警告。
    Timer {
        id: driver
        interval: 16   // ~60fps
        repeat: true
        running: root.running
        onTriggered: {
            root._t0 += 16
            if (root._phaseSwing) {
                var t = Math.min(1, root._t0 / root.sweepDuration)
                root._angle = root._fromAngle
                             + (root._toAngle - root._fromAngle) * root._easeInOutQuad(t)
                if (t >= 1) {
                    root._phaseSwing = false
                    root._phaseSettle = true
                    root._t0 = 0
                }
            } else if (root._phaseSettle) {
                // 阻尼回摆：围绕 _toAngle 的衰减振荡（~260ms）
                var st = root._t0 / 260
                if (st >= 1) {
                    root._angle = root._toAngle
                    root._phaseSettle = false
                    root._t0 = 0
                } else {
                    root._angle = root._toAngle + 5 * Math.sin(st * Math.PI * 2) * (1 - st)
                }
            } else if (root._t0 >= 260) {
                // 保持片刻后开启新一轮采样
                root._t0 = 0
                root._startSwing()
            }
        }
    }

    onRunningChanged: {
        if (root.running) {
            root._startSwing()
        } else {
            root._angle = 0
            root._phaseSwing = false
            root._phaseSettle = false
            root._t0 = 0
        }
    }
}
