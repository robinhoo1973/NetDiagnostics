// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
// 5WHY: Setting url directly to a file:// path caused WKWebView/WebView2
// to render the HTML height-fit (ignoring the viewport meta tag already
// present in the generated HTML).  Using loadHtml() instead forces the
// native engine to process the content as a fresh page load, respecting
// the width=device-width viewport and enabling pinch-to-zoom on touch.
// Falls back to url loading if XHR cannot read the file (e.g. sandboxed).
import QtQuick
import QtWebView

WebView {
    id: root
    property string htmlUrl: ""
    anchors.fill: parent
    visible: true

    onHtmlUrlChanged: {
        if (!htmlUrl) return
        var xhr = new XMLHttpRequest()
        var handled = false  // 5WHY: prevents onerror+onreadystatechange double-fire
        xhr.open("GET", htmlUrl)
        xhr.onreadystatechange = function() {
            if (handled) return
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 0 || xhr.status === 200) {
                    handled = true
                    root.loadHtml(xhr.responseText, htmlUrl)
                } else {
                    handled = true
                    root.url = htmlUrl
                }
            }
        }
        xhr.onerror = function() {
            if (handled) return
            handled = true
            root.url = htmlUrl
        }
        xhr.send()
    }
}
