// CardTopAccent.qml — 2px colored accent bar for dialog cards (Material Design 3)
// 5WHY: The 2px top accent bar was duplicated in all 4 capture overlay files
// with only the color varying.  Extract once so the height, radius, and
// anchoring are a single design decision.
import QtQuick

Rectangle {
    anchors { top: parent.top; left: parent.left; right: parent.right }
    height: 2; radius: 2
    color: "transparent"  // overridden at call site
}
