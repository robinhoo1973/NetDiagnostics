// =============================================================================
// DetailSections.qml — 详情页七区块单一装配点
// =============================================================================
// 5WHY (2026-09-27 双装配点): 七 Section 清单曾两处字面量维护（PageDetailSheet
// 与 DetailPage），漂移已发生一次（Toast 接线）。单一装配组件：宿主只传
// detailData；Section 增删/重排/契约变更只改这一处。
import QtQuick
import QtQuick.Layouts
import sections as S
import theme

ColumnLayout {
    id: root
    property var detailData: ({})
    readonly property var _data: detailData ? detailData : ({})
    spacing: ThemeEngine.spacing.sm

    S.PageHeroSection {
        Layout.fillWidth: true
        detailData: root._data
    }
    S.PageSummarySection {
        Layout.fillWidth: true
        detailData: root._data
    }
    S.PageMetricSection {
        Layout.fillWidth: true
        detailData: root._data
    }
    S.PageErrorSection {
        Layout.fillWidth: true
        detailData: root._data
    }
    S.PagePropertiesSection {
        Layout.fillWidth: true
        detailData: root._data
    }
    S.PageChartsSection {
        Layout.fillWidth: true
        detailData: root._data
    }
    S.PageTerminalSection {
        Layout.fillWidth: true
        detailData: root._data
    }
}
