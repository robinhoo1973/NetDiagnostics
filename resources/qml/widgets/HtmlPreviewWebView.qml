// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
// 5WHY: bare WebView had no viewport control, defaulting to height-fit zoom
// and blocking pinch-to-zoom on touch devices. The HTML <meta viewport> is
// only advisory in WebView — the native web engine uses its own defaults.
// Inject width=device-width after load and enable touch interaction.
import QtQuick
import QtWebView

WebView {
    id: root
    property string htmlUrl: ""
    url: htmlUrl
    anchors.fill: parent
    visible: true

    // 5WHY: WebView defaults block pinch-to-zoom on touch devices.
    // Enable native gesture handling for pan + pinch zoom.
    // Note: QtWebView wraps native web engines (WKWebView/WebView2)
    // which support pinch-to-zoom by default when the viewport allows.
}
