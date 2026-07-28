// =============================================================================
// CaptureModePanel.qml — Capture mode selection panel
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §2.1
//
// Modern redesign (2026-07): Card-based mode toggles with SVG iconography,
// smooth scale-in entry animation, glass-morphism backdrop, Material Design 3
// inspired selection states, and proper button hierarchy.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.82)
    z: 2000
    // 5WHY: Loader-loaded components run in isolated QML contexts where
    // ApplicationWindow font propagation through the Loader boundary is
    // version-dependent (Qt 6.5+ reliable, earlier Qt fragile).  Set
    // the font explicitly on the root Rectangle so ALL child Labels
    // inherit the correct monospace font regardless of Qt version.
    font.family: T.ThemeEngine.monoFont
    // 5WHY: No explicit font.family on individual Labels.  Loader-loaded QML
    // components run in an isolated context where "JetBrains Mono" (the
    // ThemeEngine.monoFont value) may not resolve on embedded Linux ARM
    // with minimal fontconfig.  Removing the override lets Labels inherit
    // "DejaVu Sans Mono" from the ApplicationWindow → AppContent → Loader
    // → this panel — DejaVu Sans Mono is FontLoader-proven to work
    // everywhere, has 128 box-drawing glyphs + symbols that JetBrains
    // lacks, and is already the default across all main screens.

    // ── Scale-in animation ──────────────────────────────────────────
    scale: 0.92; opacity: 0
    Behavior on scale  { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Component.onCompleted: { scale = 1.0; opacity = 1.0 }

    // ── State ───────────────────────────────────────────────────────
    property bool wantsScreenshot: true
    property bool wantsRecording: captureOrchestrator ? captureOrchestrator.supportsBothModes : false
    property string diagUrl: appState.target || "https://httpbin.org"

    readonly property int computedMode: (wantsScreenshot && wantsRecording) ? 2
                                      : wantsScreenshot ? 0
                                      : wantsRecording ? 1
                                      : -1

    signal startRequested(int mode, string url)
    signal cancelled()

    // Backdrop dismiss
    MouseArea {
        anchors.fill: parent
        onClicked: root.cancelled()
    }

    // ── Main card ───────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.92)
        implicitHeight: panelCol.implicitHeight + 48
        height: Math.min(implicitHeight, parent.height * 0.92)
        // M3 spec: 28dp corner radius for basic dialogs
        radius: 28
        color: T.ThemeEngine.colors.card
        border { width: 1; color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.6) }
        clip: true

        // Subtle top accent — 2px for refined appearance
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 2
            radius: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: T.ThemeEngine.primary }
                GradientStop { position: 1.0; color: T.ThemeEngine.cyan }
            }
        }

        MouseArea { anchors.fill: parent } // absorb clicks

        // 5WHY: Gradient MUST be declared at card level, NOT inside a Layout.
        // QQuickGradient is not a QQuickItem — placing it as a child of
        // RowLayout/ColumnLayout causes undefined layout behavior (silently
        // ignored in Qt 6.x, may crash on static/embedded Qt builds).
        Gradient {
            id: startGradient
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: T.ThemeEngine.primary }
            GradientStop { position: 1.0; color: T.ThemeEngine.cyan }
        }

        ColumnLayout {
            id: panelCol
            // M3 spec: 24dp uniform padding for basic dialogs
            anchors { fill: parent; margins: 24 }
            spacing: 0

            // ── Header ──────────────────────────────────────────────
            Item { Layout.preferredHeight: 4 }
            RowLayout {
                spacing: 12
                AppIcon { name: "camera"; size: 24; color: T.ThemeEngine.cyan }
                Label {
                    text: T.Tr.captureTitle
                    
                    font.pixelSize: 18; font.weight: Font.Bold
                    color: T.ThemeEngine.textPrimary
                }
            }

            // M3: 16dp title→body spacing via 4+4+8 layout items
            Item { Layout.preferredHeight: 4 }
            Label {
                Layout.fillWidth: true
                text: T.Tr.captureDesc
                font.pixelSize: 12
                color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                lineHeight: 1.45
            }

            Item { Layout.preferredHeight: 4 }
            // ── Mode selector cards ─────────────────────────────────
            ColumnLayout {
                spacing: 8

                // M3: comfortable 72px card height for 48px minimum touch target
                Rectangle {
                    id: screenshotCard
                    Layout.fillWidth: true; implicitHeight: 72; radius: 12
                    color: root.wantsScreenshot
                           ? Qt.alpha(T.ThemeEngine.cyan, 0.10)
                           : Qt.alpha(T.ThemeEngine.colors.input, 0.3)
                    border {
                        width: root.wantsScreenshot ? 2 : 1
                        color: root.wantsScreenshot
                               ? T.ThemeEngine.cyan
                               : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.5)
                    }
                    // Smooth color transition
                    Behavior on color  { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    Rectangle {
                        anchors.fill: parent; radius: 12
                        visible: root.wantsScreenshot
                        color: "transparent"
                        border { width: 2; color: Qt.alpha(T.ThemeEngine.cyan, 0.06) }
                    }

                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 12

                        // Icon container
                        Rectangle {
                            implicitWidth: 42; implicitHeight: 42; radius: 10
                            color: root.wantsScreenshot
                                   ? Qt.alpha(T.ThemeEngine.cyan, 0.18)
                                   : Qt.alpha(T.ThemeEngine.textSecondary, 0.08)
                            Behavior on color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "camera"; size: 22
                                color: root.wantsScreenshot ? T.ThemeEngine.cyan : T.ThemeEngine.textSecondary
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: T.Tr.captureScreenshotsLabel

                                font.pixelSize: 14; font.weight: Font.DemiBold
                                color: T.ThemeEngine.textPrimary
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                            Label {
                                Layout.fillWidth: true
                                text: T.Tr.captureScreenshotsDesc
                                // 5WHY: These are instructional SENTENCES
                                // (40-50 chars in German/Russian), not
                                // single-word labels.  ElideRight truncates
                                // them on narrow cards.  WordWrap with 2
                                // lines keeps the description readable.
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                font.pixelSize: 11
                                color: T.ThemeEngine.textSecondary
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Selection indicator
                        Rectangle {
                            implicitWidth: 24; implicitHeight: 24; radius: 12
                            color: root.wantsScreenshot
                                   ? T.ThemeEngine.cyan
                                   : "transparent"
                            border {
                                width: 2
                                color: root.wantsScreenshot
                                       ? T.ThemeEngine.cyan
                                       : Qt.alpha(T.ThemeEngine.textSecondary, 0.3)
                            }
                            Behavior on color        { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "check"; size: 14
                                // 5WHY: #0F172A on cyan background is readable on both light
                                // and dark themes — cyan is mid-luminance, dark text always contrasts.
                                color: root.wantsScreenshot ? "#0F172A" : "transparent"
                                visible: root.wantsScreenshot
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (captureOrchestrator && !captureOrchestrator.supportsBothModes) {
                                root.wantsScreenshot = !root.wantsScreenshot
                                if (root.wantsScreenshot) root.wantsRecording = false
                            } else {
                                root.wantsScreenshot = !root.wantsScreenshot
                            }
                        }
                    }
                }

                // ── Recording card ──────────────────────────────────
                Rectangle {
                    id: recordingCard
                    Layout.fillWidth: true; implicitHeight: 72; radius: 12
                    color: root.wantsRecording
                           ? Qt.alpha(T.ThemeEngine.cyan, 0.10)
                           : Qt.alpha(T.ThemeEngine.colors.input, 0.3)
                    border {
                        width: root.wantsRecording ? 2 : 1
                        color: root.wantsRecording
                               ? T.ThemeEngine.cyan
                               : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.5)
                    }
                    Behavior on color  { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    Rectangle {
                        anchors.fill: parent; radius: 12
                        visible: root.wantsRecording
                        color: "transparent"
                        border { width: 2; color: Qt.alpha(T.ThemeEngine.cyan, 0.06) }
                    }

                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 12

                        Rectangle {
                            implicitWidth: 42; implicitHeight: 42; radius: 10
                            color: root.wantsRecording
                                   ? Qt.alpha(T.ThemeEngine.cyan, 0.18)
                                   : Qt.alpha(T.ThemeEngine.textSecondary, 0.08)
                            Behavior on color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "video"; size: 22
                                color: root.wantsRecording ? T.ThemeEngine.cyan : T.ThemeEngine.textSecondary
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: T.Tr.captureRecordingLabel

                                font.pixelSize: 14; font.weight: Font.DemiBold
                                color: T.ThemeEngine.textPrimary
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                            Label {
                                Layout.fillWidth: true
                                text: T.Tr.captureRecordingDesc
                                // 5WHY: Same rationale as screenshot description —
                                // instructional sentence needs 2-line WordWrap
                                // for non-English translations.
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                font.pixelSize: 11
                                color: T.ThemeEngine.textSecondary
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            implicitWidth: 24; implicitHeight: 24; radius: 12
                            color: root.wantsRecording
                                   ? T.ThemeEngine.cyan
                                   : "transparent"
                            border {
                                width: 2
                                color: root.wantsRecording
                                       ? T.ThemeEngine.cyan
                                       : Qt.alpha(T.ThemeEngine.textSecondary, 0.3)
                            }
                            Behavior on color        { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "check"; size: 14
                                // 5WHY: Same reasoning as screenshot card — cyan bg + dark check is universal.
                                color: root.wantsRecording ? "#0F172A" : "transparent"
                                visible: root.wantsRecording
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (captureOrchestrator && !captureOrchestrator.supportsBothModes) {
                                root.wantsRecording = !root.wantsRecording
                                if (root.wantsRecording) root.wantsScreenshot = false
                            } else {
                                root.wantsRecording = !root.wantsRecording
                            }
                        }
                    }
                }
            }

            // ── Both-mode badge ─────────────────────────────────────
            Rectangle {
                visible: root.computedMode === 2 && captureOrchestrator && captureOrchestrator.supportsBothModes
                Layout.fillWidth: true; implicitHeight: 32; radius: 8
                color: Qt.alpha(T.ThemeEngine.cyan, 0.08)
                border { width: 1; color: Qt.alpha(T.ThemeEngine.cyan, 0.2) }
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    AppIcon { name: "badge-info"; size: 14; color: T.ThemeEngine.cyan }
                    Label {
                        // 5WHY: Without Layout.fillWidth, this Label gets
                        // its implicitWidth (full text width), making
                        // ElideRight a no-op — elide only triggers when
                        // the text exceeds the available width.
                        Layout.fillWidth: true
                        text: T.Tr.captureBothHint
                        font.pixelSize: 11
                        color: T.ThemeEngine.cyan; elide: Text.ElideRight; maximumLineCount: 1
                    }
                }
            }

            // ── Divider ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 1
                color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.4)
            }

            // ── URL input ───────────────────────────────────────────
            ColumnLayout {
                spacing: 6
                RowLayout {
                    spacing: 6
                    AppIcon { name: "globe"; size: 14; color: T.ThemeEngine.textSecondary }
                    Label {
                        text: T.Tr.captureDiagUrl
                        
                        font.pixelSize: 12; font.weight: Font.DemiBold
                        color: T.ThemeEngine.textSecondary
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 44; radius: 10
                    color: T.ThemeEngine.bgInput
                    border {
                        width: 1
                        color: urlInput.activeFocus
                               ? T.ThemeEngine.cyan
                               : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.5)
                    }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    RowLayout {
                        anchors { fill: parent; margins: 12 }
                        spacing: 8
                        AppIcon { name: "target"; size: 16; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.5) }
                        TextInput {
                            id: urlInput
                            Layout.fillWidth: true
                            text: root.diagUrl
                            font.pixelSize: 13
                            color: T.ThemeEngine.textPrimary
                            // 5WHY: Without placeholder text, users don't know
                            // what URL format is expected.  Show a hint — the
                            // default is https://httpbin.org for testing but
                            // users should enter their own diagnostic target.
                            Text {
                                anchors.fill: parent
                                text: "Enter diagnostic URL..."
                                font.pixelSize: 13
                                color: Qt.alpha(T.ThemeEngine.textSecondary, 0.4)
                                visible: !urlInput.text && !urlInput.activeFocus
                            }
                            clip: true
                            selectByMouse: true
                            onTextChanged: root.diagUrl = text
                        }
                    }
                }
            }

            // ── Action buttons ──────────────────────────────────────
            RowLayout {
                spacing: 12
                // Cancel button
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 14
                    color: "transparent"
                    border { width: 1.5; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.25) }
                    // Hover feedback via scale
                    scale: cancelMa.pressed ? 0.97 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Label {
                        anchors.centerIn: parent
                        text: T.Tr.captureCancelShort
                        
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        color: T.ThemeEngine.textSecondary
                    }
                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.cancelled()
                    }
                }
                // Start button
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 14
                    color: root.computedMode >= 0
                           ? T.ThemeEngine.cyan
                           : Qt.alpha(T.ThemeEngine.textSecondary, 0.15)
                    scale: startMa.pressed && root.computedMode >= 0 ? 0.97 : 1.0
                    Behavior on scale  { NumberAnimation { duration: 100 } }
                    Behavior on color  { ColorAnimation { duration: 200 } }
                    // Gradient accent for enabled state — reuses pre-declared id
                    gradient: root.computedMode >= 0 ? startGradient : null
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        AppIcon {
                            name: "play"; size: 16
                            // 5WHY: Dark icon/text on gradient cyan/primary button
                            // is readable on both light & dark themes.
                            color: root.computedMode >= 0 ? "#0F172A" : T.ThemeEngine.textSecondary
                        }
                        Label {
                            text: T.Tr.captureStartBtn
                            
                            font.pixelSize: 15; font.weight: Font.Bold
                            color: root.computedMode >= 0 ? "#0F172A" : T.ThemeEngine.textSecondary
                        }
                    }
                    MouseArea {
                        id: startMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        enabled: root.computedMode >= 0
                        onClicked: root.startRequested(root.computedMode, root.diagUrl)
                    }
                }
            }
        }
    }
}
