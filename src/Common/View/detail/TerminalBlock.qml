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
    // 主题自适应终端底色（共用 helper，见 ThemeEngine.terminalBg 7-5 修复注释）
    property color terminalColor: ThemeEngine.terminalBg   // 属性（主题切换可响应）

    implicitWidth: 300
    // 5WHY: implicitHeight must track the dynamic content height so parent
    // ColumnLayouts can size correctly.  Using contentCol.implicitHeight
    // ensures this works on iOS static builds where anchors-positioned
    // children can collapse to zero height.
    implicitHeight: Math.max(60, contentCol.implicitHeight + 2 * padding)

    // 5WHY (review 2026-08-17, RTL): 终端输出是 LTR-forever 内容（列对齐、
    // 时间戳、IP）——阿拉伯语界面全局镜像会把命令行右对齐/从右滑入。
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: false

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

    // 全文本的 TextMetrics（5WHY review round 3: advanceWidth 对多行文本
    // 返回最宽行——无需 JS 逐行扫描 _widestLine 的派生状态）
    TextMetrics {
        id: widestMetrics
        font.family: ThemeEngine.monoFont
        font.pixelSize: 11
        text: root.text
    }

    // ── Visual ────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        // 5WHY: Terminal output must always render on a dark background for
        // readability of success monospace text.  Use the public terminalColor
        // property so embedders can override (e.g., DetailPage sets it for
        // light-theme coherence).
        color: root.terminalColor
        radius: ThemeEngine.radius.md  // 8
        border {
            width: 1
            color: ThemeEngine.colors.outlineVariant
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
            // 5WHY (review 2026-08-17): 长行（TLS 证书、curl verbose 头部）在
            // 窄屏/手机上超出卡片宽度且无换行——必须提供水平滚动路径。
            flickableDirection: Flickable.AutoFlickIfNeeded
            // 5WHY: interactive only when content overflows — prevents the
            // Flickable from stealing touch events when all lines fit.
            interactive: contentHeight > height || contentWidth > width

            Column {
                id: contentCol
                // 宽度=视口与最长行中的较大者（水平滚动空间由 contentWidth 提供）
                width: Math.max(flick.width, widestMetrics.advanceWidth)
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
                            horizontalAlignment: Text.AlignLeft
                            font.family: ThemeEngine.monoFont
                            font.pixelSize: 11
                            // 终端输出文本令牌：暗色 success(#4ADE80)；亮色深翡翠
                            // #047857（浅底 ~4.5:1 WCAG AA）。5WHY review 2026-08-17：
                            // 亮色分支曾是裸字面量，主题重调不会传播——现用
                            // Palette.js 的 terminalText 角色统一取色。
                            color: ThemeEngine.colors.terminalText
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
