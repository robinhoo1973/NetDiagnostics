import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── TypeTextAnimation.qml — 图标内文字打字机逐字点亮 ─────────────────────
// 5WHY (2026-08-23 用户裁定 "IP Configure 的动画更新为类似打字一样逐个
// 出现 1.1.1.1 的字符"): IP Configuration 曾挂通用 Jiggle——图标本体是
// 深色屏幕 + Hershey「1.1.1.1」，摆动与内容无关联。改为逐字符点亮：
// 字形 = 母版数字路径逐字拆分（AnimationTokens.js glyphTypeSets，单一
// 来源同 wifiWaveArcSets 约定），经 data-URI SVG 复绘着色（WifiWave/
// BarsCycle 同机制，QSvgPlugin 已显式导入）。
//
// 时序：第 i 字符在 i×stepMs 瞬现（打字机无淡入），全部出现后保持
// holdMs，清屏 clearMs 后循环。周期 = N×step + hold + clear
// （ip-config: 7×240+520+200 = 2400 = replayWindowMs）。
// 点亮色 = primary（IconProvider 对 #B00001 槽位的深墨/白字之上，
// 蓝色逐字点亮在双主题屏幕灰底上均可读；检测结束静态数字原样可见）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    // 打字点亮色：primary 蓝（暗/亮主题各自角色值，随 ThemeEngine 解析）
    readonly property string _inkHex: {
        var c = T.ThemeEngine.colors.primary.toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }
    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.glyphTypeSets[root.iconName] || null) : null
    readonly property int _count: _set ? _set.chars.length : 0
    readonly property int _step: Tokens.tokens.typeTextStepMs
    readonly property int _holdMs: Tokens.tokens.typeTextHoldMs
    readonly property int _clearMs: Tokens.tokens.typeTextClearMs

    function _charUri(i) {
        if (!_set) return ""
        var tf = _set.tf && _set.tf !== "" ? '<g transform="' + _set.tf + '">' : ""
        var close = tf !== "" ? '</g>' : ""
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' width="24" height="24" fill="none" stroke-linecap="round"'
                + ' stroke-linejoin="round">' + tf
                + '<path d="' + _set.chars[i].d + '" stroke="#' + _inkHex
                + '" stroke-width="' + _set.sw + '" fill="none"/>' + close + '</svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }

    Repeater {
        id: rep
        model: root._count
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
                // 打字机瞬现（无淡入）：i×step 前隐藏
                PauseAnimation { duration: index * root._step }
                PropertyAction { target: glyphImg; property: "opacity"; value: 1 }
                // 与后续字符一起保持到周期尾部
                PauseAnimation {
                    duration: Math.max(0, root._count * root._step + root._holdMs
                        - index * root._step)
                }
                // 清屏段全体熄灭 → 循环
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
        for (var i = 0; i < _count; ++i) {
            var d = rep.itemAt(i)
            if (d) d.opacity = 0
        }
    }
}
