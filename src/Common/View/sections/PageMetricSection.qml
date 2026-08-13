// PageMetricSection.qml — DetailPage 关键指标（page-detail.md §2.3；KM + MetricCard）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import detail
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain
    bottomMargin: ThemeEngine.spacing.sm
    minHeight: 72

    property var detailData: ({})

    // 自门控：KeyMetric 单一来源（与 DiagBlock 瓦片同源）
    readonly property var _km: root.detailData
        ? KeyMetric.keyMetric(root.detailData.data, root.detailData.durationMs || 0)
        : ({ ok: false, value: 0, unitKey: "", labelKey: "", precision: 0, format: "num", trailing: "" })
    readonly property bool _hasMetric: root._km.ok
    active: root._hasMetric

    Loader {   // 5WHY UI-4：sourceComponent 保留绑定活性，detail 切换值不过期
        Layout.fillWidth: true
        Layout.fillHeight: true
        active: root._hasMetric
        sourceComponent: MetricCard {
            label: T.tr(root._km.labelKey)
            value: root._km.value
            unit: T.tr(root._km.unitKey)
            precision: root._km.precision
            format: root._km.format
            trailing: root._km.trailing
            accentColor: ThemeEngine.colors.primary
        }
    }
}
