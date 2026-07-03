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
    // A report may only be generated once the run has FINISHED — not while it is
    // still running (RunStatus.Running === 1), which would produce a partial report.
    readonly property bool isRunning: appState.runStatus === 1
    readonly property bool canReport: hasResults && !isRunning
    property string lastPath: ""
    property bool lastFailed: false
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // Built-in preview overlay state
    property string previewFormat: ""     // "pdf" | "html"
    property string previewHtml: ""
    property bool previewVisible: false
    property string toast: ""             // transient status message

    // Share/subscription flow state:
    //   0 = no dialog, 1 = subscription prompt (not premium), 2 = confirm share (premium)
    property int shareStage: 0
    property string pendingShareFormat: ""

    function openPreview(fmt) {
        if (!canReport) return
        previewFormat = fmt
        // PDF = one-page summary; HTML = full detail. Both render as rich text.
        previewHtml = appState.buildReportHtml(fmt === "html")
        previewVisible = true
    }
    function requestExport(fmt) { if (canReport) appState.requestSavePath(fmt) }
    // Share button entry: check subscription. Not subscribed → guide to subscribe;
    // subscribed → ask for confirmation before sharing. Same logic on iOS/Android.
    function doShare(fmt) {
        pendingShareFormat = fmt
        shareStage = appState.isPremium ? 2 : 1
    }
    // User confirmed sharing (premium path). Hand off to the OS share sheet / mail.
    function confirmShare() {
        var fmt = pendingShareFormat
        shareStage = 0
        previewVisible = false
        appState.shareReport(fmt)
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
        // Subscription just succeeded while the prompt was open → advance to the
        // confirmation step so the flow continues seamlessly (subscribe → confirm → share).
        function onPremiumChanged() {
            if (page.shareStage === 1 && appState.isPremium) page.shareStage = 2
        }
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

    // Centered content — scrollable; a holder keeps the column vertically centered
    // when it fits and lets it scroll when it's taller than the viewport (portrait),
    // so nothing is clipped off-screen.
    Flickable {
        id: reportFlick
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentWidth: width
        contentHeight: reportCol.implicitHeight + (page.isMobile ? 30 : 52)
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: reportCol
            x: (reportFlick.width - width) / 2
            y: page.isMobile ? 14 : 24
            width: Math.min(440, reportFlick.width - (page.isMobile ? 32 : 80))
            spacing: 0

            // Icon container
            Rectangle {
                Layout.preferredWidth: page.isMobile ? 72 : 100; Layout.preferredHeight: page.isMobile ? 72 : 100
                Layout.alignment: Qt.AlignHCenter
                radius: 24; color: Qt.alpha(Theme.cyan, 0.08)
                border { width: 1.5; color: Qt.alpha(Theme.cyan, 0.2) }
                AppIcon { anchors.centerIn: parent; name: "report"; size: page.isMobile ? 36 : 48; color: Qt.alpha(Theme.cyan, 0.6) }
            }
            Item { Layout.preferredHeight: page.isMobile ? 14 : 24 }

            // Title
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: Tr.reportPreview
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: page.isMobile ? 19 : 22; font.weight: Font.DemiBold; color: Theme.textPrimary
            }
            Item { Layout.preferredHeight: page.isMobile ? 8 : 12 }

            // Subtitle
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: page.isRunning ? Tr.runningDots
                      : (page.hasResults ? Tr.reportExportHint : Tr.reportRunFirst)
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 14; color: Qt.alpha(Theme.textSecondary, 0.6)
                horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5
            }
            Item { Layout.preferredHeight: page.isMobile ? 16 : 24 }

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
            Item { Layout.preferredHeight: page.isMobile ? 18 : 32 }

            // Status indicator — running / results-ready / no-results
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: statusRow.implicitWidth + 32; implicitHeight: 40; radius: 8
                color: page.isRunning ? Qt.alpha(Theme.cyan, 0.1)
                       : (hasResults ? Qt.alpha(Theme.passGreen, 0.1) : Qt.alpha(Theme.warnYellow, 0.1))
                border { width: 1; color: page.isRunning ? Qt.alpha(Theme.cyan, 0.3)
                       : (hasResults ? Qt.alpha(Theme.passGreen, 0.3) : Qt.alpha(Theme.warnYellow, 0.3)) }
                RowLayout {
                    id: statusRow
                    anchors.centerIn: parent
                    AppIcon { name: page.isRunning ? "spinner" : (hasResults ? "badge-check" : "badge-info"); size: 12; color: "white" }
                    Item { width: 8 }
                    Label {
                        text: page.isRunning ? Tr.runningStatus
                              : (hasResults ? appState.totalCompleted + Tr.reportResultsAvailable : Tr.reportNoResults)
                        font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 12
                        color: page.isRunning ? Theme.cyan : (hasResults ? Theme.passGreen : Theme.warnYellow)
                    }
                }
            }
            Item { Layout.preferredHeight: page.isMobile ? 16 : 40 }
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
            // Responsive: on tall screens use 720×700, on short (portrait) screens scale down
            width: Math.min(720, parent.width - 20)
            height: Math.max(300, Math.min(700, parent.height - 60))
            radius: 12; color: "#1F1F32"
            border { width: 2; color: "#5A5A7A" }

            ColumnLayout {
                anchors { fill: parent; margins: 12 }
                spacing: 10
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: headerRow.implicitHeight + 16
                    color: Qt.alpha(Theme.cyan, 0.08)
                    radius: 8
                    RowLayout {
                        id: headerRow
                        anchors { fill: parent; margins: 8 }
                        AppIcon { name: page.previewFormat === "pdf" ? "report" : "globe"; size: 20; color: Theme.cyan }
                        Item { width: 8 }
                        Label {
                            Layout.fillWidth: true
                            text: page.previewFormat === "pdf" ? Tr.previewPdfTitle : Tr.previewHtmlTitle
                            font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 16; font.weight: Font.Bold; color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            implicitWidth: 32; implicitHeight: 32; radius: 16; color: Qt.alpha("#FF5577", 0.2)
                            AppIcon { anchors.centerIn: parent; name: "close"; size: 16; color: "#FF6688" }
                            MouseArea { anchors.fill: parent; onClicked: page.previewVisible = false; cursorShape: Qt.PointingHandCursor }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: "#F8F9FA"; radius: 8; clip: true
                    border { width: 1; color: "#E5E7EB" }
                    Flickable {
                        anchors { fill: parent; margins: 14 }
                        clip: true
                        contentWidth: width
                        contentHeight: previewText.implicitHeight
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            width: 6
                            contentItem: Rectangle { color: "#A0A0B8"; radius: 3 }
                        }
                        Text {
                            id: previewText
                            width: parent.width
                            text: page.previewHtml
                            textFormat: Text.RichText
                            color: "#1A1A2E"
                            wrapMode: Text.WordWrap
                            font.pixelSize: 13
                            font.family: "Helvetica"
                            lineHeight: 1.35
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.topMargin: 4
                    PreviewBtn {
                        Layout.fillWidth: true
                        label: page.isMobile ? Tr.shareBtn : Tr.emailBtn
                        accent: Theme.cyan
                        locked: !appState.isPremium
                        onClicked: page.doShare(page.previewFormat)
                    }
                }
            }
        }
    }

    // ── Subscription / share-confirmation dialog ──────────────────────
    // shareStage 1 = not subscribed → guide to subscribe.
    // shareStage 2 = subscribed → confirm, then share. Identical on iOS/Android.
    Rectangle {
        id: shareDialog
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        color: "#AA000000"
        visible: page.shareStage !== 0
        z: 1100
        MouseArea { anchors.fill: parent } // absorb background clicks

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 40)
            implicitHeight: dlgCol.implicitHeight + 40
            radius: 14; color: "#1F1F32"
            border { width: 1.5; color: "#4A4A6A" }

            ColumnLayout {
                id: dlgCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
                spacing: 14

                // Icon badge — lock/info for subscribe, report for confirm
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 60; height: 60; radius: 30
                    color: Qt.alpha(page.shareStage === 1 ? Theme.warnYellow : Theme.cyan, 0.12)
                    border { width: 1.5; color: Qt.alpha(page.shareStage === 1 ? Theme.warnYellow : Theme.cyan, 0.35) }
                    AppIcon {
                        anchors.centerIn: parent
                        name: page.shareStage === 1 ? "badge-info" : "report"
                        size: 28
                        color: page.shareStage === 1 ? Theme.warnYellow : Theme.cyan
                    }
                }
                // Title
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: page.shareStage === 1 ? Tr.subscribeTitle : Tr.confirmShareTitle
                    font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
                    font.pixelSize: 17; font.weight: Font.Bold; color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                // Body
                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: page.shareStage === 1 ? Tr.subscribeBody : Tr.confirmShareBody
                    font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"
                    font.pixelSize: 13; color: Theme.textSecondary
                    wrapMode: Text.WordWrap; lineHeight: 1.25
                }
                // PRO badge (subscribe stage only)
                Rectangle {
                    visible: page.shareStage === 1
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: proRow.implicitWidth + 20; implicitHeight: 26; radius: 13
                    color: Qt.alpha(Theme.warnYellow, 0.15)
                    RowLayout {
                        id: proRow
                        anchors.centerIn: parent; spacing: 5
                        AppIcon { name: "badge-check"; size: 12; color: Theme.warnYellow }
                        Label { text: Tr.premiumBadge; color: Theme.warnYellow
                            font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 11; font.weight: Font.Bold }
                    }
                }
                // Action buttons
                RowLayout {
                    Layout.fillWidth: true; Layout.topMargin: 4; spacing: 10
                    DialogBtn {
                        Layout.fillWidth: true
                        label: page.shareStage === 1 ? Tr.subscribeNotNow : Tr.dialogCancel
                        accent: Theme.textSecondary; filled: false
                        onClicked: page.shareStage = 0
                    }
                    DialogBtn {
                        Layout.fillWidth: true
                        label: page.shareStage === 1 ? Tr.subscribeBtn
                                                     : (page.isMobile ? Tr.shareBtn : Tr.emailBtn)
                        accent: page.shareStage === 1 ? Theme.warnYellow : Theme.cyan
                        filled: true
                        onClicked: {
                            if (page.shareStage === 1) appState.requestSubscription()
                            else page.confirmShare()
                        }
                    }
                }
            }
        }
    }

    component DialogBtn: Rectangle {
        id: dbtn
        property string label: ""
        property color accent: Theme.cyan
        property bool filled: false
        signal clicked()
        implicitHeight: 42; radius: 8
        color: dbtn.filled ? dbtn.accent : "transparent"
        border { width: 1; color: dbtn.filled ? "transparent" : Qt.alpha(dbtn.accent, 0.5) }
        Label {
            anchors.centerIn: parent
            text: dbtn.label
            color: dbtn.filled ? "#101018" : dbtn.accent
            font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 13; font.weight: Font.DemiBold
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dbtn.clicked() }
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
        opacity: page.canReport ? 1.0 : 0.4
        color: Qt.alpha(accent, 0.10)
        border { width: 1; color: Qt.alpha(accent, 0.35) }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: btn.iconName; size: 18; color: btn.accent }
            Item { width: 12 }
            Label { Layout.fillWidth: true; text: btn.label; color: Theme.textPrimary
                font.family: "JetBrains Mono, Noto Sans Mono CJK SC, Microsoft YaHei"; font.pixelSize: 13; font.weight: Font.Medium }
        }
        MouseArea { anchors.fill: parent; enabled: page.canReport
            cursorShape: Qt.PointingHandCursor; onClicked: btn.clicked() }
    }
}
