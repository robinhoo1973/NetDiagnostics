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
    // 5WHY (2026-09-27 守卫收敛): 曾 _data 中间层再叠一层空守卫——默认值
    // ({}) 已保证 detailData 非空，宿主守卫各自保留一层即可。
    property var detailData: ({})
    spacing: ThemeEngine.spacing.sm

    S.PageHeroSection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
    S.PageSummarySection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
    S.PageMetricSection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
    S.PageErrorSection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
    S.PagePropertiesSection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
    S.PageChartsSection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
    S.PageTerminalSection {
        Layout.fillWidth: true
        detailData: root.detailData
    }
}
