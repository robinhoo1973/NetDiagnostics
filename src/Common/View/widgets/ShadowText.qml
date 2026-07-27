// ShadowText.qml — QtQuick Text with built-in Text.Raised drop-shadow
// Eliminates 5x duplication of style: Text.Raised + styleColor: "#80000000".
// Light-source convention: light from top-left, shadow to bottom-right.
import QtQuick
import "../theme" as T

Text {
    style: Text.Raised
    styleColor: "#80000000"
    // 5WHY: Text (QQuickText) is an Item, not a QtQuick.Controls Control.
    // Qt Quick Controls 2 font propagation (ApplicationWindow.font) only
    // reaches Control-derived types (Label, Button).  Text needs its own
    // explicit font.family or it renders in the system proportional default.
    // Use ThemeEngine.monoFont (the centralized monospace font configuration)
    // so a font change updates ALL components uniformly.
    font.family: T.ThemeEngine.monoFont
}
