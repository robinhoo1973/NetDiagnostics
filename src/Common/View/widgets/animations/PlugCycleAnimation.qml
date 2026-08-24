import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── PlugCycleAnimation.qml — Active Connections 双插头靠拢/分开 ──────────
// 5WHY (2026-08-23 用户裁定 "两个插头图形不停的靠拢再分开"): Active
// Connections 曾挂通用 Path（横杆跳点）——与 Tabler plug-connected 母版
// （两插头咬合）无运动关联。改为两插头组件（母版路径按上右/下左分组，
// plugCycleSets 逐字）沿咬合轴往复平移：分开 → 靠拢咬合 → 分开循环。
// 平移 = data-URI 整组 Image 的 x/y 绑定（纯 Image + 属性绑定，iOS 安全）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    readonly property var _set: root.iconName !== ""
        ? (Tokens.tokens.plugCycleSets[root.iconName] || null) : null
    readonly property real _u: root.width / 24
    readonly property string _inkHex: {
        var c = T.ThemeEngine.colors.iconInk.toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }
    readonly property string _softHex: {
        var c = T.ThemeEngine.colors.onSurfaceVariant.toString()
        return c.length > 7 ? c.slice(3) : c.slice(1)
    }
    // 咬合轴单位向量（右上 ↔ 左下对角）
    readonly property real _ax: 0.7071
    readonly property real _ay: -0.7071
    readonly property real _amp: root._u * 1.3   // 单侧最大位移

    // 0..1 相位：0=分开(±amp)，1=咬合(0)
    property real _t: 0
    SequentialAnimation on _t {
        running: root.running && root._set !== null
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 650; easing.type: Easing.InOutQuad }
        NumberAnimation { from: 1; to: 0; duration: 650; easing.type: Easing.InOutQuad }
    }
    onRunningChanged: if (!running) _t = 0

    Repeater {
        model: root._set ? root._set.groups.length : 0
        delegate: Image {
            id: grpImg
            anchors.fill: parent
            sourceSize.width: 96
            sourceSize.height: 96
            smooth: true
            mipmap: true
            source: {
                if (!root._set) return ""
                var g = root._set.groups[index]
                var body = g.body
                    .replace(/__C__/g, root._softHex)
                    .replace(/#FFFFFF/g, root._inkHex)
                var svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
                        + ' fill="none" stroke-linecap="round" stroke-linejoin="round">'
                        + body + '</svg>'
                return "data:image/svg+xml;base64," + Qt.btoa(svg)
            }
            // 上右组沿 +轴向右上方退开；下左组反向
            property real _sgn: index === 0 ? -1 : 1
            transform: Translate {
                x: root._ax * root._amp * root._t * grpImg._sgn * -1
                y: root._ay * root._amp * root._t * grpImg._sgn * -1
            }
        }
    }

    function resetVisuals() { _t = 0 }
}
