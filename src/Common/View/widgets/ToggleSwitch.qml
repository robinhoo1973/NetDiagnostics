// ToggleSwitch.qml — 紧凑开关（36×20 轨道 / 16 拇指；设置页标准形态）
// 设计（UI 评审）：原生 Switch 轨道过大（~40px 高），行内垂直对齐失衡；
// 统一为业界设置页惯用的紧凑形态（iOS/tailwind 风格），动画 120-140ms。
import QtQuick
import QtQuick.Controls
import theme

Switch {
    id: control
    implicitWidth: 36
    implicitHeight: 20
    padding: 0
    spacing: 0
    // 几何加固：空 contentItem 消除字体行高对控件高度的影响，
    // 尺寸完全由 36×20 轨道决定（行内垂直对齐确定性修复）。
    contentItem: Item { implicitWidth: 0; implicitHeight: 0 }

    indicator: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        implicitWidth: 36
        implicitHeight: 20
        radius: 10
        color: control.checked ? ThemeEngine.colors.primary
                               : (control.hovered ? Qt.alpha(ThemeEngine.colors.onSurfaceVariant, 0.25)
                                                  : ThemeEngine.colors.outlineVariant)
        border {
            width: 1
            color: control.checked ? ThemeEngine.colors.primary
                                   : ThemeEngine.colors.textMuted
        }
        Behavior on color { ColorAnimation { duration: 120 } }
        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: (parent.height - height) / 2
            width: 16; height: 16; radius: 8
            color: ThemeEngine.colors.onPrimary   // 5WHY (2026-08-22 P2-11): 硬编码白
            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
    }
}
