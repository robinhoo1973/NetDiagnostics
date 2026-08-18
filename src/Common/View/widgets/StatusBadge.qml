// StatusBadge.qml — 状态徽标（statusCode 驱动集中色板/图标，归档原样移植）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

RowLayout {
    property int statusCode: -1
    property color accent: statusCode >= 0 && statusCode < ThemeEngine.statusColors.length
        ? ThemeEngine.statusColors[statusCode]
        : ThemeEngine.colors.skip
    property string iconName: statusCode >= 0 && statusCode < ThemeEngine.statusIconNames.length
        ? ThemeEngine.statusIconNames[statusCode]
        : "badge-info"
    property int count: 0
    // 5WHY (复核 2026-08-18 窄屏溢出): 7 徽标行在 320dp 手机上溢出 ~30px。
    // compact 模式缩小图标(14→12)/字号(12→11)——每徽标 ~32px→~28px，
    // 7 徽标簇 ≈220px 可容纳于 296px 内容宽。桌面保持全尺寸。
    property bool compact: false
    // UX 评审 2：零计数隐藏——初始/运行早期不显示 " 0" 徽标
    visible: count > 0
    spacing: 2

    AppIcon { name: iconName; size: compact ? 12 : 14; color: accent }
    Label {
        text: ThemeEngine.pad2(count)
        // 5WHY (复核 2026-08-18 令牌化): compact 分支的 11/12 裸数字是
        // fontSize.caption/mono 的重复硬编码——字号表变更时静默脱节。
        font.family: ThemeEngine.monoFont
        font.pixelSize: compact ? ThemeEngine.fontSize.caption : ThemeEngine.fontSize.mono
        font.weight: Font.Bold; color: accent
    }
}
