import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"

// ══════════════════════════════════════════════════════════════════════════════
// ShareButtons — Reusable PDF/HTML share button pair
//
// Four visual modes:
//   "bare"     — icon-only, no background wrapper (AppBar)
//   "compact"  — solid-color icon squares (36dp), icon fills button (100%)
//   "labeled"  — icon+text buttons (fillWidth), for Dashboard preview overlay
//   "wide"     — icon+text buttons (fillWidth, 48dp height), for Report preview
//
// Accent colors set by each page:
//   - Diagnostic:  pdfAccent=ThemeEngine.cyan,  htmlAccent=ThemeEngine.primary
//   - Dashboard:   pdfAccent=ThemeEngine.cyan,  htmlAccent=ThemeEngine.primary
//   - Report:      pdfAccent=ThemeEngine.cyan,  htmlAccent=ThemeEngine.primary
// ══════════════════════════════════════════════════════════════════════════════

RowLayout {
    id: shareRoot
    spacing: _spacing

    // ── Public API ──────────────────────────────────────────────────────────
    property string mode: "compact"           // "bare" | "compact" | "labeled" | "wide"
    property bool   locked: !appState.isPremium
    property color  pdfAccent: ThemeEngine.failRed
    property color  htmlAccent: ThemeEngine.accentBlue
    signal shareRequested(string format)

    readonly property bool _isMobile: ThemeEngine.isMobile

    // ── Sizes derived from mode ─────────────────────────────────────────────
    // 5WHY: "bare" mode — clickable icons without any button rectangle.
    // Icon size 20dp matches AppBar icon scale; spacing iconSize/4 ≈ 5dp.
    readonly property int _iconSize:  mode === "bare"    ? 20 :
                                      mode === "compact" ? _btnHeight :
                                      mode === "wide"    ? 40 : Math.round(_btnHeight * 0.9)
    readonly property int _spacing:   mode === "bare"    ? Math.max(1, Math.round(_iconSize / 4)) : 6
    readonly property int _btnHeight: mode === "compact" ? 36 :
                                      mode === "wide"    ? 48 : 40
    readonly property int _btnRadius: mode === "wide"    ? 10 : 8

    // ── PDF button ──────────────────────────────────────────────────────────
    Loader {
        id: pdfLoader
        Layout.fillWidth: shareRoot.mode !== "compact" && shareRoot.mode !== "bare"
        sourceComponent: shareRoot.mode === "compact" ? compactBtn
                       : shareRoot.mode === "bare"    ? bareBtn : labeledBtn
        onLoaded: {
            item.iconName = "file-pdf"
            item.locked = Qt.binding(function() { return shareRoot.locked })
            item.formatTag = "pdf"
            item.labelText = Qt.binding(function() { return shareRoot._isMobile ? Tr.sharePdfBtn : Tr.emailPdfBtn })
        }
    }

    // ── HTML button ─────────────────────────────────────────────────────────
    Loader {
        id: htmlLoader
        Layout.fillWidth: shareRoot.mode !== "compact" && shareRoot.mode !== "bare"
        sourceComponent: shareRoot.mode === "compact" ? compactBtn
                       : shareRoot.mode === "bare"    ? bareBtn : labeledBtn
        onLoaded: {
            item.iconName = "file-html"
            item.locked = Qt.binding(function() { return shareRoot.locked })
            item.formatTag = "html"
            item.labelText = Qt.binding(function() { return shareRoot._isMobile ? Tr.shareHtmlBtn : Tr.emailHtmlBtn })
        }
    }

    // ── Bare: icon-only without button wrapper (AppBar) ────────────────
    Component {
        id: bareBtn
        Item {
            id: bareRoot
            // 5WHY: bareBtn is loaded as the top-level item by Loader, so
            // `parent` inside child elements refers to bareRoot, not the
            // Loader.  Unlike compactBtn/labeledBtn where the Rectangle is
            // the wrapper, bareRoot is an explicit Item carrying the props.
            property string iconName: ""
            property color accent: "transparent"
            property bool locked: false
            property string formatTag: ""
            property string labelText: ""

            implicitWidth: shareRoot._iconSize
            implicitHeight: shareRoot._iconSize

            AppIcon {
                id: bareIcon
                anchors.centerIn: parent
                name: bareRoot.iconName; size: shareRoot._iconSize
                color: bareRoot.accent
                opacity: bareRoot.locked ? 0.4 : 1.0
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: shareRoot.shareRequested(bareRoot.formatTag)
            }
            Accessible.name: bareRoot.labelText
            Binding {
                target: bareRoot
                property: "accent"
                value: bareRoot.iconName === "file-pdf" ? shareRoot.pdfAccent :
                       bareRoot.iconName === "file-html" ? shareRoot.htmlAccent :
                       "transparent"
            }
        }
    }

    // ── Compact: icon-only solid button (AppBar / header) ─────────────
    // 5WHY: subtle background made icons washed out. Now solid accent
    // fill + white icon for high contrast (WCAG 2.1 SC 1.4.11).
    // 5WHY: Binding { target; property; value: ternary } replaces the
    // previous Binding on accent { when } pattern.  Two separate
    // Binding-on-accent elements on the same property can conflict in
    // Qt 6 when instantiated via Loader — the second Binding can
    // silently fail to activate, leaving accent=transparent.
    // Single explicit Binding with a ternary expression that tracks ALL
    // dependencies (iconName + pdfAccent + htmlAccent) is battle-tested
    // since Qt 5.0 and works reliably in Loader-created instances.
    Component {
        id: compactBtn
        Rectangle {
            id: compactRect
            property string iconName: ""
            property color accent: "transparent"
            property bool locked: false
            property string formatTag: ""
            property string labelText: ""

            implicitWidth: shareRoot._btnHeight; implicitHeight: shareRoot._btnHeight
            radius: shareRoot._btnRadius
            opacity: locked ? 0.4 : 1.0
            color: accent
            AppIcon {
                anchors.centerIn: parent
                name: parent.iconName; size: shareRoot._iconSize
                color: "white"
            }
            MouseArea {
                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                onClicked: shareRoot.shareRequested(parent.formatTag)
            }
            Accessible.name: parent.labelText
            // ── Single explicit Binding (Qt 5.0+ compatible, Loader-safe) ──
            Binding {
                target: compactRect
                property: "accent"
                value: compactRect.iconName === "file-pdf" ? shareRoot.pdfAccent :
                       compactRect.iconName === "file-html" ? shareRoot.htmlAccent :
                       "transparent"
            }
        }
    }

    // ── Labeled: icon+text button (Dashboard/Report overlays) ───────────────
    Component {
        id: labeledBtn
        Rectangle {
            id: labeledRect
            property string iconName: ""
            property color accent: "transparent"
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
            // ── Single explicit Binding (Qt 5.0+ compatible, Loader-safe) ──
            Binding {
                target: labeledRect
                property: "accent"
                value: labeledRect.iconName === "file-pdf" ? shareRoot.pdfAccent :
                       labeledRect.iconName === "file-html" ? shareRoot.htmlAccent :
                       "transparent"
            }
        }
    }
}
