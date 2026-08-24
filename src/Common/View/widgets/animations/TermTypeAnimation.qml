import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── TermTypeAnimation.qml — 终端协议名打字（SSH/FTP/TELNET）──────────────
// 5WHY (2026-08-23 v2 复核 用户澄清): 其一，闪烁的"下划线" = 图标里 >
// 提示符边上的静态 _（x6.4..9.8, y12.1）——动画以 primary 高亮条覆盖其上
// 明灭；其二，打字对象 = 图标底部一行的协议名大字（Hershey「FTP/SSH/
// TELNET」，y16.1..19.0，母版 #B00005 路径按 x 聚类拆分为字母），每两拍
// 瞬现一个字母，全部出现后保持、清屏循环。v1 的"提示符行后打印+光标推进"
// 理解废弃。
//
// 字形 = termTypeSets（母版逐字事实单一来源）；data-URI SVG 复绘 primary
// （压过静态 terminalInk 字色 → 逐字点亮，WifiWave 同机制）。时序：
// 光标恒闪（半周期 220ms 不停）；第 i 字母于 2×220×(i+1) ms 瞬现；
// 全词后保持 holdMs、清屏 clearMs。
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
    readonly property int _n: _set ? _set.letters.length : 0
    readonly property real _half: Tokens.tokens.termBlinkHalfMs
    readonly property int _holdMs: Tokens.tokens.termHoldMs
    readonly property int _clearMs: Tokens.tokens.termClearMs
    readonly property real _u: root.width / 24
    // 周期 = 两拍/字 × N + 保持 + 清屏
    readonly property int _period: Math.round(2 * _half) * _n + _holdMs + _clearMs

    function _letterUri(i) {
        if (!_set) return ""
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' fill="none" stroke-linecap="round" stroke-linejoin="round">'
                + '<path d="' + _set.letters[i].d + '" stroke="#' + _inkHex
                + '" stroke-width="0.85" fill="none"/></svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }

    // ── 光标高亮条：覆盖静态 _，恒常明灭 ─────────────────────────────────
    Rectangle {
        id: cursor
        visible: root._n > 0 && root.running
        x: root._u * 6.15
        y: root._u * 11.55
        width: root._u * 3.9
        height: root._u * 1.1
        radius: root._u * 0.35
        color: root._inkHex

        SequentialAnimation {
            id: blinkSeq
            loops: Animation.Infinite
            PropertyAction { target: cursor; property: "opacity"; value: 0.9 }
            PauseAnimation { duration: root._half }
            PropertyAction { target: cursor; property: "opacity"; value: 0 }
            PauseAnimation { duration: root._half }
        }
        RestartController {
            running: root.running && root._n > 0
            target: blinkSeq
        }
    }

    // ── 协议名字母：每两拍瞬现一个 ────────────────────────────────────────
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
            source: root._letterUri(index)
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
    }
}
