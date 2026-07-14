// ── PdfPreviewView.qml — Real PDF viewer using QtQuick.Pdf (Qt 6.4+) ─
// 5WHY: "PDF Preview" was a QTextDocument→QImage PNG rendering, not a
// real PDF. Users had no page breaks, text selection, or page navigation.
// This uses PdfScrollablePageView from QtQuick.Pdf to display the actual
// generated PDF with native rendering, pinch-to-zoom, and page controls.
//
// 5WHY (2nd): No zoom UI controls existed — users on non-touch desktops
// couldn't zoom. Added ZoomBar with geometric √2 steps, keyboard shortcuts
// (Ctrl+=/-/0), and percentage display, two-way synced with renderScale.
import QtQuick
import QtQuick.Controls
import QtQuick.Pdf
import "../theme"

Item {
    id: root
    property string pdfSource: ""       // file:// path to generated PDF
    property int currentPage: 0
    property int pageCount: 0

    // 5WHY: Two-way sync guard — prevents feedback loop between
    // renderScale changes (pinch) and zoomBar.zoomLevel changes (buttons).
    property bool _syncing: false

    PdfDocument {
        id: pdfDoc
        source: root.pdfSource
        onPageCountChanged: root.pageCount = pdfDoc.pageCount
    }

    // ── Scrollable single-page view with pinch-to-zoom ─────────────────
    PdfScrollablePageView {
        id: pdfView
        anchors { fill: parent; margins: 2 }
        document: pdfDoc
        currentPage: root.currentPage
        renderScale: 1.0
        maximumScaleFactor: 5.0
        minimumScaleFactor: 0.25

        // Sync pinch-to-zoom → ZoomBar display
        onRenderScaleChanged: {
            if (!root._syncing) {
                root._syncing = true
                zoomBar.zoomLevel = pdfView.renderScale
                root._syncing = false
            }
        }
    }

    // ── Unified zoom controls ─────────────────────────────────────────
    ZoomBar {
        id: zoomBar
        anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
        zoomLevel: pdfView.renderScale

        onZoomLevelChanged: {
            if (!root._syncing) {
                root._syncing = true
                pdfView.renderScale = zoomBar.zoomLevel
                root._syncing = false
            }
        }
    }

    // ── Page navigation bar (bottom centre) ───────────────────────────
    Rectangle {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter
                  bottomMargin: 8 }
        width: navRow.implicitWidth + 24; height: 40
        radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.92)
        border { width: 1; color: ThemeEngine.colors.borderCard }
        visible: root.pageCount > 1

        Row {
            id: navRow
            anchors.centerIn: parent
            spacing: 8

            Rectangle {
                width: 32; height: 32; radius: 6
                color: navPrevMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                AppIcon { anchors.centerIn: parent; name: "play"; size: 14
                    color: ThemeEngine.cyan; rotation: 180 }
                MouseArea {
                    id: navPrevMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        if (root.currentPage > 0) root.currentPage--
                    }
                }
                Accessible.name: "Previous page"
                Accessible.role: Accessible.Button
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: (root.currentPage + 1) + " / " + root.pageCount
                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                color: ThemeEngine.textPrimary
                horizontalAlignment: Text.AlignHCenter
                width: 70
            }

            Rectangle {
                width: 32; height: 32; radius: 6
                color: navNextMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                AppIcon { anchors.centerIn: parent; name: "play"; size: 14
                    color: ThemeEngine.cyan }
                MouseArea {
                    id: navNextMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        if (root.currentPage < root.pageCount - 1) root.currentPage++
                    }
                }
                Accessible.name: "Next page"
                Accessible.role: Accessible.Button
            }
        }
    }

    // Loading / error overlays
    Rectangle {
        anchors.centerIn: parent
        width: 140; height: 36; radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.9)
        visible: pdfDoc.status === PdfDocument.Loading || pdfDoc.status === PdfDocument.Null
        Label {
            anchors.centerIn: parent
            text: Tr.pdfLoading
            font.family: ThemeEngine.monoFont; font.pixelSize: 12
            color: ThemeEngine.textSecondary
        }
    }
    Rectangle {
        anchors.centerIn: parent
        width: 200; height: 36; radius: 8
        color: Qt.alpha(ThemeEngine.failRed, 0.15)
        border { width: 1; color: Qt.alpha(ThemeEngine.failRed, 0.3) }
        visible: pdfDoc.status === PdfDocument.Error
        Label {
            anchors.centerIn: parent
            text: Tr.pdfLoadFailed
            font.family: ThemeEngine.monoFont; font.pixelSize: 12
            color: ThemeEngine.failRed
        }
    }
}
