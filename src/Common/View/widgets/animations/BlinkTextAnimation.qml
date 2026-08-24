import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── BlinkTextAnimation.qml — 图标内文字整体闪烁 ──────────────────────────
// 5WHY (2026-08-23 用户裁定 "DNS Server 的闪烁 DNS 三个字符"): DNS
// Servers 曾挂通用 Pulse（整图呼吸光晕）——图标显示器内本有 Hershey
// 「DNS」三字母，闪烁对象应为文字本身。字形 = 母版路径逐字拆分
// （glyphBlinkSets，D/N/S 三组单一来源），经 data-URI SVG 复绘为
// onAccentPad 白（双主题恒白，压过静态 accent 蓝/青字色 → 亮相可读），
// 整词同相同步明灭。
//
// 时序：亮相 onMs → 熄灭 offMs 循环（周期 800ms）。静态字母始终在
// 底层，熄灭段即静态原样——"闪一下高亮"语义。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    // 闪烁高亮色：onAccentPad 恒白（Palette 双主题同值）
    readonly property var _set0: root.iconName !== ""
        ? (Tokens.tokens.glyphBlinkSets[root.iconName] || null) : null
    // 可选 per-set flashRole（Palette 角色名）——缺省 onAccentPad 白
    readonly property string _flashHex: {
        var role = (_set0 && _set0.flashRole) ? _set0.flashRole : "onAccentPad"
        var c = T.ThemeEngine.colors[role].toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }
    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.glyphBlinkSets[root.iconName] || null) : null
    readonly property int _count: _set ? _set.letters.length : 0
    readonly property int _onMs: Tokens.tokens.blinkTextOnMs
    readonly property int _offMs: Tokens.tokens.blinkTextOffMs

    function _letterUri(i) {
        if (!_set) return ""
        var tf = _set.tf && _set.tf !== "" ? '<g transform="' + _set.tf + '">' : ""
        var close = tf !== "" ? '</g>' : ""
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' width="24" height="24" fill="none" stroke-linecap="round"'
                + ' stroke-linejoin="round">' + tf
                + '<path d="' + _set.letters[i].d + '" stroke="#' + _flashHex
                + '" stroke-width="' + _set.sw + '" fill="none"/>' + close + '</svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }

    Repeater {
        id: rep
        model: root._count
        delegate: Image {
            id: letterImg
            anchors.fill: parent
            sourceSize.width: 96
            sourceSize.height: 96
            smooth: true
            mipmap: true
            source: root._letterUri(index)
            opacity: 0

            SequentialAnimation {
                id: blinkSeq
                loops: Animation.Infinite
                PropertyAction { target: letterImg; property: "opacity"; value: 0.95 }
                PauseAnimation { duration: root._onMs }
                PropertyAction { target: letterImg; property: "opacity"; value: 0 }
                PauseAnimation { duration: root._offMs }
            }
            RestartController {
                running: root.running
                target: blinkSeq
                onStopped: letterImg.opacity = 0
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
