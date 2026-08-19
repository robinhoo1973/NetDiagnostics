import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── TypeAnimation.qml — Sequential element reveal for ~5 diagnostic tests ──
// ARP Table, URL Parsing, HTTP Headers, SSH, CurlVerbose, Ldap.
// Vertical bars appear one after another, simulating data rows being typed.
//
// Usage: TypeAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int charDelay: Tokens.tokens.typeCharDelay

    // ── 3 animated bars representing rows of text ────────────────────────
    Column {
        anchors { centerIn: parent }
        spacing: 4

        Repeater {
            model: 3
            Rectangle {
                id: typeBar
                width: 32; height: 3; radius: 1.5
                color: root.accentColor
                opacity: 0

                SequentialAnimation {
                    running: root.running
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 120 }
                    // Each delegate targets its own opacity via `parent`
                    // (enclosing Rectangle), avoiding fragile
                    // parent.children[index+1] scope-chain indexing.
                    PropertyAction { target: typeBar; property: "opacity"; value: 0.7 }
                    PauseAnimation { duration: 200 }
                    PropertyAction { target: typeBar; property: "opacity"; value: 0.15 }
                    PauseAnimation { duration: 120 }
                }
            }
        }
    }
}
