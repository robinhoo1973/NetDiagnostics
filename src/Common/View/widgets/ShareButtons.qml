import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// ══════════════════════════════════════════════════════════════════════════════
// ShareButtons — Reusable PDF/HTML share button pair
//
// Three visual modes:
//   "compact"  — icon-only squares (48dp), 34dp icons, for Diagnostic header
//   "labeled"  — icon+text buttons (fillWidth), for Dashboard preview overlay
//   "wide"     — icon+text buttons (fillWidth, 48dp height), for Report preview
//
// Color overrides let each page set accent colours appropriate to its theme:
//   - Diagnostic:  pdfAccent=ThemeEngine.cyan,  htmlAccent=ThemeEngine.primary
//   - Dashboard:   pdfAccent=ThemeEngine.cyan,  htmlAccent=ThemeEngine.primary
//   - Report:      pdfAccent=ThemeEngine.cyan,  htmlAccent=ThemeEngine.primary
//
// Icons: uses file-pdf.svg / file-html.svg (not -sm variants).
// At 34dp in a 48dp button (71% fill), the "PDF"/"HTML" text in the document
// icon is legible without the simplified single-letter -sm variants.
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
    // 5WHY: compact icon was 26dp inside 48dp button (54% fill) using -sm SVG
    // variants with simplified single-letter content ("P"/"H").  Users reported
    // icons still too small to distinguish PDF from HTML at a glance.
    // Fix: use full file-pdf/file-html SVGs (not -sm) at 34dp (71% fill).
    // The regular SVGs have "PDF"/"HTML" text in the document icon — legible
    // at 34dp (viewBox 24 → 34dp render ≈ 7px text height).
    readonly property int _iconSize:  mode === "compact" ? 34 :
                                      (mode === "wide" ? 20 : 16)
    readonly property int _btnHeight: mode === "compact" ? 48 :
                                      (mode === "wide" ? 48 : 40)
    readonly property int _btnRadius: mode === "wide" ? 10 : 8

    // ── PDF button ──────────────────────────────────────────────────────────
    Loader {
        id: pdfLoader
        Layout.fillWidth: shareRoot.mode !== "compact"
        sourceComponent: shareRoot.mode === "compact" ? compactBtn : labeledBtn
        onLoaded: {
            // 5WHY: one-shot assignment means locked/accent changes (e.g.
            // premium purchase) are not reflected. Use Qt.binding() to keep
            // inner properties reactive.
            // 5WHY: compact mode used file-pdf-sm (single "P" letter) — too
            // small to distinguish. Now uses file-pdf ("PDF" text) at 34dp.
            item.iconName = "file-pdf"
            item.accent = Qt.binding(function() { return shareRoot.pdfAccent })
            item.locked = Qt.binding(function() { return shareRoot.locked })
            item.formatTag = "pdf"
            item.labelText = Qt.binding(function() { return shareRoot._isMobile ? Tr.sharePdfBtn : Tr.emailPdfBtn })
        }
    }

    // ── HTML button ─────────────────────────────────────────────────────────
    Loader {
        id: htmlLoader
        Layout.fillWidth: shareRoot.mode !== "compact"
        sourceComponent: shareRoot.mode === "compact" ? compactBtn : labeledBtn
        onLoaded: {
            // 5WHY: compact mode used file-html-sm — too small. Now uses
            // file-html ("HTML" text) at 34dp, matching the PDF button.
            item.iconName = "file-html"
            item.accent = Qt.binding(function() { return shareRoot.htmlAccent })
            item.locked = Qt.binding(function() { return shareRoot.locked })
            item.formatTag = "html"
            item.labelText = Qt.binding(function() { return shareRoot._isMobile ? Tr.shareHtmlBtn : Tr.emailHtmlBtn })
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
                    font.pixelSize: 12
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
