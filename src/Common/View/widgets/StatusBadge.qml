// StatusBadge.qml — 状态徽标（statusCode 驱动集中色板/图标，归档原样移植）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

RowLayout {
    property int statusCode: -1
    property color accent: statusCode >= 0 && statusCode < ThemeEngine.statusColors.length
        ? ThemeEngine.statusColors[statusCode]
        : ThemeEngine.colors.skipGray
    property string iconName: statusCode >= 0 && statusCode < ThemeEngine.statusIconNames.length
        ? ThemeEngine.statusIconNames[statusCode]
        : "badge-info"
    property int count: 0
    // UX 评审 2：零计数隐藏——初始/运行早期不显示 " 0" 徽标
    visible: count > 0
    spacing: 2

    AppIcon { name: iconName; size: 14; color: accent }
    Label {
        text: ThemeEngine.pad2(count)
        font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold; color: accent
    }
}
