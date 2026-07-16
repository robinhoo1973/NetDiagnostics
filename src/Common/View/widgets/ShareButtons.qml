import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// ══════════════════════════════════════════════════════════════════════════════
// ShareButtons — Reusable PDF/HTML share button pair
//
// Three visual modes:
//   "compact"  — icon-only squares (42dp), for Diagnostic header
//   "labeled"  — icon+text buttons (fillWidth), for Dashboard preview overlay
//   "wide"     — icon+text buttons (fillWidth, 48dp height), for Report preview
//
// Color overrides let each page set accent colours appropriate to its theme:
//   - Diagnostic:  pdfAccent=ThemeEngine.failRed,  htmlAccent=ThemeEngine.accentBlue
//   - Dashboard:   pdfAccent=ThemeEngine.failRed,  htmlAccent=ThemeEngine.accentBlue
//   - Report:      pdfAccent=ThemeEngine.cyan,     htmlAccent=ThemeEngine.primary
// ══════════════════════════════════════════════════════════════════════════════

RowLayout {
    id: shareRoot
    spacing: 6

    // ── Public API ──────────────────────────────────────────────────────────
    property string mode: "compact"           // "compact" | "labeled" | "wide"
    property bool   locked: !appState.isPremium
    property color  pdfAccent: ThemeEngine.failRed
    property color  htmlAccent: ThemeEngine.accentBlue
    signal shareRequested(string format)

    readonly property bool _isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // ── Sizes derived from mode ─────────────────────────────────────────────
    readonly property int _iconSize:  mode === "compact" ? 28 :
                                      (mode === "wide" ? 20 : 16)
    readonly property int _btnHeight: mode === "compact" ? 42 :
                                      (mode === "wide" ? 48 : 40)
    readonly property int _btnRadius: mode === "wide" ? 10 : 8

    // ── PDF button ──────────────────────────────────────────────────────────
    Loader {
        id: pdfLoader
        Layout.fillWidth: shareRoot.mode !== "compact"
        sourceComponent: shareRoot.mode === "compact" ? compactBtn : labeledBtn
        onLoaded: {
            item.iconName = shareRoot.mode === "compact" ? "file-pdf-sm" : "file-pdf"
            item.accent = shareRoot.pdfAccent
            item.locked = shareRoot.locked
            item.formatTag = "pdf"
            item.labelText = shareRoot._isMobile ? Tr.sharePdfBtn : Tr.emailPdfBtn
        }
    }

    // ── HTML button ─────────────────────────────────────────────────────────
    Loader {
        id: htmlLoader
        Layout.fillWidth: shareRoot.mode !== "compact"
        sourceComponent: shareRoot.mode === "compact" ? compactBtn : labeledBtn
        onLoaded: {
            item.iconName = shareRoot.mode === "compact" ? "file-html-sm" : "file-html"
            item.accent = shareRoot.htmlAccent
            item.locked = shareRoot.locked
            item.formatTag = "html"
            item.labelText = shareRoot._isMobile ? Tr.shareHtmlBtn : Tr.emailHtmlBtn
        }
    }

    // ── Compact: icon-only square (Diagnostic header) ───────────────────────
    Component {
        id: compactBtn
        Rectangle {
            property string iconName: ""
            property color accent: ThemeEngine.failRed
            property bool locked: false
            property string formatTag: ""
            property string labelText: ""

            implicitWidth: shareRoot._btnHeight; implicitHeight: shareRoot._btnHeight
            radius: shareRoot._btnRadius
            opacity: locked ? 0.4 : 1.0
            color: Qt.alpha(accent, 0.12)
            border { width: 1; color: Qt.alpha(accent, 0.35) }
            AppIcon {
                anchors.centerIn: parent
                name: parent.iconName; size: shareRoot._iconSize; color: parent.accent
            }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: shareRoot.shareRequested(parent.formatTag)
            }
            Accessible.name: parent.labelText
        }
    }

    // ── Labeled: icon+text button (Dashboard/Report overlays) ───────────────
    Component {
        id: labeledBtn
        Rectangle {
            property string iconName: ""
            property color accent: ThemeEngine.cyan
            property bool locked: false
            property string formatTag: ""
            property string labelText: ""

            Layout.minimumWidth: 80
            implicitHeight: shareRoot._btnHeight; radius: shareRoot._btnRadius
            clip: true
            opacity: locked ? 0.4 : 1.0
            color: Qt.alpha(accent, locked ? 0.06 : 0.12)
            border { width: 1; color: Qt.alpha(accent, locked ? 0.25 : 0.4) }
            RowLayout {
                anchors.centerIn: parent; spacing: 6
                AppIcon {
                    visible: parent.parent.iconName !== ""
                    name: parent.parent.iconName; size: shareRoot._iconSize
                    color: parent.parent.accent
                }
                Label {
                    text: parent.parent.labelText + (parent.parent.locked ? "  " + Tr.premiumBadge : "")
                    color: ThemeEngine.textPrimary
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: shareRoot.mode === "wide" ? 12 : 12
                    font.weight: Font.Medium
                }
            }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: shareRoot.shareRequested(parent.formatTag)
            }
        }
    }
}
