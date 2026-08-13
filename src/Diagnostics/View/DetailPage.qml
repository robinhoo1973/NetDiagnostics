// =============================================================================
// DetailPage.qml — PageDisplay 子类装配（page-detail.md §3）
//
// 导航：页面不直接访问 StackView（UI-10）——返回/复制经 sectionAction 路由，
// AppContent 监听并 pop / 复制。detail 由推入方注入（AppState.resultFor）。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import sections as S
import theme

PageDisplay {
    id: page
    objectName: "detail"

    property var detail: ({})
    readonly property var resultData: detail ? detail : ({})

    // ── Toast（NEW-7：页面自持）──
    property string toastText: ""
    function showToast(msg) {
        toastText = msg
        toastTimer.restart()
    }
    Timer {
        id: toastTimer
        interval: ThemeEngine.toastDurationMs
        onTriggered: page.toastText = ""
    }

    headerContent: [
        S.PageDetailHeaderSection {
            detail: page.detail
            onBackRequested: page.emitSectionAction("detail", "back", {})
            onCopyRequested: {
                if (page.detail.diagId === undefined) return
                AppState.copyDetailToClipboard(page.detail.diagId)
                page.showToast(T.tr("detailCopied"))
            }
        }
    ]

    bodyContent: [
        S.PageHeroSection { detailData: page.resultData },
        S.PageMetricSection { detailData: page.resultData },
        S.PageErrorSection { detailData: page.resultData },
        S.PagePropertiesSection { detailData: page.resultData },
        S.PageChartsSection { detailData: page.resultData },
        S.PageTerminalSection { detailData: page.resultData }
    ]

    floatingContent: [
        S.PageToastSection { toastText: page.toastText }
    ]

    // 5WHY（C1 栈溢出）：这里曾对 "back" 再次 emitSectionAction —— emitSectionAction
    // 同步重入 onSectionAction → 无限递归崩溃。返回已由 PageDetailHeaderSection
    // 的 backRequested 直接 emitSectionAction，AppContent handlePageAction 监听
    // "back" 并 pop——页面自身不需要任何转发。
}
