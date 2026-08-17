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
    property color iconColor: ThemeEngine.colors.primary
    property int iconSize: 44

    radius: width * 0.28
    color: Qt.alpha(root.tint, ThemeEngine.colors.iconPadAlpha)
    border { width: 1; color: Qt.alpha(root.tint, ThemeEngine.colors.iconPadBorderAlpha) }

    AppIcon {
        anchors.centerIn: parent
        name: root.iconName
        size: root.iconSize
        color: root.iconColor
    }
}
