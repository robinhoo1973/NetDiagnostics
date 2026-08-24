import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── FlashLabelAnimation.qml — 数据库名称闪烁（MySQL/PostgreSQL/Redis/MongoDB）
// 5WHY (2026-08-23 用户裁定 "数据库图标不要用 Pulse——通过闪烁数据库名称
// 来展示动画"): 四 DB 图标曾挂通用 Pulse 呼吸光晕——与图标内容（页面上的
// 数据库名）无关联。修正：以页面底色覆盖条在名称标签 bbox 上显隐——
// 名称闪烁（覆盖=名称熄灭），与 FlashContent 同拍。bbox = chromium 实测
// master #B00003 名称行（labelFlashSets 单一来源）；覆盖色 = FIXED_COLORS
// #B00001 页面底双主题槽（coverDark/coverLight）。
// 纯 Rectangle + opacity —— 无 Canvas/ShapePath/ShaderEffect（iOS 静态
// Qt 安全，同 GeoLocate 契约）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.labelFlashSets[root.iconName] || null) : null
    readonly property int _onMs: Tokens.tokens.labelFlashOnMs
    readonly property int _offMs: Tokens.tokens.labelFlashOffMs
    readonly property int _times: Tokens.tokens.labelFlashTimes
    readonly property int _restMs: Tokens.tokens.labelFlashRestMs
    // 覆盖色 = 页面底色（双主题槽；缺表时回退 #007CC9/#0094F5）
    readonly property color _cover: T.ThemeEngine.isDark
        ? (_set ? _set.coverDark : "#007CC9")
        : (_set ? _set.coverLight : "#0094F5")

    Rectangle {
        id: cover
        visible: root._set !== null
        x: root._set ? root._set.rect.x / 24 * root.width : 0
        y: root._set ? root._set.rect.y / 24 * root.height : 0
        width: root._set ? root._set.rect.w / 24 * root.width : 0
        height: root._set ? root._set.rect.h / 24 * root.height : 0
        color: root._cover
        opacity: 0

        // 用户裁定（2026-08-23）：闪烁恰好 5 次后长休整再循环
        SequentialAnimation {
            id: blinkSeq
            loops: Animation.Infinite
            PropertyAction { target: cover; property: "opacity"; value: 0 }
            SequentialAnimation {
                loops: root._times
                PauseAnimation { duration: root._onMs }
                PropertyAction { target: cover; property: "opacity"; value: 1 }
                PauseAnimation { duration: root._offMs }
                PropertyAction { target: cover; property: "opacity"; value: 0 }
            }
            PauseAnimation { duration: root._restMs }
        }
        RestartController {
            running: root.running
            target: blinkSeq
            onStopped: cover.opacity = 0
        }
    }

    function resetVisuals() { cover.opacity = 0 }
}
