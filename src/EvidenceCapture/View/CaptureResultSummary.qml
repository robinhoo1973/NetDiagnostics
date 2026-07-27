// =============================================================================
// CaptureResultSummary.qml — Completion summary after capture finishes
// =============================================================================
// Shows success or failure result.  In failure mode, the capture count,
// recording path, and elapsed time are omitted; instead the error code
// and user-facing message are displayed so the user knows why it failed.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.85)
    z: 2100

    property string sessionPath: ""
    property int totalScreenshots: 0
    property string recordingFile: ""
    property string elapsedTime: ""
    // 5WHY: Added error-state properties so the overlay can be reused for
    // captureFailed signals instead of silently hiding with no feedback.
    property bool isError: false
    property string errorMessage: ""
    property string errorCode: ""

    // Countdown auto-close — longer for recording (video encoding may still run)
    property int countdown: root.recordingFile !== "" ? 30 : 15
    // 5WHY: Name _dismissed (not dismissed) to avoid collision with signal dismissed()
    // QML property and signal must not share the same name — property shadows signal
    // for non-call access, breaking item.dismissed.connect(...) in AppContent.qml.
    property bool _dismissed: false

    // 5WHY: Hoist platform checks to a single readonly property instead of
    // evaluating Qt.platform.os === "ios" in 7 separate QML property bindings
    // (visible, color × 2, text, color, underline, cursorShape, enabled).
    // The platform never changes at runtime, so one evaluation is sufficient.
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

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.9)
        implicitHeight: sumCol.implicitHeight + 48
        // 5WHY: No max-height constraint — on landscape the dialog can overflow.
        height: Math.min(implicitHeight, parent.height * 0.92)
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: root.isError ? Qt.alpha(T.ThemeEngine.failRed, 0.3) : T.ThemeEngine.colors.borderCard }
        clip: true

        MouseArea { anchors.fill: parent } // absorb clicks

        ColumnLayout {
            id: sumCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            // Status icon — success (green) or error (red)
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 64; implicitHeight: 64; radius: 32
                color: root.isError
                    ? Qt.alpha(T.ThemeEngine.failRed, 0.12)
                    : Qt.alpha(T.ThemeEngine.passGreen, 0.12)
                Label {
                    anchors.centerIn: parent
                    text: root.isError ? "❌" : "✅"
                    font.pixelSize: 32
                }
            }

            // Title
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.isError ? "Capture Failed" : "Capture Complete"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 18
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
            }

            // Error details (visible only when isError)
            ColumnLayout { spacing: 6; visible: root.isError
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: root.errorCode
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 11
                    color: T.ThemeEngine.failRed; font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: root.errorMessage
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                    color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                }
            }

            // Results (visible only on success)
            ColumnLayout { spacing: 8; visible: !root.isError
                RowLayout {
                    Label { text: "📸 Screenshots:"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; color: T.ThemeEngine.textSecondary }
                    Item { Layout.fillWidth: true }
                    Label { text: root.totalScreenshots; font.family: T.ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.Bold; color: T.ThemeEngine.textPrimary }
                }
                RowLayout {
                    visible: root.recordingFile !== ""
                    Label { text: "🎥 Recording:"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; color: T.ThemeEngine.textSecondary }
                    Item { Layout.fillWidth: true }
                    Label { text: root.recordingFile !== "" ? "✓ Saved" : "—"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.Bold; color: T.ThemeEngine.passGreen }
                }
                RowLayout {
                    Label { text: "⏱️ Duration:"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; color: T.ThemeEngine.textSecondary }
                    Item { Layout.fillWidth: true }
                    Label { text: root.elapsedTime; font.family: T.ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.Bold; color: T.ThemeEngine.textPrimary }
                }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: T.ThemeEngine.colors.borderCard; visible: !root.isError }

            // Session path (success only)
            Label {
                Layout.fillWidth: true; visible: !root.isError
                text: "📁 " + root.sessionPath
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 10
                color: Qt.alpha(T.ThemeEngine.textSecondary, 0.7)
                wrapMode: Text.WrapAnywhere
            }

            // ── Focus mode exit reminder ────────────────────────────
            // 5WHY: iOS cannot programmatically disable Focus mode — the
            // user MUST manually exit DND via Settings.  Android auto-disables
            // DND via restoreSystemState() → platformDisableFocusMode().
            // Desktop never enabled DND (platformEnableFocusMode is a no-op).
            //
            // Platform-aware behaviour:
            //   iOS:     Show manual DND exit reminder (tappable → Settings)
            //   Android: Show brief confirmation that DND was auto-disabled
            //   Desktop: Hidden (no DND was ever active)
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 36; radius: 8
                visible: _isIos || Qt.platform.os === "android"
                color: _isIos
                    ? Qt.alpha(T.ThemeEngine.warnYellow, 0.08)
                    : Qt.alpha(T.ThemeEngine.passGreen, 0.08)
                border { width: 1
                    color: _isIos
                        ? Qt.alpha(T.ThemeEngine.warnYellow, 0.2)
                        : Qt.alpha(T.ThemeEngine.passGreen, 0.2)
                }
                Label {
                    anchors { fill: parent; margins: 10 }
                    text: _isIos
                        ? Tr.captureDndIosMsg
                        : Tr.captureDndAndroidMsg
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 11
                    color: _isIos
                        ? T.ThemeEngine.warnYellow
                        : T.ThemeEngine.passGreen
                    font.underline: _isIos
                    verticalAlignment: Text.AlignVCenter
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: _isIos ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: _isIos
                    onClicked: {
                        if (captureOrchestrator) {
                            captureOrchestrator.openFocusSettings()
                        }
                    }
                }
            }
            // Done / Dismiss button with countdown
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 44; radius: 12
                color: root.isError ? Qt.alpha(T.ThemeEngine.failRed, 0.2) : T.ThemeEngine.cyan
                Label {
                    anchors.centerIn: parent
                    text: root.isError
                        ? "✕ Dismiss"
                        : (root.countdown > 0 ? "✓ Done (" + root.countdown + "s)" : "✓ Done")
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                    font.weight: Font.Bold; color: root.isError ? T.ThemeEngine.failRed : "#0F172A"
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        countdownTimer.stop()
                        // 5WHY: Defensive guard — if the countdown auto-timer
                        // fires on the same event-loop tick, root._dismissed
                        // is already true and the signal was already emitted
                        // (see countdownTimer.onTriggered).  Prevent double-fire
                        // of the dismissed signal — the handler in AppContent
                        // is idempotent, but a future handler may not be.
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
