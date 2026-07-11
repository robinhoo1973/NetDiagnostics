// ── NativePdfPageView.qml — Mobile PDF viewer using platform-native rendering ─
// iOS: CGPDFDocument, Android: PdfRenderer — renders pages to PNG data URIs
// via NativePdfDocument (C++ QObject). Page-based with prev/next navigation.
import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: root
    property string pdfSource: ""      // file:// path to PDF
    property int currentPage: 0

    // ── Native PDF document (C++ backend) ──────────────────────────────
    property var pdfDoc: null

    Component.onCompleted: {
        if (typeof NativePdfDocument !== 'undefined') {
            pdfDoc = Qt.createQmlObject(
                'import NetDiagnostics 1.0; NativePdfDocument {}', root)
        }
    }

    // Reload when source changes
    onPdfSourceChanged: {
        if (pdfDoc && pdfSource) {
            pdfDoc.source = pdfSource
        }
    }

    // ── Page image display ────────────────────────────────────────────
    Image {
        id: pageImage
        anchors { fill: parent; margins: 4 }
        fillMode: Image.PreserveAspectFit
        cache: false
        source: ""
    }

    // Update image when page changes or PDF loads
    Connections {
        target: pdfDoc
        function onLoadedChanged() {
            if (pdfDoc && pdfDoc.loaded) loadPage()
        }
    }

    function loadPage() {
        if (!pdfDoc || !pdfDoc.loaded) return
        var uri = pdfDoc.renderPageDataUri(root.currentPage, pageImage.width)
        if (uri) pageImage.source = uri
    }

    onCurrentPageChanged: loadPage()
    // Re-render when width changes (rotation, resize)
    onWidthChanged: loadPage()

    // ── Page navigation bar ───────────────────────────────────────────
    Rectangle {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter
                  bottomMargin: 8 }
        width: navRow.implicitWidth + 24; height: 40
        radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.92)
        border { width: 1; color: ThemeEngine.colors.borderCard }
        visible: pdfDoc && pdfDoc.loaded && pdfDoc.pageCount > 1

        Row {
            id: navRow
            anchors.centerIn: parent; spacing: 8

            // 5WHY: 32px touch target is below both Apple HIG (44pt) and
            // Material Design (48dp). NativePdfPageView is mobile-only,
            // so use 44px minimum (meets both standards at typical mobile DPI).
            Rectangle {
                width: 44; height: 44; radius: 8
                color: navPrevMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                AppIcon { anchors.centerIn: parent; name: "play"; size: 18
                    color: ThemeEngine.cyan; rotation: 180 }
                MouseArea {
                    id: navPrevMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                    onClicked: { if (root.currentPage > 0) root.currentPage-- }
                }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: (root.currentPage + 1) + " / " + (pdfDoc ? pdfDoc.pageCount : 0)
                font.family: ThemeEngine.monoFont; font.pixelSize: 12
                color: ThemeEngine.textPrimary; width: 70
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                width: 44; height: 44; radius: 8
                color: navNextMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                AppIcon { anchors.centerIn: parent; name: "play"; size: 18
                    color: ThemeEngine.cyan }
                MouseArea {
                    id: navNextMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor; hoverEnabled: true
                    onClicked: {
                        if (pdfDoc && root.currentPage < pdfDoc.pageCount - 1)
                            root.currentPage++
                    }
                }
            }
        }
    }

    // Loading state
    Rectangle {
        anchors.centerIn: parent; width: 120; height: 36; radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.9)
        visible: !pdfDoc || !pdfDoc.loaded
        Label {
            anchors.centerIn: parent; text: "Loading PDF..."
            font.family: ThemeEngine.monoFont; font.pixelSize: 12
            color: ThemeEngine.textSecondary
        }
    }
}
