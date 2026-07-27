// ShadowText.qml — QtQuick Text with built-in Text.Raised drop-shadow
// Eliminates 5x duplication of style: Text.Raised + styleColor: "#80000000".
// Light-source convention: light from top-left, shadow to bottom-right.
import QtQuick

Text {
    style: Text.Raised
    styleColor: "#80000000"
}
