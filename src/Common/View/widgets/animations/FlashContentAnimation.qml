import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── FlashContentAnimation.qml — HTTP 文件图标内容闪烁（HTML 字符恒显）────
// 5WHY (2026-08-23 用户裁定 "HTTP 相关的动画就单纯的闪烁文件图形内部
// 除了 HTML 的字符或图形"): HTTP 三图标（curl-verbose/http-headers/
// security-headers，blankfile 底形家族）曾挂通用 Bounce/Type/Lock——
// 运动与"文件 + 内容 + HTML 文字"构图无关。改为内容层整体闪烁：
// 内部图形 = 母版内容组逐字复绘（flashContentSets，含原 transform，
// HTML Hershey 文本不在集合内 → 恒显），经 data-URI SVG 注入
// onAccentPad 白（双主题恒白，压蓝页可读）。
//
// 着色规则（构建 data-URI 时重写）：stroke="任意" → 闪色；fill="非
// none" → 闪色（圆点/实心点同色）；fill="none" 保持。
//
// 时序：亮相 onMs → 熄灭 offMs 循环。静态内容恒在底层，熄灭段即静态
// 原样——"内容闪一下高亮、HTML 不动"语义。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    readonly property string _flashHex: {
        var c = T.ThemeEngine.colors.onAccentPad.toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }
    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.flashContentSets[root.iconName] || null) : null
    readonly property int _onMs: Tokens.tokens.flashOnMs
    readonly property int _offMs: Tokens.tokens.flashOffMs

    function _uri() {
        if (!_set) return ""
        var body = _set.body
            .replace(/stroke="[^"]*"/g, 'stroke="#' + _flashHex + '"')
            .replace(/fill="[^"]*"/g, function(m) {
                return m.indexOf("none") >= 0 ? m : 'fill="#' + _flashHex + '"'
            })
        var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                + ' width="24" height="24" fill="none" stroke-linecap="round"'
                + ' stroke-linejoin="round"><g transform="' + _set.tf + '">'
                + body + '</g></svg>'
        return "data:image/svg+xml;base64," + Qt.btoa(svg)
    }

    Image {
        id: contentImg
        anchors.fill: parent
        sourceSize.width: 96
        sourceSize.height: 96
        smooth: true
        mipmap: true
        source: root._uri()
        opacity: 0

        SequentialAnimation {
            id: flashSeq
            loops: Animation.Infinite
            PropertyAction { target: contentImg; property: "opacity"; value: 0.95 }
            PauseAnimation { duration: root._onMs }
            PropertyAction { target: contentImg; property: "opacity"; value: 0 }
            PauseAnimation { duration: root._offMs }
        }
        RestartController {
            running: root.running
            target: flashSeq
            onStopped: contentImg.opacity = 0
        }
    }

    function resetVisuals() { contentImg.opacity = 0 }
}
