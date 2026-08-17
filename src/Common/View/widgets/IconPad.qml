// =============================================================================
// IconPad.qml — 图标光晕垫（squircle 圆角方垫 + 同色低透明光晕，无硬染色）
//
// 45 图标全彩常显方案的共享垫：DiagBlock 瓦片与 PageHeroSection hero 共用。
// 5WHY（2026-08-17 review）：两处逐行复制同一光晕垫标记（填充 + 边框 + 居中
// AppIcon），半径/光晕规格改动只落一处时另一处静默漂移；抽出组件后光晕
// 规格只在一处维护。附加覆盖层（终端光标、运行动画等）以子项注入垫内。
// =============================================================================
import QtQuick
import theme
import widgets

Rectangle {
    id: root

    property string iconName: "circle"
    // 5WHY (simplify 2026-08-17): tint 默认从 iconName 经 ThemeEngine.iconPadTint
    // 派生（单一访问点内置进组件）；调用方不再各自维护 _padTint 两步咒语，
    // 漏传也不会静默回退主色。显式赋值仍可覆盖。
    property color tint: ThemeEngine.iconPadTint(root.iconName)
    // iconInk：light 主题深蓝油墨（≈9.5:1 白面），dark 与 primary 同值
    property color iconColor: ThemeEngine.colors.iconInk
    property int iconSize: 44
    // 5WHY (review round 4): 缺 mirror 转发（镜像调用方只能重写光晕垫）且
    // 空名不隐藏（空垫）。隐式尺寸兜底——调用方漏设宽高时不再塌成 0。
    property bool iconMirror: false
    implicitWidth: iconSize + 16
    implicitHeight: iconSize + 16
    visible: iconName !== ""

    radius: width * 0.28
    // 5WHY (review 2026-08-17, 用户诉求 light 可读): tint 表烘焙的是暗色系
    // 主导色——light 白面上 0.12 透明度合成后 ≈1.0:1，光晕垫不可见。
    // light 运行时加深 1.5×（Qt.darker 乘 2/3 亮度），dark 不变。
    readonly property color _effTint: ThemeEngine.isDark ? root.tint
                                                         : Qt.darker(root.tint, 1.5)
    color: Qt.alpha(root._effTint, ThemeEngine.colors.iconPadAlpha)
    border { width: 1; color: Qt.alpha(root._effTint, ThemeEngine.colors.iconPadBorderAlpha) }

    AppIcon {
        anchors.centerIn: parent
        name: root.iconName
        size: root.iconSize
        color: root.iconColor
        mirror: root.iconMirror
    }
}
