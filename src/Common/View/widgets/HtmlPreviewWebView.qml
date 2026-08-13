// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
// 5WHY (v3): loadHtml() does NOT exist on QtWebView (only on QtWebEngine).
// WKWebView on iOS / WebView2 on Windows use the minimal QtWebView API
// which only exposes url/title/canGoBack/canGoForward/loading/loadProgress.
// 
// Fix: encode the fetched HTML as a data:text/html URI and assign to
// webView.url.  WKWebView natively supports data: URIs and processes
// viewport meta tags correctly via this path, enabling width=device-width
// scaling and pinch-to-zoom on touch devices.
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
    // 5WHY (v3): QtWebView.WebView has no loadHtml() — use data: URI
    // which WKWebView/WebView2 both support natively with viewport.
    onHtmlUrlChanged: {
        if (!htmlUrl) return
        var xhr = new XMLHttpRequest()
        xhr.open("GET", htmlUrl)
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 0 || xhr.status === 200) {
                    // 5WHY (v3): Encode as data: URI — QtWebView
                    // has no loadHtml(). WKWebView supports data:
                    // natively with full viewport handling.
                    var dataUri = "data:text/html;charset=utf-8," + encodeURIComponent(xhr.responseText)
                    webView.url = dataUri
                }
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
