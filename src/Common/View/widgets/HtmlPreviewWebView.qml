// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
// 5WHY: Setting url directly to a file:// path caused WKWebView/WebView2
// to render the HTML height-fit. Using loadHtml() forces the native engine
// to process content as a fresh page load, respecting viewport + enabling
// pinch-to-zoom. Width-constraining CSS (overflow-x:auto, max-width:100%)
// is now in ReportEngine.cpp's kCss — no QML injection needed.
//
// Zoom uses shared ZoomBar component with CSS zoom via runJavaScript.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebView
import "../theme"

Item {
    id: root
    property string htmlUrl: ""
    property real zoomFactor: 1.0
    anchors.fill: parent

    // 5WHY: CSS 'zoom' is non-standard and unreliable on iOS WKWebView
    // (scales the element but not the viewport coordinate system).
    // Use viewport initial-scale manipulation as the PRIMARY method —
    // this directly controls WKWebView's native UIScrollView zoom,
    // working identically to Safari pinch-to-zoom on iOS.
    // Falls back to CSS zoom for engines that don't support viewport
    // meta manipulation (older WebView2).
    function applyZoom() {
        var zf = root.zoomFactor
        webView.runJavaScript(
            "(function(z){" +
            "var m=document.querySelector('meta[name=viewport]');" +
            "if(m){" +
            "m.content='width=device-width,initial-scale='+z+',maximum-scale=5.0,user-scalable=yes'" +
            "}else{" +
            "var s=document.body;if(s)s.style.zoom=z" +
            "}" +
            "})(" + zf + ")")
    }

    // Width-constraining CSS is included in the generated HTML
    // (ReportEngine.cpp kCss for buildRichDocument). loadHtml() is
    // used instead of url assignment so the native engine processes
    // content with correct viewport + pinch-to-zoom handling.
    onHtmlUrlChanged: {
        if (!htmlUrl) return
        var xhr = new XMLHttpRequest()
        xhr.open("GET", htmlUrl)
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 0 || xhr.status === 200)
                    webView.loadHtml(xhr.responseText, htmlUrl)
                else
                    webView.url = htmlUrl
            }
        }
        xhr.onerror = function() { webView.url = htmlUrl }
        xhr.send()
    }

    // 5WHY: Sync ZoomBar zoomLevel ↔ zoomFactor for CSS zoom application
    onZoomFactorChanged: applyZoom()

    WebView {
        id: webView
        anchors.fill: parent
        visible: true
    }

    // ── Unified zoom controls ─────────────────────────────────────────
    ZoomBar {
        id: zoomBar
        anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
        zoomLevel: root.zoomFactor

        onZoomLevelChanged: {
            root.zoomFactor = zoomBar.zoomLevel
        }
    }
}
