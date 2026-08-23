import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── TermTypeAnimation.qml — 终端命令行打字（SSH/FTP/TELNET）──────────────
// 5WHY (2026-08-23 用户复核裁定 "打字不是下面协议名大字，是 > 提示符后
// 显示；闪烁下划线是 > 旁的那个 _"): 前版另起一根光标条（x10.6 处
// 1.4×0.9u）排在静态 _ 右侧——观者读到"下划线之后又一根新下划线"且
// 光条位 y12.3 偏下一行，语义歧义。修正为命令行惯例：> 后唯一光标就是
// 静态 _ 本身——以屏幕底色覆盖条显隐令 _ 闪烁（覆盖=熄灭），打字字符
// 紧随 _ 右缘（起笔 ~10.7）逐字瞬现；全词保持 holdMs、清屏 clearMs。
//
// 几何：静态 _ = 母版 M6.4 12.1 H9.8（三终端同规格）；覆盖条 x6.2/y11.6
// 起 3.8×1.2u（比 _ 外扩 0.3u 兜住抗锯齿）。屏幕底色 = termScreenColors
// （FIXED_COLORS #B00001 双主题槽同源）。字形 = Hershey 逐字预解析
// （tokens termTypeSets），data-URI SVG 复绘 primary（WifiWave 同机制）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    readonly property string _inkHex: {
        var c = T.ThemeEngine.colors.primary.toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }
    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.termTypeSets[root.iconName] || null) : null
    readonly property int _n: _set ? _set.chars.length : 0
    readonly property real _half: Tokens.tokens.termBlinkHalfMs
    readonly property int _holdMs: Tokens.tokens.termHoldMs
    readonly property int _clearMs: Tokens.tokens.termClearMs
    readonly property real _u: root.width / 24
    // 周期 = 两拍/字 × N + 保持 + 清屏
    readonly property int _period: Math.round(2 * _half) * _n + _holdMs + _clearMs
    // 覆盖条颜色 = 终端屏幕底色（token 双主题）
    readonly property color _screen: T.ThemeEngine.isDark
        ? Tokens.tokens.termScreenColors.dark : Tokens.tokens.termScreenColors.light

    function _charUri(i) {
        if (!_set) return ""
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' fill="none" stroke-linecap="round" stroke-linejoin="round">'
                + '<path d="' + _set.chars[i].d + '" stroke="#' + _inkHex
                + '" stroke-width="1.1" fill="none"/></svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }

    // ── 静态 _ 本身闪烁：屏幕底色覆盖条显隐（覆盖 = 下划线熄灭）────────
    Rectangle {
        id: underscoreCover
        visible: root._n > 0
        x: root._u * 6.2
        y: root._u * 11.6
        width: root._u * 3.8
        height: root._u * 1.2
        color: root._screen
        opacity: 0

        SequentialAnimation {
            id: blinkSeq
            loops: Animation.Infinite
            PropertyAction { target: underscoreCover; property: "opacity"; value: 0 }
            PauseAnimation { duration: root._half }
            PropertyAction { target: underscoreCover; property: "opacity"; value: 1 }
            PauseAnimation { duration: root._half }
        }
        RestartController {
            running: root.running && root._n > 0
            target: blinkSeq
            onStopped: underscoreCover.opacity = 0
        }
    }

    // ── 字符：紧随 _ 右缘逐字瞬现（打字机无淡入，位置固定不随光标移动）──
    Repeater {
        id: rep
        model: root._n
        delegate: Image {
            id: glyphImg
            anchors.fill: parent
            sourceSize.width: 96
            sourceSize.height: 96
            smooth: true
            mipmap: true
            source: root._charUri(index)
            opacity: 0

            SequentialAnimation {
                id: charSeq
                loops: Animation.Infinite
                PauseAnimation { duration: 2 * root._half * (index + 1) }
                PropertyAction { target: glyphImg; property: "opacity"; value: 1 }
                PauseAnimation {
                    duration: Math.max(0, root._period - 2 * root._half * (index + 1)
                                             - root._clearMs)
                }
                PropertyAction { target: glyphImg; property: "opacity"; value: 0 }
                PauseAnimation { duration: root._clearMs }
            }
            RestartController {
                running: root.running
                target: charSeq
                onStopped: glyphImg.opacity = 0
            }
        }
    }

    function resetVisuals() {
        for (var i = 0; i < _n; ++i) {
            var d = rep.itemAt(i)
            if (d) d.opacity = 0
        }
        underscoreCover.opacity = 0
}}