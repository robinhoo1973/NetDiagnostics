import QtQuick
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
            id: repeater
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

    // 5WHY (复核 2026-08-20 直接实例化残留): 停止时条停在 PropertyAction
    // 中途透明度（0.7）——补复位（与 PathAnimation 同模式）。
    function resetVisuals() {
        for (var i = 0; i < repeater.count; ++i) {
            var d = repeater.itemAt(i)
            if (d) d.opacity = 0
        }
    }
}
