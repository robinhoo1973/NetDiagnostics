import QtQuick

// ── Shared font loading — used by main.qml ──
// Place one instance in each ApplicationWindow root; Qt deduplicates
// the underlying TTF registrations across windows.
Item {
    FontLoader { id: jetbrainsMonoRegular; source: "qrc:/fonts/JetBrainsMono-Regular.ttf" }
    FontLoader { id: jetbrainsMonoBold;    source: "qrc:/fonts/JetBrainsMono-Bold.ttf" }
    FontLoader { id: dejavuMono;           source: "qrc:/fonts/DejaVuSansMono.ttf" }
}
