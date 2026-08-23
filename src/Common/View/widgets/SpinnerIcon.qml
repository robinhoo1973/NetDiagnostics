// =============================================================================
// SpinnerIcon.qml — 主题化运行 spinner（历史实现对齐）
//
// 5WHY (2026-08-23 用户 "spinner 与历史不同"): 历史版（0.0.3）运行指示为
// 专用 "spinner" 图标（30% 底圆 + 90° 弧段，整体旋转）+ 历史 cyan 着色 +
// 1000ms/圈，停止时 rotation 归零；重构后图标集未收录 spinner 母版，替代
// 轮曾用 "activity"（脉搏图）造成视觉不一致。已按历史恢复：fff 母版
// spinner.svg 从历史包拷回并重跑管线，本组件同步历史参数。
// =============================================================================
import QtQuick
import theme
import widgets

AppIcon {
    id: root
    name: "spinner"
    // 历史运行态着色 = cyan（Palette.js tertiary 即原 cyan）
    color: ThemeEngine.colors.tertiary
    RotationAnimation on rotation {
        from: 0
        to: 360
        duration: 1000
        loops: Animation.Infinite
        running: root.visible
        // 历史语义：停止即归零，避免复用组件时残留旋转角
        onStopped: root.rotation = 0
    }
}
