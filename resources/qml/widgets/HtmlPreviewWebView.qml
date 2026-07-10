// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
import QtQuick
import QtWebView

WebView {
    id: root
    property string htmlUrl: ""
    url: htmlUrl
    anchors.fill: parent
    visible: true
}
