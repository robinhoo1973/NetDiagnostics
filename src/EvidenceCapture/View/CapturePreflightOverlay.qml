// =============================================================================
// CapturePreflightOverlay.qml — Focus/DND guide before capture starts
// =============================================================================
// Modern redesign: Clean card with staged instructions, SVG icon badges,
// proper button hierarchy (outlined secondary → filled primary), smooth entry.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.86)
    z: 2100
    font.family: T.ThemeEngine.monoFont

    // ── Entry animation ─────────────────────────────────────────────
    scale: 0.92; opacity: 0
    Behavior on scale  { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Component.onCompleted: { scale = 1.0; opacity = 1.0 }

    signal preflightConfirmed()
    signal cancelled()

    MouseArea { anchors.fill: parent } // absorb clicks

    // ── Card ────────────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.9)
        height: Math.min(preCol.implicitHeight + 48, parent.height * 0.92)
        radius: 28
        color: T.ThemeEngine.colors.card
        border { width: 1; color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.6) }
        clip: true

        // Top accent — 2px refined
        CardTopAccent { color: T.ThemeEngine.warnYellow }

        Flickable {
            id: cardFlick
            anchors.fill: parent
            contentWidth: width
            contentHeight: preCol.implicitHeight + 48
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: preCol
                width: cardFlick.width
                spacing: 18
                anchors {
                    left: parent.left; leftMargin: 24
                    right: parent.right; rightMargin: 24
                    top: parent.top; topMargin: 24
                }

                // ── Icon ────────────────────────────────────────────
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 64; implicitHeight: 64; radius: 32
                    color: Qt.alpha(T.ThemeEngine.warnYellow, 0.10)
                    border { width: 1; color: Qt.alpha(T.ThemeEngine.warnYellow, 0.2) }
                    AppIcon {
                        anchors.centerIn: parent
                        name: "warning"; size: 30
                        color: T.ThemeEngine.warnYellow
                    }
                }

                // ── Title ───────────────────────────────────────────
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: T.Tr.capturePrepare
                    font.pixelSize: 18
                    font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
                }

                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: T.Tr.capturePreflightTitle
                    font.pixelSize: 12
                    color: T.ThemeEngine.textSecondary
                }

                // ── Checklist items ─────────────────────────────────
                ColumnLayout {
                    spacing: 10
                    // Item 1
                    RowLayout {
                        spacing: 12
                        AppIcon { name: "badge-info"; size: 18; color: T.ThemeEngine.textSecondary }
                        Label {
                            Layout.fillWidth: true
                            text: T.Tr.captureDoNotTouch
                            font.pixelSize: 13
                            color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                        }
                    }
                    // Item 2
                    RowLayout {
                        spacing: 12
                        AppIcon { name: "sun"; size: 18; color: T.ThemeEngine.textSecondary }
                        Label {
                            Layout.fillWidth: true
                            text: T.Tr.captureScreenAwake
                            font.pixelSize: 13
                            color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                        }
                    }
                    // Item 3
                    RowLayout {
                        spacing: 12
                        AppIcon { name: "timer"; size: 18; color: T.ThemeEngine.textSecondary }
                        Label {
                            Layout.fillWidth: true
                            text: T.Tr.captureEstTime
                            wrapMode: Text.WordWrap
                            font.pixelSize: 13
                            color: T.ThemeEngine.textSecondary
                        }
                    }
                }

                // ── DND / Focus mode guide card ─────────────────────
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: dndCol.implicitHeight + 20
                    radius: 14
                    color: Qt.alpha(T.ThemeEngine.warnYellow, 0.06)
                    border { width: 1; color: Qt.alpha(T.ThemeEngine.warnYellow, 0.18) }

                    ColumnLayout {
                        id: dndCol
                        anchors { fill: parent; margins: 16 }
                        spacing: 10

                        RowLayout {
                            spacing: 8
                            AppIcon { name: "warning"; size: 16; color: T.ThemeEngine.warnYellow }
                            Label {
                                text: T.Tr.captureDndRequired
                                
                                font.pixelSize: 12; font.weight: Font.DemiBold
                                color: T.ThemeEngine.warnYellow
                            }
                        }

                        Label {
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: T.Tr.captureDndInstructions
                            font.pixelSize: 11
                            color: T.ThemeEngine.textSecondary; lineHeight: 1.5
                        }

                        RowLayout {
                            spacing: 10
                            // Open Settings
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: 42; radius: 10
                                color: "transparent"
                                border { width: 1.5; color: Qt.alpha(T.ThemeEngine.warnYellow, 0.35) }
                                scale: settingsMa.pressed ? 0.97 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                RowLayout {
                                    anchors.centerIn: parent; spacing: 6
                                    AppIcon { name: "gear"; size: 14; color: T.ThemeEngine.warnYellow }
                                    Label {
                                        text: T.Tr.captureOpenSettings
                                        
                                        font.pixelSize: 12; font.weight: Font.DemiBold
                                        color: T.ThemeEngine.warnYellow
                                    }
                                }
                                MouseArea {
                                    id: settingsMa
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { if (captureOrchestrator) captureOrchestrator.openFocusSettings() }
                                }
                            }
                            // I'm Ready
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: 42; radius: 10
                                color: T.ThemeEngine.cyan
                                scale: readyMa.pressed ? 0.97 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                RowLayout {
                                    anchors.centerIn: parent; spacing: 6
                                    AppIcon { name: "check"; size: 14
                                        // 5WHY: Dark check on cyan-filled button is readable on both themes.
                                        color: T.ThemeEngine.textOnAccent }
                                    Label {
                                        text: T.Tr.captureImReady
                                        
                                        font.pixelSize: 12; font.weight: Font.Bold
                                        // 5WHY: Dark text on cyan button is readable on both themes.
                                        color: T.ThemeEngine.textOnAccent
                                    }
                                }
                                MouseArea {
                                    id: readyMa
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: root.preflightConfirmed()
                                }
                            }
                        }
                    }
                }

                // ── Cancel button ───────────────────────────────────
                OutlineButton {
                    text: T.Tr.captureCancelBtn
                    onClicked: root.cancelled()
                }
            }
        }
    }
}
