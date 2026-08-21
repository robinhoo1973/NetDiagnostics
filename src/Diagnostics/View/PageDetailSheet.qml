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
    // 5WHY (2026-08-22, issue 1): 头部返回/拷贝栏必须是屏幕顶端的导航栏——
    // 贴边无 margin 且内容滚动时固定。原实现头部放在滚动 Flickable 内、外层
    // Card 带 12px 内边距——改为 Plain（零内边距）+ 头部钉在滚动区外。
    backgroundStyle: PageSection.Plain

    property var detailData: ({})
    readonly property var _data: detailData ? detailData : ({})
    signal backRequested()

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        S.PageDetailHeaderSection {
            Layout.fillWidth: true
            title: root._data.displayName || ""
            // 5WHY (review round 2, UX+PM): 头部不展示诊断图标——hero 光晕垫
            // 是唯一身份元素（page-detail.md §2.1/§2.2）。
            onBackRequested: root.backRequested()
        }
        // 7-5：全窗详情需滚动（内容可超视口高度）——仅内容区滚动
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
}
