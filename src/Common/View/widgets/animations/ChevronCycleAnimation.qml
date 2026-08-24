import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── ChevronCycleAnimation.qml — Proxy 双箭头颜色互换 ─────────────────────
// 5WHY (2026-08-23 用户裁定 "右侧两个 > 符号的颜色循环更替，一个高亮黄
// 一个高亮蓝"): Proxy 曾挂通用 Path（横杆跳点）——与 v4 母版"服务器机架
// + 转发双箭头"构图无关联。改为两枚转发箭头复绘层（chevronCycleSets，
// 路径 = 母版逐字）每 stepMs 互换高亮色：一枚 warning 黄、一枚 primary
// 蓝，交替往复——"流量经代理持续转发"。静态 accent 箭头恒在底层，
// 复绘层不透明覆盖 → 仅见轮换双色。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.chevronCycleSets[root.iconName] || null) : null
    readonly property int _n: _set ? _set.strokes.length : 0
    readonly property int _step: Tokens.tokens.chevronCycleStepMs

    // 高亮双色：warning 黄 / primary 蓝（Palette Dark 角色即时解析）
    readonly property var _palette: [T.ThemeEngine.colors.warning,
        T.ThemeEngine.colors.primary]

    function _hexOf(c) {
        var s = c.toString()
        return s.length > 7 ? s.slice(3) : s.slice(1)
    }
    function _uri(i, palIdx) {
        if (!_set) return ""
        var hex = _hexOf(_palette[palIdx % 2])
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' fill="none" stroke-linecap="round" stroke-linejoin="round">'
                + '<path d="' + _set.strokes[i].d + '" stroke="#' + hex
                + '" stroke-width="' + _set.sw + '" fill="none"/></svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }

    Repeater {
        model: root._n
        delegate: Image {
            id: chevImg
            anchors.fill: parent
            sourceSize.width: 96
            sourceSize.height: 96
            smooth: true
            mipmap: true
            // 起色按 index 错位：任一时刻两箭头各持一色
            property int _palIdx: index
            source: root._uri(index, _palIdx)
            opacity: root._n > 0 ? 1 : 0

            SequentialAnimation {
                id: cycleSeq
                loops: Animation.Infinite
                ScriptAction {
                    script: { chevImg._palIdx = (chevImg._palIdx + 1) % 2 }
                }
                PauseAnimation { duration: root._step }
            }
            RestartController {
                running: root.running
                target: cycleSeq
                onStopped: chevImg._palIdx = index
            }
        }
    }

    function resetVisuals() {}
}
