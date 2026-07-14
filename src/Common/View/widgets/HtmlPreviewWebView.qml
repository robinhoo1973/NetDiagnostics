// ── HtmlPreviewWebView.qml — WebView-based HTML preview (requires QtWebView) ─
// 5WHY: Setting url directly to a file:// path caused WKWebView/WebView2
// to render the HTML height-fit (ignoring the viewport meta tag already
// present in the generated HTML).  Using loadHtml() instead forces the
// native engine to process the content as a fresh page load, respecting
// the width=device-width viewport and enabling pinch-to-zoom on touch.
// Falls back to url loading if XHR cannot read the file (e.g. sandboxed).
//
// 5WHY (2nd pass): loadHtml() alone didn't guarantee viewport compliance
// on all engines (WebView2 sometimes ignores <meta viewport> in loadHtml).
// Now injects an explicit viewport-constraining <style> block that sets
// max-width: 100% on body/html and overflow-wrap: break-word, ensuring
// the content fits the WebView width regardless of engine quirks.
//
// 5WHY (3rd pass): No zoom controls existed — users couldn't adjust the
// preview size. Added zoom in/out/reset buttons at the bottom-right that
// use runJavaScript() to apply CSS zoom on the document body.
//
// 5WHY (4th pass): onHtmlUrlChanged was on the nested WebView child but
// htmlUrl is a property of the parent Item. QML property-change signals
// are object-scoped — the handler never fired. Moved to the root Item.
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

    // Apply CSS zoom to the WebView's document body.
    // QtWebView does not expose a native zoomFactor property, so we use
    // runJavaScript() to set the CSS zoom property on <body>.
    // Guard against null body (page not yet loaded) and platforms where
    // runJavaScript may not be available (older QtWebView backends).
    function applyZoom() {
        if (!webView || typeof webView.runJavaScript !== 'function') return
        webView.runJavaScript(
            "if(document.body)document.body.style.zoom='" + root.zoomFactor + "'")
    }

    // 5WHY: onHtmlUrlChanged MUST be on the element that owns htmlUrl.
    // Previously it was on the nested WebView child → never fired.
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

    // Apply zoom whenever zoomFactor changes
    onZoomFactorChanged: applyZoom()

    WebView {
        id: webView
        anchors.fill: parent
        visible: true
    }

    // ── Zoom controls (bottom-right) ──────────────────────────────────
    Rectangle {
        anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
        width: zoomRow.implicitWidth + 12; height: 36
        radius: 8
        color: Qt.alpha(ThemeEngine.colors.card, 0.92)
        border { width: 1; color: ThemeEngine.colors.borderCard }

        Row {
            id: zoomRow
            anchors.centerIn: parent
            spacing: 4

            Rectangle {
                width: 28; height: 28; radius: 5
                color: zoomOutMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                Label {
                    anchors.centerIn: parent
                    text: "−"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold
                    color: ThemeEngine.textPrimary
                }
                MouseArea {
                    id: zoomOutMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: { root.zoomFactor = Math.max(0.25, root.zoomFactor - 0.25) }
                }
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: Math.round(root.zoomFactor * 100) + "%"
                font.family: ThemeEngine.monoFont; font.pixelSize: 11
                color: ThemeEngine.textSecondary
                horizontalAlignment: Text.AlignHCenter
                width: 40
            }

            Rectangle {
                width: 28; height: 28; radius: 5
                color: zoomInMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                              : Qt.alpha(ThemeEngine.cyan, 0.08)
                Label {
                    anchors.centerIn: parent
                    text: "+"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold
                    color: ThemeEngine.textPrimary
                }
                MouseArea {
                    id: zoomInMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: { root.zoomFactor = Math.min(5.0, root.zoomFactor + 0.25) }
                }
            }

            Rectangle {
                width: 28; height: 28; radius: 5
                color: zoomResetMa.containsMouse ? Qt.alpha(ThemeEngine.cyan, 0.2)
                                                 : "transparent"
                Label {
                    anchors.centerIn: parent
                    text: "1:1"
                    font.family: ThemeEngine.monoFont; font.pixelSize: 9; font.weight: Font.Bold
                    color: ThemeEngine.textSecondary
                }
                MouseArea {
                    id: zoomResetMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: { root.zoomFactor = 1.0 }
                }
            }
        }
    }
}
