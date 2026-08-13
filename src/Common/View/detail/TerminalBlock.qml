// =============================================================================
// TerminalBlock.qml — Styled terminal output block with typewriter animation
//
// Renders diagnostic terminal output (multi-line text) in a dark terminal-style
// card.  Lines appear one by one with an 80ms stagger, each fading and sliding
// in from the left.  Pure PropertyAnimation — no Canvas, no ShaderEffect — safe
// for iOS static Qt builds (5WHY #4: platform-safe rendering primitives only).
//
// Usage:
//   TerminalBlock {
//       text: "PING 8.8.8.8\n64 bytes from 8.8.8.8: icmp_seq=1 ttl=117\n..."
//       typewriter: true
//   }
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property string text: ""
    property bool typewriter: true
    // 主题自适应终端底色：暗色=surface，亮色=input（7-5 修复：原亮色下硬编码
    // 深藏青导致文字几乎不可读）
    property color terminalColor: ThemeEngine.isDark
        ? ThemeEngine.colors.surface : ThemeEngine.colors.input

    implicitWidth: 300
    // 5WHY: implicitHeight must track the dynamic content height so parent
    // ColumnLayouts can size correctly.  Using contentCol.implicitHeight
    // ensures this works on iOS static builds where anchors-positioned
    // children can collapse to zero height.
    implicitHeight: Math.max(60, contentCol.implicitHeight + 2 * padding)

    // ── Internal state ────────────────────────────────────────────────────
    readonly property int padding: 12
    readonly property var _lines: root.text ? root.text.split('\n') : []
    property int _visibleCount: 0

    // 5WHY: onTextChanged resets _visibleCount and restarts the typing
    // sequence.  Without this, switching from one diag's output to another
    // would show stale lines from the previous text until the old animation
    // finished.  Reset + restart guarantees fresh content each time.
    onTextChanged: {
        _visibleCount = 0
        if (typewriter && _lines.length > 0) {
            staggerTimer.start()
        } else {
            _visibleCount = _lines.length
        }
    }
    // 5WHY: When typewriter is toggled on after text is already set,
    // restart the animation from zero.  When toggled off, reveal all.
    onTypewriterChanged: {
        if (typewriter && _lines.length > 0 && _visibleCount >= _lines.length) {
            _visibleCount = 0
            staggerTimer.start()
        } else if (!typewriter) {
            _visibleCount = _lines.length
        }
    }

    // 5WHY: Internal restart point — called when component completes or
    // text arrives before the component initializes.  Ensures animation
    // always starts on first display.
    Component.onCompleted: {
        if (typewriter && _lines.length > 0) {
            staggerTimer.start()
        } else if (!typewriter) {
            _visibleCount = _lines.length
        }
    }

    // ── Stagger timer: reveals one line every 80ms ────────────────────────
    Timer {
        id: staggerTimer
        interval: 80
        repeat: true
        onTriggered: {
            _visibleCount += 1
            if (_visibleCount >= _lines.length) {
                stop()
            }
        }
    }

    // ── Visual ────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        // 5WHY: Terminal output must always render on a dark background for
        // readability of passGreen monospace text.  Use the public terminalColor
        // property so embedders can override (e.g., DetailPage sets it for
        // light-theme coherence).
        color: root.terminalColor
        radius: ThemeEngine.radius.md  // 8
        border {
            width: 1
            color: ThemeEngine.colors.borderCard
        }

        // ── Empty state ──────────────────────────────────────────────────
        Label {
            anchors.centerIn: parent
            visible: _lines.length === 0
            text: "—"
            font.family: ThemeEngine.monoFont
            font.pixelSize: 11
            color: ThemeEngine.colors.textMuted
        }

        // ── Terminal content ─────────────────────────────────────────────
        // 5WHY: Column (not ColumnLayout) inside Flickable — terminal output
        // can be tall (traceroute may produce 30+ lines).  Flickable provides
        // scrolling without requiring the parent to grow unboundedly.
        Flickable {
            id: flick
            anchors {
                fill: parent
                margins: root.padding
            }
            contentWidth: contentCol.width
            contentHeight: contentCol.height
            clip: true
            flickableDirection: Flickable.VerticalFlick
            // 5WHY: interactive only when content overflows — prevents the
            // Flickable from stealing touch events when all lines fit.
            interactive: contentHeight > height

            Column {
                id: contentCol
                width: flick.width
                spacing: 2

                Repeater {
                    id: lineRepeater
                    model: _lines

                    // 5WHY: Each line is an Item (not just a Label) because
                    // we need to animate both opacity AND x-translation.
                    // A plain Label's x property is not animatable via
                    // Behavior due to layout positioning conflicts.
                    Item {
                        id: lineItem
                        // 5WHY: width must be explicit — Column children
                        // without explicit width default to their implicit
                        // width, which prevents the x-translation slide
                        // from having visible space to move into.
                        width: contentCol.width
                        implicitHeight: lineLabel.implicitHeight + 2

                        // Visibility driven by typewriter counter
                        readonly property bool revealed: index < root._visibleCount
                        visible: revealed

                        // ── Slide + fade animation ────────────────────
                        // Opacity animates from 0 to 1; x from -8 to 0.
                        // The Behavior on opacity handles the fade-in;
                        // x translation uses a SequentialAnimation inside
                        // onRevealedChanged so it resets properly.
                        property real _opacity: revealed ? 1.0 : 0.0
                        property real _xOffset: revealed ? 0.0 : -8.0

                        opacity: _opacity
                        x: _xOffset

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 180
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on x {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutQuad
                            }
                        }

                        // ── Line text ─────────────────────────────────
                        // 5WHY: Text is rendered with word-wrap off
                        // (no wrapMode) to preserve terminal output layout.
                        // Terminal output typically has fixed-width columns
                        // and newlines are explicit; wrapping would break
                        // alignment (e.g. traceroute hop columns).
                        Label {
                            id: lineLabel
                            anchors {
                                left: parent.left
                                right: parent.right
                            }
                            text: modelData
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: 11
                            // 5WHY: 绿色终端美学；暗色 passGreen(#4ADE80)，
                            // 亮色用深翡翠 #047857（在浅底上 ~4.5:1，WCAG AA）
                            color: ThemeEngine.isDark
                                ? ThemeEngine.colors.passGreen : "#047857"
                            elide: Text.ElideNone
                            maximumLineCount: 1
                        }
                    }
                }
            }
        }
    }

    // ── Accessibility ─────────────────────────────────────────────────────
    Accessible.name: "Terminal output"
    Accessible.role: Accessible.StaticText
    // 5WHY: Accessible.description provides the full text content for screen
    // readers even while the typewriter animation is in progress — the
    // visual partial reveal doesn't block AT from reading the complete output.
    Accessible.description: root.text
}
