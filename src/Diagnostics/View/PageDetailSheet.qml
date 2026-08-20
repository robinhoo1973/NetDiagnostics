// =============================================================================
// PageDetailSheet.qml — Detail 内容表单（P1 概念验证：六区块按契约装配）
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import sections as S
import theme

PageSection {
    id: root
    backgroundStyle: PageSection.Card

    property var detailData: ({})
    readonly property var _data: detailData ? detailData : ({})
    signal backRequested()

    // 7-5：全窗详情需滚动（内容可超视口高度）
    Flickable {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        contentHeight: sheetCol.implicitHeight
        ColumnLayout {
            id: sheetCol
            width: parent.width
            spacing: ThemeEngine.spacing.sm

            S.PageDetailHeaderSection {
                Layout.fillWidth: true
                title: root._data.displayName || ""
                // 5WHY (review round 2, UX+PM): 头部不展示诊断图标——hero 光晕垫
                // 是唯一身份元素（page-detail.md §2.1/§2.2）。
                onBackRequested: root.backRequested()
            }
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
    }
}
