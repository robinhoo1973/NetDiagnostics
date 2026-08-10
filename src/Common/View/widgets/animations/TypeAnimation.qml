import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── TypeAnimation.qml — Sequential element reveal for ~5 diagnostic tests ──
// ARP Table, URL Parsing, HTTP Headers, SSH, CurlVerbose, Ldap.
// Vertical bars appear one after another, simulating data rows being typed.
//
// Usage: TypeAnimation { anchors.fill: parent; running: testRunning }

Item {
    id: root
    property bool running: false
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"
    property int charDelay: Tokens.tokens.typeCharDelay

    // ── 3 animated bars representing rows of text ────────────────────────
    Column {
        anchors { centerIn: parent }
        spacing: 4

        Repeater {
            model: 3
            Rectangle {
                width: 32; height: 3; radius: 1.5
                color: root.accentColor
                opacity: 0

                SequentialAnimation {
                    running: root.running
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 120 }
                    // 5WHY: parent.children[0] is the Repeater, so the Nth delegate
                    // Rectangle is at parent.children[N+1].  Use index+1 to target
                    // the correct child — targeting index alone hits one off.
                    PropertyAction { target: parent.children[index + 1]; property: "opacity"; value: 0.7 }
                    PauseAnimation { duration: 200 }
                    PropertyAction { target: parent.children[index + 1]; property: "opacity"; value: 0.15 }
                    PauseAnimation { duration: 120 }
                }
            }
        }
    }
}
