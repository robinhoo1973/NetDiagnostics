import QtQuick
import "../../theme/AnimationTokens.js" as Tokens

// ── MeterAnimation.qml — 表针往复摆动（Internet Connectivity & Speed）──
// 表针底部固定在图标圆心，模拟检测采样：指针连续往复——每轮从上一轮
// 落点摆向对侧随机角度（左→右→左→右…），到位后小幅回摆稳定，保持片刻
// 再开新一轮；无瞬跳，运动轨迹贴合 gauge（测速表）指针模式。
// 纯 Rectangle + Rotation；运动由 Timer 逐帧驱动（无 QML NumberAnimation
// 绑定——对随机变化目标值会产生绑定环/递归重启，见 5WHY）。
// 无 Canvas/ShapePath/ShaderEffect（iOS 静态 Qt 安全）。
// 5WHY (2026-08-19 用户诉求 "指针轨迹脱离图标圆心"): 旧锚点
// (0.80,0.44) 依据"母版含 gauge 表盘"的假设——逐通道渲染实测（QSvgRenderer
// + 像素聚类）证明 internet 母版（管线 v4 以来字节级未变）是 徽章+白地球
// +三色轨道弧，并无表盘；该锚点落在右侧绿弧顶端，指针在图形外空挥。
// 业界惯例（罗盘/测速针语义）：针轴置于图标圆心，针长覆盖地球半径
// （≈0.42×宽，恰达轨道弧），绕心扫掠——轨迹始终贴合图形本体。
//
// Usage: MeterAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int sweepDuration: Tokens.tokens.meterSweepDuration

    // 5WHY (复核 2026-08-19 单源收敛): 锚点几何（针轴/针长）与 GeoRadar
    // 同机制——C++ AppState.diagAnimationAnchor 单一来源经 DiagAnimator
    // 下发；保留同值默认供直接实例化回退（Usage 注释的用法）。母版 SVG
    // 再生成位移时仅改 C++ 一处，不再 QML 硬编码漂移。
    property real anchorCx: 0.5
    property real anchorCy: 0.5
    property real anchorMaxR: 0.42
    // 针轴 = 图标圆心（internet 母版地球圆心；实测地球半径 ≈0.42×宽）
    readonly property real _cx: parent.width * root.anchorCx
    readonly property real _cy: parent.height * root.anchorCy
    readonly property real _needleLen: parent.width * root.anchorMaxR
    readonly property real _stroke: Math.max(1.6, parent.width * 2.6 / 24)
    readonly property real _hub: Math.max(2.4, parent.width * 3.6 / 24)

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
        // 5WHY (2026-08-19 用户诉求 "指针不按 gauge 模式往复"):
        // 旧实现每轮固定"左随机角 → 右随机角"单向扫掠——落到右侧后
        // 直接瞬跳回左侧再右摆：右→左腿永远缺席，观感是"单摆 + 瞬移"
        // 而非测速表指针往复。业界惯例（转速表/测速表指针）：指针停在
        // 哪侧，下一轮就从该落点连续摆向对侧——双向连续扫掠、零瞬跳。
        // 5WHY (复核 2026-08-20 窗口起始瞬跳): 首轮曾"左随机角→右随机角"——
        // 复位态指针在 0°，首个驱动 tick 从 0° 瞬跳 ~18-55° 到左侧起点，
        // 每次回放窗口开头可见一次 snap。首轮改从 0° 连续摆出（起点即
        // 当前静息角），后续轮保持落点→对侧连续往复。
        if (root._angle === 0) {
            root._fromAngle = 0
            root._toAngle   = Math.random() < 0.5
                ? -(18 + Math.random() * 37)
                :  (18 + Math.random() * 37)
        } else {
            root._fromAngle = root._angle   // 起点 = 上轮落点（连续往复）
            root._toAngle   = (root._angle >= 0)
                ? -(18 + Math.random() * 37)   // 右侧落点 → 摆向左侧随机角
                :  (18 + Math.random() * 37)   // 左侧落点 → 摆向右侧随机角
        }
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

    // 5WHY (复核 2026-08-19 基类契约): 本文件的 onRunningChanged 声明会遮蔽
    // AnimationBase 的同名处理器（QML 信号处理器不链式继承）——基类的
    // "!running → resetVisuals()" 钩子对本类型永不触发。重置臂显式收敛进
    // resetVisuals() 覆写，与基类钩子契约一致（行为不变）。
    onRunningChanged: {
        if (root.running) root._startSwing()
        else resetVisuals()
    }
    // 5WHY (复核 2026-08-20 创建即真): 属性变更处理器不响应创建期初值——
    // 以 running:true 直接实例化（Usage 契约）时针不动约 260ms 才被
    // 计时器兜底启动。onCompleted 补启动判定（与 GeoLocate 同模式）。
    Component.onCompleted: if (root.running) root._startSwing()
    function resetVisuals() {
        root._angle = 0
        root._phaseSwing = false
        root._phaseSettle = false
        root._t0 = 0
    }
}
