// =============================================================================
// CaptureResultSummary.qml — Completion summary after capture finishes
// =============================================================================
// Modern redesign: Icon-driven status display, stat-row layout with SVG icons,
// gradient action button, smooth entry animation, and platform-aware DND notice.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.85)
    z: 2100

    // ── Entry animation ─────────────────────────────────────────────
    scale: 0.92; opacity: 0
    Behavior on scale  { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Component.onCompleted: { scale = 1.0; opacity = 1.0 }

    property string sessionPath: ""
    property int totalScreenshots: 0
    property string recordingFile: ""
    property string elapsedTime: ""
    property bool isError: false
    property string errorMessage: ""
    property string errorCode: ""

    // 5WHY: countdown starts as a declarative binding (30s for recording,
    // 15s for screenshot-only).  The Timer's onTriggered imperatively assigns
    // root.countdown = ..., which BREAKS the binding after the first tick.
    // This is safe because AppContent sets recordingFile SYNCHRONOUSLY via
    // onCaptureCompleted → the binding resolves to its final value before the
    // 1-second Timer fires.  If recordingFile ever arrives asynchronously
    // (e.g. from a network callback), the countdown would not extend to 30s.
    property int countdown: root.recordingFile !== "" ? 30 : 15
    property bool _dismissed: false
    readonly property bool _isIos: Qt.platform.os === "ios"

    signal dismissed()

    Timer {
        id: countdownTimer
        interval: 1000; running: true; repeat: true
        onTriggered: {
            root.countdown = Math.max(0, root.countdown - 1)
            if (root.countdown <= 0 && !root._dismissed) {
                root._dismissed = true
                countdownTimer.stop()
                root.dismissed()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (!root._dismissed) {
                root._dismissed = true
                root.dismissed()
            }
        }
    }

    // ── Card ────────────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.9)
        implicitHeight: sumCol.implicitHeight + 48
        height: Math.min(implicitHeight, parent.height * 0.92)
        radius: 28
        color: T.ThemeEngine.colors.card
        border {
            width: 1
            color: root.isError
                ? Qt.alpha(T.ThemeEngine.failRed, 0.3)
                : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.6)
        }
        clip: true

        // Top accent — 2px refined, red for error, green for success
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 2; radius: 2
            color: root.isError ? T.ThemeEngine.failRed : T.ThemeEngine.passGreen
        }

        MouseArea { anchors.fill: parent }

        // 5WHY: Gradient at card level (not inside sumCol ColumnLayout).
        // Same reasoning as CaptureModePanel — QQuickGradient is not an Item.
        Gradient {
            id: doneGradient
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: T.ThemeEngine.primary }
            GradientStop { position: 1.0; color: T.ThemeEngine.cyan }
        }

        ColumnLayout {
            id: sumCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            // ── Status icon ─────────────────────────────────────────
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 64; implicitHeight: 64; radius: 32
                color: root.isError
                    ? Qt.alpha(T.ThemeEngine.failRed, 0.10)
                    : Qt.alpha(T.ThemeEngine.passGreen, 0.10)
                border {
                    width: 1
                    color: root.isError
                        ? Qt.alpha(T.ThemeEngine.failRed, 0.2)
                        : Qt.alpha(T.ThemeEngine.passGreen, 0.2)
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: root.isError ? "error" : "check"
                    size: 30
                    color: root.isError ? T.ThemeEngine.failRed : T.ThemeEngine.passGreen
                }
            }

            // ── Title ───────────────────────────────────────────────
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.isError ? T.Tr.captureFailed : T.Tr.captureComplete
                font.pixelSize: 18
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
            }

            // ── Error details ───────────────────────────────────────
            ColumnLayout {
                spacing: 8; visible: root.isError
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 32; radius: 8
                    color: Qt.alpha(T.ThemeEngine.failRed, 0.08)
                    border { width: 1; color: Qt.alpha(T.ThemeEngine.failRed, 0.15) }
                    Label {
                        anchors.centerIn: parent
                        text: root.errorCode
                        font.pixelSize: 12
                        color: T.ThemeEngine.failRed; font.weight: Font.DemiBold
                    }
                }
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: root.errorMessage
                    font.pixelSize: 13
                    color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                    lineHeight: 1.4
                }
            }

            // ── Stats rows (success only) ───────────────────────────
            ColumnLayout {
                spacing: 8; visible: !root.isError

                // Screenshots count
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 10
                    color: Qt.alpha(T.ThemeEngine.colors.input, 0.4)
                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 12
                        AppIcon { name: "camera"; size: 18; color: T.ThemeEngine.cyan }
                        Label {
                            text: T.Tr.captureScreenshotsCount
                            font.pixelSize: 13
                            color: T.ThemeEngine.textSecondary
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: root.totalScreenshots
                            
                            font.pixelSize: 18; font.weight: Font.Bold
                            color: T.ThemeEngine.textPrimary
                        }
                    }
                }

                // Recording status
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 10
                    visible: root.recordingFile !== ""
                    color: Qt.alpha(T.ThemeEngine.colors.input, 0.4)
                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 12
                        AppIcon { name: "video"; size: 18; color: T.ThemeEngine.passGreen }
                        Label {
                            text: T.Tr.captureRecordingLabel2
                            font.pixelSize: 13
                            color: T.ThemeEngine.textSecondary
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            implicitWidth: 64; implicitHeight: 24; radius: 6
                            color: Qt.alpha(T.ThemeEngine.passGreen, 0.12)
                            Label {
                                anchors.centerIn: parent
                                text: T.Tr.captureSavedBadge
                                
                                font.pixelSize: 11; font.weight: Font.DemiBold
                                color: T.ThemeEngine.passGreen
                            }
                        }
                    }
                }

                // Duration
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 10
                    color: Qt.alpha(T.ThemeEngine.colors.input, 0.4)
                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 12
                        AppIcon { name: "timer"; size: 18; color: T.ThemeEngine.accentBlue }
                        Label {
                            text: T.Tr.captureTotalDuration
                            font.pixelSize: 13
                            color: T.ThemeEngine.textSecondary
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: root.elapsedTime
                            
                            font.pixelSize: 18; font.weight: Font.Bold
                            color: T.ThemeEngine.textPrimary
                        }
                    }
                }
            }

            // ── Divider ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 1
                visible: !root.isError
                color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.4)
            }

            // ── Session path (success only) ─────────────────────────
            Label {
                Layout.fillWidth: true; visible: !root.isError
                text: root.sessionPath
                font.pixelSize: 10
                color: Qt.alpha(T.ThemeEngine.textSecondary, 0.6)
                // 5WHY: elide:Text.ElideMiddle is dead when wrapMode is set —
                // Qt docs: elide only takes effect with NoWrap or maximumLineCount:1.
                wrapMode: Text.WrapAnywhere
            }

            // ── Focus mode reminder ─────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 40; radius: 10
                visible: _isIos || Qt.platform.os === "android"
                color: _isIos
                    ? Qt.alpha(T.ThemeEngine.warnYellow, 0.06)
                    : Qt.alpha(T.ThemeEngine.passGreen, 0.06)
                border { width: 1
                    color: _isIos
                        ? Qt.alpha(T.ThemeEngine.warnYellow, 0.15)
                        : Qt.alpha(T.ThemeEngine.passGreen, 0.15)
                }
                RowLayout {
                    anchors { fill: parent; margins: 10 }
                    spacing: 8
                    AppIcon {
                        name: _isIos ? "warning" : "check"
                        size: 14
                        color: _isIos ? T.ThemeEngine.warnYellow : T.ThemeEngine.passGreen
                    }
                    Label {
                        Layout.fillWidth: true
                        text: _isIos ? T.Tr.captureDndIosMsg : T.Tr.captureDndAndroidMsg
                        font.pixelSize: 11
                        color: _isIos ? T.ThemeEngine.warnYellow : T.ThemeEngine.passGreen
                        font.underline: _isIos
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: _isIos ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: _isIos
                    onClicked: {
                        if (captureOrchestrator) captureOrchestrator.openFocusSettings()
                    }
                }
            }

            // ── Dismiss button ──────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 50; radius: 14
                color: root.isError
                    ? Qt.alpha(T.ThemeEngine.failRed, 0.12)
                    : T.ThemeEngine.cyan
                scale: dismissMa.pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }
                gradient: root.isError ? null : doneGradient
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    AppIcon {
                        name: root.isError ? "close" : "check"
                        size: 18
                        // 5WHY: Dark text/icon on cyan-filled button is readable on both themes.
                        color: root.isError ? T.ThemeEngine.failRed : "#0F172A"
                    }
                    Label {
                        // 5WHY: Show countdown on both success and error so the user
                        // knows when the overlay will auto-dismiss.  Without
                        // this, the 15s error auto-dismiss is invisible.
                        text: (root.isError ? T.Tr.captureDismiss : T.Tr.captureDone)
                            + (root.countdown > 0 ? " (" + root.countdown + ")" : "")
                        
                        font.pixelSize: 15; font.weight: Font.Bold
                        color: root.isError ? T.ThemeEngine.failRed : "#0F172A"
                    }
                }
                MouseArea {
                    id: dismissMa
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        countdownTimer.stop()
                        if (!root._dismissed) {
                            root._dismissed = true
                            root.dismissed()
                        }
                    }
                }
            }
        }
    }
}
