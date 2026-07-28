import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
import "../dialogs"

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
    readonly property bool isMobile: ThemeEngine.isMobile
    // 5WHY: DashboardScreen exports overlayVisible (aliased to previewOverlay.visible)
    // so AppContent.navBlocked can detect when a preview is open and prevent nav taps
    // during automated capture.  ReportScreen had no such alias — a nav tap during a
    // pending capture completion would switch tabs, the Timer would fire on the wrong
    // screen, and the ResultSummary would appear in an unexpected context.
    // Export the same alias so navBlocked sees ReportScreen previews as blocking.
    readonly property alias overlayVisible: previewOverlay.visible

    // Built-in preview overlay state
    property string previewImagePath: ""   // rendered preview image (data: URI)
    property bool previewVisible: false
    property string toast: ""             // transient status message

    // Share/subscription flow state:
    //   0 = no dialog, 1 = subscription prompt (not premium), 2 = confirm share (premium)
    property int shareStage: 0
    property string pendingShareFormat: ""

    // 5WHY: darkBackground was hardcoded to true — preview never reflected
    // the user's actual theme choice. Now reads ThemeEngine.isDark reactively.

    function openPreview() {
        if (!canReport) return
        // 5WHY: QtWebView has no loadHtml() on iOS/Android, QtPdf is
        // unreliable, NativePdf requires CGPDFDocument.  Use the ONE
        // approach that works everywhere: QTextDocument→QImage rendering.
        // The rendered image looks identical to the real report, supports
        // pinch-to-zoom via PinchHandler, and needs zero platform deps.
        //
        // 5WHY: buildReportHtml() + renderPreviewImage() are synchronous
        // calls that freeze the UI.  Show the overlay immediately with a
        // blank image, then defer the heavy work via Qt.callLater.
        previewVisible = true
        previewImagePath = ""
        Qt.callLater(function() {
            var darkBg = ThemeEngine.isDark
            var html = appState.buildReportHtml(true, darkBg)
            var imgPath = appState.renderPreviewImage(html, page.isMobile ? 480 : 960)
            previewImagePath = imgPath || ""
        })
    }
    function requestExport(fmt) { if (canReport) appState.requestSavePath(fmt) }
    // Share button entry: check subscription. Not subscribed → guide to subscribe;
    // subscribed → ask for confirmation before sharing. Same logic on iOS/Android.
    function doShare(fmt) {
        pendingShareFormat = fmt
        shareStage = appState.isPremium ? 2 : 1
    }
    // Preview uses QTextDocument→QImage (data: URI), not a file, so
    // sharing must generate the actual PDF/HTML file on demand.
    function confirmShare() {
        var fmt = pendingShareFormat
        shareStage = 0
        appState.shareReport(fmt)
    }

    onPreviewVisibleChanged: {
        if (!previewVisible) {
            // 5WHY: Clear the image path when the preview closes so the
            // old data: URI doesn't persist in memory.  openPreview()
            // regenerates a fresh image before setting previewVisible=true.
            previewImagePath = ""
        }
    }

    Timer { id: toastTimer; interval: ThemeEngine.toastDurationMs; onTriggered: page.toast = "" }

    // 5WHY: Preview image was static once generated. Auto-refresh when:
    // - Theme changes (dark/light toggle)
    // - New diagnostic results complete (totalCompleted increments)
    Connections {
        target: ThemeEngine
        function onModeChanged() {
            if (page.previewVisible) page.openPreview()
        }
    }
    Connections {
        target: appState
        function onProgressChanged() {
            // 5WHY: If preview is open after a run completes, regenerate
            // so the user sees the latest results immediately.
            // buildReportHtml() reads m_results fresh each call.
            if (page.previewVisible && appState.runStatus !== 1)
                page.openPreview()
        }
    }

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

    // AppBar
    AppBar {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        iconName: "report"
        title: Tr.reportPreview
        Item { Layout.fillWidth: true }
    }

    // Centered content — scrollable; a holder keeps the column vertically centered
    // when it fits and lets it scroll when it's taller than the viewport (portrait),
    // so nothing is clipped off-screen.
    //
    // contentHeight binds to reportCol.height — the ACTUAL allocated height
    // set by the Layout manager during the deferred layout pass.  Unlike
    // implicitHeight (a preferred-size hint), height has a direct NOTIFY
    // signal that fires synchronously when the Layout sets the new value,
    // so the declarative binding always re-evaluates with the correct size.
    Flickable {
        id: reportFlick
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true
        contentWidth: width
        contentHeight: reportCol.y + reportCol.height + (page.isMobile ? 30 : 52)
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: reportCol
            x: (reportFlick.width - width) / 2
            y: page.isMobile ? 14 : 24
            // Column width: proportional on mobile (6 % total margin → 3 % per side)
            // so narrow screens (320 px) keep enough room for button labels in
            // long-translation languages.  Desktop uses a fixed 80 px margin
            // (the screen is wide enough either way).
            width: page.isMobile ? reportFlick.width * 0.94 : reportFlick.width - 80
            spacing: 0

            // Icon container
            Rectangle {
                Layout.preferredWidth: page.isMobile ? 72 : 100; Layout.preferredHeight: page.isMobile ? 72 : 100
                Layout.alignment: Qt.AlignHCenter
                radius: 24; color: Qt.alpha(ThemeEngine.cyan, 0.08)
                border { width: 1.5; color: Qt.alpha(ThemeEngine.cyan, 0.2) }
                AppIcon { anchors.centerIn: parent; name: "report"; size: page.isMobile ? 36 : 48; color: Qt.alpha(ThemeEngine.cyan, 0.6) }
            }
            Item { Layout.preferredHeight: page.isMobile ? 14 : 24 }

            // Title — fill column width so long translations fit
            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                text: Tr.reportPreview
                font.family: ThemeEngine.monoFont; font.pixelSize: page.isMobile ? 19 : 22; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary
                elide: Text.ElideRight; maximumLineCount: 1
            }
            Item { Layout.preferredHeight: page.isMobile ? 8 : 12 }

            // Subtitle — MUST fill the column width for text to wrap
            Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                text: page.isRunning ? Tr.runningDots
                      : (page.hasResults ? Tr.reportExportHint : Tr.reportRunFirst)
                font.family: ThemeEngine.monoFont; font.pixelSize: 14; color: Qt.alpha(ThemeEngine.textSecondary, 0.6)
                horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5
                wrapMode: Text.WordWrap
            }
            Item { Layout.preferredHeight: page.isMobile ? 16 : 24 }

            // Single "Review Report" button — opens unified image preview
            ColumnLayout { spacing: 10; Layout.fillWidth: true
                ExportButton { iconName: "report"; label: Tr.reportReviewBtn; accent: ThemeEngine.cyan; onClicked: page.openPreview() }
                Label {
                    visible: page.toast !== "" || page.lastPath !== "" || page.lastFailed
                    Layout.fillWidth: true; Layout.topMargin: 4
                    text: page.toast !== "" ? page.toast
                          : (page.lastFailed ? Tr.reportExportFailed : (Tr.reportSavedTo + " " + page.lastPath))
                    color: page.lastFailed ? ThemeEngine.failRed : (page.toast !== "" ? ThemeEngine.cyan : ThemeEngine.passGreen)
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere; horizontalAlignment: Text.AlignHCenter
                }
            }
            Item { Layout.preferredHeight: page.isMobile ? 18 : 32 }

            // Status indicator — running / results-ready / no-results
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: statusRow.implicitWidth + 32; implicitHeight: 40; radius: 8
                color: page.isRunning ? Qt.alpha(ThemeEngine.cyan, 0.1)
                       : (hasResults ? Qt.alpha(ThemeEngine.passGreen, 0.1) : Qt.alpha(ThemeEngine.warnYellow, 0.1))
                border { width: 1; color: page.isRunning ? Qt.alpha(ThemeEngine.cyan, 0.3)
                       : (hasResults ? Qt.alpha(ThemeEngine.passGreen, 0.3) : Qt.alpha(ThemeEngine.warnYellow, 0.3)) }
                RowLayout {
                    id: statusRow
                    anchors.centerIn: parent
                    AppIcon { name: page.isRunning ? "spinner" : (hasResults ? "badge-check" : "badge-info"); size: 12; color: ThemeEngine.textPrimary }
                    Item { width: 8 }
                    Label {
                        Layout.fillWidth: true
                        text: page.isRunning ? Tr.runningStatus
                              : (hasResults ? appState.totalCompleted + Tr.reportResultsAvailable : Tr.reportNoResults)
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12
                        color: page.isRunning ? ThemeEngine.cyan : (hasResults ? ThemeEngine.passGreen : ThemeEngine.warnYellow)
                        elide: Text.ElideRight
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
        color: Qt.alpha(ThemeEngine.colors.surface, 0.85)
        visible: page.previewVisible
        z: 1000
        // 5WHY: Overlay was modal — background MouseArea swallowed clicks
        // without closing.  Now clicking outside the card dismisses the
        // preview (same as clicking the close button).
        MouseArea { anchors.fill: parent; onClicked: page.previewVisible = false }

        Rectangle {
            anchors {
                fill: parent
                margins: page.isMobile ? 0 : 8
            }
            radius: page.isMobile ? 0 : 12; color: ThemeEngine.colors.card
            // Absorb clicks inside the card so they don't reach the background
            // MouseArea and dismiss the overlay
            MouseArea { anchors.fill: parent }
            clip: true
            border { width: page.isMobile ? 0 : 2; color: ThemeEngine.colors.borderFocused }

            ColumnLayout {
                anchors { fill: parent; margins: page.isMobile ? 8 : 12 }
                spacing: 10
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: headerRow.implicitHeight + 16
                    color: Qt.alpha(ThemeEngine.cyan, 0.08)
                    radius: 8
                    RowLayout {
                        id: headerRow
                        anchors { fill: parent; margins: 8 }
                        AppIcon { name: "report"; size: 20; color: ThemeEngine.cyan }
                        Item { width: 8 }
                        Label {
                            Layout.fillWidth: true
                            text: Tr.reportReviewBtn
                            font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: ThemeEngine.textPrimary
                            elide: Text.ElideRight
                        }
                        Rectangle {
                            id: closeBtn
                            // 5WHY: 34pt is below Apple HIG (44pt) and Material Design
                            // (48dp) minimum touch targets. On mobile, users cannot
                            // reliably tap the close button. Use 48pt on mobile.
                            readonly property int btnSize: page.isMobile ? 48 : 34
                            implicitWidth: btnSize; implicitHeight: btnSize; radius: btnSize / 2
                            color: closeMouse.containsMouse ? Qt.alpha(ThemeEngine.failRed, 0.35)
                                                            : Qt.alpha(ThemeEngine.failRed, 0.15)
                            AppIcon { anchors.centerIn: parent; name: "close"; size: page.isMobile ? 22 : 16; color: ThemeEngine.failRed }
                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                // 5WHY: Resetting previewFormat deactivates the Loader,
                                // destroying the WebView (frees WebView2/WKWebView resources).
                                // Without this, the Loader stays active and keeps the
                                // WebView in memory until previewFormat changes.
                                onClicked: { page.previewVisible = false }
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: ThemeEngine.colors.surface; radius: 8; clip: true
                    border { width: 1; color: ThemeEngine.colors.borderCard }

                    // ── Unified image-based preview (PDF + HTML) ───────────
                    // 5WHY: QtWebView has no loadHtml() on iOS/Android,
                    // QtPdf is unreliable, NativePdf needs CGPDFDocument.
                    // The ONE approach that works everywhere: render both
                    // PDF and HTML as a QTextDocument→QImage (data: URI),
                    // display in a zoomable Flickable with PinchHandler.
                    // Looks identical, supports pinch+button zoom, zero deps.
                    Flickable {
                        id: previewFlick
                        anchors { fill: parent; margins: 14 }
                        clip: true
                        contentWidth: previewImage.width * previewScale
                        contentHeight: previewImage.height * previewScale
                        property real previewScale: 1.0
                        property real startScale: 1.0
                        property bool pinching: false
                        interactive: !pinching
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded; width: 6
                            contentItem: Rectangle { color: ThemeEngine.textMuted; radius: 3 }
                        }
                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded; height: 6
                            contentItem: Rectangle { color: ThemeEngine.textMuted; radius: 3 }
                        }
                        PinchHandler {
                            target: null
                            onActiveChanged: {
                                previewFlick.pinching = active
                                if (active) {
                                    previewFlick.startScale = previewFlick.previewScale
                                    previewFlick.returnToBounds()
                                }
                            }
                            onScaleChanged: {
                                previewFlick.previewScale = Math.max(0.25,
                                    Math.min(previewFlick.startScale * scale, 5.0))
                            }
                        }
                        Image {
                            id: previewImage
                            width: previewFlick.width
                            fillMode: Image.PreserveAspectFit
                            source: previewImagePath
                            cache: false
                            transform: Scale {
                                origin.x: previewImage.width / 2
                                origin.y: previewImage.height / 2
                                xScale: previewFlick.previewScale
                                yScale: previewFlick.previewScale
                            }
                        }
                    }
                    // ── Unified zoom controls ─────────────────────────────
                    ZoomBar {
                        id: zoomBar
                        anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
                        zoomLevel: previewFlick.previewScale
                        onZoomLevelChanged: {
                            previewFlick.previewScale = zoomBar.zoomLevel
                        }
                    }
                }
                // ── Share buttons (PDF + HTML) ───────────────────────────
                ShareButtons {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 10
                    mode: "wide"
                    pdfAccent: ThemeEngine.cyan
                    htmlAccent: ThemeEngine.primary
                    onShareRequested: function(fmt) { page.doShare(fmt) }
                }
}
        }
    }

    // ── Share subscription dialog ───────────────────────────────────────
    ShareSubscriptionDialog {
        shareStage: page.shareStage; isMobile: page.isMobile
        showProBadge: true; showIconBorder: true
        onDismissed: page.shareStage = 0
        onActionRequested: {
            if (page.shareStage === 1) appState.requestSubscription()
            else page.confirmShare()
        }
    }

    // 5WHY: DialogBtn extracted to shared widget; remains here for ExportButton use.
    component DialogBtn: Rectangle {
        id: dbtn
        property string label: ""
        property color accent: ThemeEngine.cyan
        property bool filled: false
        signal clicked()
        implicitHeight: 42; radius: 8
        color: dbtn.filled ? dbtn.accent : "transparent"
        border { width: 1; color: dbtn.filled ? "transparent" : Qt.alpha(dbtn.accent, 0.5) }
        Label {
            anchors.centerIn: parent
            text: dbtn.label
            color: dbtn.filled ? ThemeEngine.bgDark : dbtn.accent
            font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dbtn.clicked() }
    }

    // 5WHY: PreviewBtn component removed — dead code (never instantiated).
    // The main content area uses ExportButton; the preview overlay uses
    // built-in UI with its own close button.

    component ExportButton: Rectangle {
        id: btn
        property string iconName: ""
        property string label: ""
        property color accent: ThemeEngine.cyan
        signal clicked()
        Layout.fillWidth: true
        implicitHeight: 48; radius: 10
        clip: true  // safety net for long translations on narrow screens
        opacity: page.canReport ? 1.0 : 0.4
        color: Qt.alpha(accent, 0.10)
        border { width: 1; color: Qt.alpha(accent, 0.35) }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: btn.iconName; size: 18; color: btn.accent }
            Item { width: 12 }
            Label { Layout.fillWidth: true; text: btn.label; color: ThemeEngine.textPrimary
                elide: Text.ElideRight; maximumLineCount: 1
                font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Medium }
        }
        MouseArea { anchors.fill: parent; enabled: page.canReport
            cursorShape: Qt.PointingHandCursor; onClicked: btn.clicked() }
    }
}