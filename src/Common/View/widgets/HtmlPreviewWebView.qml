// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
// 5WHY: Setting url directly to a file:// path caused WKWebView/WebView2
// to render the HTML height-fit (ignoring the viewport meta tag already
// present in the generated HTML).  Using loadHtml() instead forces the
// native engine to process the content as a fresh page load, respecting
// the width=device-width viewport and enabling pinch-to-zoom on touch.
// Falls back to url loading if XHR cannot read the file (e.g. sandboxed).
//
// 5WHY (2nd): loadHtml() alone didn't guarantee viewport compliance on
// WebView2. Now injects viewport-constraining CSS + inline body style
// + viewport meta tag for belt-and-suspenders width enforcement.
//
// 5WHY (3rd): Inline zoom controls were inconsistent with other preview
// tiers. Now uses shared ZoomBar component for unified UX (geometric √2
// steps, keyboard shortcuts, percentage display, same visual design).
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

    function injectViewportCss(html) {
        var viewportCss = "<style>" +
            "html,body{max-width:100%!important;overflow-x:hidden!important;overflow-wrap:break-word!important;word-wrap:break-word!important}" +
            "img,svg,table,pre,code{max-width:100%!important;height:auto!important}" +
            "table{display:block!important;overflow-x:auto!important}" +
            "</style>"
        var bodyStyle = ' style="max-width:100%;overflow-x:hidden;overflow-wrap:break-word;word-wrap:break-word"'
        html = html.replace(/<body([^>]*)>/i, '<body$1' + bodyStyle + '>')
        if (!/<meta[^>]+viewport/i.test(html)) {
            var metaVp = '<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=5.0,user-scalable=yes">'
            html = html.replace(/<head[^>]*>/i, '$&' + metaVp)
        }
        var headIdx = html.search(/<\/head>/i)
        if (headIdx >= 0) {
            return html.substring(0, headIdx) + viewportCss + html.substring(headIdx)
        }
        var htmlTag = html.search(/<html[^>]*>/i)
        if (htmlTag >= 0) {
            var endTag = html.indexOf('>', htmlTag)
            return html.substring(0, endTag + 1) + "<head>" + viewportCss + "</head>" + html.substring(endTag + 1)
        }
        return viewportCss + html
    }

    function applyZoom() {
        if (!webView || typeof webView.runJavaScript !== 'function') return
        webView.runJavaScript(
            "if(document.body)document.body.style.zoom='" + root.zoomFactor + "'")
    }

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
                    var html = root.injectViewportCss(xhr.responseText)
                    webView.loadHtml(html, htmlUrl)
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
