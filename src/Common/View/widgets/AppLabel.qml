// =============================================================================
// AppLabel.qml — Label with RTL-aware default text alignment
//
// Qt Quick's `Label` defaults to Text.AlignLeft regardless of language.  For
// right-to-left scripts (Arabic) a full-width label must hug the START edge
// (right in RTL).  This thin wrapper applies T.textAlignStart (left in LTR,
// right in RTL) by default.  Use it for full-width text labels; centered and
// short content-sized labels keep using plain Label (mirroring positions them).
//
// Elide direction is still set per-usage (T.textElideStart) where needed.
// =============================================================================
import QtQuick
import QtQuick.Controls
import "../theme"

Label {
    horizontalAlignment: T.textAlignStart
}
