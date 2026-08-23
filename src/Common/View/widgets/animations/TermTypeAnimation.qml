import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── TermTypeAnimation.qml — 终端命令行打字（SSH/FTP/TELNET）──────────────
// 5WHY (2026-08-23 用户裁定 "下划线不停地闪烁，每闪烁两次用打字机的方式
// 打印出对应协议的一个字符，直到所有字符都打印出来"): 三终端图标曾挂通用
// Type（三横杆闪现）——与「终端 >_ 提示符」构图无关联。改为命令行语义：
// 光标条在静态 _ 之后不停闪烁（半周期 220ms），每两拍（880ms）以 Hershey
// Roman Simplex 真字体打印协议名的一个字符（termTypeSets 逐字预解析，
// cx = 该字符落笔后的光标左缘），全词完成后保持 holdMs、清屏 clearMs。
//
// 几何：起笔 x10.6（紧随静态 _ 右缘）、基线 y12.1（与提示符同线）、字帽
// 2.0u（TELNET 六字全长 ≤22.5 不越框）。字形经 data-URI SVG 复绘 primary
// 色（终端屏 #1E293B/#0F172A 深底双主题可读）；光标条独立无限闪烁——
// "不停地闪烁"不随打字周期重启。
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
    readonly property real _startX: 10.6   // 首字符起笔（静态 _ 右缘之后）

    function _charUri(i) {
        if (!_set) return ""
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' fill="none" stroke-linecap="round" stroke-linejoin="round">'
                + '<path d="' + _set.chars[i].d + '" stroke="#' + _inkHex
                + '" stroke-width="1.1" fill="none"/></svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }
    function _placeCursorAtStart() {
        if (_n > 0 && root.width > 0)
            cursor.x = _startX * _u
    }
    Component.onCompleted: _placeCursorAtStart()
    onWidthChanged: if (!running) _placeCursorAtStart()

    // ── 光标条：恒常闪烁（独立无限循环，不随打字周期重启）────────────────
    Rectangle {
        id: cursor
        visible: root._n > 0
        width: root._u * 1.4
        height: root._u * 0.9
        radius: width * 0.18
        color: root._inkHex
        y: root._u * 12.3

        SequentialAnimation {
            id: blinkSeq
            loops: Animation.Infinite
            PropertyAction { target: cursor; property: "opacity"; value: 0.95 }
            PauseAnimation { duration: root._half }
            PropertyAction { target: cursor; property: "opacity"; value: 0 }
            PauseAnimation { duration: root._half }
        }
        RestartController {
            running: root.running && root._n > 0
            target: blinkSeq
        }
    }

    // ── 字符：每两拍瞬现一个；落笔同时推进光标 ────────────────────────────
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
                PropertyAction {
                    target: cursor; property: "x"
                    value: root._set.chars[index].cx * root._u
                }
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
                onStopped: {
                    glyphImg.opacity = 0
                    root._placeCursorAtStart()
                }
            }
        }
    }

    function resetVisuals() {
        for (var i = 0; i < _n; ++i) {
            var d = rep.itemAt(i)
            if (d) d.opacity = 0
        }
        _placeCursorAtStart()
    }
}
