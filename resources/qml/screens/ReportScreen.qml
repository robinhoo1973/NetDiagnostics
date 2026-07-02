import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter ReportPreviewScreen 1:1 — with AppBar ─────────────────────
Item {
    id: page
    objectName: "report"
    property bool hasResults: appState.totalCompleted > 0
    property string lastPath: ""
    property bool lastFailed: false
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // Built-in preview overlay state
    property string previewFormat: ""     // "pdf" | "html"
    property string previewHtml: ""
    property bool previewVisible: false
    property string toast: ""             // transient status message

    function openPreview(fmt) {
        if (!hasResults) return
        previewFormat = fmt
        // PDF = one-page summary; HTML = full detail. Both render as rich text.
        previewHtml = appState.buildReportHtml(fmt === "html")
        previewVisible = true
    }
    function requestExport(fmt) { if (hasResults) appState.requestSavePath(fmt) }
    function doShare(fmt) {
        if (!appState.isPremium) { page.toast = Tr.premiumRequiredMsg; toastTimer.restart(); return }
        appState.shareReport(fmt)
        page.previewVisible = false
    }

    Timer { id: toastTimer; interval: 3500; onTriggered: page.toast = "" }

    Connections {
        target: appState
        function onSavePathPicked(format, path) {
            var saved = (format === "pdf") ? appState.exportPdf(path) : appState.exportHtml(path)
            page.lastFailed = (saved === "")
            page.lastPath = saved
        }
        function onPremiumRequired() { page.toast = Tr.premiumRequiredMsg; toastTimer.restart() }
        function onReportShared(ok) { page.toast = ok ? Tr.reportShareOk : Tr.reportShareFail; toastTimer.restart() }
    }

    // AppBar (Flutter: Scaffold.appBar with "Report Preview" title)
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 52; color: "#1A1A2E"
        border { width: 1; color: "#3A3A5A" }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "report"; size: 20; color: Theme.cyan }
            Item { width: 10 }
            Label { text: Tr.reportPreview; font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
        }
    }

    // Centered content (Flutter: Center > Padding(40) > Column)
    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentHeight: reportCol.implicitHeight

        ColumnLayout {
            id: reportCol
            width: Math.min(500, parent.width - 80)
            anchors.centerIn: parent
            spacing: 0

            // Icon container (Flutter: 100x100, borderRadius 24, bg cyan8%, border cyan20%)
            Rectangle {
                Layout.preferredWidth: 100; Layout.preferredHeight: 100
                Layout.alignment: Qt.AlignHCenter
                radius: 24; color: Qt.alpha(Theme.cyan, 0.08)
                border { width: 1.5; color: Qt.alpha(Theme.cyan, 0.2) }
                AppIcon { anchors.centerIn: parent; name: "report"; size: 48; color: Qt.alpha(Theme.cyan, 0.6) }
            }
            Item { Layout.preferredHeight: 24 }

            // Title
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: Tr.reportPreview
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 22; font.weight: Font.DemiBold; color: Theme.textPrimary
            }
            Item { Layout.preferredHeight: 12 }

            // Subtitle
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: page.hasResults ? Tr.reportExportHint : Tr.reportRunFirst
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 14; color: Qt.alpha(Theme.textSecondary, 0.6)
                horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5
            }
            Item { Layout.preferredHeight: 24 }

            // Preview buttons (open the built-in preview window)
            ColumnLayout { spacing: 10; Layout.fillWidth: true
                ExportButton { iconName: "report"; label: Tr.reportPreviewPdfBtn; accent: Theme.cyan; onClicked: page.openPreview("pdf") }
                ExportButton { iconName: "globe"; label: Tr.reportPreviewHtmlBtn; accent: Theme.accentBlue; onClicked: page.openPreview("html") }
                Label {
                    visible: page.toast !== "" || page.lastPath !== "" || page.lastFailed
                    Layout.fillWidth: true; Layout.topMargin: 4
                    text: page.toast !== "" ? page.toast
                          : (page.lastFailed ? Tr.reportExportFailed : (Tr.reportSavedTo + " " + page.lastPath))
                    color: page.lastFailed ? Theme.failRed : (page.toast !== "" ? Theme.cyan : Theme.passGreen)
                    font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere; horizontalAlignment: Text.AlignHCenter
                }
            }
            Item { Layout.preferredHeight: 32 }

            // Status indicator (Flutter: padding h16 v10, borderRadius 8, conditional color)
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: statusRow.implicitWidth + 32; implicitHeight: 40; radius: 8
                color: hasResults ? Qt.alpha(Theme.passGreen, 0.1) : Qt.alpha(Theme.warnYellow, 0.1)
                border { width: 1; color: hasResults ? Qt.alpha(Theme.passGreen, 0.3) : Qt.alpha(Theme.warnYellow, 0.3) }
                RowLayout {
                    id: statusRow
                    anchors.centerIn: parent
                    AppIcon { name: hasResults ? "badge-check" : "badge-info"; size: 12; color: "white" }
                    Item { width: 8 }
                    Label {
                        text: hasResults ? appState.totalCompleted + Tr.reportResultsAvailable : Tr.reportNoResults
                        font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12; color: hasResults ? Theme.passGreen : Theme.warnYellow
                    }
                }
            }
            Item { Layout.preferredHeight: 40 }
        }
    }

    // ── Built-in report preview overlay (PDF summary / full HTML) ──────
    Rectangle {
        id: previewOverlay
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        color: "#AA000000"
        visible: page.previewVisible
        z: 1000
        MouseArea { anchors.fill: parent } // absorb background clicks

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(720, parent.width - 24)
            height: Math.min(parent.height - 40, 760)
            radius: 12; color: "#252538"
            border { width: 1.5; color: "#4A4A6A" }

            ColumnLayout {
                anchors { fill: parent; margins: 12 }
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    AppIcon { name: page.previewFormat === "pdf" ? "report" : "globe"; size: 18; color: Theme.cyan }
                    Item { width: 8 }
                    Label {
                        Layout.fillWidth: true
                        text: page.previewFormat === "pdf" ? Tr.previewPdfTitle : Tr.previewHtmlTitle
                        font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        implicitWidth: 28; implicitHeight: 28; radius: 14; color: "#E94560"
                        AppIcon { anchors.centerIn: parent; name: "close"; size: 14; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: page.previewVisible = false }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: "white"; radius: 6; clip: true
                    Flickable {
                        anchors { fill: parent; margins: 12 }
                        clip: true
                        contentWidth: width
                        contentHeight: previewText.implicitHeight
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        Text {
                            id: previewText
                            width: parent.width
                            text: page.previewHtml
                            textFormat: Text.RichText
                            color: "#111111"
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    PreviewBtn { label: Tr.reportSaveBtn; accent: Theme.textSecondary
                        onClicked: { page.previewVisible = false; page.requestExport(page.previewFormat) } }
                    Item { Layout.fillWidth: true }
                    PreviewBtn {
                        label: page.isMobile ? Tr.shareBtn : Tr.emailBtn
                        accent: Theme.cyan
                        locked: !appState.isPremium
                        onClicked: page.doShare(page.previewFormat)
                    }
                }
            }
        }
    }

    component PreviewBtn: Rectangle {
        id: pbtn
        property string label: ""
        property color accent: Theme.cyan
        property bool locked: false
        signal clicked()
        implicitWidth: pbtnRow.implicitWidth + 28; implicitHeight: 40; radius: 8
        color: Qt.alpha(accent, 0.12)
        border { width: 1; color: Qt.alpha(accent, 0.4) }
        RowLayout {
            id: pbtnRow
            anchors.centerIn: parent; spacing: 6
            Label {
                text: pbtn.label + (pbtn.locked ? "  " + Tr.premiumBadge : "")
                color: Theme.textPrimary
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12; font.weight: Font.Medium
            }
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: pbtn.clicked() }
    }

    component ExportButton: Rectangle {
        id: btn
        property string iconName: ""
        property string label: ""
        property color accent: Theme.cyan
        signal clicked()
        Layout.fillWidth: true
        implicitHeight: 48; radius: 10
        opacity: page.hasResults ? 1.0 : 0.4
        color: Qt.alpha(accent, 0.10)
        border { width: 1; color: Qt.alpha(accent, 0.35) }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: btn.iconName; size: 18; color: btn.accent }
            Item { width: 12 }
            Label { Layout.fillWidth: true; text: btn.label; color: Theme.textPrimary
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 13; font.weight: Font.Medium }
        }
        MouseArea { anchors.fill: parent; enabled: page.hasResults
            cursorShape: Qt.PointingHandCursor; onClicked: btn.clicked() }
    }
}
