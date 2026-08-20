// PageHeroSection.qml — DetailPage 英雄卡（page-detail.md §2.2）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    topMargin: ThemeEngine.spacing.lg
    bottomMargin: ThemeEngine.spacing.sm
    minHeight: 80
    cardRadius: ThemeEngine.radius.lg

    property var detailData: ({})   // NEW-14：data 遮蔽内建属性，统一 detailData

    // 5WHY (review round 2+3): 垫只承载真实诊断图标——_hasDiagIcon 门控
    // 可见性，无需回退分支（回退状态徽标会与状态圆盘同图形异色冲突）。
    readonly property bool _hasDiagIcon: detailData.iconName !== undefined && detailData.iconName !== ""
    // 5WHY (复核 2026-08-19 单点派生): 状态默认值三元曾在 4 处逐字复制、
    // statusColors 表达式两处（色盘 + 状态名文字）——加状态/改回退须全改
    // 且一处漏改即色盘与文字异色。收敛为 _status/_statusColor 单一派生。
    readonly property int _status: detailData.status !== undefined ? detailData.status : 5
    readonly property color _statusColor: ThemeEngine.statusColors[root._status] || ThemeEngine.colors.skip
    // 5WHY (复核 2026-08-19): 状态名经 statusRows 单一表派生（色盘读屏名 +
    // 可见状态文字共用；v0.0.3 有状态名文字行，恢复对等呈现）。
    readonly property string _statusLabel: {
        for (var i = 0; i < ThemeEngine.statusRows.length; ++i)
            if (ThemeEngine.statusRows[i].code === root._status)
                return ThemeEngine.statusRows[i].labelKey
        return "summaryInfo"
    }

    // 5WHY (2026-08-19 用户诉求 "进入详情也播放检测项动画"): 瓦片动画由
    // DiagBlock 承载，hero 只有静态 IconPad——进详情时检测项身份动效缺失。
    // 业界惯例（详情页身份动效）：进入/切换检测项时有界窗口回放一次瓦片
    // 同款动画（DiagAnimator 复用，animType 由 diagId 经 C++ 单一来源解析，
    // 与瓦片同源不漂移）。
    // 5WHY (复核 2026-08-19): 有界窗口收敛进 DiagAnimator（bounded/restart，
    // 窗口不再消费方硬编码）；触发源唯一化——onCompleted 臂在两类消费方
    // （DetailPage createObject 后赋值 / Sheet 预建绑定）均不生效，删除，
    // 仅 onDetailDataChanged 重放。
    readonly property int _diagId: (detailData && detailData.diagId !== undefined)
        ? detailData.diagId : -1
    // 浮层/页面复用：detailData 切换（打开另一检测项详情）即重放
    // 5WHY (复核 2026-08-19 陈旧读取): 门控曾读 _diagId——属性变更处理器
    // 先于依赖绑定重估执行（DiagAnimator 注释同款已证顺序），首次赋值时
    // 读到陈旧 -1 → 重放永不触发。改读变更源 detailData 本身（处理器运行时
    // 已是新值）。
    onDetailDataChanged: {
        var d = root.detailData
        if (d && d.diagId !== undefined && d.diagId >= 0) heroAnim.restart()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: ThemeEngine.spacing.md

        // 45 图标全彩常显：诊断图标光晕垫（共享 IconPad——tint 由组件从
        // iconName 派生，调用方不再维护 _padTint）
        IconPad {
            id: iconWell
            visible: root._hasDiagIcon
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            iconName: detailData.iconName || ""
            iconSize: 40
            iconColor: ThemeEngine.colors.iconInk

            // 检测项动画有界回放（5WHY 2026-08-19，见 _diagId）
            // 5WHY (复核 2026-08-19): 曾缺 targetItem——Jiggle 类动画
            // （IP 配置/网络档案/TCP 设置/DNS 缓存/MTU/HTTP 压缩/HTTP
            // 计时/邮件等 8 项）门控 `running && targetItem !== null`
            // 恒不启动，重放特性对其静默失效。补与瓦片同款 iconWell。
            // 5WHY (复核 2026-08-19 锚点空间): 动画几何按图标框计量
            // （GeoLocate 针头锚点 / WifiWave 弧组焦点），但垫 56px > 图标 40px
            // ——直接填垫把锚点画错位（针头偏移 ~3px、环溢出垫外）。动画
            // 层与 AppIcon 同框：DiagAnimator 自身承载几何（居中 + iconSize
            // 正方形），不另设匿名包装层。
            DiagAnimator {
                id: heroAnim
                // 堆叠契约（与 DiagBlock 同源）：动画覆盖层高于图标层——
                // 此处 AppIcon 无显式 z（0），声明顺序已在上层；显式 z:1
                // 固化契约，防止未来图标加 z 后重演 DiagBlock 的遮挡缺陷。
                z: 1
                anchors.centerIn: parent
                width: iconWell.iconSize; height: iconWell.iconSize
                diagId: root._diagId
                bounded: true
                targetItem: iconWell
                // 锚点按图标分派（与 DiagBlock 同契约）
                iconName: detailData.iconName || ""
            }
        }

        // 状态圆盘（5WHY 复核 2026-08-19 v0.0.3 对等 + a11y）: v0.0.3 详情
        // 浮层有状态名文字行（"Pass/Warning/Fail" + 时长）——现仅色盘传达
        // 状态身份，色盲/读屏用户丢失该信息。补状态名文字 + 读屏名。
        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: 22
            color: Qt.alpha(root._statusColor, 0.14)
            AppIcon {
                anchors.centerIn: parent
                name: detailData.statusIcon || ThemeEngine.statusIconNames[root._status] || "badge-info"
                size: 24
                color: root._statusColor
            }
            Accessible.name: T.tr(root._statusLabel)
            Accessible.role: Accessible.Graphic
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: detailData.displayName || ""
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.title
                font.weight: Font.Bold
                color: ThemeEngine.colors.onSurface
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            // v0.0.3 对等：状态名文字（色盲/读屏可达的状态身份，5WHY 见
            // _statusLabel 派生）
            Label {
                text: T.tr(root._statusLabel)
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.caption
                font.weight: Font.DemiBold
                color: root._statusColor
            }
            Label {
                visible: text !== ""
                text: detailData.summary || ""
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                color: ThemeEngine.colors.onSurfaceVariant
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
            Label {
                visible: detailData.durationMs !== undefined && detailData.durationMs > 0
                text: ThemeEngine.formatDuration(detailData.durationMs || 0)
                font.family: ThemeEngine.monoFont
                font.pixelSize: ThemeEngine.fontSize.caption
                color: ThemeEngine.colors.textMuted
            }
        }
    }
}
