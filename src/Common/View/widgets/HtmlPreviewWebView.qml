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

    // CSS 'zoom' is non-standard but supported by all major engines
    // (Blink/WebKit/Gecko). The assignment never throws — unsupported
    // engines silently ignore it, so no try/catch or fallback is needed.
    function applyZoom() {
        if (!webView || typeof webView.runJavaScript !== 'function') return
        webView.runJavaScript(
            "var s=document.body;if(s){" +
            "s.style.zoom='" + root.zoomFactor + "'" +
            "}")
    }

    // 5WHY: injectViewportCss() was removed — width-constraining CSS is
    // now in ReportEngine.cpp's kCss (max-width, overflow-x:auto, etc.).
    // loadHtml() is still used instead of setting url directly, so the
    // native engine processes the content as a fresh page load with
    // correct viewport handling (unlike file:// URL assignment).
    onHtmlUrlChanged: {
        if (!htmlUrl) return
        var xhr = new XMLHttpRequest()
        var handled = false
        xhr.open("GET", htmlUrl)
        xhr.onreadystatechange = function() {
            if (handled) return
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 0 || xhr.status === 200) {
                    handled = true
                    webView.loadHtml(xhr.responseText, htmlUrl)
                } else {
                    handled = true
                    webView.url = htmlUrl
                }
            }
        }
        xhr.onerror = function() {
            if (handled) return
            handled = true
            webView.url = htmlUrl
        }
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
