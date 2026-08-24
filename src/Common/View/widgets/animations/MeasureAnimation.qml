import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── MeasureAnimation.qml — MTU Discovery 尺寸箭头伸缩 ────────────────────
// 5WHY (2026-08-23 用户裁定 "MTU 不用 Jiggle——伸缩下面的箭头长短来展示
// 动画"): MTU 曾挂通用 Jiggle 摆动——与 2026-08-23 新母版（包封 + 底部
// 尺寸双箭头）无关联。修正为工程制图卡尺语义：底部尺寸线两端箭头自中线
// 同步外伸至满幅再回缩，循环往复（外伸/回缩各 measureExtend/RetractMs，
// InOutQuad）。
//
// 几何锚定母版（24 系逐字，母版位移改此处）：尺寸线 M4 18.8 H20（中线
// x12、满幅半长 8）；左头 M4 18.8 l2.5±2.5（> 形）、右头 M20 18.8
// l-2.5±2.5（< 形）；延长线 x6.8/17.2 y13.2..16.3。轴 = 纯 Rectangle
// 宽度动画；箭头头 = data-URI SVG 复绘（WifiWave 同机制，iOS 静态 Qt
// 安全）。DIAG_ACCENT(g4-mtu) = warning 琥珀（与母版 accent 同源）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    accentColor: T.ThemeEngine.colors.warning

    readonly property int _extendMs: Tokens.tokens.measureExtendMs
    readonly property int _retractMs: Tokens.tokens.measureRetractMs
    readonly property real _u: root.width / 24
    readonly property real _cy: 18.8 * _u            // 尺寸线 y
    readonly property real _minLen: 1.2 * _u         // 最短半长
    readonly property real _maxLen: 8.0 * _u         // 满幅半长（x4..20 中点 12）
    readonly property real _shaftH: Math.max(1.2, root.width * 1.6 / 24)
    property real _len: _minLen                      // 动画写入（非 readonly）

    readonly property string _hex: {
        var c = T.ThemeEngine.colors.warning.toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }

    function _headUri(left) {
        var d = left ? "M4 18.8 l2.5 -2.5 M4 18.8 l2.5 2.5"
                     : "M20 18.8 l-2.5 -2.5 M20 18.8 l-2.5 2.5"
        return "data:image/svg+xml;base64," + Qt.btoa(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
            + ' width="24" height="24" fill="none" stroke-linecap="round"'
            + ' stroke-linejoin="round">'
            + '<path d="' + d + '" stroke="#' + _hex
            + '" stroke-width="1.6" fill="none"/></svg>')
    }

    // ── 轴：自中线向两侧伸缩 ──────────────────────────────────────────────
    // 用户裁定（2026-08-23 #13）：伸缩时母版静态双头箭头须隐藏——以垫底色
    // 覆盖条盖住箭头带（轴线 y18.8 ± 翼展），停止时露出静态原样。
    Rectangle {
        visible: root.running
        x: root._u * 1.5
        y: root._u * 16.4
        width: root._u * 21
        height: root._u * 4.6
        color: T.ThemeEngine.colors.surfaceContainerLow
    }
    Rectangle {
        id: leftShaft
        x: root.width / 2 - root._len
        y: root._cy - height / 2
        width: root._len
        height: root._shaftH
        radius: height / 2
        color: root.accentColor
    }
    Rectangle {
        id: rightShaft
        x: root.width / 2
        y: root._cy - height / 2
        width: root._len
        height: root._shaftH
        radius: height / 2
        color: root.accentColor
    }

    // ── 箭头头：固定在两轴尖端随 _len 移动（viewBox 4/20 点对齐尖端）──────
    Image {
        id: leftHead
        width: root.width
        height: root.height
        x: root.width / 2 - root._len - root._u * 4
        y: 0
        sourceSize.width: 96
        sourceSize.height: 96
        smooth: true
        mipmap: true
        source: root._headUri(true)
    }
    Image {
        id: rightHead
        width: root.width
        height: root.height
        x: root.width / 2 + root._len - root._u * 20
        y: 0
        sourceSize.width: 96
        sourceSize.height: 96
        smooth: true
        mipmap: true
        source: root._headUri(false)
    }

    SequentialAnimation {
        id: seq
        loops: Animation.Infinite
        NumberAnimation {
            target: root; property: "_len"
            from: root._minLen; to: root._maxLen
            duration: root._extendMs
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: root; property: "_len"
            from: root._maxLen; to: root._minLen
            duration: root._retractMs
            easing.type: Easing.InOutQuad
        }
    }
    RestartController {
        running: root.running
        target: seq
        onStopped: root._len = root._minLen
    }

    function resetVisuals() { root._len = root._minLen }
}
