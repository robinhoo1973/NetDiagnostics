// =============================================================================
// SpinnerIcon.qml — 主题化运行 spinner（替换 QQC2 BusyIndicator）
//
// 5WHY (2026-08-23 用户 "运行时无 spinner"): QQC2 BusyIndicator 以控件
// palette.text 着色——全仓自定义令牌主题从未设置控件调色板，dark 主题下
// 深色圆盘在深色底上不可见（桌面/iOS 同款）。改为令牌着色 AppIcon +
// RotationAnimation 匀速旋转：属性动画路线与 JiggleAnimation 同源，iOS
// 静态构建安全；颜色随主题切换由 image://icon 重染。
// =============================================================================
import QtQuick
import theme
import widgets

AppIcon {
    id: root
    name: "activity"
    color: ThemeEngine.colors.primary
    RotationAnimation on rotation {
        from: 0
        to: 360
        duration: 1100
        loops: Animation.Infinite
        running: root.visible
    }
}
