// ── NativePdfPageView.qml — Mobile PDF viewer using platform-native rendering ─
// iOS: CGPDFDocument, Android: PdfRenderer — renders pages to PNG data URIs
// via NativePdfDocument (C++ QObject). Page-based with prev/next navigation.
//
// 5WHY: No zoom at all — Image was fixed fit-width, no pinch gesture, no
// zoom buttons. Mobile users couldn't zoom into PDF pages at all.
// Now: Flickable + PinchHandler + Scale transform for pinch-to-zoom,
// ZoomBar for button/kbd zoom, and debounced high-res re-render when
// zoomed past 1.5x so the image stays sharp.
import QtQuick
import QtQuick.Controls
import "../theme"

Item {
    id: root
    property string pdfSource: ""      // file:// path to PDF
    property int currentPage: 0
    property real zoomLevel: 1.0

    // ── Native PDF document (C++ backend) ──────────────────────────────
    property var pdfDoc: null

    Component.onCompleted: {
        if (typeof NativePdfDocument !== 'undefined') {
            pdfDoc = Qt.createQmlObject(
                'import NetDiagnostics 1.0; NativePdfDocument {}', root)
            if (pdfDoc) {
                pdfDoc.loadedChanged.connect(function() {
                    if (pdfDoc.loaded) loadPage()
                })
                if (pdfSource) {
                    pdfDoc.source = pdfSource
                    if (pdfDoc.loaded) loadPage()
                }
            }
        }
    }

    onPdfSourceChanged: {
        if (pdfDoc && pdfSource) {
            pdfDoc.source = pdfSource
        }
    }

    // ── Re-render tracking ────────────────────────────────────────────
    property real _lastRenderZoom: 1.0
    property bool _pendingReRender: false

    Timer {
        id: reRenderTimer
        interval: 150
        onTriggered: {
            if (root._pendingReRender) {
                root._pendingReRender = false
                root._lastRenderZoom = root.zoomLevel
                loadPage()
            }
        }
    }

    function requestReRender() {
        // 5WHY: Re-rendering on every pinch pixel is too expensive.
        // Only re-render when zoom changes >15% from last render zoom,
        // debounced by 150ms so intermediate pinch frames just use
        // GPU-scaled interpolation (acceptable blur during gesture).
        if (Math.abs(root._lastRenderZoom - root.zoomLevel) / Math.max(root.zoomLevel, 0.1) > 0.15) {
            root._pendingReRender = true
            reRenderTimer.restart()
        }
    }

    // ── Zoom-aware page rendering ─────────────────────────────────────
    function loadPage() {
        if (!pdfDoc || !pdfDoc.loaded) return
        // Render at higher resolution when zoomed in for sharpness
        var renderWidth = Math.round(flick.width * Math.max(1.0, root.zoomLevel))
        renderWidth = Math.min(renderWidth, 4000) // cap at 4000px to limit memory
        var uri = pdfDoc.renderPageDataUri(root.currentPage, renderWidth)
        if (uri) pageImage.source = uri
    }

    onCurrentPageChanged: {
        _lastRenderZoom = 1.0
        zoomLevel = 1.0
        loadPage()
    }
    onWidthChanged: loadPage()

    // ── Scrollable zoomable page view ─────────────────────────────────
    Flickable {
        id: flick
        anchors { fill: parent; margins: 4 }
        clip: true
        contentWidth: pageImage.width * root.zoomLevel
        contentHeight: pageImage.height * root.zoomLevel
        interactive: !pinching

        property bool pinching: false
        property real startZoom: 1.0

        Image {
            id: pageImage
            width: flick.width
            height: flick.height
            fillMode: Image.PreserveAspectFit
            cache: false
            source: ""
            transform: Scale {
                origin.x: pageImage.width / 2
                origin.y: pageImage.height / 2
                xScale: root.zoomLevel
                yScale: root.zoomLevel
            }
        }

        PinchHandler {
            target: null
            onActiveChanged: {
                flick.pinching = active
                if (active) {
                    flick.startZoom = root.zoomLevel
                    flick.returnToBounds()
                } else {
                    root.requestReRender()
                }
            }
            onScaleChanged: {
                root.zoomLevel = Math.max(0.25,
                    Math.min(flick.startZoom * scale, 5.0))
            }
        }
    }

    // ── Unified zoom controls ─────────────────────────────────────────
    ZoomBar {
        id: zoomBar
        anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
        zoomLevel: root.zoomLevel

        onZoomLevelChanged: {
            root.zoomLevel = zoomBar.zoomLevel
            root.requestReRender()
        }
    }

    // ── Page navigation bar ───────────────────────────────────────────
    Rectangle {
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter
                  bottomMargin: 8 }
        width: navRow.implicitWidth + 24; height: 44
        radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.92)
        border { width: 1; color: ThemeEngine.colors.borderCard }
        visible: pdfDoc && pdfDoc.loaded && pdfDoc.pageCount > 1

        Row {
            id: navRow
            anchors.centerIn: parent; spacing: 8

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
                Accessible.name: "Previous page"
                Accessible.role: Accessible.Button
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: (root.currentPage + 1) + " / " + (pdfDoc ? pdfDoc.pageCount : 0)
                font.family: ThemeEngine.monoFont; font.pixelSize: 13
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
                Accessible.name: "Next page"
                Accessible.role: Accessible.Button
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
